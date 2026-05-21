#include "FFmpegDecoder.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#define PROFILE_TIMING 1

#define LOG_DEC_info(msg) qInfo() << "[FFmpegDecoder]" << msg
#define LOG_DEC_warning(msg) qWarning() << "[FFmpegDecoder]" << msg
#define LOG_DEC_critical(msg) qCritical() << "[FFmpegDecoder]" << msg
#define LOG_DEC_debug(msg) qDebug() << "[FFmpegDecoder]" << msg
#define LOG_DEC(level, msg) LOG_DEC_##level(msg)

FFmpegDecoder::FFmpegDecoder(QObject *parent) : QThread(parent) {}

FFmpegDecoder::~FFmpegDecoder() { close(); }

bool FFmpegDecoder::open(const QString &path) {
  close();

  int ret = avformat_open_input(&fmtCtx_, path.toUtf8().constData(), nullptr,
                                nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_DEC(critical, "Failed to open input:" << errbuf);
    emit decodeError(QString("Failed to open input: %1").arg(errbuf));
    return false;
  }

  ret = avformat_find_stream_info(fmtCtx_, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_DEC(critical, "Failed to find stream info:" << errbuf);
    avformat_close_input(&fmtCtx_);
    emit decodeError(QString("Failed to find stream info: %1").arg(errbuf));
    return false;
  }

  for (unsigned int i = 0; i < fmtCtx_->nb_streams; ++i) {
    AVStream *stream = fmtCtx_->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        videoStreamIdx_ == -1) {
      videoStreamIdx_ = static_cast<int>(i);
      const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
      if (!codec) {
        LOG_DEC(critical, "Failed to find video decoder");
        avformat_close_input(&fmtCtx_);
        emit decodeError("Failed to find video decoder");
        return false;
      }
      videoCodecCtx_ = avcodec_alloc_context3(codec);
      if (!videoCodecCtx_) {
        LOG_DEC(critical, "Failed to allocate video codec context");
        avformat_close_input(&fmtCtx_);
        emit decodeError("Failed to allocate video codec context");
        return false;
      }
      ret = avcodec_parameters_to_context(videoCodecCtx_, stream->codecpar);
      if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOG_DEC(critical, "Failed to copy video codec parameters:" << errbuf);
        avcodec_free_context(&videoCodecCtx_);
        avformat_close_input(&fmtCtx_);
        emit decodeError(
            QString("Failed to copy video codec parameters: %1").arg(errbuf));
        return false;
      }
      // Enable multi-threaded video decoding
      videoCodecCtx_->thread_count = 0; // auto
      videoCodecCtx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
      ret = avcodec_open2(videoCodecCtx_, codec, nullptr);
      if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOG_DEC(critical, "Failed to open video codec:" << errbuf);
        avcodec_free_context(&videoCodecCtx_);
        avformat_close_input(&fmtCtx_);
        emit decodeError(QString("Failed to open video codec: %1").arg(errbuf));
        return false;
      }
      reusableFrame_ = av_frame_alloc();
      videoTimeBase_ = stream->time_base;
      videoSize_ = QSize(videoCodecCtx_->width, videoCodecCtx_->height);
      hasVideo_ = true;
      if (videoCodecCtx_->codec) {
        videoCodecName_ = QString::fromUtf8(videoCodecCtx_->codec->name);
      }
      videoBitRate_ = videoCodecCtx_->bit_rate;
      if (stream->avg_frame_rate.den != 0) {
        fps_ = av_q2d(stream->avg_frame_rate);
      }
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
               audioStreamIdx_ == -1) {
      audioStreamIdx_ = static_cast<int>(i);
      const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
      if (!codec) {
        LOG_DEC(critical, "Failed to find audio decoder");
        avcodec_free_context(&videoCodecCtx_);
        avformat_close_input(&fmtCtx_);
        emit decodeError("Failed to find audio decoder");
        return false;
      }
      audioCodecCtx_ = avcodec_alloc_context3(codec);
      if (!audioCodecCtx_) {
        LOG_DEC(critical, "Failed to allocate audio codec context");
        avcodec_free_context(&videoCodecCtx_);
        avformat_close_input(&fmtCtx_);
        emit decodeError("Failed to allocate audio codec context");
        return false;
      }
      ret = avcodec_parameters_to_context(audioCodecCtx_, stream->codecpar);
      if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOG_DEC(critical, "Failed to copy audio codec parameters:" << errbuf);
        avcodec_free_context(&audioCodecCtx_);
        avcodec_free_context(&videoCodecCtx_);
        avformat_close_input(&fmtCtx_);
        emit decodeError(
            QString("Failed to copy audio codec parameters: %1").arg(errbuf));
        return false;
      }
      // Enable multi-threaded audio decoding
      audioCodecCtx_->thread_count = 0; // auto
      audioCodecCtx_->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
      ret = avcodec_open2(audioCodecCtx_, codec, nullptr);
      if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOG_DEC(critical, "Failed to open audio codec:" << errbuf);
        avcodec_free_context(&audioCodecCtx_);
        avcodec_free_context(&videoCodecCtx_);
        avformat_close_input(&fmtCtx_);
        emit decodeError(QString("Failed to open audio codec: %1").arg(errbuf));
        return false;
      }
      audioTimeBase_ = stream->time_base;
      audioSampleRate_ = audioCodecCtx_->sample_rate;
      audioChannels_ = audioCodecCtx_->ch_layout.nb_channels;
      if (audioCodecCtx_->codec) {
        audioCodecName_ = QString::fromUtf8(audioCodecCtx_->codec->name);
      }
      audioBitRate_ = audioCodecCtx_->bit_rate;
      audioBitDepth_ = audioCodecCtx_->bits_per_raw_sample > 0
                           ? audioCodecCtx_->bits_per_raw_sample
                           : audioCodecCtx_->sample_fmt;
      hasAudio_ = true;
    }
  }

  if (!hasVideo_ && !hasAudio_) {
    LOG_DEC(critical, "No video or audio streams found");
    avformat_close_input(&fmtCtx_);
    emit decodeError("No video or audio streams found");
    return false;
  }

  if (fmtCtx_->duration > 0) {
    durationMs_ = fmtCtx_->duration * 1000 / AV_TIME_BASE;
  } else if (hasVideo_ && fmtCtx_->streams[videoStreamIdx_]->duration > 0) {
    durationMs_ =
        static_cast<qint64>(fmtCtx_->streams[videoStreamIdx_]->duration *
                            av_q2d(videoTimeBase_) * 1000);
  } else if (hasAudio_ && fmtCtx_->streams[audioStreamIdx_]->duration > 0) {
    durationMs_ =
        static_cast<qint64>(fmtCtx_->streams[audioStreamIdx_]->duration *
                            av_q2d(audioTimeBase_) * 1000);
  }

  // Read media creation_time from metadata if available
  if (fmtCtx_->metadata) {
    AVDictionaryEntry *entry =
        av_dict_get(fmtCtx_->metadata, "creation_time", nullptr, 0);
    if (entry && entry->value) {
      mediaCreationTime_ = QString::fromUtf8(entry->value);
    }
  }

  LOG_DEC(info, "Opened:" << path);
  LOG_DEC(info, "Duration:" << durationMs_ << "ms");
  LOG_DEC(info, "Video:" << hasVideo_ << "size=" << videoSize_.width() << "x"
                         << videoSize_.height() << "fps=" << fps_);
  LOG_DEC(info, "Audio:" << hasAudio_ << "rate=" << audioSampleRate_
                         << "channels=" << audioChannels_);

  return true;
}

