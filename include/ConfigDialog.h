#pragma once
#include "BaseDialog.h"
#include <QVariantMap>

class QListWidget;
class QStackedWidget;
class QComboBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QGridLayout;
class ThemeSelectorWidget;
class ColorSelectorWidget;
class QAction;
class QGroupBox;
class QKeySequenceEdit;
class QProgressBar;
class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QUrl;

class ConfigDialog : public BaseDialog {
  Q_OBJECT
public:
  explicit ConfigDialog(QWidget *parent = nullptr);
  ~ConfigDialog() override;

signals:
  void configApplied();

protected:
  void changeEvent(QEvent *event) override;

private:
  void onApply();
  void onOk();
  void onCancel();
  void checkDirtyState();
  void setupUi();
  void setupTitleBar();
  void loadConfig();
  void saveConfig();
  void retranslateUi();
  bool isDirty() const;
  void updateStorageFields(const QString &provider);
  void updateStorageLabels(const QString &provider);
  void onStorageProviderChanged(const QString &provider);

  QLabel *titleLeftLabel = nullptr;
  QLabel *titleRightLabel = nullptr;

  QListWidget *sidebarList_;
  QStackedWidget *stackedWidget_;

  // General Page
  QComboBox *langCombo_;
  ThemeSelectorWidget *themeSelector_;
  ColorSelectorWidget *colorSelector_;
  QLabel *langLabel_;
  QLabel *themeLabel_;
  QLabel *colorLabel_;

  // Storage Page
  QComboBox *storageProviderCombo_;
  QLineEdit *ossBucketEdit_;
  QLineEdit *ossRegionEdit_;
  QLineEdit *ossAccessKeyEdit_;
  QLineEdit *ossSecretKeyEdit_;
  QLabel *stProvLabel_;
  QLabel *bucketLabel_;
  QLabel *regionLabel_;
  QLabel *akLabel_;
  QLabel *skLabel_;

  // ASR Page
  QComboBox *asrProviderCombo_;
  QLineEdit *tencentAppIdEdit_;
  QLineEdit *tencentSecretIdEdit_;
  QLineEdit *tencentSecretKeyEdit_;
  QLabel *asrProvLabel_;
  QLabel *appIdLabel_;
  QLabel *sidLabel_;
  QLabel *skeyLabel_;
  QCheckBox *speakerDiarizationCheck_;
  QSpinBox *sentenceMaxLengthSpin_;
  QComboBox *engineModelTypeCombo_;
  QLabel *speakerDiarizationLabel_;
  QLabel *maxLenLabel_;
  QLabel *engineLabel_;

  QWidget *tencentAsrContainer_ = nullptr;
  QWidget *whisperAsrContainer_ = nullptr;
  QLineEdit *whisperModelPathEdit_ = nullptr;
  QPushButton *whisperModelPathBtn_ = nullptr;
  QComboBox *whisperModelCombo_ = nullptr;
  QComboBox *whisperLangCombo_ = nullptr;
  QSpinBox *whisperThreadsSpin_ = nullptr;

  QLabel *whisperModelPathLabel_ = nullptr;
  QLabel *whisperModelLabel_ = nullptr;
  QLabel *whisperLangLabel_ = nullptr;
  QLabel *whisperThreadsLabel_ = nullptr;

  QLabel *whisperModelStatusLabel_ = nullptr;
  QPushButton *btnDownloadWhisperModel_ = nullptr;
  QProgressBar *whisperDownloadProgressBar_ = nullptr;

  void updateWhisperStatus();
  bool checkModelExists(const QString &modelName);
  void onDownloadWhisperModelClicked();
  void startWhisperDownload(const QUrl &url, const QString &savePath,
                            int redirectCount = 0);
  void resetWhisperDownloadState();

  QNetworkAccessManager *whisperNetworkManager_ = nullptr;
  QNetworkReply *whisperReply_ = nullptr;
  QFile *whisperDownloadFile_ = nullptr;
  bool whisperIsDownloading_ = false;
  QString whisperDownloadError_;
  QString whisperLastCheckedModel_;
  int whisperLastReportedPercent_ = -1;

  // Subtitle Settings Page
  QComboBox *subtitleFontFamilyCombo_;
  QSpinBox *subtitleFontSizeSpin_;
  QCheckBox *subtitleBoldCheck_;
  QCheckBox *subtitleItalicCheck_;
  QCheckBox *subtitleUnderlineCheck_;
  QComboBox *subtitleAlignmentCombo_;
  QSpinBox *subtitleRectXSpin_;
  QSpinBox *subtitleRectYSpin_;
  QSpinBox *subtitleRectWSpin_;
  QSpinBox *subtitleRectHSpin_;
  QSpinBox *subtitleRotationSpin_;
  QLineEdit *speakerBgFolderEdit_;
  QPushButton *speakerBgFolderBtn_;
  QSpinBox *speakerMarginLeftSpin_;
  QSpinBox *speakerMarginTopSpin_;
  QSpinBox *speakerMarginRightSpin_;
  QSpinBox *speakerMarginBottomSpin_;

  QGroupBox *fontStyleGroup_ = nullptr;
  QGroupBox *positionGroup_ = nullptr;
  QGroupBox *speakerGroup_ = nullptr;

  QLabel *subtitleFontFamilyLabel_;
  QLabel *subtitleFontSizeLabel_;
  QLabel *subtitleBoldLabel_;
  QLabel *subtitleItalicLabel_;
  QLabel *subtitleUnderlineLabel_;
  QLabel *subtitleAlignmentLabel_;
  QLabel *subtitleStyleGroupLabel_;
  QLabel *subtitleRectXLabel_;
  QLabel *subtitleRectYLabel_;
  QLabel *subtitleRectWLabel_;
  QLabel *subtitleRectHLabel_;
  QLabel *subtitleRotationLabel_;
  QLabel *speakerBgFolderLabel_;
  QLabel *speakerMarginLeftLabel_;
  QLabel *speakerMarginTopLabel_;
  QLabel *speakerMarginRightLabel_;
  QLabel *speakerMarginBottomLabel_;

  // Shortcuts Page
  QMap<QString, QKeySequenceEdit *> shortcutEdits_;
  QMap<QString, QLabel *> shortcutLabels_;

  // Footer
  QLabel *dirtyLabel_;
  QPushButton *btnApply_;
  QPushButton *btnOk_;
  QPushButton *btnCancel_;

  QVariantMap initialConfig_;

  QString currentProvider_;
  QString tempAliBucket_;
  QString tempAliRegion_;
  QString tempAliAk_;
  QString tempAliSk_;

  QString tempCosBucket_;
  QString tempCosRegion_;
  QString tempCosAk_;
  QString tempCosSk_;

  QAction *ossEyeAction_ = nullptr;
  QAction *asrEyeAction_ = nullptr;
};
