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

  bool isChinese =
      (lang == "zh" || (lang == "auto" && cfg.language() == "zh_CN"));

  // 启用词级时间戳，由后处理分词做参考
  wparams.token_timestamps = true;
  // 中文没有空格分词，若启用 split_on_word 会导致整句被当成一个词而不切分。
  // 因此中文需设为 false（按字/Token 切分），其他语言设为 true。
  wparams.split_on_word = !isChinese;
  // 设为 0 以输出原始完整自然段，我们在后处理中用 Qt 对 QString
  // 进行无损的标点与字数切分，避免中文字符截断乱码
  wparams.max_len = 0;

  // 根据语言偏好，自动注入简体中文提示词（Prompt）以避免简繁混杂
  if (isChinese) {
    wparams.initial_prompt = "以下是简体中文内容。";
  }

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

  // 4. 解析结果切片并进行标点与字数后处理切分
  int maxLen = cfg.whisperMaxLen();
  int n_segments = whisper_full_n_segments(ctx_);
  whisper_token eotToken = whisper_token_eot(ctx_);

  for (int i = 0; i < n_segments; ++i) {
    if (isAborted())
      break;

    int n_tokens = whisper_full_n_tokens(ctx_, i);
    if (n_tokens <= 0) {
      continue;
    }

    struct Clause {
      QByteArray bytes;
      int64_t t0 = -1;
      int64_t t1 = -1;
    };

    QList<Clause> clauses;
    Clause currentClause;

    for (int j = 0; j < n_tokens; ++j) {
      whisper_token id = whisper_full_get_token_id(ctx_, i, j);
      if (id >= eotToken) {
        continue; // 过滤特殊标记 token
      }

      whisper_token_data tokenData = whisper_full_get_token_data(ctx_, i, j);
      const char *rawText = whisper_full_get_token_text(ctx_, i, j);
      if (!rawText) {
        continue;
      }
      QByteArray tokenBytes(rawText);
      if (tokenBytes.isEmpty()) {
        continue;
      }

      QString tokenText = QString::fromUtf8(tokenBytes);
      bool isPunctuation = false;
      if (tokenText.length() == 1) {
        QChar ch = tokenText[0];
        if (ch == QChar(0xff0c) || // ，
            ch == QChar(0x3002) || // 。
            ch == QChar(0xff1f) || // ？
            ch == QChar(0xff01) || // ！
            ch == QChar(0x3001) || // 、
            ch == QChar(0xff1b) || // ；
            ch == QChar(0xff1a) || // ：
            ch == ',' || ch == '.' || ch == '?' || ch == '!' || ch == ';' ||
            ch == ':') {
          isPunctuation = true;
        }
      }

      if (isPunctuation) {
        // 标点符号触发分句，并且不保留该标点符号
        if (!currentClause.bytes.isEmpty()) {
          QString decoded = QString::fromUtf8(currentClause.bytes).trimmed();
          if (!decoded.isEmpty()) {
            clauses.append(currentClause);
          }
        }
        currentClause = Clause();
      } else {
        // 检查加上当前 token 之后是否会超出单行最大字数
        QByteArray combinedBytes = currentClause.bytes + tokenBytes;
        QString combinedText = QString::fromUtf8(combinedBytes).trimmed();
        bool wouldExceed = (maxLen > 0 && combinedText.length() > maxLen);

        if (wouldExceed) {
          if (!currentClause.bytes.isEmpty()) {
            QString decoded = QString::fromUtf8(currentClause.bytes).trimmed();
            if (!decoded.isEmpty()) {
              clauses.append(currentClause);
            }
          }
          currentClause = Clause();
        }

        // 将当前 token 合并到当前子句中
        if (currentClause.bytes.isEmpty()) {
          // 略过开头的空白 token
          if (!tokenText.trimmed().isEmpty()) {
            currentClause.bytes = tokenBytes;
            currentClause.t0 = tokenData.t0;
            currentClause.t1 = tokenData.t1;
          }
        } else {
          currentClause.bytes += tokenBytes;
          if (currentClause.t0 == -1) {
            currentClause.t0 = tokenData.t0;
          }
          if (tokenData.t1 != -1) {
            currentClause.t1 = tokenData.t1;
          }
        }
      }
    }

    if (!currentClause.bytes.isEmpty()) {
      QString decoded = QString::fromUtf8(currentClause.bytes).trimmed();
      if (!decoded.isEmpty()) {
        clauses.append(currentClause);
      }
    }

    // 将子句输出为字幕切片
    for (const auto &clause : clauses) {
      TranscriptSegment seg;
      seg.text = QString::fromUtf8(clause.bytes).trimmed();

      // 使用精确的 token 级起始时间，如果没有则回退到段落时间
      int64_t startVal =
          (clause.t0 != -1) ? clause.t0 : whisper_full_get_segment_t0(ctx_, i);
      int64_t endVal = clause.t1;
      if (endVal == -1) {
        if (clause.t0 != -1) {
          endVal = clause.t0 + 100; // 默认给 1 秒
        } else {
          endVal = whisper_full_get_segment_t1(ctx_, i);
        }
      }

      int64_t segmentEnd = whisper_full_get_segment_t1(ctx_, i);
      if (endVal > segmentEnd) {
        endVal = segmentEnd; // 确保不超出段落范围
      }

      seg.startMs = startVal * 10;
      seg.endMs = endVal * 10;
      seg.speakerId = -1;

      if (!seg.text.isEmpty() && seg.endMs > seg.startMs) {
        result.segments.append(seg);
      }
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