void FFmpegDecoder::close() {
  stop();
  if (!wait(5000)) {
    LOG_DEC(warning, "Thread did not stop in time");
  }
  clearAllQueues();
  if (reusableFrame_) {
    av_frame_free(&reusableFrame_);
    reusableFrame_ = nullptr;
  }
  if (swsCtx_) {
    sws_freeContext(swsCtx_);
    swsCtx_ = nullptr;
  }
  if (audioSwrCtx_) {
    swr_free(&audioSwrCtx_);
    audioSwrCtx_ = nullptr;
  }
  avcodec_free_context(&videoCodecCtx_);
  avcodec_free_context(&audioCodecCtx_);
  avformat_close_input(&fmtCtx_);

  videoStreamIdx_ = -1;
  audioStreamIdx_ = -1;
  videoTimeBase_ = {0, 0};
  audioTimeBase_ = {0, 0};
  durationMs_ = 0;
  fps_ = 0.0;
  videoSize_ = QSize();
  audioSampleRate_ = 0;
  audioChannels_ = 0;
  swrSampleRate_ = 0;
  swrChannels_ = 0;
  swrFormat_ = AV_SAMPLE_FMT_NONE;
  hasVideo_ = false;
  hasAudio_ = false;
  videoCodecName_.clear();
  audioCodecName_.clear();
  videoBitRate_ = 0;
  audioBitRate_ = 0;
  audioBitDepth_ = 0;
  mediaCreationTime_.clear();

  LOG_DEC(info, "close() complete");
}

