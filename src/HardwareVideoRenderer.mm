#include "HardwareVideoRenderer.h"
#include "CursorManager.h"
#include "SubtitleRenderer.h"
#include "ThemeManager.h"

#include <QCoreApplication>
#include <QCursor>
#include <QDebug>
#include <QElapsedTimer>
#include <QMap>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

#ifdef Q_OS_MAC
#import <CoreImage/CoreImage.h>
#import <CoreVideo/CoreVideo.h>
#include <OpenGL/OpenGL.h>
#endif

#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE 0x84F5
#endif

static QCursor getResizeCursor(HardwareVideoRenderer::DragMode hit,
                               double rotation, const QWidget *widget) {
  double baseAngle = 0.0;
  switch (hit) {
  case HardwareVideoRenderer::DragResizeMR:
    baseAngle = 0.0;
    break;
  case HardwareVideoRenderer::DragResizeBR:
    baseAngle = 45.0;
    break;
  case HardwareVideoRenderer::DragResizeBM:
    baseAngle = 90.0;
    break;
  case HardwareVideoRenderer::DragResizeBL:
    baseAngle = 135.0;
    break;
  case HardwareVideoRenderer::DragResizeML:
    baseAngle = 180.0;
    break;
  case HardwareVideoRenderer::DragResizeTL:
    baseAngle = 225.0;
    break;
  case HardwareVideoRenderer::DragResizeTM:
    baseAngle = 270.0;
    break;
  case HardwareVideoRenderer::DragResizeTR:
    baseAngle = 315.0;
    break;
  default:
    return Qt::ArrowCursor;
  }

  double actualAngle = baseAngle + rotation;
  return CursorManager::resizeCursor(actualAngle, widget);
}

#ifdef QT_DEBUG
#define PROFILE_TIMING 1
#else
#define PROFILE_TIMING 0
#endif

#define LOG_RENDER_info(msg) qInfo() << "[HardwareVideoRenderer]" << msg
#define LOG_RENDER_warning(msg) qWarning() << "[HardwareVideoRenderer]" << msg
#define LOG_RENDER_critical(msg) qCritical() << "[HardwareVideoRenderer]" << msg
#define LOG_RENDER_debug(msg) qDebug() << "[HardwareVideoRenderer]" << msg
#define LOG_RENDER(level, msg) LOG_RENDER_##level(msg)

// SubtitleLineEdit defined in SoftwareVideoRenderer.h / shared.
// Since SoftwareVideoRenderer.h defines SubtitleLineEdit, we can use it
// directly or implement a duplicate, but wait! SoftwareVideoRenderer.h is
// included? No. Let's use SoftwareVideoRenderer.h's definition of
// SubtitleLineEdit. Wait! Let's check: does include/SoftwareVideoRenderer.h
// define SubtitleLineEdit? Yes! In lines 17-96 of SoftwareVideoRenderer.h, it
// defines SubtitleLineEdit. So we can just #include "SoftwareVideoRenderer.h"
// or define it. To avoid compile cycle and keep them separate, we can just
// #include "SoftwareVideoRenderer.h" in HardwareVideoRenderer.cpp, or we can
// forward-declare it in the header and #include "SoftwareVideoRenderer.h" in
// the source! Yes, including it in HardwareVideoRenderer.cpp is perfect.
#include "SoftwareVideoRenderer.h"

HardwareVideoRenderer::HardwareVideoRenderer(QWidget *parent)
    : QOpenGLWidget(parent), videoSize_(1920, 1080) {
  // 强制请求 OpenGL Compatibility Profile 2.1，并开启 4x 多重采样抗锯齿
  QSurfaceFormat fmt;
  fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
  fmt.setVersion(2, 1);
  fmt.setSamples(4);
  setFormat(fmt);

  signals_ = new VideoRendererSignals(this);
  setMinimumSize(320, 180);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setMouseTracking(true);

  connect(&cursorTimer_, &QTimer::timeout, this, [this]() {
    cursorVisible_ = !cursorVisible_;
    update();
  });
}

HardwareVideoRenderer::~HardwareVideoRenderer() {
#ifdef Q_OS_MAC
  if (currentHwFrame_) {
    CVPixelBufferRelease(currentHwFrame_);
  }
  if (textureCache_) {
    CFRelease(textureCache_);
  }
  if (textureId_ != 0) {
    glDeleteTextures(1, &textureId_);
  }
  if (ciContext_) {
    [(id)ciContext_ release];
    ciContext_ = nullptr;
  }
#endif
}

void HardwareVideoRenderer::renderFrame(const DecodedVideoFrame &frame) {
#if PROFILE_TIMING
  QElapsedTimer copyTimer;
  copyTimer.start();
#endif
  {
    QMutexLocker lock(&imageMutex_);
#ifdef Q_OS_MAC
    if (currentHwFrame_) {
      CVPixelBufferRelease(currentHwFrame_);
      currentHwFrame_ = nullptr;
    }
    if (frame.hwFrame) {
      currentHwFrame_ = static_cast<CVPixelBufferRef>(frame.hwFrame);
      CVPixelBufferRetain(currentHwFrame_);
    }
#endif
    if (!frame.rgbaData.isEmpty() && frame.hwFrame == nullptr) {
      currentSwFrame_ =
          QImage(reinterpret_cast<const uchar *>(frame.rgbaData.constData()),
                 frame.width, frame.height, QImage::Format_RGBA8888)
              .copy();
    } else {
      currentSwFrame_ = QImage();
    }
    currentWidth_ = frame.width;
    currentHeight_ = frame.height;
    hasFrame_ = (currentHwFrame_ != nullptr || !currentSwFrame_.isNull());
  }
  double qScale = frame.qualityScale > 0.0 ? frame.qualityScale : 1.0;
  videoSize_ =
      QSize(qRound(frame.width / qScale), qRound(frame.height / qScale));
  QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);

#if PROFILE_TIMING
  static int renderLogCounter2 = 0;
  if (++renderLogCounter2 % 30 == 0) {
    qInfo() << "[TIMING:hw_render_copy] size=" << frame.width << "x"
            << frame.height << " cost_us=" << (copyTimer.nsecsElapsed() / 1000);
  }
#endif
}

