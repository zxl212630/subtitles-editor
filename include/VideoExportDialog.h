#pragma once

#include <QDialog>
#include <QString>

class QProgressBar;
class QLabel;
class QPushButton;
class QTimer;
class VideoExporter;

class VideoExportDialog : public QDialog {
  Q_OBJECT

public:
  explicit VideoExportDialog(VideoExporter *exporter,
                             QWidget *parent = nullptr);
  ~VideoExportDialog() override;

  QString errorString() const { return errorString_; }

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
