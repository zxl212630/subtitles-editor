#include "SeekDecoder.h"

#include <QDebug>
#include <QElapsedTimer>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#define PROFILE_TIMING 1

#define LOG_SEEK_info(msg) qInfo() << "[SeekDecoder]" << msg
#define LOG_SEEK_warning(msg) qWarning() << "[SeekDecoder]" << msg
#define LOG_SEEK_critical(msg) qCritical() << "[SeekDecoder]" << msg
#define LOG_SEEK_debug(msg) qDebug() << "[SeekDecoder]" << msg
#define LOG_SEEK(level, msg) LOG_SEEK_##level(msg)

SeekDecoder::SeekDecoder(QObject *parent) : QThread(parent) {}

SeekDecoder::~SeekDecoder() {
  shutdown();
  close();
}

void SeekDecoder::setCancelOpen(bool cancel) { cancelOpen_.store(cancel); }

int SeekDecoder::decodeInterruptCb(void *ctx) {
  if (!ctx)
    return 0;
  auto *self = static_cast<SeekDecoder *>(ctx);
  return self->cancelOpen_.load() ? 1 : 0;
}

bool SeekDecoder::open(const QString &path) {
  close();
  cancelOpen_.store(false);

  fmtCtx_ = avformat_alloc_context();
  if (!fmtCtx_) {
    LOG_SEEK(critical, "Failed to allocate AVFormatContext");
    return false;
  }
  fmtCtx_->interrupt_callback.callback = decodeInterruptCb;
  fmtCtx_->interrupt_callback.opaque = this;

  std::string stdPath = path.toStdString();
  int ret = avformat_open_input(&fmtCtx_, stdPath.c_str(), nullptr, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_SEEK(critical, "Failed to open input: " << errbuf);
    fmtCtx_ = nullptr;
    return false;
  }

  ret = avformat_find_stream_info(fmtCtx_, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_SEEK(critical, "Failed to find stream info: " << errbuf);
    avformat_close_input(&fmtCtx_);
    return false;
  }

  // Find video stream
  for (unsigned int i = 0; i < fmtCtx_->nb_streams; ++i) {
    if (fmtCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      videoStreamIdx_ = static_cast<int>(i);
      break;
    }
  }

  if (videoStreamIdx_ == -1) {
    LOG_SEEK(critical, "No video stream found");
    avformat_close_input(&fmtCtx_);
    return false;
  }

  AVStream *stream = fmtCtx_->streams[videoStreamIdx_];
  const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec) {
    LOG_SEEK(critical, "Failed to find decoder");
    avformat_close_input(&fmtCtx_);
    return false;
  }

  videoCodecCtx_ = avcodec_alloc_context3(codec);
  if (!videoCodecCtx_) {
    LOG_SEEK(critical, "Failed to allocate codec context");
    avformat_close_input(&fmtCtx_);
    return false;
  }

  ret = avcodec_parameters_to_context(videoCodecCtx_, stream->codecpar);
  if (ret < 0) {
    avcodec_free_context(&videoCodecCtx_);
    avformat_close_input(&fmtCtx_);
    return false;
  }

  // 启用多线程解码提高拖动预览的追帧效率
  videoCodecCtx_->thread_count = 0; // auto
  videoCodecCtx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

  ret = avcodec_open2(videoCodecCtx_, codec, nullptr);
  if (ret < 0) {
    avcodec_free_context(&videoCodecCtx_);
    avformat_close_input(&fmtCtx_);
    return false;
  }

  videoTimeBase_ = stream->time_base;
  nativeSize_ = QSize(videoCodecCtx_->width, videoCodecCtx_->height);
  lastSeekTargetMs_ = -1;
  lastDecodedPtsMs_ = -1;

  LOG_SEEK(info, "Opened successfully. Native size: "
                     << nativeSize_.width() << "x" << nativeSize_.height());

  // 开启工作线程
  start();
  return true;
}

void SeekDecoder::close() {
  if (isRunning()) {
    shutdown();
  }

  if (swsCtx_) {
    sws_freeContext(swsCtx_);
    swsCtx_ = nullptr;
  }
  if (videoCodecCtx_) {
    avcodec_free_context(&videoCodecCtx_);
    videoCodecCtx_ = nullptr;
  }
  if (fmtCtx_) {
    avformat_close_input(&fmtCtx_);
    fmtCtx_ = nullptr;
  }
  videoStreamIdx_ = -1;
  lastSeekTargetMs_ = -1;
  lastDecodedPtsMs_ = -1;
  lastSrcW_ = -1;
  lastSrcH_ = -1;
  lastDstW_ = -1;
  lastDstH_ = -1;
  lastFormat_ = -1;
}

