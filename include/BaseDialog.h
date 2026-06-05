#pragma once

#include <QDialog>
#include <QEvent>

namespace QWK {
class WidgetWindowAgent;
}
class QFrame;
class QLabel;
class QPushButton;

class BaseDialog : public QDialog {
  Q_OBJECT
public:
  explicit BaseDialog(QWidget *parent = nullptr);
  ~BaseDialog() override;

protected:
  void setupWindowAgent(QFrame *customTitleBar);
  void changeEvent(QEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

  QWK::WidgetWindowAgent *windowAgent = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;

#ifdef Q_OS_WIN
  QPushButton *minBtn = nullptr;
  QPushButton *maxBtn = nullptr;
  QPushButton *closeBtn = nullptr;
#endif
};
