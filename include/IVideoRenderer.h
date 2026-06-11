#pragma once

#include "FFmpegDecoder.h"
#include "SubtitleItem.h"
#include <QFont>
#include <QMargins>
#include <QObject>
#include <QRectF>
#include <QTransform>
#include <QWidget>

class VideoRendererSignals : public QObject {
  Q_OBJECT
public:
  explicit VideoRendererSignals(QObject *parent = nullptr) : QObject(parent) {}
signals:
  void subtitleRectChanged(const QRectF &rect);
  void subtitleRotationChanged(double rotation);
  void subtitleClicked();
  void subtitleDoubleClicked();
  void subtitleTextEdited(const QString &text);
  void subtitleFontSizeChanged(int size);
};

class IVideoRenderer {
public:
  virtual ~IVideoRenderer() = default;

  virtual void renderFrame(const DecodedVideoFrame &frame) = 0;
  virtual void clear() = 0;
  virtual void setSubtitleText(const QString &text) = 0;
  virtual void setSubtitleFont(const QFont &font) = 0;
  virtual void setSubtitleBg(const QString &imagePath, bool is9Patch,
                             const QMargins &margins) = 0;
  virtual void clearSubtitleBg() = 0;
  virtual void setSubtitleStyle(const SubtitleItem &style) = 0;

  virtual QSize videoSize() const = 0;
  virtual void setVideoSize(const QSize &size) = 0;

  // 字幕对齐与排版包围框设置
  virtual void setSubtitleAlignment(int alignment) = 0;
  virtual void setSubtitleNormalizedRect(const QRectF &rect) = 0;
  virtual void setSubtitleRotation(double rotation) = 0;
  virtual void setShowEditFrame(bool show) = 0;
  virtual QRectF subtitleNormalizedRect() const = 0;
  virtual double subtitleRotation() const = 0;
  virtual QTransform getSubtitleTransform() const = 0;

  virtual QWidget *asWidget() = 0;
  virtual VideoRendererSignals *videoSignals() = 0;
  virtual bool supportsHardwareFrames() const = 0;
};
