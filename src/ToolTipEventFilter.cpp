#include "ToolTipEventFilter.h"
#include <QEvent>
#include <QPoint>
#include <QRect>
#include <QToolTip>
#include <QWidget>

ToolTipEventFilter *ToolTipEventFilter::instance() {
  static ToolTipEventFilter inst;
  return &inst;
}

ToolTipEventFilter::ToolTipEventFilter(QObject *parent) : QObject(parent) {}

bool ToolTipEventFilter::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::ToolTip) {
    auto *widget = qobject_cast<QWidget *>(watched);
    if (widget) {
      QString text = widget->toolTip();
      if (!text.isEmpty()) {
        // 计算按钮正下方偏移 4 像素的位置作为锚点，并映射到全局坐标
        // 在 QToolTip::showText 中传入 widget->rect()
        // 这样 Qt 就会自动按照这个包围框水平居中，并对齐显示在正下方
        QPoint bottomCenter = QPoint(widget->width() / 2, widget->height() + 4);
        QPoint globalPos = widget->mapToGlobal(bottomCenter);
        QToolTip::showText(globalPos, text, widget, widget->rect());
        return true;
      }
    }
  }
  return QObject::eventFilter(watched, event);
}
