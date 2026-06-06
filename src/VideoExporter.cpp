#include "VideoExporter.h"
#include "SubtitleRenderer.h"
#include "SubtitleTrack.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QPainter>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#define LOG_EXP_info(msg) qInfo() << "[VideoExporter]" << msg
#define LOG_EXP_warning(msg) qWarning() << "[VideoExporter]" << msg
#define LOG_EXP_critical(msg) qCritical() << "[VideoExporter]" << msg
#define LOG_EXP_debug(msg) qDebug() << "[VideoExporter]" << msg

static const AVCodec *find_fallback_video_encoder(AVCodecID codecId) {
  if (codecId == AV_CODEC_ID_H264) {
    const char *H264_ENCODERS[] = {"h264_videotoolbox", "h264_mf", "h264_nvenc",
                                   "libx264", nullptr};
    for (int i = 0; H264_ENCODERS[i] != nullptr; ++i) {
      const AVCodec *c = avcodec_find_encoder_by_name(H264_ENCODERS[i]);
      if (c) {
        AVCodecContext *ctx = avcodec_alloc_context3(c);
        if (ctx) {
          ctx->width = 64;
          ctx->height = 64;
          ctx->pix_fmt = AV_PIX_FMT_YUV420P;
          ctx->time_base = {1, 25};
          ctx->framerate = {25, 1};
          ctx->bit_rate = 1000000;
          int ret = avcodec_open2(ctx, c, nullptr);
          avcodec_free_context(&ctx);
          if (ret >= 0)
            return c;
        }
      }
    }
  } else if (codecId == AV_CODEC_ID_HEVC) {
    const char *HEVC_ENCODERS[] = {"hevc_videotoolbox", "hevc_mf", "hevc_nvenc",
                                   "libx265", nullptr};
    for (int i = 0; HEVC_ENCODERS[i] != nullptr; ++i) {
      const AVCodec *c = avcodec_find_encoder_by_name(HEVC_ENCODERS[i]);
      if (c) {
        AVCodecContext *ctx = avcodec_alloc_context3(c);
        if (ctx) {
          ctx->width = 64;
          ctx->height = 64;
          ctx->pix_fmt = AV_PIX_FMT_YUV420P;
          ctx->time_base = {1, 25};
          ctx->framerate = {25, 1};
          ctx->bit_rate = 1000000;
          int ret = avcodec_open2(ctx, c, nullptr);
          avcodec_free_context(&ctx);
          if (ret >= 0)
            return c;
        }
      }
    }
  }
  return avcodec_find_encoder(codecId);
}

VideoExporter::VideoExporter(QObject *parent) : QThread(parent) {}

VideoExporter::~VideoExporter() { cleanup(); }

void VideoExporter::setConfig(const VideoExportConfig &config) {
  config_ = config;
}

void VideoExporter::setSubtitleTrack(const SubtitleTrack *track) {
  track_ = track;
}

void VideoExporter::requestCancel() { cancelRequested_.store(true); }

int VideoExporter::progressPercent() const { return lastProgressPercent_; }

qint64 VideoExporter::elapsedMs() const {
  return elapsedTimer_.isValid() ? elapsedTimer_.elapsed() : 0;
}

qint64 VideoExporter::estimatedRemainingMs() const {
  int percent = lastProgressPercent_;
  if (percent <= 0)
    return 0;
  qint64 elapsed = elapsedMs();
  return (elapsed * (100 - percent)) / percent;
}

void VideoExporter::run() {
  cancelRequested_.store(false);
  lastProgressPercent_ = 0;
  lastProcessedPtsMs_ = 0;

  elapsedTimer_.start();

  if (!openInput()) {
    cleanup();
    return;
  }

  if (!setupOutput()) {
    cleanup();
    return;
  }

  LOG_EXP_info("Start exporting pipeline...");
  if (!processFrames()) {
    cleanup();
    // 如果失败了，删除未写完的损坏文件
    QFile::remove(config_.outputPath);
    return;
  }

  flushEncoders();

  // 写入文件尾
  int ret = av_write_trailer(outputFmtCtx_);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_EXP_critical("Failed to write trailer:" << errbuf);
    emit exportFailed(tr("写入视频尾部失败：%1").arg(errbuf));
    cleanup();
    QFile::remove(config_.outputPath);
    return;
  }

  cleanup();

  if (cancelRequested_.load()) {
    QFile::remove(config_.outputPath);
    emit exportCancelled();
    LOG_EXP_info("Export cancelled by user.");
  } else {
    lastProgressPercent_ = 100;
    emit progressChanged(100);
    emit exportFinished(config_.outputPath);
    LOG_EXP_info("Export finished successfully.");
  }
}

