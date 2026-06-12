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

#ifdef Q_OS_MAC
#include <CoreVideo/CoreVideo.h>
#endif

struct DecodedVideoFrame {
  qint64 ptsMs = 0;
  qint64 targetMs = 0;
  int width = 0;
  int height = 0;
  QByteArray rgbaData;
  void *hwFrame = nullptr; // CVPixelBufferRef on macOS
  double qualityScale = 1.0;

  DecodedVideoFrame() = default;
  ~DecodedVideoFrame() {
#ifdef Q_OS_MAC
    if (hwFrame) {
      CVPixelBufferRelease(static_cast<CVPixelBufferRef>(hwFrame));
    }
#endif
  }

  DecodedVideoFrame(const DecodedVideoFrame &other)
      : ptsMs(other.ptsMs), targetMs(other.targetMs), width(other.width),
        height(other.height), rgbaData(other.rgbaData), hwFrame(other.hwFrame),
        qualityScale(other.qualityScale) {
#ifdef Q_OS_MAC
    if (hwFrame) {
      CVPixelBufferRetain(static_cast<CVPixelBufferRef>(hwFrame));
    }
#endif
  }

  DecodedVideoFrame(DecodedVideoFrame &&other) noexcept
      : ptsMs(other.ptsMs), targetMs(other.targetMs), width(other.width),
        height(other.height), rgbaData(std::move(other.rgbaData)),
        hwFrame(other.hwFrame), qualityScale(other.qualityScale) {
    other.hwFrame = nullptr;
  }

  DecodedVideoFrame &operator=(const DecodedVideoFrame &other) {
    if (this != &other) {
#ifdef Q_OS_MAC
      if (hwFrame) {
        CVPixelBufferRelease(static_cast<CVPixelBufferRef>(hwFrame));
      }
#endif
      ptsMs = other.ptsMs;
      targetMs = other.targetMs;
      width = other.width;
      height = other.height;
      rgbaData = other.rgbaData;
      hwFrame = other.hwFrame;
      qualityScale = other.qualityScale;
#ifdef Q_OS_MAC
      if (hwFrame) {
        CVPixelBufferRetain(static_cast<CVPixelBufferRef>(hwFrame));
      }
#endif
    }
    return *this;
  }

  DecodedVideoFrame &operator=(DecodedVideoFrame &&other) noexcept {
    if (this != &other) {
#ifdef Q_OS_MAC
      if (hwFrame) {
        CVPixelBufferRelease(static_cast<CVPixelBufferRef>(hwFrame));
      }
#endif
      ptsMs = other.ptsMs;
      targetMs = other.targetMs;
      width = other.width;
      height = other.height;
      rgbaData = std::move(other.rgbaData);
      hwFrame = other.hwFrame;
      qualityScale = other.qualityScale;
      other.hwFrame = nullptr;
    }
    return *this;
  }
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
  void setHardwareDecodeEnabled(bool enabled);
  void setVideoQuality(double scale);
  double videoQuality() const;
  void setOutputSize(const QSize &size);

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
  bool hwDecodeEnabled_ = true;
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

  QMutex queueFullMutex_;
  QWaitCondition queueNotFull_;

  bool discardBeforeTarget_ = false;
  qint64 lastEnqueuedVideoPts_ = -1;

  double qualityScale_ = 1.0;
  QSize outputSize_;
  int lastSwsW_ = -1;
  int lastSwsH_ = -1;
  int lastDstW_ = -1;
  int lastDstH_ = -1;
  int lastSwsFormat_ = -1;

  static constexpr int MAX_VIDEO_QUEUE_MS = 500;
  static constexpr int MAX_AUDIO_QUEUE_MS = 500;
};