void HardwareVideoRenderer::setVideoSize(const QSize &size) {
  if (size.isValid()) {
    videoSize_ = size;
    update();
  }
}

void HardwareVideoRenderer::clear() {
  {
    QMutexLocker lock(&imageMutex_);
#ifdef Q_OS_MAC
    if (currentHwFrame_) {
      CVPixelBufferRelease(currentHwFrame_);
      currentHwFrame_ = nullptr;
    }
#endif
    currentSwFrame_ = QImage();
    hasFrame_ = false;
    currentWidth_ = 0;
    currentHeight_ = 0;
  }
  videoSize_ = QSize(1920, 1080);
  cancelEditing();
  update();
}

int HardwareVideoRenderer::heightForWidth(int width) const {
  if (!videoSize_.isEmpty()) {
    return qRound(static_cast<double>(width) * videoSize_.height() /
                  videoSize_.width());
  }
  return width * 9 / 16;
}

void HardwareVideoRenderer::setSubtitleText(const QString &text) {
  if (isEditing_)
    return;
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleText_ = text;
  }
  update();
}

void HardwareVideoRenderer::setSubtitleFont(const QFont &font) {
  if (isEditing_)
    return;
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleFont_ = font;
  }
  update();
}

void HardwareVideoRenderer::setSubtitleBg(const QString &imagePath,
                                          bool is9Patch,
                                          const QMargins &margins) {
  {
    QMutexLocker lock(&bgMutex_);
    bgImagePath_ = imagePath;
    bgIs9Patch_ = is9Patch;
    bgMargins_ = margins;
  }
  update();
}

void HardwareVideoRenderer::clearSubtitleBg() {
  {
    QMutexLocker lock(&bgMutex_);
    bgImagePath_.clear();
  }
  update();
}

void HardwareVideoRenderer::setSubtitleStyle(const SubtitleItem &style) {
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleStyle_ = style;
    subtitleText_ = style.text;
    subtitleFont_.setFamily(style.fontFamily);
    subtitleFont_.setPointSize(style.fontSize);
    subtitleFont_.setBold(style.bold);
    subtitleFont_.setItalic(style.italic);
    subtitleFont_.setUnderline(style.underline);
    subtitleAlignment_ = style.alignment;
    if (dragMode_ == DragNone) {
      subtitleRotation_ = style.rotation;
      subtitleNormalizedRect_ =
          QRectF(style.rectX, style.rectY, style.rectW, style.rectH);
    }
  }
  update();
}

void HardwareVideoRenderer::drawNinePatch(QPainter &painter, const QImage &src,
                                          const QRect &target,
                                          const QMargins &m) {
  int sw = src.width();
  int sh = src.height();
  int tw = target.width();
  int th = target.height();

  int ml = m.left(), mr = m.right(), mt = m.top(), mb = m.bottom();

  ml = qMin(ml, sw / 2);
  mr = qMin(mr, sw / 2);
  mt = qMin(mt, sh / 2);
  mb = qMin(mb, sh / 2);

  QRect sTL(0, 0, ml, mt);
  QRect sTC(ml, 0, sw - ml - mr, mt);
  QRect sTR(sw - mr, 0, mr, mt);
  QRect sML(0, mt, ml, sh - mt - mb);
  QRect sMC(ml, mt, sw - ml - mr, sh - mt - mb);
  QRect sMR(sw - mr, mt, mr, sh - mt - mb);
  QRect sBL(0, sh - mb, ml, mb);
  QRect sBC(ml, sh - mb, sw - ml - mr, mb);
  QRect sBR(sw - mr, sh - mb, mr, mb);

  int tx = target.x(), ty = target.y();

  QRect dTL(tx, ty, ml, mt);
  QRect dTC(tx + ml, ty, tw - ml - mr, mt);
  QRect dTR(tx + tw - mr, ty, mr, mt);
  QRect dML(tx, ty + mt, ml, th - mt - mb);
  QRect dMC(tx + ml, ty + mt, tw - ml - mr, th - mt - mb);
  QRect dMR(tx + tw - mr, ty + mt, mr, th - mt - mb);
  QRect dBL(tx, ty + th - mb, ml, mb);
  QRect dBC(tx + ml, ty + th - mb, tw - ml - mr, mb);
  QRect dBR(tx + tw - mr, ty + th - mb, mr, mb);

  painter.drawImage(dTL, src, sTL);
  painter.drawImage(dTC, src, sTC);
  painter.drawImage(dTR, src, sTR);
  painter.drawImage(dML, src, sML);
  painter.drawImage(dMC, src, sMC);
  painter.drawImage(dMR, src, sMR);
  painter.drawImage(dBL, src, sBL);
  painter.drawImage(dBC, src, sBC);
  painter.drawImage(dBR, src, sBR);
}

QRect HardwareVideoRenderer::getTargetRect() const {
  int vw = videoSize_.width();
  int vh = videoSize_.height();
  if (vw <= 0 || vh <= 0) {
    return rect();
  }

  double widgetRatio =
      static_cast<double>(width()) / static_cast<double>(height());

  int cw = 0;
  int ch = 0;
  {
    QMutexLocker lock(&imageMutex_);
    cw = currentWidth_;
    ch = currentHeight_;
  }
  if (cw <= 0 || ch <= 0) {
    cw = vw;
    ch = vh;
  }

  double imageRatio = static_cast<double>(cw) / static_cast<double>(ch);

  int newWidth;
  int newHeight;
  if (widgetRatio > imageRatio) {
    newHeight = height();
    newWidth = static_cast<int>(height() * imageRatio);
  } else {
    newWidth = width();
    newHeight = static_cast<int>(width() / imageRatio);
  }

  int x = (width() - newWidth) / 2;
  int y = (height() - newHeight) / 2;
  return QRect(x, y, newWidth, newHeight);
}

QRect HardwareVideoRenderer::getSubtitlePixelRect() const {
  QRect targetRect = getTargetRect();

  int rx =
      targetRect.x() + qRound(targetRect.width() * subtitleNormalizedRect_.x());
  int ry = targetRect.y() +
           qRound(targetRect.height() * subtitleNormalizedRect_.y());
  int rw = qRound(targetRect.width() * subtitleNormalizedRect_.width());
  int rh = qRound(targetRect.height() * subtitleNormalizedRect_.height());
  return QRect(rx, ry, rw, rh);
}