bool VideoExporter::openInput() {
  int ret = avformat_open_input(
      &inputFmtCtx_, config_.inputPath.toUtf8().constData(), nullptr, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("无法打开输入视频文件：%1").arg(errbuf));
    return false;
  }

  ret = avformat_find_stream_info(inputFmtCtx_, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("无法获取视频流信息：%1").arg(errbuf));
    return false;
  }

  // 寻找音视频流
  for (unsigned int i = 0; i < inputFmtCtx_->nb_streams; ++i) {
    AVStream *stream = inputFmtCtx_->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
        inputVideoStreamIdx_ == -1) {
      inputVideoStreamIdx_ = i;
      inVideoStream_ = stream;
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
               inputAudioStreamIdx_ == -1) {
      inputAudioStreamIdx_ = i;
      inAudioStream_ = stream;
    }
  }

  if (inputVideoStreamIdx_ == -1) {
    emit exportFailed(tr("源视频中未找到视频轨道。"));
    return false;
  }

  // 初始化视频解码器
  const AVCodec *videoDec =
      avcodec_find_decoder(inVideoStream_->codecpar->codec_id);
  if (!videoDec) {
    emit exportFailed(tr("未找到对应的视频解码器。"));
    return false;
  }

  videoDecCtx_ = avcodec_alloc_context3(videoDec);
  avcodec_parameters_to_context(videoDecCtx_, inVideoStream_->codecpar);
  videoDecCtx_->thread_count = 0; // 自动多线程解码

  ret = avcodec_open2(videoDecCtx_, videoDec, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("打开视频解码器失败：%1").arg(errbuf));
    return false;
  }

  // 初始化音频解码器 (如果有音频且用户选择导出音频)
  if (inAudioStream_ && config_.exportAudio) {
    const AVCodec *audioDec =
        avcodec_find_decoder(inAudioStream_->codecpar->codec_id);
    if (audioDec) {
      audioDecCtx_ = avcodec_alloc_context3(audioDec);
      avcodec_parameters_to_context(audioDecCtx_, inAudioStream_->codecpar);
      ret = avcodec_open2(audioDecCtx_, audioDec, nullptr);
      if (ret < 0) {
        LOG_EXP_warning("Failed to open audio decoder, audio will be skipped.");
        avcodec_free_context(&audioDecCtx_);
      }
    }
  }

  // 计算总时长
  if (inputFmtCtx_->duration > 0) {
    totalDurationMs_ = inputFmtCtx_->duration * 1000 / AV_TIME_BASE;
  } else {
    totalDurationMs_ = static_cast<qint64>(
        inVideoStream_->duration * av_q2d(inVideoStream_->time_base) * 1000.0);
  }
  if (totalDurationMs_ <= 0) {
    totalDurationMs_ = 1; // 防止除以 0
  }

  decVideoFrame_ = av_frame_alloc();
  decAudioFrame_ = av_frame_alloc();

  return true;
}

