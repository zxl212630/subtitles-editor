#include "SoftwareVideoRenderer.h"
#include "SubtitleRenderer.h"
#include "ThemeManager.h"

#include <QCursor>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>

static QCursor getRotateCursor() {
  static bool initialized = false;
  static QCursor rotateCursor;
  if (!initialized) {
    QPixmap pixmap(":/icons/rotate.svg");
    if (!pixmap.isNull()) {
      QSize cursorSize(18, 18);
      QPixmap scaledPixmap = pixmap.scaled(cursorSize, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
      rotateCursor = QCursor(scaledPixmap, cursorSize.width() / 2,
                             cursorSize.height() / 2);
    } else {
      rotateCursor = QCursor(Qt::PointingHandCursor);
    }
    initialized = true;
  }
  return rotateCursor;
}
#define PROFILE_TIMING 1

#define LOG_RENDER_info(msg) qInfo() << "[VideoRenderer]" << msg
#define LOG_RENDER_warning(msg) qWarning() << "[VideoRenderer]" << msg
#define LOG_RENDER_critical(msg) qCritical() << "[VideoRenderer]" << msg
#define LOG_RENDER_debug(msg) qDebug() << "[VideoRenderer]" << msg
#define LOG_RENDER(level, msg) LOG_RENDER_##level(msg)

SoftwareVideoRenderer::SoftwareVideoRenderer(QWidget *parent)
    : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMinimumSize(320, 180);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setMouseTracking(true);
}

void SoftwareVideoRenderer::renderFrame(const DecodedVideoFrame &frame) {
#if PROFILE_TIMING
  QElapsedTimer copyTimer;
  copyTimer.start();
#endif
  {
    QMutexLocker lock(&imageMutex_);
    currentRgbaData_ = frame.rgbaData; // O(1) 隐式共享，无数据拷贝
    currentWidth_ = frame.width;
    currentHeight_ = frame.height;
    hasFrame_ = true;
  }
  videoSize_ = QSize(frame.width, frame.height);
  QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);

#if PROFILE_TIMING
  static int renderLogCounter2 = 0;
  if (++renderLogCounter2 % 30 == 0) {
    qInfo() << "[TIMING:render_copy] size=" << frame.width << "x"
            << frame.height << " cost_us=" << (copyTimer.nsecsElapsed() / 1000);
  }
#else
  LOG_RENDER(debug,
             "renderFrame() size=" << frame.width << "x" << frame.height);
#endif
}

void SoftwareVideoRenderer::clear() {
  {
    QMutexLocker lock(&imageMutex_);
    hasFrame_ = false;
  }
  videoSize_ = QSize();
  update();
}

int SoftwareVideoRenderer::heightForWidth(int width) const {
  if (!videoSize_.isEmpty()) {
    return qRound(static_cast<double>(width) * videoSize_.height() /
                  videoSize_.width());
  }
  return width * 9 / 16;
}

void SoftwareVideoRenderer::setSubtitleText(const QString &text) {
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleText_ = text;
  }
  update();
}

void SoftwareVideoRenderer::setSubtitleFont(const QFont &font) {
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleFont_ = font;
  }
  update();
}

