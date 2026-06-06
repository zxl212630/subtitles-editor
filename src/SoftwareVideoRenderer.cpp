#include "SoftwareVideoRenderer.h"
#include "SubtitleRenderer.h"
#include "ThemeManager.h"

#include <QCoreApplication>
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
    : QWidget(parent), videoSize_(1920, 1080) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMinimumSize(320, 180);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setMouseTracking(true);

  connect(&cursorTimer_, &QTimer::timeout, this, [this]() {
    cursorVisible_ = !cursorVisible_;
    update();
  });
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

void SoftwareVideoRenderer::setVideoSize(const QSize &size) {
  if (size.isValid()) {
    videoSize_ = size;
    update();
  }
}

void SoftwareVideoRenderer::clear() {
  {
    QMutexLocker lock(&imageMutex_);
    hasFrame_ = false;
    currentRgbaData_.clear();
    currentWidth_ = 0;
    currentHeight_ = 0;
  }
  videoSize_ = QSize(1920, 1080);
  cancelEditing();
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
  if (isEditing_)
    return;
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleText_ = text;
  }
  update();
}

void SoftwareVideoRenderer::setSubtitleFont(const QFont &font) {
  if (isEditing_)
    return;
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
  if (isEditing_)
    return;
  {
    QMutexLocker lock(&subtitleMutex_);
    subtitleAlignment_ = alignment;
  }
  update();
}

