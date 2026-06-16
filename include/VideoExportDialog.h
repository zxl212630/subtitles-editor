#pragma once

#include "BaseDialog.h"
#include <QString>

class QProgressBar;
class QLabel;
class QPushButton;
class QTimer;
class QFrame;
class QEvent;
class QCloseEvent;
class VideoExporter;

class VideoExportDialog : public BaseDialog {
  Q_OBJECT

public:
  explicit VideoExportDialog(VideoExporter *exporter,
                             QWidget *parent = nullptr);
  ~VideoExportDialog() override;

  QString errorString() const { return errorString_; }

protected:
  void changeEvent(QEvent *event) override;
  void closeEvent(QCloseEvent *event) override;

private slots:
  void onProgressChanged(int percent);
  void onExportFinished(const QString &outputPath);
  void onExportFailed(const QString &error);
  void onExportCancelled();

  void onTimerUpdate();
  void onCancelClicked();

private:
  void setupUi();
  void retranslateUi();
  QString formatTime(qint64 ms) const;

  VideoExporter *exporter_ = nullptr;
  QTimer *updateTimer_ = nullptr;

  QProgressBar *progressBar_ = nullptr;
  QLabel *progressTextLabel_ = nullptr;
  QLabel *elapsedLabel_ = nullptr;
  QLabel *remainingLabel_ = nullptr;
  QPushButton *cancelBtn_ = nullptr;

  bool isFinished_ = false;
  QString errorString_;
};
