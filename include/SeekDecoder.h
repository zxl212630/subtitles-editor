#pragma once

#include "FFmpegDecoder.h" // for DecodedVideoFrame

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <atomic>

struct AVFormatContext;
struct AVCodecContext;
struct SwsContext;

extern "C" {
#include <libavutil/rational.h>
}

class SeekDecoder : public QThread {
  Q_OBJECT

public:
  explicit SeekDecoder(QObject *parent = nullptr);
  ~SeekDecoder() override;

  // 打开视频文件进行 Seek 预览
  bool open(const QString &path);
  void close();
  void setCancelOpen(bool cancel);

  // 设置预览输出的限宽限高尺寸（降分辨率缩放提高 sws_scale 效率）
  void setOutputSize(QSize size);

  // 发起 Seek 请求（线程安全，支持高频调用，自动合并）
  void requestSeek(qint64 targetMs, bool precise = true);

  // 停止并退出线程
  void shutdown();

signals:
  // 当精确解码出目标帧时发射
  void frameReady(DecodedVideoFrame frame);

protected:
  void run() override;

private:
  // 执行 Seek 和帧读取，直到找到最接近 targetMs 且 >= targetMs 的视频帧
  DecodedVideoFrame decodeOneFrame(qint64 targetMs, bool precise);

  // 格式转换 (YUV -> RGBA) 并且进行降分辨率处理
  DecodedVideoFrame convertFrame(AVFrame *frame, qint64 ptsMs);

  AVFormatContext *fmtCtx_ = nullptr;
  AVCodecContext *videoCodecCtx_ = nullptr;
  SwsContext *swsCtx_ = nullptr;
  int videoStreamIdx_ = -1;
  AVRational videoTimeBase_{0, 0};

  // SwsContext tracker
  int lastSrcW_ = -1;
  int lastSrcH_ = -1;
  int lastDstW_ = -1;
  int lastDstH_ = -1;
  int lastFormat_ = -1; // stores AVPixelFormat

  QSize outputSize_;
  QSize nativeSize_;
  mutable QMutex outputSizeMutex_;

  // 接收的 Seek 请求时间戳及请求计数版本号
  std::atomic<qint64> requestedMs_{-1};
  std::atomic<bool> precise_{true};
  std::atomic<int> seekGeneration_{0};
  int lastProcessedGeneration_ = 0;

  std::atomic<bool> running_{false};
  std::atomic<bool> cancelOpen_{false};
  static int decodeInterruptCb(void *ctx);
  QMutex wakeMutex_;
  QWaitCondition wakeCondition_;

  // 用于小范围智能 seek 的状态记录
  qint64 lastSeekTargetMs_ = -1;
  qint64 lastDecodedPtsMs_ = -1;

  QByteArray reusableRgbaBuffer_;
};
