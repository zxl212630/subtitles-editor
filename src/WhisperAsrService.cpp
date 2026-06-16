#include "WhisperAsrService.h"
#include "ConfigManager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QVector>
#include <whisper.h>

WhisperAsrService::WhisperAsrService(QObject *parent)
    : AsrServiceBase(parent) {}

WhisperAsrService::~WhisperAsrService() {
  abort();
  if (activeThread_) {
    activeThread_->wait(3000);
    if (activeThread_->isRunning()) {
      activeThread_->terminate();
    }
  }
}

void WhisperAsrService::transcribe(const QString &audioFilePath) {
  {
    QMutexLocker locker(&mutex_);
    isAborted_ = false;
  }

  QThread *thread = QThread::create(
      [this, audioFilePath]() { runTranscribe(audioFilePath); });
  activeThread_ = thread;

  connect(thread, &QThread::finished, thread, &QThread::deleteLater);
  thread->start();
}

void WhisperAsrService::abort() {
  QMutexLocker locker(&mutex_);
  isAborted_ = true;
}

bool WhisperAsrService::isAborted() const {
  QMutexLocker locker(&mutex_);
  return isAborted_;
}

void WhisperAsrService::runTranscribe(const QString &audioFilePath) {
  TranscriptResult result;
  result.success = false;

  if (isAborted()) {
    result.errorMessage = tr("识别已被用户取消");
    emit transcribeFinished(result);
    return;
  }

  // 1. 读取并解析 WAV 文件 (16kHz, 16-bit, Mono PCM)
  QFile file(audioFilePath);
  if (!file.open(QIODevice::ReadOnly)) {
    result.errorMessage = tr("无法打开音频文件: %1").arg(audioFilePath);
    emit transcribeFinished(result);
    return;
  }

  QByteArray data = file.readAll();
  file.close();

  const char *bytes = data.constData();
  int size = data.size();

  if (size < 44 || qstrncmp(bytes, "RIFF", 4) != 0 ||
      qstrncmp(bytes + 8, "WAVE", 4) != 0) {
    result.errorMessage = tr("非法的 WAV 音频格式，必须是 RIFF WAVE 格式");
    emit transcribeFinished(result);
    return;
  }

  int pos = 12;
  int channels = 0;
  int sampleRate = 0;
  int bitsPerSample = 0;
  const char *pcmBytes = nullptr;
  int pcmSize = 0;

  while (pos + 8 <= size) {
    const char *chunkId = bytes + pos;
    uint32_t chunkSize = *reinterpret_cast<const uint32_t *>(bytes + pos + 4);
    pos += 8;

    if (qstrncmp(chunkId, "fmt ", 4) == 0) {
      if (pos + 16 <= size) {
        channels = *reinterpret_cast<const uint16_t *>(bytes + pos + 2);
        sampleRate = *reinterpret_cast<const uint32_t *>(bytes + pos + 4);
        bitsPerSample = *reinterpret_cast<const uint16_t *>(bytes + pos + 14);
      }
    } else if (qstrncmp(chunkId, "data", 4) == 0) {
      pcmBytes = bytes + pos;
      pcmSize = qMin(chunkSize, static_cast<uint32_t>(size - pos));
      break;
    }
    pos += chunkSize;
  }

  if (!pcmBytes || pcmSize <= 0) {
    result.errorMessage = tr("未在 WAV 文件中找到音频 data 数据块");
    emit transcribeFinished(result);
    return;
  }

  if (channels != 1 || sampleRate != 16000 || bitsPerSample != 16) {
    result.errorMessage =
        tr("不支持的音频格式！Whisper 要求 16kHz, 16-bit, 单声道 (Mono) "
           "WAV。当前音频为: %1 声道, %2Hz, %3-bit")
            .arg(channels)
            .arg(sampleRate)
            .arg(bitsPerSample);
    emit transcribeFinished(result);
    return;
  }

  int numSamples = pcmSize / 2;
  QVector<float> floatSamples(numSamples);
  const int16_t *int16Samples = reinterpret_cast<const int16_t *>(pcmBytes);
  for (int i = 0; i < numSamples; ++i) {
    floatSamples[i] = static_cast<float>(int16Samples[i]) / 32768.0f;
  }

  if (isAborted()) {
    result.errorMessage = tr("识别已被用户取消");
    emit transcribeFinished(result);
    return;
  }

  // 2. 初始化 Whisper 模型上下文
  auto &cfg = ConfigManager::instance();
  QString modelName = cfg.whisperModel();
  QString modelPath =
      cfg.whisperModelPath() + QString("/ggml-%1.bin").arg(modelName);

  if (!QFile::exists(modelPath)) {
    result.errorMessage =
        tr("找不到指定的模型文件，请先下载: %1").arg(modelPath);
    emit transcribeFinished(result);
    return;
  }

  emit transcribeProgress(5);

  struct whisper_context_params cparams = whisper_context_default_params();
  cparams.use_gpu = true;

  {
    QMutexLocker locker(&mutex_);
    ctx_ = whisper_init_from_file_with_params(modelPath.toUtf8().constData(),
                                              cparams);
  }

  if (!ctx_) {
    result.errorMessage =
        tr("初始化 Whisper 核心上下文失败，模型文件可能已损坏: %1")
            .arg(modelPath);
    emit transcribeFinished(result);
    return;
  }

  if (isAborted()) {
    result.errorMessage = tr("识别已被用户取消");
    whisper_free(ctx_);
    ctx_ = nullptr;
    emit transcribeFinished(result);
    return;
  }

  // 3. 配置推理参数并开始识别
  struct whisper_full_params wparams =
      whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  wparams.print_realtime = false;
  wparams.print_progress = false;
  wparams.print_timestamps = false;
  wparams.print_special = false;
  wparams.translate = false;

  QString lang = cfg.whisperLanguage();
  QByteArray langBytes = lang.toUtf8();
  wparams.language = (lang == "auto") ? nullptr : langBytes.constData();
  wparams.n_threads = cfg.whisperThreads();

  // 挂载进度条回调
  wparams.progress_callback = [](struct whisper_context * /*ctx*/,
                                 struct whisper_state * /*state*/, int progress,
                                 void *user_data) {
    auto *service = static_cast<WhisperAsrService *>(user_data);
    // 映射 Whisper 的 0-100% 进度到 ASR 管线的 5%-95% 区间
    int percent = 5 + (progress * 90) / 100;
    emit service->transcribeProgress(percent);
  };
  wparams.progress_callback_user_data = this;

  // 挂载用户取消回调
  wparams.abort_callback = [](void *user_data) {
    auto *service = static_cast<WhisperAsrService *>(user_data);
    return service->isAborted();
  };
  wparams.abort_callback_user_data = this;

  int ret = whisper_full(ctx_, wparams, floatSamples.constData(),
                         floatSamples.size());

  if (isAborted()) {
    result.errorMessage = tr("识别已被用户取消");
    whisper_free(ctx_);
    ctx_ = nullptr;
    emit transcribeFinished(result);
    return;
  }

  if (ret != 0) {
    result.errorMessage = tr("Whisper 语音推理执行失败，代码: %1").arg(ret);
    whisper_free(ctx_);
    ctx_ = nullptr;
    emit transcribeFinished(result);
    return;
  }

  // 4. 解析结果切片
  int n_segments = whisper_full_n_segments(ctx_);
  for (int i = 0; i < n_segments; ++i) {
    if (isAborted())
      break;

    TranscriptSegment seg;
    seg.text = QString::fromUtf8(whisper_full_get_segment_text(ctx_, i));
    seg.startMs = whisper_full_get_segment_t0(ctx_, i) * 10;
    seg.endMs = whisper_full_get_segment_t1(ctx_, i) * 10;
    seg.speakerId = -1;

    if (!seg.text.trimmed().isEmpty()) {
      result.segments.append(seg);
    }
  }

  whisper_free(ctx_);
  ctx_ = nullptr;

  if (isAborted()) {
    result.errorMessage = tr("识别已被用户取消");
    emit transcribeFinished(result);
  } else {
    result.success = true;
    emit transcribeProgress(100);
    emit transcribeFinished(result);
  }
}
