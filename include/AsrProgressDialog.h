#pragma once

#include <QDialog>
#include <QString>
#include <QTimer>

class QPushButton;
class QLabel;
class QPainter;

class AsrProgressDialog : public QDialog {
  Q_OBJECT
public:
  enum class Stage { Extraction = 1, Upload = 2, Recognition = 3 };

  explicit AsrProgressDialog(QWidget *parent = nullptr);
  ~AsrProgressDialog() override;

  void setStage(Stage stage);
  void setStatus(const QString &mainText, const QString &subText);
  void setError(const QString &errorMessage);

signals:
  void canceled();

protected:
  void paintEvent(QPaintEvent *event) override;
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onAnimationTick();
  void onCancelClicked();

private:
  void drawSourceIcon(QPainter &p, int cx, int cy, int size);
  void drawTargetIcon(QPainter &p, int cx, int cy, int size);
  void drawParticles(QPainter &p, int x1, int x2, int cy);

  Stage currentStage_ = Stage::Extraction;
  bool isError_ = false;
  bool canceled_ = false;

  QLabel *statusLabel_ = nullptr;
  QLabel *subStatusLabel_ = nullptr;
  QPushButton *cancelButton_ = nullptr;

  QTimer *animTimer_ = nullptr;
  int tickCount_ = 0;
};
