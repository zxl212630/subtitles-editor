#include "AudioTranscoder.h"
#include "ConfigManager.h"
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

AudioTranscoder::AudioTranscoder(QObject *parent) : QObject(parent) {
  ffmpegPath_ = ConfigManager::instance().ffmpegPath();
  if (ffmpegPath_.isEmpty()) {
    ffmpegPath_ = "/Users/zxl/Tools/ffmpeg/8.0/bin/ffmpeg";
  }
}

void AudioTranscoder::abort() {
  if (process_ && process_->state() != QProcess::NotRunning) {
    process_->kill();
    process_->waitForFinished(3000);
    emit transcodingFailed(tr("用户已取消转码"));
  }
}

QString AudioTranscoder::generateOutputPath(const QString &inputPath) {
  QFileInfo info(inputPath);
  QString basePath = info.absolutePath();
  QString baseName = info.baseName();
  return basePath + "/" + baseName + "_16k.wav";
}

void AudioTranscoder::parseProgress(const QString &line, int durationMs) {
  if (durationMs <= 0)
    return;

  // FFmpeg outputs time in format: time=00:01:23.45
  static QRegularExpression timeRegex(
      "time=(\\d{2}):(\\d{2}):(\\d{2})\\.(\\d{2})");
  QRegularExpressionMatch match = timeRegex.match(line);
  if (match.hasMatch()) {
    int hours = match.captured(1).toInt();
    int minutes = match.captured(2).toInt();
    int seconds = match.captured(3).toInt();
    int centiseconds = match.captured(4).toInt();
    int currentMs =
        hours * 3600000 + minutes * 60000 + seconds * 1000 + centiseconds * 10;
    int percent = qBound(0, (currentMs * 100) / durationMs, 100);
    emit progress(percent);
  }
}

void AudioTranscoder::transcode(const QString &inputPath) {
  if (process_ && process_->state() != QProcess::NotRunning) {
    emit transcodingFailed(tr("转码任务正在进行中"));
    return;
  }

  QString outputPath = generateOutputPath(inputPath);

  QStringList args;
  args << "-i" << inputPath << "-ar"
       << "16000" // 16k sampling rate
       << "-ac"
       << "1" // mono
       << "-acodec"
       << "pcm_s16le"
       << "-y" // overwrite output
       << outputPath;

  process_ = new QProcess(this);
  durationMs_ = 0;

  connect(process_, &QProcess::readyReadStandardError, this, [this]() {
    if (!process_)
      return;
    QString output = QString::fromUtf8(process_->readAllStandardError());

    // Parse duration from first pass
    if (durationMs_ == 0) {
      static QRegularExpression durationRegex(
          "Duration:\\s*(\\d{2}):(\\d{2}):(\\d{2})\\.(\\d{2})");
      QRegularExpressionMatch match = durationRegex.match(output);
      if (match.hasMatch()) {
        int hours = match.captured(1).toInt();
        int minutes = match.captured(2).toInt();
        int seconds = match.captured(3).toInt();
        int centiseconds = match.captured(4).toInt();
        durationMs_ = hours * 3600000 + minutes * 60000 + seconds * 1000 +
                      centiseconds * 10;
      }
    }

    // Parse current progress
    QStringList lines = output.split('\n');
    for (const QString &line : lines) {
      parseProgress(line, durationMs_);
    }
  });

  connect(
      process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      this, [this, outputPath](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
          emit progress(100);
          emit transcodingFinished(outputPath);
        } else {
          QString error = QString::fromUtf8(process_->readAllStandardError());
          emit transcodingFailed(tr("转码失败: ") + error);
        }
        process_->deleteLater();
        process_ = nullptr;
      });

  connect(process_, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            emit transcodingFailed(tr("进程错误: ") + QString::number(error));
            process_->deleteLater();
            process_ = nullptr;
          });

  emit transcodingStarted();
  process_->start(ffmpegPath_, args);
}