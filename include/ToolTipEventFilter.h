#pragma once

#include <QObject>

class ToolTipEventFilter : public QObject {
  Q_OBJECT
public:
  static ToolTipEventFilter *instance();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  explicit ToolTipEventFilter(QObject *parent = nullptr);
  ~ToolTipEventFilter() override = default;
};