void SoftwareVideoRenderer::setSubtitleBg(const QString &imagePath,
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

void SoftwareVideoRenderer::clearSubtitleBg() {
  {
    QMutexLocker lock(&bgMutex_);
    bgImagePath_.clear();
  }
  update();
}

void SoftwareVideoRenderer::drawNinePatch(QPainter &painter, const QImage &src,
                                          const QRect &target,
                                          const QMargins &m) {
  int sw = src.width();
  int sh = src.height();
  int tw = target.width();
  int th = target.height();

  int ml = m.left(), mr = m.right(), mt = m.top(), mb = m.bottom();

  // Clamp margins to source size
  ml = qMin(ml, sw / 2);
  mr = qMin(mr, sw / 2);
  mt = qMin(mt, sh / 2);
  mb = qMin(mb, sh / 2);

  // Source rects (9 regions)
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

  // Target rects
  QRect dTL(tx, ty, ml, mt);
  QRect dTC(tx + ml, ty, tw - ml - mr, mt);
  QRect dTR(tx + tw - mr, ty, mr, mt);
  QRect dML(tx, ty + mt, ml, th - mt - mb);
  QRect dMC(tx + ml, ty + mt, tw - ml - mr, th - mt - mb);
  QRect dMR(tx + tw - mr, ty + mt, mr, th - mt - mb);
  QRect dBL(tx, ty + th - mb, ml, mb);
  QRect dBC(tx + ml, ty + th - mb, tw - ml - mr, mb);
  QRect dBR(tx + tw - mr, ty + th - mb, mr, mb);

  // Draw 9 patches
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

QRect SoftwareVideoRenderer::getTargetRect() const {
  QMutexLocker lock(&imageMutex_);
  if (!hasFrame_ || currentWidth_ <= 0 || currentHeight_ <= 0) {
    return rect();
  }

  double widgetRatio =
      static_cast<double>(width()) / static_cast<double>(height());
  double imageRatio =
      static_cast<double>(currentWidth_) / static_cast<double>(currentHeight_);

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

QRect SoftwareVideoRenderer::getSubtitlePixelRect() const {
  QRect targetRect = getTargetRect();

  // 使用当前字幕对应的归一化坐标计算屏幕实际的像素排版框位置
  int rx =
      targetRect.x() + qRound(targetRect.width() * subtitleNormalizedRect_.x());
  int ry = targetRect.y() +
           qRound(targetRect.height() * subtitleNormalizedRect_.y());
  int rw = qRound(targetRect.width() * subtitleNormalizedRect_.width());
  int rh = qRound(targetRect.height() * subtitleNormalizedRect_.height());
  return QRect(rx, ry, rw, rh);
}

void SoftwareVideoRenderer::setSubtitleAlignment(int alignment) {
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleAlignment_ = alignment;
  }
  update();
}

void SoftwareVideoRenderer::setSubtitleNormalizedRect(const QRectF &rect) {
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleNormalizedRect_ = rect;
  }
  update();
}

void SoftwareVideoRenderer::setShowEditFrame(bool show) {
  showEditFrame_ = show;
  update();
}

void SoftwareVideoRenderer::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QElapsedTimer timer;
  timer.start();

  QPainter painter(this);
  QColor bgPanel = ThemeManager::instance().getBgPanelColor();
  painter.fillRect(rect(), bgPanel);

  QByteArray rgbaSnapshot;
  int w = 0;
  int h = 0;
  bool hasFrame = false;
  {
    QMutexLocker lock(&imageMutex_);
    hasFrame = hasFrame_;
    if (hasFrame) {
      rgbaSnapshot = currentRgbaData_; // O(1) 引用计数拷贝
      w = currentWidth_;
      h = currentHeight_;
    }
  }

  // Compute video target rect
  QRect targetRect = getTargetRect();
  if (hasFrame && w > 0 && h > 0) {
    // 构造零拷贝的 QImage，直接使用 rgbaSnapshot 的数据缓冲区
    QImage imageToDraw(
        reinterpret_cast<const uchar *>(rgbaSnapshot.constData()), w, h, w * 4,
        QImage::Format_RGBA8888);
    painter.drawImage(targetRect, imageToDraw);
  } else {
    LOG_RENDER(debug,
               "paintEvent() cost=" << timer.elapsed() << "ms (no frame)");
  }

  // Draw subtitle overlay (clipped to video area)
  QString text;
  QFont font;
  QString bgPath;
  bool is9Patch = false;
  QMargins bgMargins;
  {
    QMutexLocker lock(&subtitleMutex_);
    text = subtitleText_;
    font = subtitleFont_;
  }
  {
    QMutexLocker lock(&bgMutex_);
    bgPath = bgImagePath_;
    is9Patch = bgIs9Patch_;
    bgMargins = bgMargins_;
  }

  // 垂直居中，水平方向应用对齐配置
  int alignFlags = subtitleAlignment_ | Qt::AlignVCenter;

  if (!text.isEmpty()) {
    QRect textRect = getSubtitlePixelRect();

    // 1. 调用通用的 SubtitleRenderer 渲染字幕及其背景
    painter.save();
    painter.setClipRect(targetRect);
    SubtitleRenderer::renderSubtitle(painter, text, font, subtitleAlignment_,
                                     textRect, subtitleRotation_, bgPath,
                                     is9Patch, bgMargins);
    painter.restore();

    // 2. 绘制字幕的可拖拽编辑虚线框和 8 个控制点手柄，以及旋转手柄
    if (showEditFrame_) {
      painter.save();
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setClipRect(targetRect);

      // 应用中心旋转平移变换
      QTransform trans = getSubtitleTransform();
      painter.setTransform(trans, true);

      QColor primaryColor = ThemeManager::instance().getPrimaryColor();

      // 1. 绘制主色虚线框
      painter.setPen(QPen(primaryColor, 1, Qt::DashLine));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(textRect);

      // 2. 绘制旋转小圆点与连接线
      QPoint tmPt =
          QPoint(textRect.left() + textRect.width() / 2, textRect.top());
      QPoint rotPt(tmPt.x(), tmPt.y() - 25);

      painter.setPen(QPen(primaryColor, 1, Qt::DashLine));
      painter.drawLine(tmPt, rotPt);

      painter.setPen(QPen(Qt::white, 1));
      painter.setBrush(primaryColor);
      painter.drawEllipse(rotPt, 5, 5); // 直径 10 的圆

      // 3. 计算 8 个控制手柄的中心位置
      QList<QPoint> handlePoints = {
          textRect.topLeft(),
          tmPt,
          textRect.topRight(),
          QPoint(textRect.left(), textRect.top() + textRect.height() / 2),
          QPoint(textRect.right(), textRect.top() + textRect.height() / 2),
          textRect.bottomLeft(),
          QPoint(textRect.left() + textRect.width() / 2, textRect.bottom()),
          textRect.bottomRight()};

      // 4. 绘制 8 个手柄小方块，主色填充，细白色边框
      painter.setPen(QPen(Qt::white, 1));
      painter.setBrush(primaryColor);
      int hs = 6; // 手柄边长
      for (const QPoint &pt : handlePoints) {
        painter.drawRect(pt.x() - hs / 2, pt.y() - hs / 2, hs, hs);
      }
      painter.restore();
    }
  }

#if PROFILE_TIMING
  qint64 elapsed = timer.nsecsElapsed() / 1000;
  static int paintLogCounter = 0;
  if (++paintLogCounter % 30 == 0) {
    qInfo() << "[TIMING:paint] size=" << width() << "x" << height()
            << " image=" << w << "x" << h << " paint_us=" << elapsed;
  }
#endif
}