void SeekDecoder::setOutputSize(QSize size) {
  QMutexLocker locker(&outputSizeMutex_);
  outputSize_ = size;
}

void SeekDecoder::requestSeek(qint64 targetMs, bool precise) {
  requestedMs_.store(targetMs);
  precise_.store(precise);
  seekGeneration_.fetch_add(1);

  QMutexLocker locker(&wakeMutex_);
  wakeCondition_.wakeAll();
}

void SeekDecoder::shutdown() {
  running_.store(false);
  {
    QMutexLocker locker(&wakeMutex_);
    wakeCondition_.wakeAll();
  }
  wait(3000); // 等待线程安全退出
}

void SeekDecoder::run() {
  running_.store(true);
  LOG_SEEK(info, "Seek thread started");

  while (running_.load()) {
    int gen = 0;
    qint64 targetMs = -1;
    bool precise = true;

    {
      QMutexLocker locker(&wakeMutex_);
      while (seekGeneration_.load() == lastProcessedGeneration_ &&
             running_.load()) {
        wakeCondition_.wait(&wakeMutex_, 100);
      }
      if (!running_.load()) {
        break;
      }
      gen = seekGeneration_.load();
      targetMs = requestedMs_.load();
      precise = precise_.load();
      lastProcessedGeneration_ = gen;
    }

    if (targetMs < 0) {
      continue;
    }

#if PROFILE_TIMING
    QElapsedTimer totalTimer;
    totalTimer.start();
#endif

    DecodedVideoFrame frame = decodeOneFrame(targetMs, precise);

    // 线程被更新的请求中断，或者退出
    if (!running_.load()) {
      continue;
    }

    if (frame.width > 0 && !frame.rgbaData.isEmpty()) {
#if PROFILE_TIMING
      qint64 elapsedUs = totalTimer.nsecsElapsed() / 1000;
      qInfo() << "[TIMING:SeekDecoder] decoded frame at" << frame.ptsMs
               << "ms for target" << targetMs << "ms, cost"
               << (elapsedUs / 1000.0) << "ms"
               << "precise=" << precise;
#endif
      emit frameReady(std::move(frame));
    }
  }

  LOG_SEEK(info, "Seek thread stopped");
}

DecodedVideoFrame SeekDecoder::decodeOneFrame(qint64 targetMs, bool precise) {
  DecodedVideoFrame result;
  if (!fmtCtx_ || !videoCodecCtx_) {
    return result;
  }

  bool needFullSeek = true;
  int gen = seekGeneration_.load();

  // 智能 Seek：如果当前目标时间戳位于上一次解码帧时间戳前方且距离较近（<2秒），
  // 则不需要 flush 解码器状态，可以直接继续向前读包解码。
  // 注意：快速关键帧预览模式下不执行顺序追帧，直接进行关键帧定位。
  if (precise && lastDecodedPtsMs_ >= 0 && targetMs > lastDecodedPtsMs_ &&
      targetMs - lastDecodedPtsMs_ < 2000) {
    needFullSeek = false;
  }

  if (needFullSeek) {
    int64_t target =
        av_rescale_q(targetMs, AVRational{1, 1000}, AV_TIME_BASE_Q);
    int ret = avformat_seek_file(fmtCtx_, -1, INT64_MIN, target, target,
                                 AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      LOG_SEEK(warning, "avformat_seek_file failed: " << errbuf);
    }
    avcodec_flush_buffers(videoCodecCtx_);

    if (precise) {
      // 精确寻道模式下不丢弃任何帧（包括 B 帧），以确保帧精确度
      videoCodecCtx_->skip_frame = AVDISCARD_DEFAULT;
    } else {
      // 极速预览模式下只解关键帧，丢弃所有非关键帧
      videoCodecCtx_->skip_frame = AVDISCARD_NONKEY;
    }
  }

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();

  bool found = false;
  int readRet = 0;

  while ((readRet = av_read_frame(fmtCtx_, packet)) >= 0) {
    // 每次读取后检查线程是否需要退出。不再在此中断以避免高频拖动下解码器频繁重置导致的黑屏。
    if (!running_.load()) {
      break;
    }

    if (packet->stream_index != videoStreamIdx_) {
      av_packet_unref(packet);
      continue;
    }

    int sendRet = avcodec_send_packet(videoCodecCtx_, packet);
    av_packet_unref(packet);
    if (sendRet < 0) {
      break;
    }

    while (avcodec_receive_frame(videoCodecCtx_, frame) == 0) {
      qint64 pts = frame->pts != AV_NOPTS_VALUE ? frame->pts
                                                : frame->best_effort_timestamp;
      qint64 ptsMs = static_cast<qint64>(pts * av_q2d(videoTimeBase_) * 1000.0);

      lastDecodedPtsMs_ = ptsMs;

      if (!precise) {
        // 极速预览：获得最近的关键帧即返回，无需继续解码后续帧
        result = convertFrame(frame, ptsMs);
        lastSeekTargetMs_ = targetMs;
        found = true;
        av_frame_unref(frame);
        break;
      } else {
        // 精确寻道：如果当前解出来的帧 PTS 大于等于我们 Seek 要求的 pts，代表已追赶上
        if (ptsMs >= targetMs) {
          result = convertFrame(frame, ptsMs);
          lastSeekTargetMs_ = targetMs;
          found = true;
          av_frame_unref(frame);
          break;
        }
      }
      av_frame_unref(frame);
    }

    if (found) {
      break;
    }
  }

  // 释放资源
  av_frame_free(&frame);
  av_packet_free(&packet);

  // 如果遇到 EOF 或异常导致无法精确定位，重置 discard 模式
  videoCodecCtx_->skip_frame = AVDISCARD_DEFAULT;

  result.targetMs = targetMs;
  return result;
}

