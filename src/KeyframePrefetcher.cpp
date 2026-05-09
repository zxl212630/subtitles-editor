#include "KeyframePrefetcher.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QtConcurrent>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

KeyframePrefetcher::KeyframePrefetcher(QObject *parent) : QObject(parent) {}

KeyframePrefetcher::~KeyframePrefetcher() { shutdown(); }

bool KeyframePrefetcher::init(const QString &path, int videoStreamIdx,
                              AVRational timeBase, QSize videoSize) {
  shutdown();

  videoStreamIdx_ = videoStreamIdx;
  videoTimeBase_ = timeBase;
  videoSize_ = videoSize;

  int ret = avformat_open_input(&fmtCtx_, path.toUtf8().constData(), nullptr,
                                nullptr);
  if (ret < 0) {
    qWarning() << "[Prefetcher] Failed to open input";
    return false;
  }

  ret = avformat_find_stream_info(fmtCtx_, nullptr);
  if (ret < 0) {
    qWarning() << "[Prefetcher] Failed to find stream info";
    avformat_close_input(&fmtCtx_);
    return false;
  }

  AVStream *stream = fmtCtx_->streams[videoStreamIdx];
  const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec) {
    qWarning() << "[Prefetcher] Failed to find video decoder";
    avformat_close_input(&fmtCtx_);
    return false;
  }

  videoCodecCtx_ = avcodec_alloc_context3(codec);
  if (!videoCodecCtx_) {
    avformat_close_input(&fmtCtx_);
    return false;
  }

  ret = avcodec_parameters_to_context(videoCodecCtx_, stream->codecpar);
  if (ret < 0) {
    avcodec_free_context(&videoCodecCtx_);
    avformat_close_input(&fmtCtx_);
    return false;
  }

  ret = avcodec_open2(videoCodecCtx_, codec, nullptr);
  if (ret < 0) {
    avcodec_free_context(&videoCodecCtx_);
    avformat_close_input(&fmtCtx_);
    return false;
  }

  qInfo() << "[Prefetcher] Initialized for" << path;
  return true;
}

void KeyframePrefetcher::shutdown() {
  if (swsCtx_) {
    sws_freeContext(swsCtx_);
    swsCtx_ = nullptr;
  }
  if (videoCodecCtx_) {
    avcodec_free_context(&videoCodecCtx_);
  }
  if (fmtCtx_) {
    avformat_close_input(&fmtCtx_);
  }
  hasResult_ = false;
}

void KeyframePrefetcher::requestPrefetch(qint64 targetMs) {
  if (busy_.load()) {
    return;
  }
  busy_.store(true);
  prefetchedTargetMs_.store(targetMs);

  QFuture<void> future =
      QtConcurrent::run([this, targetMs]() { runPrefetch(targetMs); });

  QFutureWatcher<void> *watcher = new QFutureWatcher<void>(this);
  connect(watcher, &QFutureWatcher<void>::finished, this,
          [this, watcher, targetMs]() {
            watcher->deleteLater();
            busy_.store(false);
            {
              QMutexLocker lock(&resultMutex_);
              if (prefetchedFrame_.ptsMs >= 0) {
                hasResult_ = true;
              }
            }
            emit prefetchComplete(targetMs);
          });
  watcher->setFuture(future);
}

std::optional<DecodedVideoFrame> KeyframePrefetcher::takeResult() {
  QMutexLocker lock(&resultMutex_);
  if (!hasResult_) {
    return std::nullopt;
  }
  hasResult_ = false;
  auto result = std::move(prefetchedFrame_);
  prefetchedFrame_ = {};
  return result;
}

void KeyframePrefetcher::runPrefetch(qint64 targetMs) {
  if (!fmtCtx_ || !videoCodecCtx_) {
    return;
  }

  QElapsedTimer timer;
  timer.start();

  int64_t target = av_rescale_q(targetMs, AVRational{1, 1000}, AV_TIME_BASE_Q);
  int ret = avformat_seek_file(fmtCtx_, -1, INT64_MIN, target, target,
                               AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    return;
  }

  avcodec_flush_buffers(videoCodecCtx_);

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();

  while (av_read_frame(fmtCtx_, packet) >= 0) {
    if (packet->stream_index != videoStreamIdx_) {
      av_packet_unref(packet);
      continue;
    }

    if (!(packet->flags & AV_PKT_FLAG_KEY)) {
      av_packet_unref(packet);
      continue;
    }

    int sendRet = avcodec_send_packet(videoCodecCtx_, packet);
    av_packet_unref(packet);

    if (sendRet < 0) {
      if (sendRet == AVERROR(EAGAIN)) {
        while (avcodec_receive_frame(videoCodecCtx_, frame) == 0) {
          av_frame_unref(frame);
        }
        continue;
      }
      break;
    }

    int recvRet = avcodec_receive_frame(videoCodecCtx_, frame);
    if (recvRet == 0) {
      qint64 pts = frame->pts;
      if (pts == AV_NOPTS_VALUE) {
        pts = frame->best_effort_timestamp;
      }
      qint64 ptsMs = static_cast<qint64>(pts * av_q2d(videoTimeBase_) * 1000.0);

      int w = frame->width;
      int h = frame->height;

      QMutexLocker swsLock(&swsMutex_);
      if (!swsCtx_ || videoSize_.width() != w || videoSize_.height() != h) {
        if (swsCtx_) {
          sws_freeContext(swsCtx_);
        }
        swsCtx_ = sws_getContext(
            w, h, static_cast<AVPixelFormat>(frame->format), w, h,
            AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        videoSize_ = QSize(w, h);
      }

      QByteArray rgbaData(w * h * 4, Qt::Uninitialized);
      uint8_t *dstData[4] = {reinterpret_cast<uint8_t *>(rgbaData.data()),
                             nullptr, nullptr, nullptr};
      int dstLinesize[4] = {w * 4, 0, 0, 0};
      sws_scale(swsCtx_, frame->data, frame->linesize, 0, h, dstData,
                dstLinesize);

      DecodedVideoFrame vframe;
      vframe.ptsMs = ptsMs;
      vframe.width = w;
      vframe.height = h;
      vframe.rgbaData = std::move(rgbaData);

      {
        QMutexLocker resultLock(&resultMutex_);
        prefetchedFrame_ = std::move(vframe);
      }

      av_frame_unref(frame);
      break;
    }
  }

  av_frame_free(&frame);
  av_packet_free(&packet);

  qInfo() << "[Prefetcher] Prefetched target=" << targetMs
          << "ms took=" << timer.elapsed() << "ms";
}
