#pragma once

#include <QCursor>
#include <QString>
#include <QWidget>

class CursorManager {
public:
  static QCursor arrowCursor(const QWidget *widget = nullptr);
  static QCursor pointingHandCursor(const QWidget *widget = nullptr);
  static QCursor iBeamCursor(const QWidget *widget = nullptr);
  static QCursor sizeAllCursor(const QWidget *widget = nullptr);
  static QCursor rotateCursor(const QWidget *widget = nullptr);
  static QCursor resizeCursor(double angle, const QWidget *widget = nullptr);

private:
  static QCursor loadCustomCursor(const QString &svgPath, int hotX, int hotY,
                                  double dpr, int cursorSize = 18);
  static double getDpr(const QWidget *widget);
};