bool VideoExporter::setupOutput() {
  int ret =
      avformat_alloc_output_context2(&outputFmtCtx_, nullptr, nullptr,
                                     config_.outputPath.toUtf8().constData());
  if (ret < 0 || !outputFmtCtx_) {
    emit exportFailed(tr("无法分配输出封装上下文。"));
    return false;
  }

  // 初始化视频编码器并加入轨道
  if (!initVideoEncoder()) {
    return false;
  }

  // 初始化音频轨道
  if (audioDecCtx_) {
    // 检查是否进行 Stream Copy：
    // 若用户选择不改变音频码率和采样率，且输出格式兼容原编码，可尝试 stream
    // copy
    bool requestCopy =
        (config_.audioBitrateKbps == 0 && config_.audioSampleRate == 0);

    // MP4/MOV 对 AAC, MP3, AC3 等兼容良好。如果原音频是
    // aac，我们可以安全地流拷贝
    QString codecName =
        QString::fromUtf8(avcodec_get_name(inAudioStream_->codecpar->codec_id))
            .toLower();
    bool formatCompatible = (codecName == "aac" || codecName == "mp3");

    if (requestCopy && formatCompatible) {
      audioCopyMode_ = true;
      LOG_EXP_info("Audio: Stream Copy mode enabled.");
    } else {
      audioCopyMode_ = false;
      LOG_EXP_info("Audio: Re-encoding mode enabled.");
    }

    if (!initAudioStream()) {
      return false;
    }
  }

  // 打开输出文件
  if (!(outputFmtCtx_->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&outputFmtCtx_->pb, config_.outputPath.toUtf8().constData(),
                    AVIO_FLAG_WRITE);
    if (ret < 0) {
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      emit exportFailed(tr("无法创建或打开输出文件：%1").arg(errbuf));
      return false;
    }
  }

  // 写入封装格式头部
  ret = avformat_write_header(outputFmtCtx_, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("写入封装头部数据失败：%1").arg(errbuf));
    return false;
  }

  return true;
}

