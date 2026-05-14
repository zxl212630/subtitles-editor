#include "SoftwareVideoRenderer.h"

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
  setStyleSheet("background-color: #1e1e1e;");
}

void SoftwareVideoRenderer::renderFrame(const DecodedVideoFrame &frame) {
#if PROFILE_TIMING
  QElapsedTimer copyTimer;
  copyTimer.start();
#endif
  {
    QMutexLocker lock(&imageMutex_);
    currentImage_ =
        QImage(reinterpret_cast<const uchar *>(frame.rgbaData.constData()),
               frame.width, frame.height, QImage::Format_RGBA8888)
            .copy();
    hasFrame_ = true;
  }
  videoSize_ = QSize(frame.width, frame.height);
  QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);

#if PROFILE_TIMING
  static int renderLogCounter2 = 0;
  if (++renderLogCounter2 % 30 == 0) {
    qInfo() << "[TIMING:render_copy] size=" << frame.width << "x"
            << frame.height << " copy_us=" << (copyTimer.nsecsElapsed() / 1000);
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
  painter.fillRect(rect(), QColor("#1e1e1e"));

  QImage imageToDraw;
  bool hasFrame;
  {
    QMutexLocker lock(&imageMutex_);
    hasFrame = hasFrame_;
    if (hasFrame) {
      imageToDraw = currentImage_;
    }
  }

  // Compute video target rect (used for both video and subtitle clipping)
  QRect targetRect;
  if (hasFrame && !imageToDraw.isNull()) {
    double widgetRatio =
        static_cast<double>(width()) / static_cast<double>(height());
    double imageRatio = static_cast<double>(imageToDraw.width()) /
                        static_cast<double>(imageToDraw.height());

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
            << " image=" << imageToDraw.width() << "x" << imageToDraw.height()
            << " paint_us=" << elapsed;
  }
#endif
}