void HardwareVideoRenderer::setSubtitleAlignment(int alignment) {
  if (isEditing_)
    return;
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleAlignment_ = alignment;
  }
  update();
}

void HardwareVideoRenderer::setSubtitleNormalizedRect(const QRectF &rect) {
  if (isEditing_)
    return;
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleNormalizedRect_ = rect;
  }
  update();
}

void HardwareVideoRenderer::setShowEditFrame(bool show) {
  showEditFrame_ = show;
  update();
}

void HardwareVideoRenderer::initializeGL() {
  initializeOpenGLFunctions();

  // 打印分配的 OpenGL 版本及硬件驱动平台信息以供调试诊断
  const GLubyte *version = glGetString(GL_VERSION);
  const GLubyte *vendor = glGetString(GL_VENDOR);
  const GLubyte *renderer = glGetString(GL_RENDERER);
  qInfo() << "[HardwareVideoRenderer] GL_VERSION:"
          << (version ? reinterpret_cast<const char *>(version) : "null")
          << "GL_VENDOR:"
          << (vendor ? reinterpret_cast<const char *>(vendor) : "null")
          << "GL_RENDERER:"
          << (renderer ? reinterpret_cast<const char *>(renderer) : "null");

#ifdef Q_OS_MAC
  CGLContextObj cglContext = CGLGetCurrentContext();
  if (cglContext) {
    CGLPixelFormatObj cglPixelFormat = CGLGetPixelFormat(cglContext);
    if (cglPixelFormat) {
      CVReturn cvRet =
          CVOpenGLTextureCacheCreate(kCFAllocatorDefault, nullptr, cglContext,
                                     cglPixelFormat, nullptr, &textureCache_);
      if (cvRet != kCVReturnSuccess) {
        qWarning()
            << "[HardwareVideoRenderer] Failed to create CVOpenGLTextureCache:"
            << cvRet;
      } else {
        qInfo() << "[HardwareVideoRenderer] CVOpenGLTextureCache created "
                   "successfully in initializeGL";
      }

      // 创建基于当前 OpenGL 上下文的 CIContext，并手动 retain
      // 增加引用计数防止被 AutoreleasePool 提前释放
      ciContext_ = (void *)[[CIContext contextWithCGLContext:cglContext
                                                 pixelFormat:cglPixelFormat
                                                  colorSpace:nil
                                                     options:nil] retain];
      if (ciContext_) {
        qInfo() << "[HardwareVideoRenderer] CIContext created and retained "
                   "successfully with CGLContext in initializeGL";
      } else {
        qWarning() << "[HardwareVideoRenderer] Failed to create CIContext in "
                      "initializeGL";
      }
    }
  } else {
    qWarning() << "[HardwareVideoRenderer] CGLGetCurrentContext returned "
                  "nullptr in initializeGL";
  }
#endif
}

void HardwareVideoRenderer::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void HardwareVideoRenderer::paintGL() {
  // 留空，绘制工作已全部由 paintEvent 中接管
}

void HardwareVideoRenderer::renderHwFrameOpenGL(CVPixelBufferRef cvBuf, int w,
                                                int h) {
  // 显式规范化 Viewport（使用物理像素比），防止在 Retina 屏幕下视口缩水在左下角
  double ratio = devicePixelRatioF();
  int physicalW = static_cast<int>(width() * ratio);
  int physicalH = static_cast<int>(height() * ratio);
  glViewport(0, 0, physicalW, physicalH);

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, physicalW, 0, physicalH, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();

  // Clear target video background to black
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

#ifdef Q_OS_MAC
  // 如果 ciContext_ 尚未初始化，在此处延迟进行初始化并手动 retain
  if (cvBuf && !ciContext_) {
    CGLContextObj cglContext = CGLGetCurrentContext();
    if (cglContext) {
      CGLPixelFormatObj cglPixelFormat = CGLGetPixelFormat(cglContext);
      if (cglPixelFormat) {
        CVOpenGLTextureCacheCreate(kCFAllocatorDefault, nullptr, cglContext,
                                   cglPixelFormat, nullptr, &textureCache_);
        ciContext_ = (void *)[[CIContext contextWithCGLContext:cglContext
                                                   pixelFormat:cglPixelFormat
                                                    colorSpace:nil
                                                       options:nil] retain];
      }
    }
  }

  if (cvBuf && ciContext_) {
    @autoreleasepool {
      CIImage *ciImage = [CIImage imageWithCVPixelBuffer:cvBuf];
      if (ciImage) {
        CIContext *context = (__bridge CIContext *)ciContext_;

        // 根据视口大小及 aspect ratio，计算在物理像素下的渲染目标坐标区间
        double destW = physicalW;
        double destH = physicalH;
        if (w > 0 && h > 0) {
          double widgetRatio = (double)physicalW / physicalH;
          double videoRatio = (double)w / h;
          if (widgetRatio > videoRatio) {
            destW = physicalH * videoRatio;
          } else {
            destH = physicalW / videoRatio;
          }
        }
        double destX = (physicalW - destW) / 2.0;
        double destY = (physicalH - destH) / 2.0;

        // 1. 初始化或重建渲染用 2D 纹理
        if (textureId_ == 0 || lastTexW_ != w || lastTexH_ != h) {
          if (textureId_ != 0) {
            glDeleteTextures(1, &textureId_);
          }
          glGenTextures(1, &textureId_);
          glBindTexture(GL_TEXTURE_2D, textureId_);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                       GL_UNSIGNED_BYTE, nullptr);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          lastTexW_ = w;
          lastTexH_ = h;
        }

        // 2. 将 CIImage 缩放到目标大小，并渲染至该 OpenGL 纹理中（在 GPU
        // 上零拷贝执行缩放与格式转换）
        double scaleX = (double)w / [ciImage extent].size.width;
        double scaleY = (double)h / [ciImage extent].size.height;
        CIImage *scaledImage = [ciImage
            imageByApplyingTransform:CGAffineTransformMakeScale(scaleX,
                                                                scaleY)];

        CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
        [context render:scaledImage
              toTexture:textureId_
                 bounds:CGRectMake(0, 0, w, h)
             colorSpace:cs];
        CGColorSpaceRelease(cs);

        // 3. 使用标准固定管线绘制纹理贴图 Quad，完美兼容各种 OpenGL 上下文环境
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureId_);

        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);

        glBegin(GL_QUADS);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(destX, destY);
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(destX + destW, destY);
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(destX + destW, destY + destH);
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(destX, destY + destH);
        glEnd();

        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);

        GLenum errAfterDraw = glGetError();
        if (errAfterDraw != GL_NO_ERROR) {
          qWarning()
              << "[HardwareVideoRenderer] GL error right after texture render:"
              << errAfterDraw;
        }
      } else {
        qWarning() << "[HardwareVideoRenderer] Failed to create CIImage from "
                      "CVPixelBuffer";
      }
    }
  } else {
    static int missingContextCount = 0;
    if (missingContextCount++ < 5) {
      qWarning()
          << "[HardwareVideoRenderer] cvBuf or ciContext_ missing. cvBuf:"
          << cvBuf << "ciContext_:" << ciContext_;
    }
  }
