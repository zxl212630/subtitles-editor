#include "AudioTranscoder.h"
#include "ConfigManager.h"
#include <QFileInfo>
#include <QProcess>

AudioTranscoder::AudioTranscoder(QObject *parent) : QObject(parent) {
  ffmpegPath_ = ConfigManager::instance().ffmpegPath();
  if (ffmpegPath_.isEmpty()) {
    ffmpegPath_ = "/Users/zxl/Tools/ffmpeg/8.0/bin/ffmpeg";
  }
}

QString AudioTranscoder::generateOutputPath(const QString &inputPath) {
  QFileInfo info(inputPath);
  QString basePath = info.absolutePath();
  QString baseName = info.baseName();
  return basePath + "/" + baseName + "_16k.wav";
}

void AudioTranscoder::transcode(const QString &inputPath) {
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

  QProcess *process = new QProcess(this);
  connect(
      process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      this, [this, process, outputPath](int, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit) {
          emit transcodingFinished(outputPath);
        } else {
          QString error = QString::fromUtf8(process->readAllStandardError());
          emit transcodingFailed("Transcoding failed: " + error);
        }
        process->deleteLater();
      });

  emit transcodingStarted();
  process->start(ffmpegPath_, args);
}