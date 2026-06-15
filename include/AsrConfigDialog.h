#pragma once

#include "BaseDialog.h"
#include <QString>

class QComboBox;
class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;

class AsrConfigDialog : public BaseDialog {
  Q_OBJECT
public:
  explicit AsrConfigDialog(QWidget *parent = nullptr);
  ~AsrConfigDialog() override = default;

  QString asrProvider() const;

  // Tencent ASR
  QString engineModelType() const;
  int sentenceMaxLength() const;
  bool speakerDiarization() const;

  // Whisper Local ASR
  QString whisperModel() const;
  QString whisperLanguage() const;
  int whisperThreads() const;

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupUi();
  void setupTitleBar();
  void retranslateUi();
  void loadDefaultConfig();
  bool checkModelExists(const QString &modelName);
  void updateWhisperStatus();
  void onDownloadClicked();

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

  QPushButton *btnOk_ = nullptr;
  QPushButton *btnCancel_ = nullptr;
};
