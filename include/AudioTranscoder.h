#pragma once

#include <QObject>
#include <QString>

class AudioTranscoder : public QObject {
  Q_OBJECT

public:
  explicit AudioTranscoder(QObject *parent = nullptr);

signals:
  void transcodingStarted();
  void transcodingFinished(const QString &outputPath);
  void transcodingFailed(const QString &errorMessage);

public slots:
  void transcode(const QString &inputPath);

private:
  QString generateOutputPath(const QString &inputPath);
  QString ffmpegPath_;
};