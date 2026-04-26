#pragma once

#include <QObject>
#include <QSize>
#include <QTimer>

class FFmpegDecoder;
class QtAudioOutput;
class SoftwareVideoRenderer;

class MediaPlayer : public QObject {
  Q_OBJECT

public:
  enum State {
    Stopped,
    Loading,
    Ready,
    Playing,
    Paused
  };
  Q_ENUM(State)

  explicit MediaPlayer(QObject *parent = nullptr);
  ~MediaPlayer() override;

  bool load(const QString &path);
  void play();
  void pause();
  void stop();
  void seek(qint64 ms);
  void stepForward();
  void stepBackward();

  State state() const { return state_; }
  qint64 currentTimeMs() const { return currentTimeMs_; }

signals:
  void stateChanged(State state);
  void mediaLoaded(qint64 durationMs, QSize videoSize);
  void timeChanged(qint64 ms);
  void playbackFinished();
  void playbackError(const QString &error);

private slots:
  void onPlaybackTimer();
  void onDecoderError(const QString &error);
  void onEndOfStream();

private:
  FFmpegDecoder *decoder_ = nullptr;
  QtAudioOutput *audioOutput_ = nullptr;
  SoftwareVideoRenderer *videoRenderer_ = nullptr;
  QTimer *playbackTimer_ = nullptr;

  qint64 currentTimeMs_ = 0;
  State state_ = Stopped;
  int droppedFrames_ = 0;
  int renderedFrames_ = 0;
};