#endif

  // 恢复之前的矩阵栈状态
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();

  // 捕获并汇报 OpenGL 调用错误
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    qWarning() << "[HardwareVideoRenderer] GL error in renderHwFrameOpenGL:"
               << err;
  }
}

void HardwareVideoRenderer::paintEvent(QPaintEvent *event) {
#if PROFILE_TIMING
  QElapsedTimer timer;
  timer.start();
#endif

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::TextAntialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  // 1. 填充背景面板色彩
  QColor bgPanel = ThemeManager::instance().getBgPanelColor();
  painter.fillRect(rect(), bgPanel);

  QRect targetRect = getTargetRect();
  painter.fillRect(targetRect, Qt::black);

  bool isHwFrame = false;
  CVPixelBufferRef cvBuf = nullptr;
  QImage swImg;
  int w = 0;
  int h = 0;

  {
    QMutexLocker lock(&imageMutex_);
    if (hasFrame_) {
      if (currentHwFrame_ != nullptr) {
        isHwFrame = true;
        cvBuf = currentHwFrame_;
#ifdef Q_OS_MAC
        CVPixelBufferRetain(cvBuf);
#endif
        w = currentWidth_;
        h = currentHeight_;
      } else if (!currentSwFrame_.isNull()) {
        swImg = currentSwFrame_;
      }
    }
  }

  // 2. 根据帧类型选择渲染通道
  if (isHwFrame && cvBuf) {
    // 硬件帧：切入 native 绘画，利用 OpenGL 进行 GPU 零拷贝渲染
    painter.beginNativePainting();
    renderHwFrameOpenGL(cvBuf, w, h);
    painter.endNativePainting();
#ifdef Q_OS_MAC
    CVPixelBufferRelease(cvBuf);
#endif
  } else if (!swImg.isNull()) {
    // 软件帧 / 软解回退帧：使用普通的 QPainter::drawImage 绘制画面
    painter.drawImage(targetRect, swImg);
  }

  // 3. 统一绘制字幕及编辑虚线框等叠加元素
  drawSubtitlesOverlay(painter, targetRect);

#if PROFILE_TIMING
  qint64 elapsed = timer.nsecsElapsed() / 1000;
  static int paintLogCounter = 0;
  if (++paintLogCounter % 30 == 0) {
    qInfo() << "[TIMING:hw_paint] size=" << width() << "x" << height()
            << " paint_us=" << elapsed;
  }
#endif
}