DecodedVideoFrame SeekDecoder::convertFrame(AVFrame *frame, qint64 ptsMs) {
  DecodedVideoFrame vf;
  int srcW = frame->width;
  int srcH = frame->height;

  QSize outSize;
  {
    QMutexLocker locker(&outputSizeMutex_);
    outSize = outputSize_;
  }

  // 等比例计算降分辨率输出尺寸
  int dstW = srcW;
  int dstH = srcH;
  if (outSize.isValid() && !outSize.isEmpty()) {
    double scale = qMin(static_cast<double>(outSize.width()) / srcW,
                        static_cast<double>(outSize.height()) / srcH);
    // 只在显示区域小于原始画面时才进行降分辨率，不进行拉伸放大
    if (scale < 1.0) {
      dstW = static_cast<int>(srcW * scale) & ~1; // 对齐到 2 像素边界
      dstH = static_cast<int>(srcH * scale) & ~1;
    }
  }

  // 如果尺寸或格式改变，重建 sws 上下文
  if (!swsCtx_ || lastSrcW_ != srcW || lastSrcH_ != srcH || lastDstW_ != dstW ||
      lastDstH_ != dstH || lastFormat_ != frame->format) {
    if (swsCtx_) {
      sws_freeContext(swsCtx_);
    }
    swsCtx_ =
        sws_getContext(srcW, srcH, static_cast<AVPixelFormat>(frame->format),
                       dstW, dstH, AV_PIX_FMT_RGBA,
                       SWS_FAST_BILINEAR, // 选用最快的双线性过滤算法进行降采样
                       nullptr, nullptr, nullptr);
    lastSrcW_ = srcW;
    lastSrcH_ = srcH;
    lastDstW_ = dstW;
    lastDstH_ = dstH;
    lastFormat_ = frame->format;
  }

  int bufSize = dstW * dstH * 4;
  if (reusableRgbaBuffer_.size() != bufSize) {
    reusableRgbaBuffer_.resize(bufSize);
  }

  uint8_t *dstData[4] = {
      reinterpret_cast<uint8_t *>(reusableRgbaBuffer_.data()), nullptr, nullptr,
      nullptr};
  int dstLinesize[4] = {dstW * 4, 0, 0, 0};

  sws_scale(swsCtx_, frame->data, frame->linesize, 0, srcH, dstData,
            dstLinesize);

  vf.ptsMs = ptsMs;
  vf.width = dstW;
  vf.height = dstH;
  vf.rgbaData = reusableRgbaBuffer_; // QByteArray 隐式共享拷贝

  return vf;
}