bool VideoExporter::initVideoEncoder() {
  const AVCodec *encoder = nullptr;

  // 查找对应的视频编码器
  if (!config_.videoCodec.isEmpty()) {
    encoder =
        avcodec_find_encoder_by_name(config_.videoCodec.toUtf8().constData());
  }

  if (!encoder) {
    // 安全降级
    AVCodecID codecId = (config_.videoCodec.contains("hevc") ||
                         config_.videoCodec.contains("265"))
                            ? AV_CODEC_ID_HEVC
                            : AV_CODEC_ID_H264;
    encoder = find_fallback_video_encoder(codecId);
  }

  if (!encoder) {
    emit exportFailed(tr("未找到任何可用的 H.264 或 HEVC 视频编码器。"));
    return false;
  }
  LOG_EXP_info("Using video encoder:" << encoder->name);

  outVideoStream_ = avformat_new_stream(outputFmtCtx_, nullptr);
  if (!outVideoStream_) {
    emit exportFailed(tr("无法在输出中创建视频轨道。"));
    return false;
  }

  videoEncCtx_ = avcodec_alloc_context3(encoder);
  if (!videoEncCtx_) {
    emit exportFailed(tr("分配视频编码上下文失败。"));
    return false;
  }

  // 配置分辨率与缩放
  int outWidth =
      config_.outputWidth > 0 ? config_.outputWidth : videoDecCtx_->width;
  int outHeight =
      config_.outputHeight > 0 ? config_.outputHeight : videoDecCtx_->height;

  // 强制使分辨率为偶数以支持 YUV420P
  outWidth = (outWidth / 2) * 2;
  outHeight = (outHeight / 2) * 2;

  videoEncCtx_->width = outWidth;
  videoEncCtx_->height = outHeight;

  // 编码像素格式，VideoToolbox 和 libx264 都很好地支持 YUV420P
  videoEncCtx_->pix_fmt = AV_PIX_FMT_YUV420P;

  // 帧率设置
  double outFps = config_.outputFps > 0.0
                      ? config_.outputFps
                      : av_q2d(inVideoStream_->avg_frame_rate);
  if (outFps <= 0.0)
    outFps = 25.0; // 默认防崩溃

  videoEncCtx_->time_base = av_inv_q(av_d2q(outFps, 100000));
  videoEncCtx_->framerate = av_d2q(outFps, 100000);
  outVideoStream_->time_base = videoEncCtx_->time_base;

  // 帧组 GOP
  videoEncCtx_->gop_size = qRound(outFps) * 2;
  videoEncCtx_->keyint_min = qRound(outFps);

  // 质量与码率控制
  bool useCrf = QString(encoder->name).contains("x264") ||
                QString(encoder->name).contains("x265");
  if (!useCrf) {
    // 硬件/系统编码器 (videotoolbox, nvenc, mf, d3d12va 等) 通过 bit_rate 控速
    int64_t targetBitrate = 6000000; // 默认中等质量
    if (config_.qualityMode == VideoExportConfig::QualityHigh) {
      targetBitrate = 12000000;
    } else if (config_.qualityMode == VideoExportConfig::QualityLow) {
      targetBitrate = 3000000;
    } else if (config_.qualityMode == VideoExportConfig::QualityCustomBitrate) {
      targetBitrate = static_cast<int64_t>(config_.customBitrateKbps) * 1000;
    }
    // 根据分辨率做等比调节
    double ratio = (double)(outWidth * outHeight) / (1920.0 * 1080.0);
    if (ratio < 0.1)
      ratio = 0.1;
    if (config_.qualityMode != VideoExportConfig::QualityCustomBitrate) {
      targetBitrate = static_cast<int64_t>(targetBitrate * ratio);
    }
    videoEncCtx_->bit_rate = targetBitrate;
    videoEncCtx_->rc_max_rate = targetBitrate * 1.5;
    videoEncCtx_->rc_buffer_size = targetBitrate * 2;
  } else {
    // CPU 编码器 (libx264, libx265) 使用 CRF 控制
    int crf = 23;
    if (config_.qualityMode == VideoExportConfig::QualityHigh) {
      crf = 18;
    } else if (config_.qualityMode == VideoExportConfig::QualityLow) {
      crf = 28;
    }

    if (config_.qualityMode == VideoExportConfig::QualityCustomBitrate) {
      videoEncCtx_->bit_rate =
          static_cast<int64_t>(config_.customBitrateKbps) * 1000;
      videoEncCtx_->rc_max_rate = videoEncCtx_->bit_rate * 1.5;
      videoEncCtx_->rc_buffer_size = videoEncCtx_->bit_rate * 2;
    } else {
      av_opt_set(videoEncCtx_->priv_data, "crf",
                 QString::number(crf).toUtf8().constData(), 0);
    }
    // 设置编码预设速度
    av_opt_set(videoEncCtx_->priv_data, "preset", "medium", 0);
  }

  // 必须配置全局头部 (为了兼容 MP4 格式的封包)
  if (outputFmtCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
    videoEncCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  int ret = avcodec_open2(videoEncCtx_, encoder, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("无法打开视频编码器：%1").arg(errbuf));
    return false;
  }

  avcodec_parameters_from_context(outVideoStream_->codecpar, videoEncCtx_);
  // QuickTime Player requires 'hvc1' tag for HEVC (parameter sets in file
  // header). FFmpeg defaults to 'hev1' (parameter sets in samples) when tag is
  // 0.
  if (videoEncCtx_->codec_id == AV_CODEC_ID_HEVC) {
    outVideoStream_->codecpar->codec_tag = MKTAG('h', 'v', 'c', '1');
  } else {
    outVideoStream_->codecpar->codec_tag = 0;
  }

  outputVideoStreamIdx_ = outVideoStream_->index;

  // 分配临时用于转换的编码帧
  encVideoFrame_ = av_frame_alloc();
  encVideoFrame_->format = videoEncCtx_->pix_fmt;
  encVideoFrame_->width = outWidth;
  encVideoFrame_->height = outHeight;
  av_frame_get_buffer(encVideoFrame_, 0);

  // 初始化 Sws
  // 缩放与转换上下文，使用双三次插值（SWS_BICUBIC）提高缩放后的图像清晰度
  swsToRgb_ =
      sws_getContext(videoDecCtx_->width, videoDecCtx_->height,
                     videoDecCtx_->pix_fmt, outWidth, outHeight,
                     AV_PIX_FMT_RGBA, SWS_BICUBIC, nullptr, nullptr, nullptr);

  swsFromRgb_ = sws_getContext(outWidth, outHeight, AV_PIX_FMT_RGBA, outWidth,
                               outHeight, videoEncCtx_->pix_fmt, SWS_BICUBIC,
                               nullptr, nullptr, nullptr);

  if (!swsToRgb_ || !swsFromRgb_) {
    emit exportFailed(tr("分配图像缩放与格式转换上下文失败。"));
    return false;
  }

  return true;
}

