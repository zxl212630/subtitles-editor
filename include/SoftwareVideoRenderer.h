#pragma once

#include "FFmpegDecoder.h"
#include <QHash>
#include <QImage>
#include <QMargins>
#include <QMutex>
#include <QRectF>
#include <QWidget>

class SoftwareVideoRenderer : public QWidget {
  Q_OBJECT

public:
  explicit SoftwareVideoRenderer(QWidget *parent = nullptr);

  void renderFrame(const DecodedVideoFrame &frame);
  void clear();
  void setSubtitleText(const QString &text);
  void setSubtitleFont(const QFont &font);
  void setSubtitleBg(const QString &imagePath, bool is9Patch,
                     const QMargins &margins);
  void clearSubtitleBg();

  QSize videoSize() const { return videoSize_; }
  void setVideoSize(const QSize &size);

  // === 字幕对齐与排版包围框设置 ===
  void setSubtitleAlignment(int alignment);
  void setSubtitleNormalizedRect(const QRectF &rect);
  void setSubtitleRotation(double rotation);
  void setShowEditFrame(bool show);
  QRectF subtitleNormalizedRect() const { return subtitleNormalizedRect_; }
  double subtitleRotation() const { return subtitleRotation_; }
  QTransform getSubtitleTransform() const;

signals:
  void subtitleRectChanged(const QRectF &rect);
  void subtitleRotationChanged(double rotation);

protected:
  void paintEvent(QPaintEvent *event) override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;

  // 鼠标交互重写
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  QByteArray currentRgbaData_;
  int currentWidth_ = 0;
  int currentHeight_ = 0;
  bool hasFrame_ = false;
  mutable QMutex imageMutex_;
  QSize videoSize_;

  QString subtitleText_;
  QFont subtitleFont_;
  QMutex subtitleMutex_;

  QString bgImagePath_;
  bool bgIs9Patch_ = false;
  QMargins bgMargins_;
  QHash<QString, QImage> bgCache_;
  QMutex bgMutex_;

  // 字幕排版与拖拽状态变量
  int subtitleAlignment_ = 0x84; // Qt::AlignHCenter | Qt::AlignVCenter
  QRectF subtitleNormalizedRect_{0.1, 0.75, 0.8, 0.2};
  double subtitleRotation_ = 0.0;
  bool showEditFrame_ = true;

  enum DragMode {
    DragNone,
    DragMove,
    DragResizeTL,
    DragResizeTM,
    DragResizeTR,
    DragResizeML,
    DragResizeMR,
    DragResizeBL,
    DragResizeBM,
    DragResizeBR,
    DragRotate
  };
  DragMode dragMode_ = DragNone;
  QPoint dragStartPos_;
  QRectF dragStartNormalizedRect_;
  QTransform dragStartTransform_;

  QRect getTargetRect() const;
  QRect getSubtitlePixelRect() const;
  DragMode hitTest(const QPoint &pos) const;

  void drawNinePatch(QPainter &painter, const QImage &src, const QRect &target,
                     const QMargins &margins);
};
