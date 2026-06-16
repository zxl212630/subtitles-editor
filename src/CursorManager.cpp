#include "CursorManager.h"
#include <QGuiApplication>
#include <QMap>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QSvgRenderer>
#include <cmath>

double CursorManager::getDpr(const QWidget *widget) {
  if (widget) {
    return widget->devicePixelRatioF();
  }
  if (auto screen = QGuiApplication::primaryScreen()) {
    return screen->devicePixelRatio();
  }
  return 1.0;
}

QCursor CursorManager::loadCustomCursor(const QString &svgPath, int hotX,
                                        int hotY, double dpr, int cursorSize) {
  int dprKey = static_cast<int>(qMax(1.0, dpr) * 10);
  QString cacheKey =
      QString("%1_%2_%3_%4").arg(svgPath).arg(hotX).arg(hotY).arg(dprKey);

  static QMap<QString, QCursor> cursorCache;
  auto it = cursorCache.find(cacheKey);
  if (it != cursorCache.end()) {
    return it.value();
  }

  int scaledSize = static_cast<int>(cursorSize * qMax(1.0, dpr));
  QPixmap pixmap(scaledSize, scaledSize);
  pixmap.setDevicePixelRatio(qMax(1.0, dpr));
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  QSvgRenderer renderer(svgPath);
  if (renderer.isValid()) {
    renderer.render(&painter, QRectF(0, 0, cursorSize, cursorSize));
  } else {
    painter.fillRect(QRectF(0, 0, cursorSize, cursorSize), Qt::magenta);
  }
  painter.end();

  QCursor cursor(pixmap, hotX, hotY);
  cursorCache.insert(cacheKey, cursor);
  return cursor;
}

QCursor CursorManager::resizeCursor(double angle, const QWidget *widget) {
  double dpr = getDpr(widget);

  double normAngle = std::fmod(angle, 180.0);
  if (normAngle < 0.0) {
    normAngle += 180.0;
  }
  int roundedAngle = static_cast<int>(std::round(normAngle)) % 180;

  int dprKey = static_cast<int>(qMax(1.0, dpr) * 10);
  QString cacheKey = QString("resize_%1_%2").arg(roundedAngle).arg(dprKey);

  static QMap<QString, QCursor> cursorCache;
  auto it = cursorCache.find(cacheKey);
  if (it != cursorCache.end()) {
    return it.value();
  }

  int cursorSize = 18;
  int scaledSize = static_cast<int>(cursorSize * qMax(1.0, dpr));
  QPixmap pixmap(scaledSize, scaledSize);
  pixmap.setDevicePixelRatio(qMax(1.0, dpr));
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Translate to center to rotate
  painter.translate(cursorSize / 2.0, cursorSize / 2.0);
  painter.rotate(roundedAngle);
  // Translate back
  painter.translate(-cursorSize / 2.0, -cursorSize / 2.0);

  QSvgRenderer renderer(QString(":/icons/cursor-resize.svg"));
  if (renderer.isValid()) {
    renderer.render(&painter, QRectF(0, 0, cursorSize, cursorSize));
  } else {
    painter.fillRect(QRectF(0, 0, cursorSize, cursorSize), Qt::magenta);
  }
  painter.end();

  QCursor cursor(pixmap, cursorSize / 2, cursorSize / 2);
  cursorCache.insert(cacheKey, cursor);
  return cursor;
}

QCursor CursorManager::arrowCursor(const QWidget *widget) {
  return Qt::ArrowCursor;
}

QCursor CursorManager::pointingHandCursor(const QWidget *widget) {
  return Qt::ArrowCursor;
}

QCursor CursorManager::iBeamCursor(const QWidget *widget) {
  return Qt::IBeamCursor;
}

QCursor CursorManager::sizeAllCursor(const QWidget *widget) {
  return loadCustomCursor(":/icons/cursor-move.svg", 9, 9, getDpr(widget), 18);
}

QCursor CursorManager::rotateCursor(const QWidget *widget) {
  return loadCustomCursor(":/icons/rotate.svg", 9, 9, getDpr(widget), 18);
}