void SoftwareVideoRenderer::setSubtitleNormalizedRect(const QRectF &rect) {
  if (isEditing_)
    return;
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

  // 先用黑色填充视频区域作为背景画布
  painter.fillRect(targetRect, Qt::black);

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

    // 根据预览画面实际高度与基准高度 1080.0
    // 的比例，缩放预览字体大小，保持与导出比例一致
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
      originalSize = 24; // 默认备用值
    }

    int scaledSize = qMax(1, qRound(originalSize * scale));
    drawFont.setPixelSize(scaledSize);

    // 动态判断：如果边框过小，则等比例缩放字体以适应边框
    QFontMetrics fmTemp(drawFont);
    QString textToDraw = text;
    if (isEditing_ && editor_) {
      textToDraw = editor_->text();
    }
    int textW = fmTemp.horizontalAdvance(textToDraw);
    int textH = fmTemp.height();
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

    // 调试输出
    qDebug() << "[VideoRenderer] Rendering Subtitle text=" << text
             << " font=" << drawFont.family()
             << " size=" << drawFont.pixelSize() << " textRect=" << textRect
             << " targetRect=" << targetRect << " scale=" << scale
             << " hasFrame=" << hasFrame;

    // 1. 调用通用的 SubtitleRenderer 渲染字幕及其背景
    painter.save();
    painter.setClipRect(targetRect);
    SubtitleRenderer::renderSubtitle(
        painter, textToDraw, drawFont, subtitleAlignment_, textRect,
        subtitleRotation_, bgPath, is9Patch, bgMargins);
    painter.restore();

    // 2. 如果处于编辑状态，在旋转坐标系下自定义绘制光标和选区
    if (isEditing_ && editor_) {
      painter.save();
      painter.setRenderHint(QPainter::Antialiasing, true);
      QTransform trans = getSubtitleTransform();
      painter.setTransform(trans, true);

      QFontMetrics fm(drawFont);
      int totalTextWidth = fm.horizontalAdvance(editor_->text());
      int startX = textRect.left();
      if (subtitleAlignment_ & Qt::AlignHCenter) {
        startX = textRect.center().x() - totalTextWidth / 2;
      } else if (subtitleAlignment_ & Qt::AlignRight) {
        startX = textRect.right() - totalTextWidth;
      }

      int cursorYTop = textRect.center().y() - fm.height() / 2;
      int cursorYBottom = textRect.center().y() + fm.height() / 2;

      // 1. 绘制选区高亮
      if (editor_->hasSelectedText()) {
        int selStart = editor_->selectionStart();
        int selLength = editor_->selectionLength();
        QString textBeforeSel = editor_->text().left(selStart);
        QString selText = editor_->text().mid(selStart, selLength);

        int selStartX = startX + fm.horizontalAdvance(textBeforeSel);
        int selWidth = fm.horizontalAdvance(selText);

        QRect selRect(selStartX, cursorYTop, selWidth, fm.height());
        QColor primary = ThemeManager::instance().getPrimaryColor();
        primary.setAlpha(100); // 半透明选择色
        painter.fillRect(selRect, primary);
      }

      // 2. 绘制闪烁光标
      if (cursorVisible_) {
        int cursorPos = editor_->cursorPosition();
        QString textBeforeCursor = editor_->text().left(cursorPos);
        int cursorX = startX + fm.horizontalAdvance(textBeforeCursor);

        QColor primary = ThemeManager::instance().getPrimaryColor();
        painter.setPen(QPen(primary, 2));
        painter.drawLine(cursorX, cursorYTop, cursorX, cursorYBottom);
      }

      painter.restore();
    }

    // 2. 绘制字幕的可拖拽编辑虚线框和 8 个控制点手柄，以及旋转手柄
    if (showEditFrame_) {
      painter.save();
      painter.setRenderHint(QPainter::Antialiasing, true);
      // Do not clip the edit frame to targetRect so handles remain visible and
      // grabbable outside the video frame

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
  if (isEditing_)
    return;
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

double SoftwareVideoRenderer::getNormalizedFontHeight() const {
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

bool SoftwareVideoRenderer::eventFilter(QObject *watched, QEvent *event) {
  if (watched == editor_ && isEditing_ && editor_) {
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
  return QWidget::eventFilter(watched, event);
}

void SoftwareVideoRenderer::mousePressEvent(QMouseEvent *event) {
  if (isEditing_ && editor_) {
    // Check if the click is within the subtitle area; if so, reposition cursor
    // directly instead of cancelling
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

      // Initialize drag start font size and reference height
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
        emit subtitleClicked();
        event->accept();
        return;
      }
    }
  }
  QWidget::mousePressEvent(event);
}

void SoftwareVideoRenderer::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && !subtitleText_.isEmpty()) {
    QRect pixelRect = getSubtitlePixelRect();
    QTransform inv = getSubtitleTransform().inverted();
    QPoint localPos = inv.map(event->pos());
    if (pixelRect.contains(localPos)) {
      isEditing_ = true;
      if (!editor_) {
        editor_ = new SubtitleLineEdit(this);
        editor_->installEventFilter(this);
        connect(editor_, &QLineEdit::returnPressed, this,
                &SoftwareVideoRenderer::commitEditing);
        connect(editor_, &SubtitleLineEdit::escPressed, this,
                &SoftwareVideoRenderer::cancelEditing);
        connect(editor_, &SubtitleLineEdit::focusLost, this,
                &SoftwareVideoRenderer::cancelEditing);
        connect(editor_, &QLineEdit::textChanged, this, [this]() {
          updateEditorGeometry();
          update();
        });
        connect(editor_, &QLineEdit::cursorPositionChanged, this, [this]() {
          cursorVisible_ = true;
          update();
        });
      }

      QRect targetRect = getTargetRect();
      double refHeight = 1080.0;
      double scale =
          (targetRect.height() > 0)
              ? (static_cast<double>(targetRect.height()) / refHeight)
              : 1.0;
      QFont drawFont = subtitleFont_;
      int originalSize = subtitleFont_.pointSize();
      if (originalSize <= 0)
        originalSize = subtitleFont_.pixelSize();
      if (originalSize <= 0)
        originalSize = 24;
      int scaledSize = qMax(1, qRound(originalSize * scale));
      drawFont.setPixelSize(scaledSize);

      // 编辑状态下如果边框过小也等比例缩小
      QRect pixelRect = getSubtitlePixelRect();
      QFontMetrics fmTemp(drawFont);
      int textW = fmTemp.horizontalAdvance(subtitleText_);
      int textH = fmTemp.height();
      double shrinkScale = 1.0;
      if (textW > pixelRect.width() && pixelRect.width() > 0) {
        shrinkScale =
            qMin(shrinkScale, static_cast<double>(pixelRect.width()) / textW);
      }
      if (textH > pixelRect.height() && pixelRect.height() > 0) {
        shrinkScale =
            qMin(shrinkScale, static_cast<double>(pixelRect.height()) / textH);
      }
      if (shrinkScale < 1.0) {
        scaledSize = qMax(1, qRound(scaledSize * shrinkScale));
        drawFont.setPixelSize(scaledSize);
      }

      // 1. 先设置样式属性，限制隐形输入框的字体族和大小
      QString style =
          QString(
              "background: transparent; border: none; outline: none; "
              "color: transparent; selection-background-color: transparent; "
              "selection-color: transparent; "
              "font-family: '%1'; font-size: %2px;")
              .arg(drawFont.family())
              .arg(scaledSize);
      editor_->setAttribute(Qt::WA_MacShowFocusRect, false);
      editor_->setStyleSheet(style);

      // 2. 设置文本和字体。为了在光标定位时精确测量坐标，使用左对齐方式
      editor_->setText(subtitleText_);
      editor_->setFont(drawFont);
      editor_->setAlignment(Qt::AlignLeft);

      // 3. 动态更新输入框几何形状，使其正好包裹文字
      updateEditorGeometry();

      editor_->show();
      editor_->setFocus();
      editor_->selectAll();

      cursorVisible_ = true;
      cursorTimer_.start(500);

      update();
      event->accept();
      return;
    }
  }
  QWidget::mouseDoubleClickEvent(event);
}