bool VideoExporter::initAudioStream() {
  outAudioStream_ = avformat_new_stream(outputFmtCtx_, nullptr);
  if (!outAudioStream_) {
    emit exportFailed(tr("无法在输出中创建音频轨道。"));
    return false;
  }

  if (audioCopyMode_) {
    // 流拷贝直接复制参数
    avcodec_parameters_copy(outAudioStream_->codecpar,
                            inAudioStream_->codecpar);
    outAudioStream_->codecpar->codec_tag = 0;
    outAudioStream_->time_base = inAudioStream_->time_base;
    outputAudioStreamIdx_ = outAudioStream_->index;
    return true;
  }

  // 重编码模式：AAC
  const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
  if (!encoder) {
    emit exportFailed(tr("未找到 AAC 音频编码器。"));
    return false;
  }

  audioEncCtx_ = avcodec_alloc_context3(encoder);
  if (!audioEncCtx_) {
    emit exportFailed(tr("分配音频编码上下文失败。"));
    return false;
  }

  // 音频参数设置
  int outSampleRate = config_.audioSampleRate > 0 ? config_.audioSampleRate
                                                  : audioDecCtx_->sample_rate;
  audioEncCtx_->sample_rate = outSampleRate;
  audioEncCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP; // AAC 常用 FLTP

  AVChannelLayout outLayout;
  av_channel_layout_default(&outLayout, audioDecCtx_->ch_layout.nb_channels);
  av_channel_layout_copy(&audioEncCtx_->ch_layout, &outLayout);
  av_channel_layout_uninit(&outLayout);

  audioEncCtx_->time_base = {1, outSampleRate};
  outAudioStream_->time_base = audioEncCtx_->time_base;

  int targetBitrate =
      config_.audioBitrateKbps > 0 ? config_.audioBitrateKbps * 1000 : 192000;
  audioEncCtx_->bit_rate = targetBitrate;

  if (outputFmtCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
    audioEncCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  int ret = avcodec_open2(audioEncCtx_, encoder, nullptr);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("无法打开音频编码器：%1").arg(errbuf));
    return false;
  }

  avcodec_parameters_from_context(outAudioStream_->codecpar, audioEncCtx_);
  outAudioStream_->codecpar->codec_tag = 0;

  outputAudioStreamIdx_ = outAudioStream_->index;

  // 初始化音频重采样
  AVSampleFormat inFmt = audioDecCtx_->sample_fmt;
  AVSampleFormat outFmt = audioEncCtx_->sample_fmt;

  ret = swr_alloc_set_opts2(&swrCtx_, &audioEncCtx_->ch_layout, outFmt,
                            outSampleRate, &audioDecCtx_->ch_layout, inFmt,
                            audioDecCtx_->sample_rate, 0, nullptr);
  if (ret < 0 || !swrCtx_) {
    emit exportFailed(tr("分配音频重采样上下文失败。"));
    return false;
  }

  ret = swr_init(swrCtx_);
  if (ret < 0) {
    emit exportFailed(tr("初始化音频重采样上下文失败。"));
    return false;
  }

  // 分配音频重采样输出缓冲区帧
  resampledAudioFrame_ = av_frame_alloc();
  resampledAudioFrame_->format = outFmt;
  resampledAudioFrame_->sample_rate = outSampleRate;
  av_channel_layout_copy(&resampledAudioFrame_->ch_layout,
                         &audioEncCtx_->ch_layout);
  resampledAudioFrame_->nb_samples = audioEncCtx_->frame_size;
  av_frame_get_buffer(resampledAudioFrame_, 0);

  // 初始化 FIFO 队列，用于打碎和重组变长的音频采样点
  audioFifo_ =
      av_audio_fifo_alloc(outFmt, audioEncCtx_->ch_layout.nb_channels, 1);
  if (!audioFifo_) {
    emit exportFailed(tr("创建音频缓冲区队列失败。"));
    return false;
  }

  nextAudioPts_ = 0;
  return true;
}

