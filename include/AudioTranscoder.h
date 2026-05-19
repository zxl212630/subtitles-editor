#pragma once

#include <QObject>
#include <QString>

class QProcess;

class AudioTranscoder : public QObject {
  Q_OBJECT

public:
  explicit AudioTranscoder(QObject *parent = nullptr);
  void abort();

signals:
  void transcodingStarted();
  void transcodingFinished(const QString &outputPath);
  void transcodingFailed(const QString &errorMessage);
  void progress(int percent);

public slots:
  void transcode(const QString &inputPath);

private:
  QString generateOutputPath(const QString &inputPath);
  void parseProgress(const QString &line, int durationMs);
  QString ffmpegPath_;
  QProcess *process_ = nullptr;
  int durationMs_ = 0;
};