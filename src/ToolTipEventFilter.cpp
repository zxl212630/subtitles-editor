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
        // 为了减小 Tooltip 与按钮之间的垂直间距，我们将定位矩形的下边缘向上收缩
        // 12 像素 这样 Qt 在定位提示框时会自动向上平移，实现紧凑的对齐
        QRect alignRect = widget->rect();
        alignRect.setBottom(widget->height() - 12);

        QPoint bottomCenter = QPoint(widget->width() / 2, alignRect.bottom());
        QPoint globalPos = widget->mapToGlobal(bottomCenter);
        QToolTip::showText(globalPos, text, widget, alignRect);
        return true;
      }
    }
  }
  return QObject::eventFilter(watched, event);
}