bool VideoExporter::processFrames() {
  AVPacket *pkt = av_packet_alloc();
  bool success = true;

  while (!cancelRequested_.load()) {
    int ret = av_read_frame(inputFmtCtx_, pkt);
    if (ret < 0) {
      if (ret == AVERROR_EOF) {
        LOG_EXP_info("Reached EOF of input file.");
        break;
      }
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      emit exportFailed(tr("读取视频帧数据错误：%1").arg(errbuf));
      success = false;
      break;
    }

    if (pkt->stream_index == inputVideoStreamIdx_) {
      if (!decodeAndProcessVideo(pkt)) {
        success = false;
        av_packet_unref(pkt);
        break;
      }
    } else if (pkt->stream_index == inputAudioStreamIdx_ &&
               config_.exportAudio) {
      if (audioCopyMode_) {
        // 直接做时基转换后写出
        pkt->stream_index = outputAudioStreamIdx_;
        av_packet_rescale_ts(pkt, inAudioStream_->time_base,
                             outAudioStream_->time_base);
        writePacket(pkt, false);
      } else {
        if (!decodeAndProcessAudio(pkt)) {
          success = false;
          av_packet_unref(pkt);
          break;
        }
      }
    }

    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);
  return success;
}

bool VideoExporter::decodeAndProcessVideo(AVPacket *pkt) {
  int ret = avcodec_send_packet(videoDecCtx_, pkt);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_EXP_warning("Video send packet failed:" << errbuf);
    return true; // 跳过坏帧
  }

  while (true) {
    ret = avcodec_receive_frame(videoDecCtx_, decVideoFrame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      LOG_EXP_warning("Video receive frame failed:" << errbuf);
      return true;
    }

    // 1. 缩放并转为 RGBA 格式 QImage
    int outWidth = videoEncCtx_->width;
    int outHeight = videoEncCtx_->height;

    QImage img(outWidth, outHeight, QImage::Format_RGBA8888);
    uint8_t *dstData[4] = {img.bits(), nullptr, nullptr, nullptr};
    int dstLinesize[4] = {(int)img.bytesPerLine(), 0, 0, 0};
    sws_scale(swsToRgb_, decVideoFrame_->data, decVideoFrame_->linesize, 0,
              decVideoFrame_->height, dstData, dstLinesize);

    // 2. 几何字幕位置匹配的 PTS 计算
    qint64 pts = decVideoFrame_->pts;
    if (pts == AV_NOPTS_VALUE) {
      pts = decVideoFrame_->best_effort_timestamp;
    }
    qint64 ptsMs =
        static_cast<qint64>(pts * av_q2d(inVideoStream_->time_base) * 1000.0);

    // 3. 将字幕渲染到帧图像上
    if (track_) {
      SubtitleRenderer::render(*track_, img, ptsMs, QSize(outWidth, outHeight));
    }

    // 4. 将 RGBA 转回编码器要求的 YUV 格式
    uint8_t *srcData[4] = {img.bits(), nullptr, nullptr, nullptr};
    int srcLinesize[4] = {(int)img.bytesPerLine(), 0, 0, 0};
    sws_scale(swsFromRgb_, srcData, srcLinesize, 0, outHeight,
              encVideoFrame_->data, encVideoFrame_->linesize);

    // 5. 设置编码帧的 PTS
    if (config_.outputFps > 0.0) {
      // 重新映射的 PTS (CFR 重采样)
      double fpsVal = config_.outputFps;
      int64_t targetPts = av_rescale_q(lastProcessedPtsMs_, AVRational{1, 1000},
                                       videoEncCtx_->time_base);
      encVideoFrame_->pts = targetPts;
      lastProcessedPtsMs_ += qRound(1000.0 / fpsVal);
    } else {
      // 原始时基转换
      encVideoFrame_->pts =
          av_rescale_q(pts, inVideoStream_->time_base, videoEncCtx_->time_base);
      lastProcessedPtsMs_ = ptsMs;
    }

    encVideoFrame_->pict_type = AV_PICTURE_TYPE_NONE;

    // 6. 送入编码器编码并写出
    if (!encodeVideoFrame(encVideoFrame_)) {
      return false;
    }

    // 7. 发送进度信号
    int progress =
        qBound(0, static_cast<int>(ptsMs * 100 / totalDurationMs_), 99);
    if (progress > lastProgressPercent_) {
      lastProgressPercent_ = progress;
      emit progressChanged(progress);
    }

    av_frame_unref(decVideoFrame_);
  }

  return true;
}

