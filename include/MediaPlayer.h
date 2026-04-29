#pragma once

#include "FFmpegDecoder.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSize>
#include <QTimer>
#include <optional>

class FFmpegDecoder;
class QtAudioOutput;
class SoftwareVideoRenderer;

class MediaPlayer : public QObject {
  Q_OBJECT

public:
  enum State { Stopped, Loading, Ready, Playing, Paused };
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

  void setVideoRenderer(SoftwareVideoRenderer *renderer);

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
  std::optional<DecodedVideoFrame> pendingVideoFrame_;

  QElapsedTimer playbackElapsedTimer_;
  qint64 playbackStartTimeMs_ = 0;
  bool playbackTimerRunning_ = false;

  bool seekPreviewMode_ = false;
  QElapsedTimer seekPreviewTimer_;
  qint64 seekTargetMs_ = 0;
};
