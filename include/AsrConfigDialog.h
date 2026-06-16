#pragma once

#include "BaseDialog.h"
#include <QString>

class QComboBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QProgressBar;
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QUrl;

class AsrConfigDialog : public BaseDialog {
  Q_OBJECT
public:
  explicit AsrConfigDialog(QWidget *parent = nullptr);
  ~AsrConfigDialog() override;

  QString asrProvider() const;

  // Tencent ASR
  QString engineModelType() const;
  int sentenceMaxLength() const;
  bool speakerDiarization() const;

  // Whisper Local ASR
  QString whisperModel() const;
  QString whisperLanguage() const;
  int whisperThreads() const;
  int whisperMaxLen() const;

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUi();
  void retranslateUi();
  void loadDefaultConfig();
  bool checkModelExists(const QString &modelName);
  void updateWhisperStatus();
  void onDownloadClicked();
  void startDownload(const QUrl &url, const QString &savePath,
                     int redirectCount = 0);
  void resetDownloadState();

  QComboBox *asrProviderCombo_ = nullptr;
  QLabel *asrProvLabel_ = nullptr;

  // Tencent Container
  QWidget *tencentContainer_ = nullptr;
  QSpinBox *sentenceMaxLengthSpin_ = nullptr;
  QComboBox *engineModelTypeCombo_ = nullptr;
  QCheckBox *speakerDiarizationCheck_ = nullptr;
  QLabel *maxLenLabel_ = nullptr;
  QLabel *engineLabel_ = nullptr;
  QLabel *speakerDiarizationLabel_ = nullptr;

  // Whisper Container
  QWidget *whisperContainer_ = nullptr;
  QComboBox *whisperModelCombo_ = nullptr;
  QComboBox *whisperLangCombo_ = nullptr;
  QSpinBox *whisperThreadsSpin_ = nullptr;
  QPushButton *btnDownload_ = nullptr;
  QLabel *whisperStatusLabel_ = nullptr;
  QLabel *whisperModelLabel_ = nullptr;
  QLabel *whisperLangLabel_ = nullptr;
  QLabel *whisperThreadsLabel_ = nullptr;
  QLabel *whisperMaxLenLabel_ = nullptr;
  QSpinBox *whisperMaxLenSpin_ = nullptr;
  QProgressBar *whisperProgressBar_ = nullptr;

  QPushButton *btnOk_ = nullptr;
  QPushButton *btnCancel_ = nullptr;

  // Download Management
  QNetworkAccessManager *networkManager_ = nullptr;
  QNetworkReply *reply_ = nullptr;
  QFile *downloadFile_ = nullptr;
  bool isDownloading_ = false;
  QString downloadError_;
  QString lastCheckedModel_;
  int lastReportedPercent_ = -1;
};
