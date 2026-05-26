#include "VideoExportDialog.h"
#include "AppMessageBox.h"
#include "VideoExporter.h"
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

VideoExportDialog::VideoExportDialog(VideoExporter *exporter, QWidget *parent)
    : QDialog(parent), exporter_(exporter) {
  setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint &
                 ~Qt::WindowContextHelpButtonHint);
  setModal(true);
  setFixedWidth(400);

  setupUi();
  retranslateUi();

  // 连接信号
  connect(exporter_, &VideoExporter::progressChanged, this,
          &VideoExportDialog::onProgressChanged);
  connect(exporter_, &VideoExporter::exportFinished, this,
          &VideoExportDialog::onExportFinished);
  connect(exporter_, &VideoExporter::exportFailed, this,
          &VideoExportDialog::onExportFailed);
  connect(exporter_, &VideoExporter::exportCancelled, this,
          &VideoExportDialog::onExportCancelled);

  // 定时刷新时间和估计剩余时间
  updateTimer_ = new QTimer(this);
  connect(updateTimer_, &QTimer::timeout, this,
          &VideoExportDialog::onTimerUpdate);
  updateTimer_->start(500); // 500ms 刷新一次
}

VideoExportDialog::~VideoExportDialog() {}

void VideoExportDialog::setupUi() {
  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 20, 20, 20);
  layout->setSpacing(15);

  QLabel *titleLabel = new QLabel(tr("正在导出视频..."), this);
  titleLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
  layout->addWidget(titleLabel);

  progressBar_ = new QProgressBar(this);
  progressBar_->setRange(0, 100);
  progressBar_->setValue(0);
  progressBar_->setTextVisible(false); // 我们在旁边写百分比
  layout->addWidget(progressBar_);

  QHBoxLayout *infoLayout = new QHBoxLayout();
  progressTextLabel_ = new QLabel("0%", this);
  progressTextLabel_->setStyleSheet("font-weight: bold;");
  infoLayout->addWidget(progressTextLabel_);
  infoLayout->addStretch();
  layout->addLayout(infoLayout);

  QFrame *timeFrame = new QFrame(this);
  timeFrame->setFrameShape(QFrame::NoFrame);
  QFormLayout *timeLayout = new QFormLayout(timeFrame);
  timeLayout->setContentsMargins(0, 0, 0, 0);
  timeLayout->setSpacing(8);

  elapsedLabel_ = new QLabel("00:00", this);
  timeLayout->addRow(tr("已用时间："), elapsedLabel_);

  remainingLabel_ = new QLabel(tr("正在计算..."), this);
  timeLayout->addRow(tr("剩余时间："), remainingLabel_);

  layout->addWidget(timeFrame);

  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->addStretch();
  cancelBtn_ = new QPushButton(tr("取消导出"), this);
  connect(cancelBtn_, &QPushButton::clicked, this,
          &VideoExportDialog::onCancelClicked);
  btnLayout->addWidget(cancelBtn_);
  layout->addLayout(btnLayout);
}

void VideoExportDialog::retranslateUi() { setWindowTitle(tr("导出进度")); }

void VideoExportDialog::onProgressChanged(int percent) {
  progressBar_->setValue(percent);
  progressTextLabel_->setText(QString("%1%").arg(percent));
}

void VideoExportDialog::onTimerUpdate() {
  if (isFinished_)
    return;

  qint64 elapsed = exporter_->elapsedMs();
  elapsedLabel_->setText(formatTime(elapsed));

  int percent = exporter_->progressPercent();
  if (percent > 2) {
    qint64 remaining = exporter_->estimatedRemainingMs();
    remainingLabel_->setText(formatTime(remaining));
  } else {
    remainingLabel_->setText(tr("正在计算..."));
  }
}

void VideoExportDialog::onCancelClicked() {
  int ret = AppMessageBox::question(
      this, tr("取消导出"),
      tr("您确认要中止视频导出吗？未导出完成的文件将被删除。"));
  if (ret == AppMessageBox::Yes) {
    cancelBtn_->setEnabled(false);
    cancelBtn_->setText(tr("正在取消..."));
    exporter_->requestCancel();
  }
}

void VideoExportDialog::onExportFinished(const QString &outputPath) {
  Q_UNUSED(outputPath)
  isFinished_ = true;
  updateTimer_->stop();
  accept();
}

void VideoExportDialog::onExportFailed(const QString &error) {
  isFinished_ = true;
  updateTimer_->stop();
  AppMessageBox::critical(this, tr("导出失败"),
                          tr("视频导出失败：%1").arg(error));
  reject();
}

void VideoExportDialog::onExportCancelled() {
  isFinished_ = true;
  updateTimer_->stop();
  reject();
}

QString VideoExportDialog::formatTime(qint64 ms) const {
  qint64 totalSeconds = ms / 1000;
  int minutes = (totalSeconds / 60) % 60;
  int seconds = totalSeconds % 60;
  int hours = totalSeconds / 3600;

  if (hours > 0) {
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
  } else {
    return QString("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
  }
}