void SoftwareVideoRenderer::updateEditorGeometry() {
  if (!editor_)
    return;

  QRect pixelRect = getSubtitlePixelRect();
  QFontMetrics fm(editor_->font());
  int totalTextWidth = fm.horizontalAdvance(editor_->text());

  // 增加 20px 额外余量以容纳光标闪烁以及避免右边界裁剪
  int widgetWidth = qMax(40, totalTextWidth + 20);

  int startX = pixelRect.left();
  if (subtitleAlignment_ & Qt::AlignHCenter) {
    startX = pixelRect.center().x() - widgetWidth / 2;
  } else if (subtitleAlignment_ & Qt::AlignRight) {
    startX = pixelRect.right() - widgetWidth;
  }

  editor_->setGeometry(startX, pixelRect.y(), widgetWidth, pixelRect.height());
}

int SoftwareVideoRenderer::cursorPosFromLocalPoint(
    const QPoint &localPos) const {
  if (!editor_)
    return 0;

  // Compute the same scaled font used for visual rendering in paintEvent
  QRect targetRect = getTargetRect();
  QFont drawFont;
  {
    QMutexLocker lock(&subtitleMutex_);
    drawFont = subtitleFont_;
  }
  int originalSize = drawFont.pointSize();
  if (originalSize <= 0)
    originalSize = drawFont.pixelSize();
  if (originalSize <= 0)
    originalSize = 24;
  double scale = (targetRect.height() > 0)
                     ? (static_cast<double>(targetRect.height()) / 1080.0)
                     : 1.0;
  int scaledSize = qMax(1, qRound(originalSize * scale));
  drawFont.setPixelSize(scaledSize);

  QRect pixelRect = getSubtitlePixelRect();
  QString text = editor_->text();

  // 与 paintEvent 相同的动态缩小逻辑
  QFontMetrics fmTemp(drawFont);
  int textW = fmTemp.horizontalAdvance(text);
  int textH = fmTemp.height();
  double shrinkScale = 1.0;
  if (textW > pixelRect.width() && pixelRect.width() > 0) {
    shrinkScale =
        qMin(shrinkScale, static_cast<double>(pixelRect.width()) / textW);
  }
  if (textH > pixelRect.height() && pixelRect.height() > 0) {
    shrinkScale =
        qMin(shrinkScale, static_cast<double>(pixelRect.height()) / textH);
  }
  if (shrinkScale < 1.0) {
    scaledSize = qMax(1, qRound(scaledSize * shrinkScale));
    drawFont.setPixelSize(scaledSize);
  }

  QFontMetrics fm(drawFont);
  int totalTextWidth = fm.horizontalAdvance(text);

  // Compute text start X using the same logic as paintEvent
  int textStartX = pixelRect.left();
  if (subtitleAlignment_ & Qt::AlignHCenter) {
    textStartX = pixelRect.center().x() - totalTextWidth / 2;
  } else if (subtitleAlignment_ & Qt::AlignRight) {
    textStartX = pixelRect.right() - totalTextWidth;
  }

  int clickOffset = localPos.x() - textStartX;
  if (clickOffset <= 0)
    return 0;

  for (int i = 1; i <= text.length(); ++i) {
    int charX = fm.horizontalAdvance(text.left(i));
    if (clickOffset < charX) {
      int prevX = fm.horizontalAdvance(text.left(i - 1));
      return (clickOffset - prevX < charX - clickOffset) ? i - 1 : i;
    }
  }
  return text.length();
}