void HardwareVideoRenderer::drawSubtitlesOverlay(QPainter &painter,
                                                 const QRect &targetRect) {
  QString text;
  QFont font;
  SubtitleItem activeStyle;
  QString bgPath;
  bool is9Patch = false;
  QMargins bgMargins;
  {
    QMutexLocker lock(&subtitleMutex_);
    text = subtitleText_;
    font = subtitleFont_;
    activeStyle = subtitleStyle_;
  }
  {
    QMutexLocker lock(&bgMutex_);
    bgPath = bgImagePath_;
    is9Patch = bgIs9Patch_;
    bgMargins = bgMargins_;
  }

  if (!text.isEmpty()) {
    QRect textRect = getSubtitlePixelRect();

    QFont drawFont = font;
    double refHeight = 1080.0;
    double scale = (targetRect.height() > 0)
                       ? (static_cast<double>(targetRect.height()) / refHeight)
                       : 1.0;

    int originalSize = font.pointSize();
    if (originalSize <= 0) {
      originalSize = font.pixelSize();
    }
    if (originalSize <= 0) {
      originalSize = 24;
    }

    int scaledSize = qMax(1, qRound(originalSize * scale));
    drawFont.setPixelSize(scaledSize);
    drawFont.setStyleStrategy(QFont::PreferAntialias);
    drawFont.setHintingPreference(QFont::PreferFullHinting);

    QFontMetrics fmTemp(drawFont);
    QString textToDraw = text;
    if (isEditing_ && editor_) {
      textToDraw = editor_->text();
    }
    QStringList linesTemp = textToDraw.split('\n');
    int textW = 0;
    for (const QString &line : linesTemp) {
      textW = qMax(textW, fmTemp.horizontalAdvance(line));
    }
    int textH = qMax(1, (int)linesTemp.size()) * fmTemp.height();
    double shrinkScale = 1.0;
    if (textW > textRect.width() && textRect.width() > 0) {
      shrinkScale =
          qMin(shrinkScale, static_cast<double>(textRect.width()) / textW);
    }
    if (textH > textRect.height() && textRect.height() > 0) {
      shrinkScale =
          qMin(shrinkScale, static_cast<double>(textRect.height()) / textH);
    }
    if (shrinkScale < 1.0) {
      scaledSize = qMax(1, qRound(scaledSize * shrinkScale));
      drawFont.setPixelSize(scaledSize);
    }

    // 创建透明缓冲图片以支持软件高精度抗锯齿（高DPI屏下使用物理像素大小以消除模糊）
    double dpr = devicePixelRatioF();
    QImage overlayImage(rect().size() * dpr,
                        QImage::Format_ARGB32_Premultiplied);
    overlayImage.setDevicePixelRatio(dpr);
    overlayImage.fill(Qt::transparent);
    {
      QPainter imgPainter(&overlayImage);
      imgPainter.setRenderHint(QPainter::Antialiasing, true);
      imgPainter.setRenderHint(QPainter::TextAntialiasing, true);
      imgPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);

      if (isEditing_ && editor_) {
        imgPainter.save();
        QTransform trans = getSubtitleTransform();
        imgPainter.setTransform(trans, true);
        QColor bgColor = ThemeManager::instance().getPrimaryColor();
        bgColor.setAlpha(30);
        imgPainter.fillRect(textRect, bgColor);
        imgPainter.restore();
      }

      imgPainter.save();
      // 不设置剪裁，允许超出视频边界绘制
      SubtitleRenderer::renderSubtitle(imgPainter, textToDraw, drawFont,
                                       activeStyle, textRect, subtitleRotation_,
                                       bgPath, is9Patch, bgMargins);
      imgPainter.restore();

      if (isEditing_ && editor_) {
        imgPainter.save();
        imgPainter.setRenderHint(QPainter::Antialiasing, true);
        QTransform trans = getSubtitleTransform();
        imgPainter.setTransform(trans, true);

        QFontMetrics fm(drawFont);
        QStringList lines = editor_->text().split('\n');

        int alignFlags = subtitleAlignment_ | Qt::AlignVCenter;
        QRect br = fm.boundingRect(textRect, alignFlags, editor_->text());
        int blockTop = br.top();

        int cursorPos = editor_->cursorPosition();
        int currentPos = 0;
        int lineIndex = 0;
        int posInLine = 0;
        for (int i = 0; i < lines.size(); ++i) {
          int len = lines[i].length() + 1;
          if (currentPos + len > cursorPos || i == lines.size() - 1) {
            lineIndex = i;
            posInLine = cursorPos - currentPos;
            break;
          }
          currentPos += len;
        }

        int lineY = blockTop + lineIndex * fm.lineSpacing();
        int cursorYTop = lineY;
        int cursorYBottom = lineY + fm.height();

        int lineW = fm.horizontalAdvance(lines[lineIndex]);
        int startX = textRect.left();
        if (subtitleAlignment_ & Qt::AlignHCenter) {
          startX = textRect.center().x() - lineW / 2;
        } else if (subtitleAlignment_ & Qt::AlignRight) {
          startX = textRect.right() - lineW;
        }

        if (editor_->hasSelectedText()) {
          int selStart = editor_->selectionStart();
          int selLength = editor_->selectionLength();
          int selEnd = selStart + selLength;

          int drawPos = 0;
          for (int i = 0; i < lines.size(); ++i) {
            int lineLen = lines[i].length();
            int lineStart = drawPos;
            int lineEnd = drawPos + lineLen;

            if (selEnd > lineStart && selStart <= lineEnd) {
              int s = qMax(lineStart, selStart) - lineStart;
              int e = qMin(lineEnd, selEnd) - lineStart;

              int lw = fm.horizontalAdvance(lines[i]);
              int lx = textRect.left();
              if (subtitleAlignment_ & Qt::AlignHCenter)
                lx = textRect.center().x() - lw / 2;
              else if (subtitleAlignment_ & Qt::AlignRight)
                lx = textRect.right() - lw;

              int selX = lx + fm.horizontalAdvance(lines[i].left(s));
              int selW = fm.horizontalAdvance(lines[i].mid(s, e - s));
              QRect selRect(selX, blockTop + i * fm.lineSpacing(), selW,
                            fm.height());
              QColor primary = ThemeManager::instance().getPrimaryColor();
              primary.setAlpha(100);
              imgPainter.fillRect(selRect, primary);
            }
            drawPos += lineLen + 1;
          }
        }

        if (cursorVisible_) {
          QString textBeforeCursor = lines[lineIndex].left(posInLine);
          int cursorX = startX + fm.horizontalAdvance(textBeforeCursor);

          QColor primary = ThemeManager::instance().getPrimaryColor();
          imgPainter.setPen(QPen(primary, 2));
          imgPainter.drawLine(cursorX, cursorYTop, cursorX, cursorYBottom);
        }

        imgPainter.restore();
      }

      if (showEditFrame_) {
        imgPainter.save();
        imgPainter.setRenderHint(QPainter::Antialiasing, true);

        QTransform trans = getSubtitleTransform();
        imgPainter.setTransform(trans, true);

        QColor primaryColor = ThemeManager::instance().getPrimaryColor();

        imgPainter.setPen(QPen(primaryColor, 1, Qt::DashLine));
        imgPainter.setBrush(Qt::NoBrush);
        imgPainter.drawRect(textRect);

        QPoint tmPt =
            QPoint(textRect.left() + textRect.width() / 2, textRect.top());
        QPoint rotPt(tmPt.x(), tmPt.y() - 25);

        imgPainter.setPen(QPen(primaryColor, 1, Qt::DashLine));
        imgPainter.drawLine(tmPt, rotPt);

        imgPainter.setPen(QPen(Qt::white, 1));
        imgPainter.setBrush(primaryColor);
        imgPainter.drawEllipse(rotPt, 5, 5);

        QList<QPoint> handlePoints = {
            textRect.topLeft(),
            tmPt,
            textRect.topRight(),
            QPoint(textRect.left(), textRect.top() + textRect.height() / 2),
            QPoint(textRect.right(), textRect.top() + textRect.height() / 2),
            textRect.bottomLeft(),
            QPoint(textRect.left() + textRect.width() / 2, textRect.bottom()),
            textRect.bottomRight()};

        imgPainter.setPen(QPen(Qt::white, 1));
        imgPainter.setBrush(primaryColor);
        int hs = 6;
        for (const QPoint &pt : handlePoints) {
          imgPainter.drawRect(pt.x() - hs / 2, pt.y() - hs / 2, hs, hs);
        }
        imgPainter.restore();
      }
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(0, 0, overlayImage);
    painter.restore();
  }
}