void FFmpegDecoder::requestSeek(qint64 targetMs) {
  clearVideoQueue();
  seekTargetMs_.store(targetMs);
  seekRequested_.store(true);
}

void FFmpegDecoder::setPlaying(bool playing) {
  bool wasPlaying = playing_.exchange(playing);
  if (playing && !wasPlaying) {
    QMutexLocker locker(&playControlMutex_);
    playCondition_.wakeAll();
  }
}

void FFmpegDecoder::stop() {
  running_.store(false);
  playing_.store(false);
  {
    QMutexLocker locker(&playControlMutex_);
    playCondition_.wakeAll();
  }
  {
    QMutexLocker locker(&queueFullMutex_);
    queueNotFull_.wakeAll();
  }
}

std::optional<DecodedVideoFrame> FFmpegDecoder::dequeueVideoFrame() {
  std::optional<DecodedVideoFrame> frame;
  {
    QMutexLocker locker(&videoQueueMutex_);
    if (videoQueue_.isEmpty()) {
      return std::nullopt;
    }
    frame = videoQueue_.dequeue();
  }
  {
    QMutexLocker locker(&queueFullMutex_);
    queueNotFull_.wakeAll();
  }
  return frame;
}

std::optional<DecodedAudioFrame> FFmpegDecoder::dequeueAudioFrame() {
  std::optional<DecodedAudioFrame> frame;
  {
    QMutexLocker locker(&audioQueueMutex_);
    if (audioQueue_.isEmpty()) {
      return std::nullopt;
    }
    frame = audioQueue_.dequeue();
  }
  {
    QMutexLocker locker(&queueFullMutex_);
    queueNotFull_.wakeAll();
  }
  return frame;
}

int FFmpegDecoder::videoQueueSize() const {
  QMutexLocker locker(&videoQueueMutex_);
  return videoQueue_.size();
}

int FFmpegDecoder::audioQueueSize() const {
  QMutexLocker locker(&audioQueueMutex_);
  return audioQueue_.size();
}

qint64 FFmpegDecoder::videoQueueDurationMs() const {
  QMutexLocker locker(&videoQueueMutex_);
  if (videoQueue_.size() < 2)
    return 0;
  return videoQueue_.back().ptsMs - videoQueue_.front().ptsMs;
}

qint64 FFmpegDecoder::audioQueueDurationMs() const {
  QMutexLocker locker(&audioQueueMutex_);
  if (audioQueue_.size() < 2)
    return 0;
  return audioQueue_.back().ptsMs - audioQueue_.front().ptsMs;
}

qint64 FFmpegDecoder::durationMs() const { return durationMs_; }

double FFmpegDecoder::fps() const { return fps_; }

QSize FFmpegDecoder::videoSize() const {
  QMutexLocker locker(&metadataMutex_);
  return videoSize_;
}

bool FFmpegDecoder::hasVideo() const { return hasVideo_; }

bool FFmpegDecoder::hasAudio() const { return hasAudio_; }

int FFmpegDecoder::audioSampleRate() const { return audioSampleRate_; }

int FFmpegDecoder::audioChannels() const { return audioChannels_; }

QString FFmpegDecoder::videoCodecName() const {
  QMutexLocker locker(&metadataMutex_);
  return videoCodecName_;
}

QString FFmpegDecoder::audioCodecName() const {
  QMutexLocker locker(&metadataMutex_);
  return audioCodecName_;
}

qint64 FFmpegDecoder::videoBitRate() const {
  QMutexLocker locker(&metadataMutex_);
  return videoBitRate_;
}

qint64 FFmpegDecoder::audioBitRate() const {
  QMutexLocker locker(&metadataMutex_);
  return audioBitRate_;
}

