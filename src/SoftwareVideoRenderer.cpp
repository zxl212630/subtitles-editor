#include "SoftwareVideoRenderer.h"

#include <QElapsedTimer>
#include <QPainter>

#define LOG_RENDER_info(msg) qInfo() << "[VideoRenderer]" << msg
#define LOG_RENDER_warning(msg) qWarning() << "[VideoRenderer]" << msg
#define LOG_RENDER_critical(msg) qCritical() << "[VideoRenderer]" << msg
#define LOG_RENDER_debug(msg) qDebug() << "[VideoRenderer]" << msg
#define LOG_RENDER(level, msg) LOG_RENDER_##level(msg)

SoftwareVideoRenderer::SoftwareVideoRenderer(QWidget *parent)
    : QWidget(parent) {
  setAttribute(Qt::WA_OpaquePaintEvent);
  setMinimumSize(320, 180);
}

void SoftwareVideoRenderer::renderFrame(const DecodedVideoFrame &frame) {
  {
    QMutexLocker lock(&imageMutex_);
    currentImage_ =
        QImage(reinterpret_cast<const uchar *>(frame.rgbaData.constData()),
               frame.width, frame.height, QImage::Format_RGBA8888)
            .copy();
    hasFrame_ = true;
  }
  QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
  LOG_RENDER(debug,
             "renderFrame() size=" << frame.width << "x" << frame.height);
}

void SoftwareVideoRenderer::clear() {
  {
    QMutexLocker lock(&imageMutex_);
    hasFrame_ = false;
  }
  update();
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
  painter.fillRect(rect(), Qt::black);

  QImage imageToDraw;
  bool hasFrame;
  {
    QMutexLocker lock(&imageMutex_);
    hasFrame = hasFrame_;
    if (hasFrame) {
      imageToDraw = currentImage_;
    }
  }

  if (!hasFrame || imageToDraw.isNull()) {
    LOG_RENDER(debug,
               "paintEvent() cost=" << timer.elapsed() << "ms (no frame)");
    return;
  }

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
  QRect targetRect(x, y, newWidth, newHeight);

  painter.drawImage(targetRect, imageToDraw);

  // Draw subtitle overlay
  QString text;
  QFont font;
  {
    QMutexLocker lock(&subtitleMutex_);
    text = subtitleText_;
    font = subtitleFont_;
  }
  if (!text.isEmpty()) {
    painter.setFont(font);
    QRect textRect = rect().adjusted(40, 0, -40, -20);
    painter.setPen(
        QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
    painter.setPen(Qt::white);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
  }

  LOG_RENDER(debug, "paintEvent() cost=" << timer.elapsed() << "ms");
}