HardwareVideoRenderer::DragMode
HardwareVideoRenderer::hitTest(const QPoint &pos) const {
  if (!showEditFrame_ || subtitleText_.isEmpty())
    return DragNone;

  QRect pixelRect = getSubtitlePixelRect();
  int hs = 8;

  QTransform inv = getSubtitleTransform().inverted();
  QPoint localPos = inv.map(pos);

  QPoint tmPt(pixelRect.left() + pixelRect.width() / 2, pixelRect.top());
  QPoint rotPt(tmPt.x(), tmPt.y() - 25);
  if (QRect(rotPt.x() - hs, rotPt.y() - hs, hs * 2, hs * 2)
          .contains(localPos)) {
    return DragRotate;
  }

  auto inHandle = [localPos, hs](const QPoint &hPt) {
    return QRect(hPt.x() - hs, hPt.y() - hs, hs * 2, hs * 2).contains(localPos);
  };

  if (inHandle(pixelRect.topLeft()))
    return DragResizeTL;
  if (inHandle(tmPt))
    return DragResizeTM;
  if (inHandle(pixelRect.topRight()))
    return DragResizeTR;
  if (inHandle(
          QPoint(pixelRect.left(), pixelRect.top() + pixelRect.height() / 2)))
    return DragResizeML;
  if (inHandle(
          QPoint(pixelRect.right(), pixelRect.top() + pixelRect.height() / 2)))
    return DragResizeMR;
  if (inHandle(pixelRect.bottomLeft()))
    return DragResizeBL;
  if (inHandle(
          QPoint(pixelRect.left() + pixelRect.width() / 2, pixelRect.bottom())))
    return DragResizeBM;
  if (inHandle(pixelRect.bottomRight()))
    return DragResizeBR;

  if (pixelRect.contains(localPos)) {
    return DragMove;
  }

  return DragNone;
}

void HardwareVideoRenderer::setSubtitleRotation(double rotation) {
  if (isEditing_)
    return;
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleRotation_ = rotation;
  }
  update();
}

QTransform HardwareVideoRenderer::getSubtitleTransform() const {
  QRect pixelRect = getSubtitlePixelRect();
  QTransform trans;
  trans.translate(pixelRect.center().x(), pixelRect.center().y());
  trans.rotate(subtitleRotation_);
  trans.translate(-pixelRect.center().x(), -pixelRect.center().y());
  return trans;
}

double HardwareVideoRenderer::getNormalizedFontHeight() const {
  QFont font;
  {
    QMutexLocker lock(&subtitleMutex_);
    font = subtitleFont_;
  }
  int originalSize = font.pointSize();
  if (originalSize <= 0) {
    originalSize = font.pixelSize();
  }
  if (originalSize <= 0) {
    originalSize = 24;
  }
  QFont refFont = font;
  refFont.setPixelSize(originalSize);
  QFontMetrics fm(refFont);
  return static_cast<double>(fm.height()) / 1080.0;
}

bool HardwareVideoRenderer::eventFilter(QObject *watched, QEvent *event) {
  if (editor_ && (watched == editor_ || watched == editor_->viewport()) &&
      isEditing_) {
    if (event->type() == QEvent::MouseButtonPress) {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
      QPoint parentPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());
      QTransform inv = getSubtitleTransform().inverted();
      QPoint localPos = inv.map(parentPos);

      int cursorPos = cursorPosFromLocalPoint(localPos);
      if (editor_->hasSelectedText()) {
        editor_->deselect();
      }
      editor_->setCursorPosition(cursorPos);
      editClickAnchor_ = cursorPos;
      cursorVisible_ = true;
      update();
      return true;
    }

    if (event->type() == QEvent::MouseMove) {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->buttons() & Qt::LeftButton) {
        QPoint parentPos =
            mapFromGlobal(mouseEvent->globalPosition().toPoint());
        QTransform inv = getSubtitleTransform().inverted();
        QPoint localPos = inv.map(parentPos);

        int cursorPos = cursorPosFromLocalPoint(localPos);
        if (cursorPos != editClickAnchor_) {
          editor_->setSelection(editClickAnchor_, cursorPos - editClickAnchor_);
        } else {
          editor_->deselect();
          editor_->setCursorPosition(cursorPos);
        }
        cursorVisible_ = true;
        update();
      }
      return true;
    }

    if (event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick) {
      return true;
    }
  }
  return QOpenGLWidget::eventFilter(watched, event);
}

void HardwareVideoRenderer::mousePressEvent(QMouseEvent *event) {
  if (isEditing_ && editor_) {
    QRect pixelRect = getSubtitlePixelRect();
    QTransform inv = getSubtitleTransform().inverted();
    QPoint localPos = inv.map(event->pos());
    if (pixelRect.contains(localPos)) {
      int cursorPos = cursorPosFromLocalPoint(localPos);
      if (editor_->hasSelectedText()) {
        editor_->deselect();
      }
      editor_->setCursorPosition(cursorPos);
      editClickAnchor_ = cursorPos;
      cursorVisible_ = true;
      update();
      event->accept();
      return;
    }
    cancelEditing();
    event->accept();
    return;
  }

  if (event->button() == Qt::LeftButton) {
    dragMode_ = hitTest(event->pos());
    if (dragMode_ != DragNone) {
      dragStartPos_ = event->pos();
      dragStartNormalizedRect_ = subtitleNormalizedRect_;
      dragStartTransform_ = getSubtitleTransform();

      QFont font = subtitleFont_;
      int originalSize = font.pointSize();
      if (originalSize <= 0) {
        originalSize = font.pixelSize();
      }
      if (originalSize <= 0) {
        originalSize = 24;
      }
      dragStartFontSize_ = originalSize;

      QFont refFont = font;
      refFont.setPixelSize(originalSize);
      QFontMetrics fm(refFont);
      dragStartFontRefHeight_ = static_cast<double>(fm.height()) / 1080.0;
      currentDragFontSize_ = originalSize;

      event->accept();
      return;
    } else if (!subtitleText_.isEmpty()) {
      QRect pixelRect = getSubtitlePixelRect();
      QTransform inv = getSubtitleTransform().inverted();
      QPoint localPos = inv.map(event->pos());
      if (pixelRect.contains(localPos)) {
        emit signals_->subtitleClicked();
        event->accept();
        return;
      }
    }
  }
  QOpenGLWidget::mousePressEvent(event);
}

