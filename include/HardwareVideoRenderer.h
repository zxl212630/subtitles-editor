#pragma once

#include "IVideoRenderer.h"
#include "SubtitleItem.h"

#include <QFocusEvent>
#include <QHash>
#include <QImage>
#include <QKeyEvent>
#include <QMargins>
#include <QMutex>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QRectF>
#include <QTimer>

#ifdef Q_OS_MAC
#include <CoreVideo/CVOpenGLTextureCache.h>
#endif

class SubtitleLineEdit;

class HardwareVideoRenderer : public QOpenGLWidget,
                              protected QOpenGLFunctions,
                              public IVideoRenderer {
  Q_OBJECT

public:
  explicit HardwareVideoRenderer(QWidget *parent = nullptr);
  ~HardwareVideoRenderer() override;

  // IVideoRenderer overrides
  void renderFrame(const DecodedVideoFrame &frame) override;
  void clear() override;
  void setSubtitleText(const QString &text) override;
  void setSubtitleFont(const QFont &font) override;
  void setSubtitleBg(const QString &imagePath, bool is9Patch,
                     const QMargins &margins) override;
  void clearSubtitleBg() override;
  void setSubtitleStyle(const SubtitleItem &style) override;

  QSize videoSize() const override { return videoSize_; }
  void setVideoSize(const QSize &size) override;

  // === 字幕对齐与排版包围框设置 ===
  void setSubtitleAlignment(int alignment) override;
  void setSubtitleNormalizedRect(const QRectF &rect) override;
  void setSubtitleRotation(double rotation) override;
  void setShowEditFrame(bool show) override;
  QRectF subtitleNormalizedRect() const override {
    return subtitleNormalizedRect_;
  }
  double subtitleRotation() const override { return subtitleRotation_; }
  QTransform getSubtitleTransform() const override;

  QWidget *asWidget() override { return this; }
  VideoRendererSignals *videoSignals() override { return signals_; }
  bool supportsHardwareFrames() const override { return true; }

protected:
  // OpenGL lifecycle
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void paintEvent(QPaintEvent *event) override;

  // Event handlers
  bool eventFilter(QObject *watched, QEvent *event) override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;

  // Mouse / Keyboard interactions
  void mousePressEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
#ifdef Q_OS_MAC
  CVPixelBufferRef currentHwFrame_ = nullptr;
  CVOpenGLTextureCacheRef textureCache_ = nullptr;
  void *ciContext_ = nullptr;
  unsigned int textureId_ = 0;
  int lastTexW_ = 0;
  int lastTexH_ = 0;
#endif
  QImage currentSwFrame_;
  void renderHwFrameOpenGL(CVPixelBufferRef cvBuf, int w, int h);
  int currentWidth_ = 0;
  int currentHeight_ = 0;
  bool hasFrame_ = false;
  mutable QMutex imageMutex_;
  QSize videoSize_;

  QString subtitleText_;
  QFont subtitleFont_;
  SubtitleItem subtitleStyle_;
  mutable QMutex subtitleMutex_;

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

  double getNormalizedFontHeight() const;

  void commitEditing();
  void cancelEditing();
  void updateEditorGeometry();
  int cursorPosFromLocalPoint(const QPoint &localPos) const;

  void drawSubtitlesOverlay(QPainter &painter, const QRect &targetRect);

  SubtitleLineEdit *editor_ = nullptr;
  VideoRendererSignals *signals_ = nullptr;
  bool isEditing_ = false;
  QTimer cursorTimer_;
  bool cursorVisible_ = true;
  int editClickAnchor_ = 0;

  int dragStartFontSize_ = 24;
  double dragStartFontRefHeight_ = 0.05;
  int currentDragFontSize_ = 24;
};
