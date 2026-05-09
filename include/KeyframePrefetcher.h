#pragma once

#include "FFmpegDecoder.h"
#include <QThread>
#include <atomic>
#include <optional>

class KeyframePrefetcher : public QObject {
  Q_OBJECT

public:
  explicit KeyframePrefetcher(QObject *parent = nullptr);
  ~KeyframePrefetcher() override;

  bool init(const QString &path, int videoStreamIdx, AVRational timeBase,
            QSize videoSize);
  void shutdown();

  void requestPrefetch(qint64 targetMs);
  std::optional<DecodedVideoFrame> takeResult();

signals:
  void prefetchComplete(qint64 targetMs);

private:
  void runPrefetch(qint64 targetMs);

  AVFormatContext *fmtCtx_ = nullptr;
  AVCodecContext *videoCodecCtx_ = nullptr;
  SwsContext *swsCtx_ = nullptr;
  int videoStreamIdx_ = -1;
  AVRational videoTimeBase_{0, 0};
  QSize videoSize_;

  std::atomic<bool> busy_{false};
  std::atomic<qint64> prefetchedTargetMs_{-1};

  QMutex resultMutex_;
  QMutex swsMutex_;
  DecodedVideoFrame prefetchedFrame_;
  bool hasResult_ = false;
};