void HardwareVideoRenderer::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && !subtitleText_.isEmpty()) {
    QRect pixelRect = getSubtitlePixelRect();
    QTransform inv = getSubtitleTransform().inverted();
    QPoint localPos = inv.map(event->pos());
    if (pixelRect.contains(localPos)) {
      isEditing_ = true;
      if (!editor_) {
        editor_ = new SubtitleLineEdit(this);
        editor_->installEventFilter(this);
        editor_->viewport()->installEventFilter(this);
        connect(editor_, &SubtitleLineEdit::returnPressed, this,
                &HardwareVideoRenderer::commitEditing);
        connect(editor_, &SubtitleLineEdit::escPressed, this,
                &HardwareVideoRenderer::cancelEditing);
        connect(editor_, &SubtitleLineEdit::focusLost, this,
                &HardwareVideoRenderer::cancelEditing);
        connect(editor_, &QTextEdit::textChanged, this, [this]() {
          cursorVisible_ = true;
          updateEditorGeometry();
          update();
        });
      }
      editor_->setText(subtitleText_);
      editor_->show();
      updateEditorGeometry();

      int cursorPos = cursorPosFromLocalPoint(localPos);
      editor_->setCursorPosition(cursorPos);
      editClickAnchor_ = cursorPos;
      editor_->setFocus();

      cursorVisible_ = true;
      cursorTimer_.start(500);

      emit signals_->subtitleDoubleClicked();
      update();
      event->accept();
      return;
    }
  }
  QOpenGLWidget::mouseDoubleClickEvent(event);
}

void HardwareVideoRenderer::commitEditing() {
  if (!isEditing_ || !editor_)
    return;

  QString newText = editor_->text();
  isEditing_ = false;
  cursorTimer_.stop();
  editor_->hide();
  update();
  emit signals_->subtitleTextEdited(newText);
}

void HardwareVideoRenderer::cancelEditing() {
  if (!isEditing_ || !editor_)
    return;

  isEditing_ = false;
  cursorTimer_.stop();
  editor_->hide();
  update();
}

void HardwareVideoRenderer::updateEditorGeometry() {
  if (!editor_)
    return;
  QRect r = getSubtitlePixelRect();
  editor_->setGeometry(r);
}

int HardwareVideoRenderer::cursorPosFromLocalPoint(
    const QPoint &localPos) const {
  if (!editor_)
    return 0;

  QRect textRect = getSubtitlePixelRect();
  QString text = editor_->text();

  QFont drawFont = subtitleFont_;
  QRect targetRect = getTargetRect();
  double refHeight = 1080.0;
  double scale = (targetRect.height() > 0)
                     ? (static_cast<double>(targetRect.height()) / refHeight)
                     : 1.0;

  int originalSize = subtitleFont_.pointSize();
  if (originalSize <= 0) {
    originalSize = subtitleFont_.pixelSize();
  }
  if (originalSize <= 0) {
    originalSize = 24;
  }
  int scaledSize = qMax(1, qRound(originalSize * scale));
  drawFont.setPixelSize(scaledSize);

  QFontMetrics fmTemp(drawFont);
  QStringList linesTemp = text.split('\n');
  int textW = 0;
  for (const QString &line : linesTemp) {
    textW = qMax(textW, fmTemp.horizontalAdvance(line));
  }
  int textH = qMax(1, (int)linesTemp.size()) * fmTemp.height();
  double shrinkScale = 1.0;
  if (textW > textRect.width() && textRect.width() > 0) {
    shrinkScale =
        qMin(shrinkScale, static_cast<double>(textRect.width()) / textW);
  }
  if (textH > textRect.height() && textRect.height() > 0) {
    shrinkScale =
        qMin(shrinkScale, static_cast<double>(textRect.height()) / textH);
  }
  if (shrinkScale < 1.0) {
    scaledSize = qMax(1, qRound(scaledSize * shrinkScale));
    drawFont.setPixelSize(scaledSize);
  }

  QFontMetrics fm(drawFont);
  QStringList lines = text.split('\n');

  int alignFlags = subtitleAlignment_ | Qt::AlignVCenter;
  QRect br = fm.boundingRect(textRect, alignFlags, text);
  int blockTop = br.top();

  int clickedY = localPos.y();
  int relativeY = clickedY - blockTop;
  int lineSpacing = fm.lineSpacing();
  int lineIndex = relativeY / lineSpacing;
  lineIndex = qBound(0, lineIndex, static_cast<int>(lines.size() - 1));

  int basePos = 0;
  for (int i = 0; i < lineIndex; ++i) {
    basePos += lines[i].length() + 1;
  }

  QString lineText = lines[lineIndex];
  int lineW = fm.horizontalAdvance(lineText);
  int startX = textRect.left();
  if (subtitleAlignment_ & Qt::AlignHCenter) {
    startX = textRect.center().x() - lineW / 2;
  } else if (subtitleAlignment_ & Qt::AlignRight) {
    startX = textRect.right() - lineW;
  }

  int clickedX = localPos.x();
  int relativeX = clickedX - startX;

  if (relativeX <= 0) {
    return basePos;
  }
  if (relativeX >= lineW) {
    return basePos + lineText.length();
  }

  int bestIndex = 0;
  int minDiff = 999999;
  for (int i = 0; i <= lineText.length(); ++i) {
    int w = fm.horizontalAdvance(lineText.left(i));
    int diff = qAbs(w - relativeX);
    if (diff < minDiff) {
      minDiff = diff;
      bestIndex = i;
    }
  }
  return basePos + bestIndex;
}