void SoftwareVideoRenderer::commitEditing() {
  if (!isEditing_ || !editor_)
    return;

  QString newText = editor_->text();
  isEditing_ = false;
  cursorTimer_.stop();
  editor_->hide();
  update();
  emit subtitleTextEdited(newText);
}

void SoftwareVideoRenderer::cancelEditing() {
  if (!isEditing_ || !editor_)
    return;

  isEditing_ = false;
  cursorTimer_.stop();
  editor_->hide();
  update();
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
    } else {
      // 最小宽高度设置为 40 像素
      double minW = 40.0 / targetRect.width();
      double minH = 40.0 / targetRect.height();

      switch (dragMode_) {
      case DragResizeTL: {
        double newLeft = qMin(dragStartNormalizedRect_.left() + dx,
                              dragStartNormalizedRect_.right() - minW);
        double newTop = qMin(dragStartNormalizedRect_.top() + dy,
                             dragStartNormalizedRect_.bottom() - minH);
        newRect.setLeft(newLeft);
        newRect.setTop(newTop);
        break;
      }
      case DragResizeTM: {
        double newTop = qMin(dragStartNormalizedRect_.top() + dy,
                             dragStartNormalizedRect_.bottom() - minH);
        newRect.setTop(newTop);
        break;
      }
      case DragResizeTR: {
        double newRight = qMax(dragStartNormalizedRect_.left() + minW,
                               dragStartNormalizedRect_.right() + dx);
        double newTop = qMin(dragStartNormalizedRect_.top() + dy,
                             dragStartNormalizedRect_.bottom() - minH);
        newRect.setRight(newRight);
        newRect.setTop(newTop);
        break;
      }
      case DragResizeML: {
        double newLeft = qMin(dragStartNormalizedRect_.left() + dx,
                              dragStartNormalizedRect_.right() - minW);
        newRect.setLeft(newLeft);
        break;
      }
      case DragResizeMR: {
        double newRight = qMax(dragStartNormalizedRect_.left() + minW,
                               dragStartNormalizedRect_.right() + dx);
        newRect.setRight(newRight);
        break;
      }
      case DragResizeBL: {
        double newLeft = qMin(dragStartNormalizedRect_.left() + dx,
                              dragStartNormalizedRect_.right() - minW);
        double newBottom = qMax(dragStartNormalizedRect_.top() + minH,
                                dragStartNormalizedRect_.bottom() + dy);
        newRect.setLeft(newLeft);
        newRect.setBottom(newBottom);
        break;
      }
      case DragResizeBM: {
        double newBottom = qMax(dragStartNormalizedRect_.top() + minH,
                                dragStartNormalizedRect_.bottom() + dy);
        newRect.setBottom(newBottom);
        break;
      }
      case DragResizeBR: {
        double newRight = qMax(dragStartNormalizedRect_.left() + minW,
                               dragStartNormalizedRect_.right() + dx);
        double newBottom = qMax(dragStartNormalizedRect_.top() + minH,
                                dragStartNormalizedRect_.bottom() + dy);
        newRect.setRight(newRight);
        newRect.setBottom(newBottom);
        break;
      }
      default:
        break;
      }

      // 不再在拖拽时修改字体大小，只修改包围框
      currentDragFontSize_ = dragStartFontSize_;
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
      // 只发射包围框变更，不再发射字体大小变更
      // 因为现在是通过包围框大小动态缩放字体，不超过原设定大小
      emit subtitleRectChanged(subtitleNormalizedRect_);
    }
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}