bool VideoExporter::decodeAndProcessAudio(AVPacket *pkt) {
  if (!audioDecCtx_ || !audioEncCtx_)
    return true;

  int ret = avcodec_send_packet(audioDecCtx_, pkt);
  if (ret < 0) {
    return true; // 跳过音频错误包
  }

  while (true) {
    ret = avcodec_receive_frame(audioDecCtx_, decAudioFrame_);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      return true;
    }

    // 计算重采样所需的输出样本数
    int maxOutSamples =
        swr_get_out_samples(swrCtx_, decAudioFrame_->nb_samples);

    // 扩展重采样帧大小
    if (resampledAudioFrame_->nb_samples < maxOutSamples) {
      av_frame_unref(resampledAudioFrame_);
      resampledAudioFrame_->format = audioEncCtx_->sample_fmt;
      resampledAudioFrame_->sample_rate = audioEncCtx_->sample_rate;
      av_channel_layout_copy(&resampledAudioFrame_->ch_layout,
                             &audioEncCtx_->ch_layout);
      resampledAudioFrame_->nb_samples = maxOutSamples;
      av_frame_get_buffer(resampledAudioFrame_, 0);
    }

    // 重采样转换
    int converted =
        swr_convert(swrCtx_, resampledAudioFrame_->data, maxOutSamples,
                    const_cast<const uint8_t **>(decAudioFrame_->data),
                    decAudioFrame_->nb_samples);
    if (converted < 0) {
      LOG_EXP_warning("Audio resampling failed.");
      av_frame_unref(decAudioFrame_);
      continue;
    }

    // 写入 FIFO 队列
    av_audio_fifo_write(audioFifo_, (void **)resampledAudioFrame_->data,
                        converted);

    // 消费 FIFO 队列数据，包装成 frame_size 大小的包送去编码
    int frameSize = audioEncCtx_->frame_size;
    while (av_audio_fifo_size(audioFifo_) >= frameSize) {
      AVFrame *encFrame = av_frame_alloc();
      encFrame->format = audioEncCtx_->sample_fmt;
      encFrame->sample_rate = audioEncCtx_->sample_rate;
      av_channel_layout_copy(&encFrame->ch_layout, &audioEncCtx_->ch_layout);
      encFrame->nb_samples = frameSize;
      av_frame_get_buffer(encFrame, 0);

      av_audio_fifo_read(audioFifo_, (void **)encFrame->data, frameSize);

      encFrame->pts = nextAudioPts_;
      nextAudioPts_ += frameSize;

      if (!encodeAudioFrame(encFrame)) {
        av_frame_free(&encFrame);
        av_frame_unref(decAudioFrame_);
        return false;
      }

      av_frame_free(&encFrame);
    }

    av_frame_unref(decAudioFrame_);
  }

  return true;
}

bool VideoExporter::encodeVideoFrame(AVFrame *frame) {
  int ret = avcodec_send_frame(videoEncCtx_, frame);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("送入视频编码器失败：%1").arg(errbuf));
    return false;
  }

  AVPacket *encPkt = av_packet_alloc();
  while (true) {
    ret = avcodec_receive_packet(videoEncCtx_, encPkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      emit exportFailed(tr("获取视频编码包失败：%1").arg(errbuf));
      av_packet_free(&encPkt);
      return false;
    }

    // 时基转换为输出流时基
    encPkt->stream_index = outputVideoStreamIdx_;
    av_packet_rescale_ts(encPkt, videoEncCtx_->time_base,
                         outVideoStream_->time_base);

    writePacket(encPkt, true);
    av_packet_unref(encPkt);
  }

  av_packet_free(&encPkt);
  return true;
}

bool VideoExporter::encodeAudioFrame(AVFrame *frame) {
  int ret = avcodec_send_frame(audioEncCtx_, frame);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    emit exportFailed(tr("送入音频编码器失败：%1").arg(errbuf));
    return false;
  }

  AVPacket *encPkt = av_packet_alloc();
  while (true) {
    ret = avcodec_receive_packet(audioEncCtx_, encPkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      char errbuf[256];
      av_strerror(ret, errbuf, sizeof(errbuf));
      emit exportFailed(tr("获取音频编码包失败：%1").arg(errbuf));
      av_packet_free(&encPkt);
      return false;
    }

    encPkt->stream_index = outputAudioStreamIdx_;
    av_packet_rescale_ts(encPkt, audioEncCtx_->time_base,
                         outAudioStream_->time_base);

    writePacket(encPkt, false);
    av_packet_unref(encPkt);
  }

  av_packet_free(&encPkt);
  return true;
}