void HardwareVideoRenderer::mouseMoveEvent(QMouseEvent *event) {
  if (isEditing_) {
    QRect pixelRect = getSubtitlePixelRect();
    QTransform inv = getSubtitleTransform().inverted();
    QPoint localPos = inv.map(event->pos());
    if (pixelRect.contains(localPos)) {
      setCursor(CursorManager::iBeamCursor(this));
    } else {
      unsetCursor();
    }
    event->accept();
    return;
  }

  if (dragMode_ != DragNone) {
    QPoint delta = event->pos() - dragStartPos_;
    QRect targetRect = getTargetRect();

    if (dragMode_ == DragMove) {
      double dx = static_cast<double>(delta.x()) / targetRect.width();
      double dy = static_cast<double>(delta.y()) / targetRect.height();

      double rx = dragStartNormalizedRect_.x() + dx;
      double ry = dragStartNormalizedRect_.y() + dy;

      subtitleNormalizedRect_.moveLeft(rx);
      subtitleNormalizedRect_.moveTop(ry);
      update();
    } else if (dragMode_ == DragRotate) {
      QRect pixelRect = getSubtitlePixelRect();
      QPointF center = pixelRect.center();
      QPointF diff = event->pos() - center;
      double angleRad = std::atan2(diff.y(), diff.x());
      double angleDeg = angleRad * 180.0 / M_PI;

      double newAngle = angleDeg + 90.0;
      while (newAngle < 0.0)
        newAngle += 360.0;
      while (newAngle >= 360.0)
        newAngle -= 360.0;

      double snapThreshold = 3.0; // 3度旋转磁吸阈值
      if (std::abs(newAngle - 0.0) < snapThreshold ||
          std::abs(newAngle - 360.0) < snapThreshold) {
        newAngle = 0.0;
      } else if (std::abs(newAngle - 90.0) < snapThreshold) {
        newAngle = 90.0;
      } else if (std::abs(newAngle - 180.0) < snapThreshold) {
        newAngle = 180.0;
      } else if (std::abs(newAngle - 270.0) < snapThreshold) {
        newAngle = 270.0;
      }

      {
        QMutexLocker lock(&subtitleMutex_);
        subtitleRotation_ = newAngle;
      }
      update();
    } else {
      QTransform inv = dragStartTransform_.inverted();
      QPoint localStart = inv.map(dragStartPos_);
      QPoint localCur = inv.map(event->pos());
      QPoint localDelta = localCur - localStart;

      double dx = static_cast<double>(localDelta.x()) / targetRect.width();
      double dy = static_cast<double>(localDelta.y()) / targetRect.height();

      double left = dragStartNormalizedRect_.left();
      double top = dragStartNormalizedRect_.top();
      double right = dragStartNormalizedRect_.right();
      double bottom = dragStartNormalizedRect_.bottom();

      constexpr double minW = 0.05;
      constexpr double minH = 0.05;

      switch (dragMode_) {
      case DragResizeTL:
        left = qMin(left + dx, right - minW);
        top = qMin(top + dy, bottom - minH);
        break;
      case DragResizeTR:
        right = qMax(right + dx, left + minW);
        top = qMin(top + dy, bottom - minH);
        break;
      case DragResizeBL:
        left = qMin(left + dx, right - minW);
        bottom = qMax(bottom + dy, top + minH);
        break;
      case DragResizeBR:
        right = qMax(right + dx, left + minW);
        bottom = qMax(bottom + dy, top + minH);
        break;
      case DragResizeML:
        left = qMin(left + dx, right - minW);
        break;
      case DragResizeMR:
        right = qMax(right + dx, left + minW);
        break;
      case DragResizeTM:
        top = qMin(top + dy, bottom - minH);
        break;
      case DragResizeBM:
        bottom = qMax(bottom + dy, top + minH);
        break;
      default:
        break;
      }

      double w1 = right - left;
      double h1 = bottom - top;

      double localCenterX = left + w1 / 2.0;
      double localCenterY = top + h1 / 2.0;

      double startCenterX = dragStartNormalizedRect_.left() +
                            dragStartNormalizedRect_.width() / 2.0;
      double startCenterY = dragStartNormalizedRect_.top() +
                            dragStartNormalizedRect_.height() / 2.0;

      double angleRad = subtitleRotation_ * M_PI / 180.0;
      double cosAngle = std::cos(angleRad);
      double sinAngle = std::sin(angleRad);

      double localDeltaX = localCenterX - startCenterX;
      double localDeltaY = localCenterY - startCenterY;

      double worldDeltaX = cosAngle * localDeltaX - sinAngle * localDeltaY;
      double worldDeltaY = sinAngle * localDeltaX + cosAngle * localDeltaY;

      double newCenterX = startCenterX + worldDeltaX;
      double newCenterY = startCenterY + worldDeltaY;

      double newLeft = newCenterX - w1 / 2.0;
      double newTop = newCenterY - h1 / 2.0;

      subtitleNormalizedRect_ = QRectF(newLeft, newTop, w1, h1);

      // 不再在拖拽时修改字体大小，只修改包围框
      currentDragFontSize_ = dragStartFontSize_;
      update();
    }
    event->accept();
    return;
  }

  DragMode mode = hitTest(event->pos());
  switch (mode) {
  case DragRotate:
    setCursor(CursorManager::rotateCursor(this));
    break;
  case DragMove:
    setCursor(CursorManager::sizeAllCursor(this));
    break;
  case DragResizeTL:
  case DragResizeTM:
  case DragResizeTR:
  case DragResizeML:
  case DragResizeMR:
  case DragResizeBL:
  case DragResizeBM:
  case DragResizeBR:
    setCursor(getResizeCursor(mode, subtitleRotation_, this));
    break;
  default:
    unsetCursor();
    break;
  }

  QOpenGLWidget::mouseMoveEvent(event);
}

void HardwareVideoRenderer::mouseReleaseEvent(QMouseEvent *event) {
  if (dragMode_ != DragNone) {
    DragMode prevMode = dragMode_;
    dragMode_ = DragNone;

    // 重新计算并根据当前释放点更新 Hover
    // 状态下的光标样式，避免松开鼠标时光标卡死在普通箭头
    DragMode hit = hitTest(event->pos());
    switch (hit) {
    case DragMove:
      setCursor(CursorManager::sizeAllCursor(this));
      break;
    case DragRotate:
      setCursor(CursorManager::rotateCursor(this));
      break;
    case DragResizeTL:
    case DragResizeTM:
    case DragResizeTR:
    case DragResizeML:
    case DragResizeMR:
    case DragResizeBL:
    case DragResizeBM:
    case DragResizeBR:
      setCursor(getResizeCursor(hit, subtitleRotation_, this));
      break;
    default:
      unsetCursor();
      break;
    }

    if (prevMode == DragRotate) {
      emit signals_->subtitleRotationChanged(subtitleRotation_);
    } else {
      emit signals_->subtitleRectChanged(subtitleNormalizedRect_);
    }
    event->accept();
  }
  QOpenGLWidget::mouseReleaseEvent(event);
}
