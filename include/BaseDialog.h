#pragma once

#include <QDialog>
#include <QEvent>

namespace QWK {
class WidgetWindowAgent;
}
class QFrame;
class QLabel;
class QPushButton;

class QMoveEvent;

class BaseDialog : public QDialog {
  Q_OBJECT
public:
  explicit BaseDialog(QWidget *parent = nullptr);
  ~BaseDialog() override;

protected:
  virtual void setupTitleBar();
  void setupWindowAgent(QFrame *customTitleBar);
  void changeEvent(QEvent *event) override;
  void moveEvent(QMoveEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

  QWK::WidgetWindowAgent *windowAgent = nullptr;
  void *nsView = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;

#ifdef Q_OS_WIN
  QPushButton *minBtn = nullptr;
  QPushButton *maxBtn = nullptr;
  QPushButton *closeBtn = nullptr;
#endif
};