int FFmpegDecoder::audioBitDepth() const {
  QMutexLocker locker(&metadataMutex_);
  return audioBitDepth_;
}

QString FFmpegDecoder::mediaCreationTime() const {
  QMutexLocker locker(&metadataMutex_);
  return mediaCreationTime_;
}

void FFmpegDecoder::run() {
  if (!fmtCtx_) {
    LOG_DEC(warning, "run() called without open()");
    return;
  }

  running_.store(true);
  playing_.store(true);

  AVPacket *packet = av_packet_alloc();

  while (running_.load()) {
    if (seekRequested_.load()) {
      performSeek(seekTargetMs_.load());
      seekRequested_.store(false);
    }

    if (!playing_.load()) {
      bool needWarmup = false;
      if (hasVideo_) {
        QMutexLocker locker(&videoQueueMutex_);
        if (videoQueue_.isEmpty()) {
          needWarmup = true;
        }
      }
      if (hasAudio_ && !needWarmup) {
        QMutexLocker locker(&audioQueueMutex_);
        if (audioQueue_.isEmpty()) {
          needWarmup = true;
        }
      }

      if (!needWarmup) {
        QMutexLocker locker(&playControlMutex_);
        if (!playing_.load()) {
          playCondition_.wait(&playControlMutex_, 50);
        }
        continue;
      }
    }

    bool videoFull = !hasVideo_ || videoQueueDurationMs() >= MAX_VIDEO_QUEUE_MS;
    bool audioFull = !hasAudio_ || audioQueueDurationMs() >= MAX_AUDIO_QUEUE_MS;
    if (videoFull && audioFull) {
      QMutexLocker locker(&queueFullMutex_);
      queueNotFull_.wait(&queueFullMutex_, 50);
      continue;
    }

    int ret = av_read_frame(fmtCtx_, packet);
    if (ret < 0) {
      if (ret == AVERROR_EOF) {
        LOG_DEC(info, "EOF");
        emit endOfStream();
      } else {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        LOG_DEC(warning, "Read frame error:" << errbuf);
      }
      break;
    }

    if (packet->stream_index == videoStreamIdx_) {
      decodeVideoPacket(packet);
    } else if (packet->stream_index == audioStreamIdx_) {
      decodeAudioPacket(packet);
    }

    av_packet_unref(packet);
  }

  if (hasVideo_ && videoCodecCtx_) {
    decodeVideoPacket(nullptr);
  }
  if (hasAudio_ && audioCodecCtx_) {
    decodeAudioPacket(nullptr);
  }

  av_packet_free(&packet);
  LOG_DEC(info, "Decoder thread stopped");
}

void FFmpegDecoder::performSeek(qint64 targetMs) {
  if (!fmtCtx_) {
    LOG_DEC(warning, "No format context for seek");
    return;
  }

#if PROFILE_TIMING
  QElapsedTimer seekTimer;
  seekTimer.start();
#endif

  int64_t target = av_rescale_q(targetMs, AVRational{1, 1000}, AV_TIME_BASE_Q);
  int ret = avformat_seek_file(fmtCtx_, -1, INT64_MIN, target, target,
                               AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_DEC(warning, "Seek failed:" << errbuf);
  }
  if (videoCodecCtx_) {
    avcodec_flush_buffers(videoCodecCtx_);
  }
  if (audioCodecCtx_) {
    avcodec_flush_buffers(audioCodecCtx_);
  }
  clearAllQueues();
  discardBeforeTarget_ = true;

#if PROFILE_TIMING
  qint64 elapsed = seekTimer.nsecsElapsed() / 1000;
  qInfo() << "[TIMING:seek] targetMs=" << targetMs << " cost_us=" << elapsed;
#endif

  LOG_DEC(info, "Seek complete target=" << targetMs << "ms");
}

void FFmpegDecoder::clearAudioQueue() {
  QMutexLocker locker(&audioQueueMutex_);
  audioQueue_.clear();
}

void FFmpegDecoder::clearVideoQueue() {
  QMutexLocker locker(&videoQueueMutex_);
  videoQueue_.clear();
}

void FFmpegDecoder::clearAllQueues() {
  {
    QMutexLocker locker(&videoQueueMutex_);
    videoQueue_.clear();
  }
  {
    QMutexLocker locker(&audioQueueMutex_);
    audioQueue_.clear();
  }
}

