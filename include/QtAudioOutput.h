#pragma once

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <QObject>

class QtAudioOutput : public QObject {
  Q_OBJECT

public:
  explicit QtAudioOutput(QObject *parent = nullptr);
  ~QtAudioOutput() override;

  bool open(int sampleRate, int channels);
  void close();

  void write(const void *data, size_t size);
  void flush();

  qint64 samplesPlayed() const;
  void setVolume(qreal volume);
  bool isOpen() const;

private:
  QAudioFormat format_;
  QAudioSink *audioSink_ = nullptr;
  QIODevice *ioDevice_ = nullptr;
  qint64 totalBytesWritten_ = 0;
  int sampleRate_ = 0;
  int channels_ = 0;
};