SoftwareVideoRenderer::DragMode
SoftwareVideoRenderer::hitTest(const QPoint &pos) const {
  if (!showEditFrame_ || subtitleText_.isEmpty())
    return DragNone;

  QRect pixelRect = getSubtitlePixelRect();
  int hs = 8; // 点击检测命中大小，设为 8x8 像素保证容错率

  // 将屏幕坐标映射回未旋转的局部坐标系
  QTransform inv = getSubtitleTransform().inverted();
  QPoint localPos = inv.map(pos);

  // 1. 优先判定是否命中旋转手柄（位于边界框顶部中点正上方 25px 处）
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

void SoftwareVideoRenderer::setSubtitleRotation(double rotation) {
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleRotation_ = rotation;
  }
  update();
}

QTransform SoftwareVideoRenderer::getSubtitleTransform() const {
  QRect pixelRect = getSubtitlePixelRect();
  QTransform trans;
  trans.translate(pixelRect.center().x(), pixelRect.center().y());
  trans.rotate(subtitleRotation_);
  trans.translate(-pixelRect.center().x(), -pixelRect.center().y());
  return trans;
}

void SoftwareVideoRenderer::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    dragMode_ = hitTest(event->pos());
    if (dragMode_ != DragNone) {
      dragStartPos_ = event->pos();
      dragStartNormalizedRect_ = subtitleNormalizedRect_;
      dragStartTransform_ = getSubtitleTransform();
      event->accept();
      return;
    }
  }
  QWidget::mousePressEvent(event);
}