bool FFmpegDecoder::decodeVideoPacket(AVPacket *packet) {
#if PROFILE_TIMING
  QElapsedTimer pktTimer;
  pktTimer.start();
  qint64 send_us = 0;
  qint64 receive_us = 0;
  qint64 scale_us = 0;
  int frameCount = 0;
#endif

  int ret = avcodec_send_packet(videoCodecCtx_, packet);

  auto processFrame = [&](AVFrame *f) {
    qint64 pts = f->pts;
    if (pts == AV_NOPTS_VALUE) {
      pts = f->best_effort_timestamp;
    }
    qint64 ptsMs = static_cast<qint64>(pts * av_q2d(videoTimeBase_) * 1000.0);

    // Seek frame filtering: discard frames before target timestamp
    if (discardBeforeTarget_) {
      if (ptsMs < seekTargetMs_.load() - 50) {
        return;
      }
      discardBeforeTarget_ = false;
    }

    int w = f->width;
    int h = f->height;

    {
      QMutexLocker locker(&metadataMutex_);
      if (!swsCtx_ || videoSize_.width() != w || videoSize_.height() != h) {
        if (swsCtx_) {
          sws_freeContext(swsCtx_);
        }
        swsCtx_ = sws_getContext(w, h, static_cast<AVPixelFormat>(f->format), w,
                                 h, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr,
                                 nullptr, nullptr);
        videoSize_ = QSize(w, h);
      }
    }

#if PROFILE_TIMING
    QElapsedTimer scaleTimer;
    scaleTimer.start();
#endif

    // Reuse pre-allocated RGBA buffer
    int bufSize = w * h * 4;
    if (reusableRgbaBuffer_.size() != bufSize) {
      reusableRgbaBuffer_.resize(bufSize);
    }
    uint8_t *dstData[4] = {
        reinterpret_cast<uint8_t *>(reusableRgbaBuffer_.data()), nullptr,
        nullptr, nullptr};
    int dstLinesize[4] = {w * 4, 0, 0, 0};
    sws_scale(swsCtx_, f->data, f->linesize, 0, h, dstData, dstLinesize);

#if PROFILE_TIMING
    scale_us += scaleTimer.nsecsElapsed();
#endif

    DecodedVideoFrame vf;
    vf.ptsMs = ptsMs;
    vf.width = w;
    vf.height = h;
    vf.rgbaData =
        reusableRgbaBuffer_; // QByteArray is implicitly shared (copy-on-write)

    QMutexLocker locker(&videoQueueMutex_);
    videoQueue_.enqueue(std::move(vf));
    queueNotFull_.wakeAll();
  };

  while (true) {
#if PROFILE_TIMING
    QElapsedTimer recvTimer;
    recvTimer.start();
#endif

    ret = avcodec_receive_frame(videoCodecCtx_, reusableFrame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      LOG_DEC(warning, "Video decode error:" << errbuf);
      return false;
    }

#if PROFILE_TIMING
    receive_us += recvTimer.nsecsElapsed();
    frameCount++;
#endif

    processFrame(reusableFrame_);
    av_frame_unref(reusableFrame_);
  }

#if PROFILE_TIMING
  if (frameCount > 0) {
    send_us = pktTimer.nsecsElapsed() - receive_us - scale_us;
    qInfo() << "[TIMING:decode] frames=" << frameCount
            << " send_us=" << (send_us / 1000 / frameCount)
            << " recv_us=" << (receive_us / 1000 / frameCount)
            << " swscale_us=" << (scale_us / 1000 / frameCount)
            << " per_frame_us="
            << ((send_us + receive_us + scale_us) / 1000 / frameCount);
  }
#endif

  return true;
}

