#include "SoftwareVideoRenderer.h"
#include "ThemeManager.h"

#include <QElapsedTimer>
#include <QPainter>

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

  // Compute video target rect (used for both video and subtitle clipping)
  QRect targetRect;
  if (hasFrame && w > 0 && h > 0) {
    // 构造零拷贝的 QImage，直接使用 rgbaSnapshot 的数据缓冲区
    QImage imageToDraw(
        reinterpret_cast<const uchar *>(rgbaSnapshot.constData()), w, h, w * 4,
        QImage::Format_RGBA8888);

    double widgetRatio =
        static_cast<double>(width()) / static_cast<double>(height());
    double imageRatio = static_cast<double>(w) / static_cast<double>(h);

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
    targetRect = QRect(x, y, newWidth, newHeight);

    painter.drawImage(targetRect, imageToDraw);
  } else {
    targetRect = rect();
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

  if (!text.isEmpty()) {
    painter.setFont(font);
    QRect textRect = targetRect.adjusted(40, 0, -40, -20);

    // Draw background image if configured
    if (!bgPath.isEmpty()) {
      QImage bgImage;
      {
        QMutexLocker lock(&bgMutex_);
        if (bgCache_.contains(bgPath)) {
          bgImage = bgCache_[bgPath];
        } else {
          bgImage = QImage(bgPath);
          if (!bgImage.isNull()) {
            bgCache_[bgPath] = bgImage;
          }
        }
      }

      if (!bgImage.isNull()) {
        QFontMetrics fm(font);
        QRect textBounding = fm.boundingRect(
            textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
        
        // Expand with padding margins
        QRect bgRect = textBounding.adjusted(-bgMargins.left(), -bgMargins.top(),
                                             bgMargins.right(), bgMargins.bottom());

        if (is9Patch) {
          drawNinePatch(painter, bgImage, bgRect, bgMargins);
        } else {
          // Fixed size: center background image under the text bounding box
          int imgX = textBounding.center().x() - bgImage.width() / 2;
          int imgY = textBounding.center().y() - bgImage.height() / 2;
          painter.drawImage(imgX, imgY, bgImage);
        }
      }
    }

    painter.setPen(
        QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
    painter.setPen(Qt::white);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
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