void SoftwareVideoRenderer::mouseMoveEvent(QMouseEvent *event) {
  if (dragMode_ == DragNone) {
    DragMode hit = hitTest(event->pos());
    switch (hit) {
    case DragMove:
      setCursor(Qt::SizeAllCursor);
      break;
    case DragRotate:
      setCursor(getRotateCursor());
      break;
    case DragResizeTL:
    case DragResizeBR:
      setCursor(Qt::SizeFDiagCursor);
      break;
    case DragResizeTR:
    case DragResizeBL:
      setCursor(Qt::SizeBDiagCursor);
      break;
    case DragResizeTM:
    case DragResizeBM:
      setCursor(Qt::SizeVerCursor);
      break;
    case DragResizeML:
    case DragResizeMR:
      setCursor(Qt::SizeHorCursor);
      break;
    default:
      unsetCursor();
      break;
    }
  } else {
    QRect targetRect = getTargetRect();
    if (targetRect.width() <= 0 || targetRect.height() <= 0)
      return;

    if (dragMode_ == DragRotate) {
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

      double snapThreshold = 3.0; // 3 degrees magnetic snap threshold
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
      event->accept();
      return;
    }

    double dx = 0.0;
    double dy = 0.0;

    if (dragMode_ == DragMove) {
      QPoint delta = event->pos() - dragStartPos_;
      dx = static_cast<double>(delta.x()) / targetRect.width();
      dy = static_cast<double>(delta.y()) / targetRect.height();
    } else {
      QTransform invStart = dragStartTransform_.inverted();
      QPointF localStart = invStart.map(dragStartPos_);
      QPointF localCurrent = invStart.map(event->pos());
      QPointF localDelta = localCurrent - localStart;

      dx = localDelta.x() / targetRect.width();
      dy = localDelta.y() / targetRect.height();
    }

    QRectF newRect = dragStartNormalizedRect_;

    if (dragMode_ == DragMove) {
      newRect.translate(dx, dy);
      if (newRect.left() < 0)
        newRect.moveLeft(0);
      if (newRect.top() < 0)
        newRect.moveTop(0);
      if (newRect.right() > 1.0)
        newRect.moveRight(1.0);
      if (newRect.bottom() > 1.0)
        newRect.moveBottom(1.0);
    } else {
      double minW = 0.05;
      double minH = 0.05;

      switch (dragMode_) {
      case DragResizeTL: {
        double newLeft = qBound(0.0, dragStartNormalizedRect_.left() + dx,
                                dragStartNormalizedRect_.right() - minW);
        double newTop = qBound(0.0, dragStartNormalizedRect_.top() + dy,
                               dragStartNormalizedRect_.bottom() - minH);
        newRect.setLeft(newLeft);
        newRect.setTop(newTop);
        break;
      }
      case DragResizeTM: {
        double newTop = qBound(0.0, dragStartNormalizedRect_.top() + dy,
                               dragStartNormalizedRect_.bottom() - minH);
        newRect.setTop(newTop);
        break;
      }
      case DragResizeTR: {
        double newRight = qBound(dragStartNormalizedRect_.left() + minW,
                                 dragStartNormalizedRect_.right() + dx, 1.0);
        double newTop = qBound(0.0, dragStartNormalizedRect_.top() + dy,
                               dragStartNormalizedRect_.bottom() - minH);
        newRect.setRight(newRight);
        newRect.setTop(newTop);
        break;
      }
      case DragResizeML: {
        double newLeft = qBound(0.0, dragStartNormalizedRect_.left() + dx,
                                dragStartNormalizedRect_.right() - minW);
        newRect.setLeft(newLeft);
        break;
      }
      case DragResizeMR: {
        double newRight = qBound(dragStartNormalizedRect_.left() + minW,
                                 dragStartNormalizedRect_.right() + dx, 1.0);
        newRect.setRight(newRight);
        break;
      }
      case DragResizeBL: {
        double newLeft = qBound(0.0, dragStartNormalizedRect_.left() + dx,
                                dragStartNormalizedRect_.right() - minW);
        double newBottom = qBound(dragStartNormalizedRect_.top() + minH,
                                  dragStartNormalizedRect_.bottom() + dy, 1.0);
        newRect.setLeft(newLeft);
        newRect.setBottom(newBottom);
        break;
      }
      case DragResizeBM: {
        double newBottom = qBound(dragStartNormalizedRect_.top() + minH,
                                  dragStartNormalizedRect_.bottom() + dy, 1.0);
        newRect.setBottom(newBottom);
        break;
      }
      case DragResizeBR: {
        double newRight = qBound(dragStartNormalizedRect_.left() + minW,
                                 dragStartNormalizedRect_.right() + dx, 1.0);
        double newBottom = qBound(dragStartNormalizedRect_.top() + minH,
                                  dragStartNormalizedRect_.bottom() + dy, 1.0);
        newRect.setRight(newRight);
        newRect.setBottom(newBottom);
        break;
      }
      default:
        break;
      }
    }

    {
      QMutexLocker lock(&subtitleMutex_);
      subtitleNormalizedRect_ = newRect;
    }
    update();
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void SoftwareVideoRenderer::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && dragMode_ != DragNone) {
    DragMode prevMode = dragMode_;
    dragMode_ = DragNone;

    // 重新计算并根据当前释放点更新 Hover
    // 状态下的光标样式，避免松开鼠标时光标卡死在普通箭头
    DragMode hit = hitTest(event->pos());
    switch (hit) {
    case DragMove:
      setCursor(Qt::SizeAllCursor);
      break;
    case DragRotate:
      setCursor(getRotateCursor());
      break;
    case DragResizeTL:
    case DragResizeBR:
      setCursor(Qt::SizeFDiagCursor);
      break;
    case DragResizeTR:
    case DragResizeBL:
      setCursor(Qt::SizeBDiagCursor);
      break;
    case DragResizeTM:
    case DragResizeBM:
      setCursor(Qt::SizeVerCursor);
      break;
    case DragResizeML:
    case DragResizeMR:
      setCursor(Qt::SizeHorCursor);
      break;
    default:
      unsetCursor();
      break;
    }

    if (prevMode == DragRotate) {
      emit subtitleRotationChanged(subtitleRotation_);
    } else {
      emit subtitleRectChanged(subtitleNormalizedRect_);
    }
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}
