#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QThread>
#include <atomic>

class SubtitleTrack;

extern "C" {
struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;
struct SwrContext;
struct AVFrame;
struct AVPacket;
struct AVStream;
}

struct VideoExportConfig {
  QString inputPath;  // 源视频路径
  QString outputPath; // 目标输出路径

  // 视频编码设置
  QString videoCodec; // "libx264" | "libx265" | "h264_videotoolbox" |
                      // "hevc_videotoolbox"

  enum QualityMode {
    QualityHigh,         // 高质量 (CRF 18 / 硬件: 较高码率)
    QualityMedium,       // 中等质量 (CRF 23 / 硬件: 中等码率)
    QualityLow,          // 较低质量 (CRF 28 / 硬件: 较低码率)
    QualityCustomBitrate // 自定义码率 (customBitrateKbps)
  };
  QualityMode qualityMode = QualityMedium;
  int customBitrateKbps = 8000; // 自定义码率 (kbps)

  // 视频规格
  int outputWidth = 0;    // 0 = 保持原始
  int outputHeight = 0;   // 0 = 保持原始
  double outputFps = 0.0; // 0.0 = 保持原始

  // 音频设置
  bool exportAudio = true; // 是否导出音频
  int audioBitrateKbps = 0; // 0 = 与源视频一致，其他如 128, 192, 256, 320
  int audioSampleRate =
      0; // 0 = 与源视频一致，其他如 48000, 44100, 32000, 22050
};

class VideoExporter : public QThread {
  Q_OBJECT

public:
  explicit VideoExporter(QObject *parent = nullptr);
  ~VideoExporter() override;

  void setConfig(const VideoExportConfig &config);
  void setSubtitleTrack(const SubtitleTrack *track);

  void requestCancel();

  int progressPercent() const;
  qint64 elapsedMs() const;
  qint64 estimatedRemainingMs() const;

signals:
  void progressChanged(int percent);
  void exportFinished(const QString &outputPath);
  void exportFailed(const QString &error);
  void exportCancelled();

protected:
  void run() override;

private:
  bool openInput(bool useHwDecode = false);
  bool setupOutput();
  bool initVideoEncoder();
  bool initAudioStream();
  bool processFrames();

  bool decodeAndProcessVideo(AVPacket *pkt);
  bool decodeAndProcessAudio(AVPacket *pkt);

  bool encodeVideoFrame(AVFrame *frame);
  bool encodeAudioFrame(AVFrame *frame);

  void flushEncoders();
  void writePacket(AVPacket *pkt, bool isVideo);
  void cleanup();

  VideoExportConfig config_;
  const SubtitleTrack *track_ = nullptr;
  std::atomic<bool> cancelRequested_{false};

  // FFmpeg 上下文
  AVFormatContext *inputFmtCtx_ = nullptr;
  AVFormatContext *outputFmtCtx_ = nullptr;

  // 视频编解码
  AVCodecContext *videoDecCtx_ = nullptr;
  AVCodecContext *videoEncCtx_ = nullptr;

  // 音频编解码
  AVCodecContext *audioDecCtx_ = nullptr;
  AVCodecContext *audioEncCtx_ = nullptr;

  // 图像与音频处理上下文
  SwsContext *swsToRgb_ = nullptr;   // YUV -> RGB (用于 QPainter 画字幕)
  SwsContext *swsFromRgb_ = nullptr; // RGB -> YUV (用于送入编码器)
  SwrContext *swrCtx_ = nullptr;     // 音频重采样 (用于重编码 AAC)

  int inputVideoStreamIdx_ = -1;
  int inputAudioStreamIdx_ = -1;
  int outputVideoStreamIdx_ = -1;
  int outputAudioStreamIdx_ = -1;

  bool audioCopyMode_ = true; // 底层是否进行音频流拷贝

  // 输入/输出时间基准
  AVStream *inVideoStream_ = nullptr;
  AVStream *inAudioStream_ = nullptr;
  AVStream *outVideoStream_ = nullptr;
  AVStream *outAudioStream_ = nullptr;

  // 临时存储帧与包
  AVFrame *decVideoFrame_ = nullptr;
  AVFrame *decAudioFrame_ = nullptr;
  AVFrame *encVideoFrame_ = nullptr;        // 准备写入编码器的 YUV 帧
  AVFrame *resampledAudioFrame_ = nullptr;  // 重采样后的音频帧
  struct AVAudioFifo *audioFifo_ = nullptr; // 音频重采样缓冲区

  // 音频重采样累加 PTS / 样本计数
  int64_t nextAudioPts_ = 0;

  // 进度监控
  QElapsedTimer elapsedTimer_;
  qint64 totalDurationMs_ = 0;
  qint64 lastProcessedPtsMs_ = 0;
  int lastProgressPercent_ = 0;
};
