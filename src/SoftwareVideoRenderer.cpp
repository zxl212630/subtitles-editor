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
  {
    QMutexLocker lock(&subtitleMutex_);
    text = subtitleText_;
    font = subtitleFont_;
  }
  if (!text.isEmpty()) {
    painter.setFont(font);
    QRect textRect = targetRect.adjusted(40, 0, -40, -20);
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
