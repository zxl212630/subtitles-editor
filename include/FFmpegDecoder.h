#pragma once

#include <QByteArray>
#include <QMutex>
#include <QQueue>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <optional>

struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;
struct SwrContext;
struct AVPacket;
struct AVFrame;

extern "C" {
#include <libavutil/rational.h>
#include <libavutil/samplefmt.h>
}

struct DecodedVideoFrame {
  qint64 ptsMs = 0;
  int width = 0;
  int height = 0;
  QByteArray rgbaData;
};

struct DecodedAudioFrame {
  qint64 ptsMs = 0;
  QByteArray pcmData;
  int sampleRate = 0;
  int channels = 0;
};

class FFmpegDecoder : public QThread {
  Q_OBJECT

public:
  explicit FFmpegDecoder(QObject *parent = nullptr);
  ~FFmpegDecoder() override;

  bool open(const QString &path);
  void close();
  void setCancelOpen(bool cancel);

  void requestSeek(qint64 targetMs);
  void setPlaying(bool playing);
  void stop();

  std::optional<DecodedVideoFrame> dequeueVideoFrame();
  std::optional<DecodedAudioFrame> dequeueAudioFrame();
  int videoQueueSize() const;
  int audioQueueSize() const;
  qint64 videoQueueDurationMs() const;
  qint64 audioQueueDurationMs() const;
  void clearAudioQueue();
  void clearVideoQueue();
  void clearAllQueues();

  qint64 durationMs() const;
  double fps() const;
  QSize videoSize() const;
  bool hasVideo() const;
  bool hasAudio() const;
  int audioSampleRate() const;
  int audioChannels() const;
  QString videoCodecName() const;
  QString audioCodecName() const;
  qint64 videoBitRate() const;
  qint64 audioBitRate() const;
  int audioBitDepth() const;
  QString mediaCreationTime() const;

signals:
  void decodeError(const QString &message);
  void endOfStream();

protected:
  void run() override;

private:
  void performSeek(qint64 targetMs);
  void clearQueues();
  bool decodeVideoPacket(AVPacket *packet);
  bool decodeAudioPacket(AVPacket *packet);
  void convertAudioFrame(AVFrame *frame, DecodedAudioFrame &out);

  // FFmpeg contexts
  AVFormatContext *fmtCtx_ = nullptr;
  AVCodecContext *videoCodecCtx_ = nullptr;
  AVCodecContext *audioCodecCtx_ = nullptr;
  SwsContext *swsCtx_ = nullptr;
  SwrContext *audioSwrCtx_ = nullptr;
  int swrSampleRate_ = 0;
  int swrChannels_ = 0;
  AVSampleFormat swrFormat_ = AV_SAMPLE_FMT_NONE;

  int videoStreamIdx_ = -1;
  int audioStreamIdx_ = -1;
  AVRational videoTimeBase_{0, 0};
  AVRational audioTimeBase_{0, 0};

  // Metadata
  qint64 durationMs_ = 0;
  double fps_ = 0.0;
  QSize videoSize_;
  int audioSampleRate_ = 0;
  int audioChannels_ = 0;
  bool hasVideo_ = false;
  bool hasAudio_ = false;
  QString videoCodecName_;
  QString audioCodecName_;
  qint64 videoBitRate_ = 0;
  qint64 audioBitRate_ = 0;
  int audioBitDepth_ = 0;
  QString mediaCreationTime_;

  // Thread control
  std::atomic<bool> running_{false};
  std::atomic<bool> playing_{false};
  std::atomic<bool> seekRequested_{false};
  std::atomic<qint64> seekTargetMs_{0};
  std::atomic<bool> cancelOpen_{false};
  static int decodeInterruptCb(void *ctx);

  QMutex playControlMutex_;
  QWaitCondition playCondition_;

  mutable QMutex metadataMutex_;

  // Queues
  mutable QMutex videoQueueMutex_;
  mutable QMutex audioQueueMutex_;
  QQueue<DecodedVideoFrame> videoQueue_;
  QQueue<DecodedAudioFrame> audioQueue_;

  // Reusable decode buffers (decoder thread only)
  AVFrame *reusableFrame_ = nullptr;
  QByteArray reusableRgbaBuffer_;

  QMutex queueFullMutex_;
  QWaitCondition queueNotFull_;

  bool discardBeforeTarget_ = false;

  static constexpr int MAX_VIDEO_QUEUE_MS = 500;
  static constexpr int MAX_AUDIO_QUEUE_MS = 500;
};