bool FFmpegDecoder::decodeAudioPacket(AVPacket *packet) {
  int ret = avcodec_send_packet(audioCodecCtx_, packet);
  AVFrame *frame = av_frame_alloc();

  auto processFrame = [&](AVFrame *f) {
    DecodedAudioFrame aframe;
    convertAudioFrame(f, aframe);

    {
      QMutexLocker locker(&audioQueueMutex_);
      audioQueue_.enqueue(std::move(aframe));
    }

    av_frame_unref(f);
  };

  while (ret == AVERROR(EAGAIN)) {
    while (avcodec_receive_frame(audioCodecCtx_, frame) == 0) {
      processFrame(frame);
    }
    ret = avcodec_send_packet(audioCodecCtx_, packet);
  }

  if (ret < 0 && ret != AVERROR_EOF) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_DEC(warning, "Audio send packet error:" << errbuf);
    av_frame_free(&frame);
    return false;
  }

  int receiveRet = avcodec_receive_frame(audioCodecCtx_, frame);
  while (receiveRet == 0) {
    processFrame(frame);
    receiveRet = avcodec_receive_frame(audioCodecCtx_, frame);
  }
  if (receiveRet < 0 && receiveRet != AVERROR(EAGAIN) &&
      receiveRet != AVERROR_EOF) {
    char errbuf[256];
    av_strerror(receiveRet, errbuf, sizeof(errbuf));
    LOG_DEC(warning, "Audio receive frame error:" << errbuf);
  }

  av_frame_free(&frame);
  return true;
}

void FFmpegDecoder::convertAudioFrame(AVFrame *frame, DecodedAudioFrame &out) {
  qint64 pts = frame->pts;
  if (pts == AV_NOPTS_VALUE) {
    pts = frame->best_effort_timestamp;
  }
  out.ptsMs = static_cast<qint64>(pts * av_q2d(audioTimeBase_) * 1000.0);
  out.sampleRate = frame->sample_rate;
  out.channels = frame->ch_layout.nb_channels;

  AVSampleFormat inFormat = static_cast<AVSampleFormat>(frame->format);
  if (inFormat == AV_SAMPLE_FMT_S16) {
    int dataSize = av_samples_get_buffer_size(
        nullptr, out.channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
    out.pcmData =
        QByteArray(reinterpret_cast<const char *>(frame->data[0]), dataSize);
    return;
  }

  bool needInit = false;
  if (!audioSwrCtx_) {
    needInit = true;
  } else if (swrSampleRate_ != out.sampleRate || swrChannels_ != out.channels ||
             swrFormat_ != inFormat) {
    swr_free(&audioSwrCtx_);
    needInit = true;
  }

  if (needInit) {
    AVChannelLayout outLayout;
    av_channel_layout_copy(&outLayout, &frame->ch_layout);
    int ret = swr_alloc_set_opts2(&audioSwrCtx_, &outLayout, AV_SAMPLE_FMT_S16,
                                  out.sampleRate, &frame->ch_layout, inFormat,
                                  out.sampleRate, 0, nullptr);
    av_channel_layout_uninit(&outLayout);
    if (ret < 0 || !audioSwrCtx_) {
      LOG_DEC(warning, "Failed to allocate SwrContext");
      if (audioSwrCtx_)
        swr_free(&audioSwrCtx_);
      return;
    }

    ret = swr_init(audioSwrCtx_);
    if (ret < 0) {
      LOG_DEC(warning, "Failed to initialize SwrContext");
      swr_free(&audioSwrCtx_);
      return;
    }

    swrSampleRate_ = out.sampleRate;
    swrChannels_ = out.channels;
    swrFormat_ = inFormat;
  }

  // For AAC and similar compressed formats, nb_samples may be 0.
  // In that case, use a reasonable default buffer size to handle decoder delay.
  int outSamples = frame->nb_samples;
  if (outSamples == 0) {
    outSamples = 8192; // Default buffer for compressed audio with delay
  }

  int maxOutSamples = swr_get_out_samples(audioSwrCtx_, outSamples);
  int maxOutSize = av_samples_get_buffer_size(
      nullptr, out.channels, maxOutSamples, AV_SAMPLE_FMT_S16, 1);
  QByteArray buffer(maxOutSize, 0);
  uint8_t *outData[1] = {reinterpret_cast<uint8_t *>(buffer.data())};
  int converted =
      swr_convert(audioSwrCtx_, outData, maxOutSamples,
                  const_cast<const uint8_t **>(frame->data), outSamples);
  if (converted < 0) {
    LOG_DEC(warning, "Failed to convert audio");
    return;
  }

  int actualSize = av_samples_get_buffer_size(nullptr, out.channels, converted,
                                              AV_SAMPLE_FMT_S16, 1);
  out.pcmData = buffer.left(actualSize);
}
