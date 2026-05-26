#pragma once

#include "FFmpegDecoder.h"

#include <QElapsedTimer>
#include <QObject>
#include <QSize>
#include <QTimer>
#include <optional>

class FFmpegDecoder;
class SeekDecoder;
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
  void clear();
  void seek(qint64 ms);
  void previewSeek(qint64 ms);
  void stopPreviewDragging();
  void stepForward();
  void stepBackward();

  void setTotalDurationLimit(qint64 ms);
  qint64 totalDurationLimit() const;

  void setVideoRenderer(SoftwareVideoRenderer *renderer);

  void setVolume(qreal volume);
  qreal volume() const { return volume_; }
  void setMuted(bool muted);
  bool isMuted() const { return isMuted_; }

  State state() const { return state_; }
  qint64 currentTimeMs() const { return currentTimeMs_; }
  double decoderFps() const;
  QSize videoSize() const;
  qint64 durationMs() const;
  QString videoCodecName() const;
  QString audioCodecName() const;
  qint64 videoBitRate() const;
  qint64 audioBitRate() const;
  int audioBitDepth() const;
  QString mediaCreationTime() const;
  int audioSampleRate() const;
  int audioChannels() const;

signals:
  void stateChanged(State state);
  void mediaLoaded(qint64 durationMs, QSize videoSize);
  void timeChanged(qint64 ms);
  void playbackFinished();
  void playbackError(const QString &error);
  void volumeChanged(qreal volume, bool muted);

private slots:
  void onPlaybackTimer();
  void onDecoderError(const QString &error);
  void onEndOfStream();
  void executePendingSeek();
  void onSeekFrameReady(DecodedVideoFrame frame);
  void onPlaybackFinished();

private:
  FFmpegDecoder *decoder_ = nullptr;
  SeekDecoder *seekDecoder_ = nullptr;
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
  bool isPreviewDragging_ = false;
  bool previewFrameRendered_ = false;
  qint64 lastRenderedPreviewPts_ = -1;

  QElapsedTimer previewE2eTimer_;

  QTimer *seekCoalesceTimer_ = nullptr;
  qint64 pendingSeekMs_ = 0;
  bool hasPendingSeek_ = false;

  qreal volume_ = 1.0;
  bool isMuted_ = false;
  qint64 totalDurationLimitMs_ = 0;
};