void VideoExporter::writePacket(AVPacket *pkt, bool isVideo) {
  Q_UNUSED(isVideo)

  // 多线程写锁在封装中由 FFmpeg 自身保障，直接写入文件
  int ret = av_interleaved_write_frame(outputFmtCtx_, pkt);
  if (ret < 0) {
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    LOG_EXP_warning("av_interleaved_write_frame failed:" << errbuf);
  }
}

void VideoExporter::flushEncoders() {
  LOG_EXP_info("Flushing encoders...");

  // 冲刷视频编码器
  if (videoEncCtx_) {
    encodeVideoFrame(nullptr);
  }

  // 冲刷音频编码器
  if (audioEncCtx_ && !audioCopyMode_) {
    // 如果 FIFO 中还有剩余样点，用 0 填充送去编码
    int frameSize = audioEncCtx_->frame_size;
    int remaining = av_audio_fifo_size(audioFifo_);
    if (remaining > 0) {
      AVFrame *encFrame = av_frame_alloc();
      encFrame->format = audioEncCtx_->sample_fmt;
      encFrame->sample_rate = audioEncCtx_->sample_rate;
      av_channel_layout_copy(&encFrame->ch_layout, &audioEncCtx_->ch_layout);
      encFrame->nb_samples = frameSize;
      av_frame_get_buffer(encFrame, 0);

      // 读取剩余，空缺零填充
      av_audio_fifo_read(audioFifo_, (void **)encFrame->data, remaining);

      encFrame->pts = nextAudioPts_;

      encodeAudioFrame(encFrame);
      av_frame_free(&encFrame);
    }

    encodeAudioFrame(nullptr);
  }
}

void VideoExporter::cleanup() {
  if (audioFifo_) {
    av_audio_fifo_free(audioFifo_);
    audioFifo_ = nullptr;
  }
  if (swsToRgb_) {
    sws_freeContext(swsToRgb_);
    swsToRgb_ = nullptr;
  }
  if (swsFromRgb_) {
    sws_freeContext(swsFromRgb_);
    swsFromRgb_ = nullptr;
  }
  if (swrCtx_) {
    swr_free(&swrCtx_);
    swrCtx_ = nullptr;
  }
  if (decVideoFrame_) {
    av_frame_free(&decVideoFrame_);
    decVideoFrame_ = nullptr;
  }
  if (decAudioFrame_) {
    av_frame_free(&decAudioFrame_);
    decAudioFrame_ = nullptr;
  }
  if (encVideoFrame_) {
    av_frame_free(&encVideoFrame_);
    encVideoFrame_ = nullptr;
  }
  if (resampledAudioFrame_) {
    av_frame_free(&resampledAudioFrame_);
    resampledAudioFrame_ = nullptr;
  }
  if (videoEncCtx_) {
    avcodec_free_context(&videoEncCtx_);
    videoEncCtx_ = nullptr;
  }
  if (videoDecCtx_) {
    avcodec_free_context(&videoDecCtx_);
    videoDecCtx_ = nullptr;
  }
  if (audioEncCtx_) {
    avcodec_free_context(&audioEncCtx_);
    audioEncCtx_ = nullptr;
  }
  if (audioDecCtx_) {
    avcodec_free_context(&audioDecCtx_);
    audioDecCtx_ = nullptr;
  }
  if (outputFmtCtx_) {
    if (outputFmtCtx_->pb) {
      avio_closep(&outputFmtCtx_->pb);
    }
    avformat_free_context(outputFmtCtx_);
    outputFmtCtx_ = nullptr;
  }
  if (inputFmtCtx_) {
    avformat_close_input(&inputFmtCtx_);
    inputFmtCtx_ = nullptr;
  }

  inputVideoStreamIdx_ = -1;
  inputAudioStreamIdx_ = -1;
  outputVideoStreamIdx_ = -1;
  outputAudioStreamIdx_ = -1;
  inVideoStream_ = nullptr;
  inAudioStream_ = nullptr;
  outVideoStream_ = nullptr;
  outAudioStream_ = nullptr;
}
