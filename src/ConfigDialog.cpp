#include "ConfigDialog.h"
#include "ConfigManager.h"
#include "PaletteSelectors.h"
#include "ThemeManager.h"
#include "TranslationManager.h"
#include <QAction>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QToolButton>

namespace {
QIcon createEyeIcon(bool open) {
  QIcon icon;
  if (open) {
    icon.addFile(":/icons/eye-open.svg", QSize(), QIcon::Normal);
    icon.addFile(":/icons/eye-open-hover.svg", QSize(), QIcon::Active);
  } else {
    icon.addFile(":/icons/eye-close.svg", QSize(), QIcon::Normal);
    icon.addFile(":/icons/eye-close-hover.svg", QSize(), QIcon::Active);
  }
  return icon;
}
} // namespace

ConfigDialog::ConfigDialog(QWidget *parent) : BaseDialog(parent) {
  setWindowTitle(tr("设置"));
  setMinimumSize(800, 560);

  // Set object name for QSS
  setObjectName("ConfigDialog");

  setupTitleBar();
  setupUi();
  loadConfig();

  connect(btnCancel_, &QPushButton::clicked, this, &ConfigDialog::onCancel);
  connect(btnApply_, &QPushButton::clicked, this, &ConfigDialog::onApply);
  connect(btnOk_, &QPushButton::clicked, this, &ConfigDialog::onOk);

  // Track changes
  connect(themeSelector_, &ThemeSelectorWidget::themeSelected, this,
          &ConfigDialog::checkDirtyState);
  connect(colorSelector_, &ColorSelectorWidget::colorSelected, this,
          &ConfigDialog::checkDirtyState);
  connect(langCombo_, &QComboBox::currentTextChanged, this,
          &ConfigDialog::checkDirtyState);
  connect(storageProviderCombo_, &QComboBox::currentTextChanged, this,
          &ConfigDialog::onStorageProviderChanged);
  connect(asrProviderCombo_, &QComboBox::currentTextChanged, this,
          &ConfigDialog::checkDirtyState);

  auto lineEdits = {tencentAppIdEdit_, tencentSecretIdEdit_,
                    tencentSecretKeyEdit_};
  for (auto *le : lineEdits) {
    connect(le, &QLineEdit::textChanged, this, &ConfigDialog::checkDirtyState);
  }

  connect(ossBucketEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (currentProvider_ == "aliyun_oss")
              tempAliBucket_ = text;
            else if (currentProvider_ == "tencent_cos")
              tempCosBucket_ = text;
            checkDirtyState();
          });
  connect(ossRegionEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (currentProvider_ == "aliyun_oss")
              tempAliRegion_ = text;
            else if (currentProvider_ == "tencent_cos")
              tempCosRegion_ = text;
            checkDirtyState();
          });
  connect(ossAccessKeyEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (currentProvider_ == "aliyun_oss")
              tempAliAk_ = text;
            else if (currentProvider_ == "tencent_cos")
              tempCosAk_ = text;
            checkDirtyState();
          });
  connect(ossSecretKeyEdit_, &QLineEdit::textChanged, this,
          [this](const QString &text) {
            if (currentProvider_ == "aliyun_oss")
              tempAliSk_ = text;
            else if (currentProvider_ == "tencent_cos")
              tempCosSk_ = text;
            checkDirtyState();
          });

  connect(speakerDiarizationCheck_, &QCheckBox::stateChanged, this,
          &ConfigDialog::checkDirtyState);
  connect(sentenceMaxLengthSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(engineModelTypeCombo_, &QComboBox::currentTextChanged, this,
          &ConfigDialog::checkDirtyState);

  // 字幕设置和说话人设置控件变化连接
  connect(subtitleFontFamilyCombo_, &QComboBox::currentTextChanged, this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleFontSizeSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleBoldCheck_, &QCheckBox::stateChanged, this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleItalicCheck_, &QCheckBox::stateChanged, this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleUnderlineCheck_, &QCheckBox::stateChanged, this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleAlignmentCombo_,
          qOverload<int>(&QComboBox::currentIndexChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleRectXSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleRectYSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleRectWSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleRectHSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(subtitleRotationSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(speakerBgFolderEdit_, &QLineEdit::textChanged, this,
          &ConfigDialog::checkDirtyState);
  connect(speakerMarginLeftSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(speakerMarginTopSpin_, qOverload<int>(&QSpinBox::valueChanged), this,
          &ConfigDialog::checkDirtyState);
  connect(speakerMarginRightSpin_, qOverload<int>(&QSpinBox::valueChanged),
          this, &ConfigDialog::checkDirtyState);
  connect(speakerMarginBottomSpin_, qOverload<int>(&QSpinBox::valueChanged),
          this, &ConfigDialog::checkDirtyState);

  setupWindowAgent(titleBar);
  if (titleLabel) {
    titleLabel->setVisible(false); // ConfigDialog does not use standard centered titleLabel
  }
}

void ConfigDialog::setupTitleBar() {
  titleBar = new QFrame(this);
  titleBar->setFixedHeight(36);
  titleBar->setObjectName("TitleBar");

  auto *layout = new QHBoxLayout(titleBar);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Left part of title bar (above sidebar)
  auto *leftContainer = new QWidget(titleBar);
  leftContainer->setFixedWidth(180);
  leftContainer->setObjectName("TitleLeftContainer");
  auto *leftLayout = new QHBoxLayout(leftContainer);
  leftLayout->setContentsMargins(12, 0, 12, 0);
  titleLeftLabel = new QLabel("", leftContainer);
  titleLeftLabel->setObjectName("ConfigTitleLeftLabel");
  leftLayout->addWidget(titleLeftLabel);
  layout->addWidget(leftContainer);

  // Right part of title bar (above content)
  auto *rightContainer = new QWidget(titleBar);
  rightContainer->setObjectName("TitleRightContainer");
  auto *rightLayout = new QHBoxLayout(rightContainer);
  rightLayout->setContentsMargins(20, 0, 20, 0);
  titleRightLabel = new QLabel("", rightContainer);
  titleRightLabel->setObjectName("ConfigTitleRightLabel");
  rightLayout->addStretch();
  rightLayout->addWidget(titleRightLabel);
  rightLayout->addStretch();
  layout->addWidget(rightContainer);
}

void ConfigDialog::loadConfig() {
  auto &cfg = ConfigManager::instance();
  initialConfig_["language"] = cfg.getString("", "language");
  if (initialConfig_["language"].toString().isEmpty())
    initialConfig_["language"] = "zh_CN";

  initialConfig_["theme"] = cfg.getString("", "theme");
  if (initialConfig_["theme"].toString() == "light" ||
      initialConfig_["theme"].toString().isEmpty()) {
    initialConfig_["theme"] = "dark";
  }

  initialConfig_["primary_color"] = cfg.getString("", "primary_color");
  if (initialConfig_["primary_color"].toString().isEmpty())
    initialConfig_["primary_color"] = "blue";

  langCombo_->setCurrentIndex(langCombo_->findData(initialConfig_["language"]));
  themeSelector_->setCurrentTheme(initialConfig_["theme"].toString());
  colorSelector_->setCurrentColor(initialConfig_["primary_color"].toString());

  initialConfig_["storage_provider"] = cfg.storageProvider();
  if (initialConfig_["storage_provider"].toString().isEmpty()) {
    initialConfig_["storage_provider"] = "aliyun_oss";
  }
  currentProvider_ = initialConfig_["storage_provider"].toString();

  initialConfig_["oss_bucket"] = cfg.getString("aliyun_oss", "bucket");
  initialConfig_["oss_region"] = cfg.getString("aliyun_oss", "region");
  initialConfig_["oss_ak"] = cfg.getString("aliyun_oss", "access_key_id");
  initialConfig_["oss_sk"] = cfg.getString("aliyun_oss", "access_key_secret");

  initialConfig_["cos_bucket"] = cfg.getString("tencent_cos", "bucket");
  initialConfig_["cos_region"] = cfg.getString("tencent_cos", "region");
  initialConfig_["cos_ak"] = cfg.getString("tencent_cos", "secret_id");
  initialConfig_["cos_sk"] = cfg.getString("tencent_cos", "secret_key");

  tempAliBucket_ = initialConfig_["oss_bucket"].toString();
  tempAliRegion_ = initialConfig_["oss_region"].toString();
  tempAliAk_ = initialConfig_["oss_ak"].toString();
  tempAliSk_ = initialConfig_["oss_sk"].toString();

  tempCosBucket_ = initialConfig_["cos_bucket"].toString();
  tempCosRegion_ = initialConfig_["cos_region"].toString();
  tempCosAk_ = initialConfig_["cos_ak"].toString();
  tempCosSk_ = initialConfig_["cos_sk"].toString();

  int providerIndex = storageProviderCombo_->findData(currentProvider_);
  if (providerIndex >= 0) {
    storageProviderCombo_->setCurrentIndex(providerIndex);
  } else {
    storageProviderCombo_->setCurrentIndex(0);
  }

  updateStorageFields(currentProvider_);
  updateStorageLabels(currentProvider_);

  initialConfig_["tc_appid"] = cfg.getString("tencent_asr", "app_id");
  initialConfig_["tc_sid"] = cfg.getString("tencent_asr", "secret_id");
  initialConfig_["tc_skey"] = cfg.getString("tencent_asr", "secret_key");
  initialConfig_["tc_speaker_diarization"] = cfg.speakerDiarization();
  initialConfig_["tc_sentence_max_length"] = cfg.sentenceMaxLength();
  initialConfig_["tc_engine_model_type"] = cfg.engineModelType();

  tencentAppIdEdit_->setText(initialConfig_["tc_appid"].toString());
  tencentSecretIdEdit_->setText(initialConfig_["tc_sid"].toString());
  tencentSecretKeyEdit_->setText(initialConfig_["tc_skey"].toString());

  speakerDiarizationCheck_->setChecked(
      initialConfig_["tc_speaker_diarization"].toBool());
  sentenceMaxLengthSpin_->setValue(
      initialConfig_["tc_sentence_max_length"].toInt());
  engineModelTypeCombo_->setCurrentIndex(engineModelTypeCombo_->findData(
      initialConfig_["tc_engine_model_type"].toString()));

  // Sync title
  if (sidebarList_->currentItem()) {
    titleRightLabel->setText(sidebarList_->currentItem()->text());
  }

  // 重置密钥框为密文显示，小眼睛为闭眼
  ossSecretKeyEdit_->setEchoMode(QLineEdit::Password);
  if (ossEyeAction_) {
    ossEyeAction_->setIcon(createEyeIcon(false));
  }
  tencentSecretKeyEdit_->setEchoMode(QLineEdit::Password);
  if (asrEyeAction_) {
    asrEyeAction_->setIcon(createEyeIcon(false));
  }

  // 字幕设置
  subtitleFontFamilyCombo_->setCurrentText(
      cfg.getString("subtitle", "fontFamily", "Arial"));
  subtitleFontSizeSpin_->setValue(cfg.getInt("subtitle", "fontSize", 24));
  subtitleBoldCheck_->setChecked(cfg.getBool("subtitle", "bold", false));
  subtitleItalicCheck_->setChecked(cfg.getBool("subtitle", "italic", false));
  subtitleUnderlineCheck_->setChecked(
      cfg.getBool("subtitle", "underline", false));
  subtitleAlignmentCombo_->setCurrentIndex(subtitleAlignmentCombo_->findData(
      cfg.getInt("subtitle", "alignment", 0x84)));
  subtitleRectXSpin_->setValue(
      qRound(cfg.getDouble("subtitle", "rectX", 0.1) * 100));
  subtitleRectYSpin_->setValue(
      qRound(cfg.getDouble("subtitle", "rectY", 0.75) * 100));
  subtitleRectWSpin_->setValue(
      qRound(cfg.getDouble("subtitle", "rectW", 0.8) * 100));
  subtitleRectHSpin_->setValue(
      qRound(cfg.getDouble("subtitle", "rectH", 0.2) * 100));
  subtitleRotationSpin_->setValue(
      qRound(cfg.getDouble("subtitle", "rotation", 0.0)));
  speakerBgFolderEdit_->setText(cfg.getString("speaker", "bgFolder"));
  speakerMarginLeftSpin_->setValue(cfg.getInt("speaker", "marginLeft", 15));
  speakerMarginTopSpin_->setValue(cfg.getInt("speaker", "marginTop", 15));
  speakerMarginRightSpin_->setValue(cfg.getInt("speaker", "marginRight", 15));
  speakerMarginBottomSpin_->setValue(cfg.getInt("speaker", "marginBottom", 15));

  // 缓存字幕和说话人初始配置
  initialConfig_["sub_fontFamily"] =
      cfg.getString("subtitle", "fontFamily", "Arial");
  initialConfig_["sub_fontSize"] = cfg.getInt("subtitle", "fontSize", 24);
  initialConfig_["sub_bold"] = cfg.getBool("subtitle", "bold", false);
  initialConfig_["sub_italic"] = cfg.getBool("subtitle", "italic", false);
  initialConfig_["sub_underline"] = cfg.getBool("subtitle", "underline", false);
  initialConfig_["sub_alignment"] = cfg.getInt("subtitle", "alignment", 0x84);
  initialConfig_["sub_rectX"] =
      qRound(cfg.getDouble("subtitle", "rectX", 0.1) * 100);
  initialConfig_["sub_rectY"] =
      qRound(cfg.getDouble("subtitle", "rectY", 0.75) * 100);
  initialConfig_["sub_rectW"] =
      qRound(cfg.getDouble("subtitle", "rectW", 0.8) * 100);
  initialConfig_["sub_rectH"] =
      qRound(cfg.getDouble("subtitle", "rectH", 0.2) * 100);
  initialConfig_["sub_rotation"] =
      qRound(cfg.getDouble("subtitle", "rotation", 0.0));
  initialConfig_["spk_bgFolder"] = cfg.getString("speaker", "bgFolder");
  initialConfig_["spk_marginLeft"] = cfg.getInt("speaker", "marginLeft", 15);
  initialConfig_["spk_marginTop"] = cfg.getInt("speaker", "marginTop", 15);
  initialConfig_["spk_marginRight"] = cfg.getInt("speaker", "marginRight", 15);
  initialConfig_["spk_marginBottom"] =
      cfg.getInt("speaker", "marginBottom", 15);

  checkDirtyState();
}

bool ConfigDialog::isDirty() const {
  return (langCombo_->currentData().toString() !=
          initialConfig_["language"].toString()) ||
         (themeSelector_->currentTheme() !=
          initialConfig_["theme"].toString()) ||
         (colorSelector_->currentColor() !=
          initialConfig_["primary_color"].toString()) ||
         (storageProviderCombo_->currentData().toString() !=
          initialConfig_["storage_provider"].toString()) ||
         (tempAliBucket_ != initialConfig_["oss_bucket"].toString()) ||
         (tempAliRegion_ != initialConfig_["oss_region"].toString()) ||
         (tempAliAk_ != initialConfig_["oss_ak"].toString()) ||
         (tempAliSk_ != initialConfig_["oss_sk"].toString()) ||
         (tempCosBucket_ != initialConfig_["cos_bucket"].toString()) ||
         (tempCosRegion_ != initialConfig_["cos_region"].toString()) ||
         (tempCosAk_ != initialConfig_["cos_ak"].toString()) ||
         (tempCosSk_ != initialConfig_["cos_sk"].toString()) ||
         (tencentAppIdEdit_->text() != initialConfig_["tc_appid"].toString()) ||
         (tencentSecretIdEdit_->text() !=
          initialConfig_["tc_sid"].toString()) ||
         (tencentSecretKeyEdit_->text() !=
          initialConfig_["tc_skey"].toString()) ||
         (speakerDiarizationCheck_->isChecked() !=
          initialConfig_["tc_speaker_diarization"].toBool()) ||
         (sentenceMaxLengthSpin_->value() !=
          initialConfig_["tc_sentence_max_length"].toInt()) ||
         (engineModelTypeCombo_->currentData().toString() !=
          initialConfig_["tc_engine_model_type"].toString()) ||
         (subtitleFontFamilyCombo_->currentText() !=
          initialConfig_["sub_fontFamily"].toString()) ||
         (subtitleFontSizeSpin_->value() !=
          initialConfig_["sub_fontSize"].toInt()) ||
         (subtitleBoldCheck_->isChecked() !=
          initialConfig_["sub_bold"].toBool()) ||
         (subtitleItalicCheck_->isChecked() !=
          initialConfig_["sub_italic"].toBool()) ||
         (subtitleUnderlineCheck_->isChecked() !=
          initialConfig_["sub_underline"].toBool()) ||
         (subtitleAlignmentCombo_->currentData().toInt() !=
          initialConfig_["sub_alignment"].toInt()) ||
         (qAbs(subtitleRectXSpin_->value() -
               initialConfig_["sub_rectX"].toDouble()) > 1e-5) ||
         (qAbs(subtitleRectYSpin_->value() -
               initialConfig_["sub_rectY"].toDouble()) > 1e-5) ||
         (qAbs(subtitleRectWSpin_->value() -
               initialConfig_["sub_rectW"].toDouble()) > 1e-5) ||
         (qAbs(subtitleRectHSpin_->value() -
               initialConfig_["sub_rectH"].toDouble()) > 1e-5) ||
         (qAbs(subtitleRotationSpin_->value() -
               initialConfig_["sub_rotation"].toDouble()) > 1e-5) ||
         (speakerBgFolderEdit_->text() !=
          initialConfig_["spk_bgFolder"].toString()) ||
         (speakerMarginLeftSpin_->value() !=
          initialConfig_["spk_marginLeft"].toInt()) ||
         (speakerMarginTopSpin_->value() !=
          initialConfig_["spk_marginTop"].toInt()) ||
         (speakerMarginRightSpin_->value() !=
          initialConfig_["spk_marginRight"].toInt()) ||
         (speakerMarginBottomSpin_->value() !=
          initialConfig_["spk_marginBottom"].toInt());
}

void ConfigDialog::checkDirtyState() {
  bool dirty = isDirty();
  dirtyLabel_->setVisible(dirty);
  btnApply_->setEnabled(dirty);
}

void ConfigDialog::saveConfig() {
  auto &cfg = ConfigManager::instance();
  cfg.setValue("", "language", langCombo_->currentData().toString());
  cfg.setValue("", "theme", themeSelector_->currentTheme());
  cfg.setValue("", "primary_color", colorSelector_->currentColor());

  cfg.setValue("storage", "provider",
               storageProviderCombo_->currentData().toString());

  cfg.setValue("aliyun_oss", "bucket", tempAliBucket_);
  cfg.setValue("aliyun_oss", "region", tempAliRegion_);
  cfg.setValue("aliyun_oss", "access_key_id", tempAliAk_);
  cfg.setValue("aliyun_oss", "access_key_secret", tempAliSk_);

  cfg.setValue("tencent_cos", "bucket", tempCosBucket_);
  cfg.setValue("tencent_cos", "region", tempCosRegion_);
  cfg.setValue("tencent_cos", "secret_id", tempCosAk_);
  cfg.setValue("tencent_cos", "secret_key", tempCosSk_);

  cfg.setValue("tencent_asr", "app_id", tencentAppIdEdit_->text());
  cfg.setValue("tencent_asr", "secret_id", tencentSecretIdEdit_->text());
  cfg.setValue("tencent_asr", "secret_key", tencentSecretKeyEdit_->text());
  cfg.setSpeakerDiarization(speakerDiarizationCheck_->isChecked());
  cfg.setSentenceMaxLength(sentenceMaxLengthSpin_->value());
  cfg.setEngineModelType(engineModelTypeCombo_->currentData().toString());

  // 字幕设置
  cfg.setValue("subtitle", "fontFamily",
               subtitleFontFamilyCombo_->currentText());
  cfg.setValue("subtitle", "fontSize", subtitleFontSizeSpin_->value());
  cfg.setValue("subtitle", "bold", subtitleBoldCheck_->isChecked());
  cfg.setValue("subtitle", "italic", subtitleItalicCheck_->isChecked());
  cfg.setValue("subtitle", "underline", subtitleUnderlineCheck_->isChecked());
  cfg.setValue("subtitle", "alignment",
               subtitleAlignmentCombo_->currentData().toInt());
  cfg.setValue("subtitle", "rectX", subtitleRectXSpin_->value() / 100.0);
  cfg.setValue("subtitle", "rectY", subtitleRectYSpin_->value() / 100.0);
  cfg.setValue("subtitle", "rectW", subtitleRectWSpin_->value() / 100.0);
  cfg.setValue("subtitle", "rectH", subtitleRectHSpin_->value() / 100.0);
  cfg.setValue("subtitle", "rotation",
               static_cast<double>(subtitleRotationSpin_->value()));
  cfg.setValue("speaker", "bgFolder", speakerBgFolderEdit_->text());
  cfg.setValue("speaker", "marginLeft", speakerMarginLeftSpin_->value());
  cfg.setValue("speaker", "marginTop", speakerMarginTopSpin_->value());
  cfg.setValue("speaker", "marginRight", speakerMarginRightSpin_->value());
  cfg.setValue("speaker", "marginBottom", speakerMarginBottomSpin_->value());

  cfg.sync();

  // Update initialConfig_ directly to reflect saved state
  initialConfig_["language"] = langCombo_->currentData().toString();
  initialConfig_["theme"] = themeSelector_->currentTheme();
  initialConfig_["primary_color"] = colorSelector_->currentColor();
  initialConfig_["storage_provider"] =
      storageProviderCombo_->currentData().toString();
  initialConfig_["oss_bucket"] = tempAliBucket_;
  initialConfig_["oss_region"] = tempAliRegion_;
  initialConfig_["oss_ak"] = tempAliAk_;
  initialConfig_["oss_sk"] = tempAliSk_;
  initialConfig_["cos_bucket"] = tempCosBucket_;
  initialConfig_["cos_region"] = tempCosRegion_;
  initialConfig_["cos_ak"] = tempCosAk_;
  initialConfig_["cos_sk"] = tempCosSk_;
  initialConfig_["tc_appid"] = tencentAppIdEdit_->text();
  initialConfig_["tc_sid"] = tencentSecretIdEdit_->text();
  initialConfig_["tc_skey"] = tencentSecretKeyEdit_->text();
  initialConfig_["tc_speaker_diarization"] =
      speakerDiarizationCheck_->isChecked();
  initialConfig_["tc_sentence_max_length"] = sentenceMaxLengthSpin_->value();
  initialConfig_["tc_engine_model_type"] =
      engineModelTypeCombo_->currentData().toString();

  // 更新暂存的字幕和说话人参数
  initialConfig_["sub_fontFamily"] = subtitleFontFamilyCombo_->currentText();
  initialConfig_["sub_fontSize"] = subtitleFontSizeSpin_->value();
  initialConfig_["sub_bold"] = subtitleBoldCheck_->isChecked();
  initialConfig_["sub_italic"] = subtitleItalicCheck_->isChecked();
  initialConfig_["sub_underline"] = subtitleUnderlineCheck_->isChecked();
  initialConfig_["sub_alignment"] =
      subtitleAlignmentCombo_->currentData().toInt();
  initialConfig_["sub_rectX"] = subtitleRectXSpin_->value();
  initialConfig_["sub_rectY"] = subtitleRectYSpin_->value();
  initialConfig_["sub_rectW"] = subtitleRectWSpin_->value();
  initialConfig_["sub_rectH"] = subtitleRectHSpin_->value();
  initialConfig_["sub_rotation"] = subtitleRotationSpin_->value();
  initialConfig_["spk_bgFolder"] = speakerBgFolderEdit_->text();
  initialConfig_["spk_marginLeft"] = speakerMarginLeftSpin_->value();
  initialConfig_["spk_marginTop"] = speakerMarginTopSpin_->value();
  initialConfig_["spk_marginRight"] = speakerMarginRightSpin_->value();
  initialConfig_["spk_marginBottom"] = speakerMarginBottomSpin_->value();

  checkDirtyState();
}

void ConfigDialog::onApply() {
  saveConfig();
  TranslationManager::instance().loadLanguage(
      langCombo_->currentData().toString());
  retranslateUi();
  ThemeManager::instance().applyTheme();
  emit configApplied();
}

void ConfigDialog::onOk() {
  saveConfig();
  TranslationManager::instance().loadLanguage(
      langCombo_->currentData().toString());
  ThemeManager::instance().applyTheme();
  emit configApplied();
  accept();
}

void ConfigDialog::onCancel() {
  auto &cfg = ConfigManager::instance();
  // Restore theme if changed
  if (themeSelector_->currentTheme() != initialConfig_["theme"].toString() ||
      colorSelector_->currentColor() !=
          initialConfig_["primary_color"].toString()) {
    cfg.setValue("", "theme", initialConfig_["theme"]);
    cfg.setValue("", "primary_color", initialConfig_["primary_color"]);
    ThemeManager::instance().applyTheme();
  }
  // Restore language if changed
  if (langCombo_->currentData().toString() !=
      initialConfig_["language"].toString()) {
    cfg.setValue("", "language", initialConfig_["language"]);
    TranslationManager::instance().loadLanguage(
        initialConfig_["language"].toString());
  }
  reject();
}

void ConfigDialog::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QDialog::changeEvent(event);
}

void ConfigDialog::retranslateUi() {
  setWindowTitle(tr("设置"));

  // Sidebar
  if (auto *item = sidebarList_->item(0))
    item->setText(tr("常规配置"));
  if (auto *item = sidebarList_->item(1))
    item->setText(tr("对象存储"));
  if (auto *item = sidebarList_->item(2))
    item->setText(tr("语音识别"));

  // General page
  langLabel_->setText(tr("界面语言 (Language)"));
  themeLabel_->setText(tr("主题模式 (Theme)"));
  colorLabel_->setText(tr("主色调 (Primary Color)"));
  langCombo_->setItemText(0, tr("简体中文"));
  langCombo_->setItemText(1, tr("English"));

  // Storage page
  stProvLabel_->setText(tr("存储提供商"));
  bucketLabel_->setText(tr("存储桶 (Bucket)"));
  regionLabel_->setText(tr("地域 (Region)"));
  storageProviderCombo_->setItemText(0, tr("阿里云 OSS"));
  storageProviderCombo_->setItemText(1, tr("腾讯云 COS"));
  updateStorageLabels(storageProviderCombo_->currentData().toString());

  // ASR page
  asrProvLabel_->setText(tr("识别提供商"));
  appIdLabel_->setText(tr("应用 ID (App ID)"));
  sidLabel_->setText(tr("密钥 ID (Secret ID)"));
  skeyLabel_->setText(tr("密钥 (Secret Key)"));
  asrProviderCombo_->setItemText(0, tr("腾讯云 ASR"));

  speakerDiarizationLabel_->setText(tr("说话人识别"));
  speakerDiarizationCheck_->setText(tr("开启说话人识别"));
  maxLenLabel_->setText(tr("单行字幕最大字数"));
  engineLabel_->setText(tr("引擎模型类型"));

  engineModelTypeCombo_->setItemText(
      0, QString("16k_zh_en(%1)").arg(tr("中英粤+9种方言大模型")));
  engineModelTypeCombo_->setItemText(
      1, QString("16k_zh_large(%1)").arg(tr("普方英大模型")));
  engineModelTypeCombo_->setItemText(
      2, QString("16k_multi_lang(%1)").arg(tr("多语种大模型")));
  engineModelTypeCombo_->setItemText(
      3, QString("16k_zh(%1)").arg(tr("中文普通话通用")));
  engineModelTypeCombo_->setItemText(4, QString("16k_en(%1)").arg(tr("英语")));
  engineModelTypeCombo_->setItemText(
      5, QString("16k_en_large(%1)").arg(tr("英语大模型")));
  engineModelTypeCombo_->setItemText(6, QString("16k_yue(%1)").arg(tr("粤语")));
  engineModelTypeCombo_->setItemText(
      7, QString("16k_zh-PY(%1)").arg(tr("中英粤混合")));
  engineModelTypeCombo_->setItemText(
      8, QString("16k_zh-TW(%1)").arg(tr("中文繁体")));
  engineModelTypeCombo_->setItemText(9, QString("16k_ja(%1)").arg(tr("日语")));
  engineModelTypeCombo_->setItemText(10, QString("16k_ko(%1)").arg(tr("韩语")));
  engineModelTypeCombo_->setItemText(11,
                                     QString("16k_vi(%1)").arg(tr("越南语")));
  engineModelTypeCombo_->setItemText(12,
                                     QString("16k_ms(%1)").arg(tr("马来语")));
  engineModelTypeCombo_->setItemText(
      13, QString("16k_id(%1)").arg(tr("印度尼西亚语")));
  engineModelTypeCombo_->setItemText(
      14, QString("16k_fil(%1)").arg(tr("菲律宾语")));
  engineModelTypeCombo_->setItemText(15, QString("16k_th(%1)").arg(tr("泰语")));
  engineModelTypeCombo_->setItemText(16,
                                     QString("16k_pt(%1)").arg(tr("葡萄牙语")));
  engineModelTypeCombo_->setItemText(17,
                                     QString("16k_tr(%1)").arg(tr("土耳其语")));
  engineModelTypeCombo_->setItemText(18,
                                     QString("16k_ar(%1)").arg(tr("阿拉伯语")));
  engineModelTypeCombo_->setItemText(19,
                                     QString("16k_es(%1)").arg(tr("西班牙语")));
  engineModelTypeCombo_->setItemText(20,
                                     QString("16k_hi(%1)").arg(tr("印地语")));
  engineModelTypeCombo_->setItemText(21, QString("16k_fr(%1)").arg(tr("法语")));
  engineModelTypeCombo_->setItemText(22, QString("16k_de(%1)").arg(tr("德语")));
  engineModelTypeCombo_->setItemText(
      23, QString("16k_zh_medical(%1)").arg(tr("中文医疗")));

  // Subtitle settings page
  if (auto *item = sidebarList_->item(3))
    item->setText(tr("字幕设置"));

  if (fontStyleGroup_)
    fontStyleGroup_->setTitle(tr("默认字体样式"));
  if (positionGroup_)
    positionGroup_->setTitle(tr("默认排版位置"));
  if (speakerGroup_)
    speakerGroup_->setTitle(tr("说话人设置"));

  subtitleFontFamilyLabel_->setText(tr("字体族"));
  subtitleFontSizeLabel_->setText(tr("字号"));
  subtitleBoldLabel_->setText(tr("粗体"));
  subtitleItalicLabel_->setText(tr("斜体"));
  subtitleUnderlineLabel_->setText(tr("下划线"));
  subtitleAlignmentLabel_->setText(tr("对齐方式"));
  if (subtitleStyleGroupLabel_)
    subtitleStyleGroupLabel_->setText(tr("样式属性"));
  subtitleAlignmentCombo_->setItemText(0, tr("左对齐"));
  subtitleAlignmentCombo_->setItemText(1, tr("居中"));
  subtitleAlignmentCombo_->setItemText(2, tr("右对齐"));
  subtitleRectXLabel_->setText(tr("X (%):"));
  subtitleRectYLabel_->setText(tr("Y (%):"));
  subtitleRectWLabel_->setText(tr("宽度 (%):"));
  subtitleRectHLabel_->setText(tr("高度 (%):"));
  subtitleRotationLabel_->setText(tr("旋转 (°):"));
  speakerBgFolderLabel_->setText(tr("背景图文件夹"));
  speakerBgFolderBtn_->setText(tr("浏览..."));
  speakerMarginLeftLabel_->setText(tr("左:"));
  speakerMarginTopLabel_->setText(tr("上:"));
  speakerMarginRightLabel_->setText(tr("右:"));
  speakerMarginBottomLabel_->setText(tr("下:"));

  // Footer
  dirtyLabel_->setText(tr("有未保存的更改"));
  btnCancel_->setText(tr("取消"));
  btnApply_->setText(tr("应用"));
  btnOk_->setText(tr("确定"));

  // Sync title bar right label with current sidebar selection
  if (sidebarList_->currentItem()) {
    titleRightLabel->setText(sidebarList_->currentItem()->text());
  }
}

void ConfigDialog::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  mainLayout->addWidget(titleBar);

  auto *contentWidget = new QWidget(this);
  contentWidget->setObjectName("ConfigContentWidget");
  auto *contentLayout = new QHBoxLayout(contentWidget);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(0);

  sidebarList_ = new QListWidget(contentWidget);
  sidebarList_->setObjectName("ConfigSidebar");
  sidebarList_->setFixedWidth(180);
  sidebarList_->addItem(tr("常规配置"));
  sidebarList_->addItem(tr("对象存储"));
  sidebarList_->addItem(tr("语音识别"));
  sidebarList_->addItem(tr("字幕设置"));
  contentLayout->addWidget(sidebarList_);

  stackedWidget_ = new QStackedWidget(contentWidget);
  stackedWidget_->setObjectName("ConfigStackedWidget");

  // ------------------------------------------------------------------------
  // General Page
  // ------------------------------------------------------------------------
  auto *generalPage = new QWidget();
  auto *genLayout = new QVBoxLayout(generalPage);
  genLayout->setContentsMargins(30, 25, 30, 30);
  genLayout->setSpacing(15);

  auto *langLabel = new QLabel(tr("界面语言 (Language)"), generalPage);
  langLabel_ = langLabel;
  langLabel->setObjectName("ConfigFieldLabel");
  genLayout->addWidget(langLabel);
  langCombo_ = new QComboBox(generalPage);
  langCombo_->setFixedHeight(32);
  langCombo_->addItem(tr("简体中文"), "zh_CN");
  langCombo_->addItem(tr("English"), "en_US");
  genLayout->addWidget(langCombo_);

  auto *themeLabel = new QLabel(tr("主题模式 (Theme)"), generalPage);
  themeLabel_ = themeLabel;
  themeLabel->setObjectName("ConfigFieldLabel");
  genLayout->addWidget(themeLabel);
  themeSelector_ = new ThemeSelectorWidget(generalPage);
  themeSelector_->addTheme("dark", "#151515", "#1e1e1e", "#3b82f6");
  themeSelector_->addTheme("oled", "#000000", "#09090b", "#10b981");
  themeSelector_->addTheme("midnight", "#020617", "#0f172a", "#6366f1");
  genLayout->addWidget(themeSelector_);

  auto *colorLabel = new QLabel(tr("主色调 (Primary Color)"), generalPage);
  colorLabel_ = colorLabel;
  colorLabel->setObjectName("ConfigFieldLabel");
  genLayout->addWidget(colorLabel);
  colorSelector_ = new ColorSelectorWidget(generalPage);
  colorSelector_->addColor("purple", "#a855f7");
  colorSelector_->addColor("indigo", "#6366f1");
  colorSelector_->addColor("blue", "#3b82f6");
  colorSelector_->addColor("cyan", "#06b6d4");
  colorSelector_->addColor("teal", "#14b8a6");
  colorSelector_->addColor("green", "#10b981");
  colorSelector_->addColor("orange", "#f97316");
  colorSelector_->addColor("pink", "#ec4899");
  colorSelector_->addColor("red", "#ef4444");
  colorSelector_->addColor("sepia", "#d4ba8a");
  genLayout->addWidget(colorSelector_);

  genLayout->addStretch();
  stackedWidget_->addWidget(generalPage);

  // ------------------------------------------------------------------------
  // Storage Page
  // ------------------------------------------------------------------------
  auto *storagePage = new QWidget();
  auto *stLayout = new QVBoxLayout(storagePage);
  stLayout->setContentsMargins(30, 25, 30, 30);
  stLayout->setSpacing(15);

  auto *stProvLabel = new QLabel(tr("存储提供商"), storagePage);
  stProvLabel_ = stProvLabel;
  stProvLabel->setObjectName("ConfigFieldLabel");
  stLayout->addWidget(stProvLabel);
  storageProviderCombo_ = new QComboBox(storagePage);
  storageProviderCombo_->setFixedHeight(32);
  storageProviderCombo_->addItem(tr("阿里云 OSS"), "aliyun_oss");
  storageProviderCombo_->addItem(tr("腾讯云 COS"), "tencent_cos");
  stLayout->addWidget(storageProviderCombo_);

  auto *bucketLabel = new QLabel(tr("存储桶 (Bucket)"), storagePage);
  bucketLabel_ = bucketLabel;
  bucketLabel->setObjectName("ConfigFieldLabel");
  stLayout->addWidget(bucketLabel);
  ossBucketEdit_ = new QLineEdit(storagePage);
  ossBucketEdit_->setFixedHeight(32);
  stLayout->addWidget(ossBucketEdit_);

  auto *regionLabel = new QLabel(tr("地域 (Region)"), storagePage);
  regionLabel_ = regionLabel;
  regionLabel->setObjectName("ConfigFieldLabel");
  stLayout->addWidget(regionLabel);
  ossRegionEdit_ = new QLineEdit(storagePage);
  ossRegionEdit_->setFixedHeight(32);
  stLayout->addWidget(ossRegionEdit_);

  auto *akLabel = new QLabel(tr("访问密钥 ID (Access Key ID)"), storagePage);
  akLabel_ = akLabel;
  akLabel->setObjectName("ConfigFieldLabel");
  stLayout->addWidget(akLabel);
  ossAccessKeyEdit_ = new QLineEdit(storagePage);
  ossAccessKeyEdit_->setFixedHeight(32);
  stLayout->addWidget(ossAccessKeyEdit_);

  auto *skLabel = new QLabel(tr("访问密钥 (Access Key Secret)"), storagePage);
  skLabel_ = skLabel;
  skLabel->setObjectName("ConfigFieldLabel");
  stLayout->addWidget(skLabel);
  ossSecretKeyEdit_ = new QLineEdit(storagePage);
  ossSecretKeyEdit_->setFixedHeight(32);
  ossSecretKeyEdit_->setEchoMode(QLineEdit::Password);
  stLayout->addWidget(ossSecretKeyEdit_);

  ossEyeAction_ = ossSecretKeyEdit_->addAction(createEyeIcon(false),
                                               QLineEdit::TrailingPosition);
  connect(ossEyeAction_, &QAction::triggered, this, [this]() {
    if (ossSecretKeyEdit_->echoMode() == QLineEdit::Password) {
      ossSecretKeyEdit_->setEchoMode(QLineEdit::Normal);
      ossEyeAction_->setIcon(createEyeIcon(true));
    } else {
      ossSecretKeyEdit_->setEchoMode(QLineEdit::Password);
      ossEyeAction_->setIcon(createEyeIcon(false));
    }
  });
  for (auto *btn : ossSecretKeyEdit_->findChildren<QToolButton *>()) {
    btn->setCursor(Qt::PointingHandCursor);
  }

  stLayout->addStretch();
  stackedWidget_->addWidget(storagePage);

  // ------------------------------------------------------------------------
  // ASR Page
  // ------------------------------------------------------------------------
  auto *asrScrollArea = new QScrollArea();
  asrScrollArea->setWidgetResizable(true);
  asrScrollArea->setFrameShape(QFrame::NoFrame);
  auto *asrPage = new QWidget();
  auto *asrLayout = new QVBoxLayout(asrPage);
  asrLayout->setContentsMargins(30, 25, 30, 30);
  asrLayout->setSpacing(15);

  auto *asrProvLabel = new QLabel(tr("识别提供商"), asrPage);
  asrProvLabel_ = asrProvLabel;
  asrProvLabel->setObjectName("ConfigFieldLabel");
  asrLayout->addWidget(asrProvLabel);
  asrProviderCombo_ = new QComboBox(asrPage);
  asrProviderCombo_->setFixedHeight(32);
  asrProviderCombo_->addItem(tr("腾讯云 ASR"), "tencent_asr");
  asrLayout->addWidget(asrProviderCombo_);

  auto *appIdLabel = new QLabel(tr("应用 ID (App ID)"), asrPage);
  appIdLabel_ = appIdLabel;
  appIdLabel->setObjectName("ConfigFieldLabel");
  asrLayout->addWidget(appIdLabel);
  tencentAppIdEdit_ = new QLineEdit(asrPage);
  tencentAppIdEdit_->setFixedHeight(32);
  asrLayout->addWidget(tencentAppIdEdit_);

  auto *sidLabel = new QLabel(tr("密钥 ID (Secret ID)"), asrPage);
  sidLabel_ = sidLabel;
  sidLabel->setObjectName("ConfigFieldLabel");
  asrLayout->addWidget(sidLabel);
  tencentSecretIdEdit_ = new QLineEdit(asrPage);
  tencentSecretIdEdit_->setFixedHeight(32);
  asrLayout->addWidget(tencentSecretIdEdit_);

  auto *skeyLabel = new QLabel(tr("密钥 (Secret Key)"), asrPage);
  skeyLabel_ = skeyLabel;
  skeyLabel->setObjectName("ConfigFieldLabel");
  asrLayout->addWidget(skeyLabel);
  tencentSecretKeyEdit_ = new QLineEdit(asrPage);
  tencentSecretKeyEdit_->setFixedHeight(32);
  tencentSecretKeyEdit_->setEchoMode(QLineEdit::Password);
  asrLayout->addWidget(tencentSecretKeyEdit_);

  asrEyeAction_ = tencentSecretKeyEdit_->addAction(createEyeIcon(false),
                                                   QLineEdit::TrailingPosition);
  connect(asrEyeAction_, &QAction::triggered, this, [this]() {
    if (tencentSecretKeyEdit_->echoMode() == QLineEdit::Password) {
      tencentSecretKeyEdit_->setEchoMode(QLineEdit::Normal);
      asrEyeAction_->setIcon(createEyeIcon(true));
    } else {
      tencentSecretKeyEdit_->setEchoMode(QLineEdit::Password);
      asrEyeAction_->setIcon(createEyeIcon(false));
    }
  });
  for (auto *btn : tencentSecretKeyEdit_->findChildren<QToolButton *>()) {
    btn->setCursor(Qt::PointingHandCursor);
  }

  speakerDiarizationLabel_ = new QLabel(tr("说话人识别"), asrPage);
  speakerDiarizationLabel_->setObjectName("ConfigFieldLabel");
  asrLayout->addWidget(speakerDiarizationLabel_);
  speakerDiarizationCheck_ = new QCheckBox(tr("开启说话人识别"), asrPage);
  speakerDiarizationCheck_->setFixedHeight(32);
  speakerDiarizationCheck_->setObjectName("ConfigCheckBox");
  asrLayout->addWidget(speakerDiarizationCheck_);

  maxLenLabel_ = new QLabel(tr("单行字幕最大字数"), asrPage);
  maxLenLabel_->setObjectName("ConfigFieldLabel");
  asrLayout->addWidget(maxLenLabel_);
  sentenceMaxLengthSpin_ = new QSpinBox(asrPage);
  sentenceMaxLengthSpin_->setFixedHeight(32);
  sentenceMaxLengthSpin_->setRange(6, 40);
  sentenceMaxLengthSpin_->setValue(16);
  sentenceMaxLengthSpin_->setObjectName("ConfigSpinBox");
  asrLayout->addWidget(sentenceMaxLengthSpin_);

  engineLabel_ = new QLabel(tr("引擎模型类型"), asrPage);
  engineLabel_->setObjectName("ConfigFieldLabel");
  asrLayout->addWidget(engineLabel_);
  engineModelTypeCombo_ = new QComboBox(asrPage);
  engineModelTypeCombo_->setFixedHeight(32);
  engineModelTypeCombo_->addItem("16k_zh_en(中英粤+9种方言大模型)",
                                 "16k_zh_en");
  engineModelTypeCombo_->addItem("16k_zh_large(普方英大模型)", "16k_zh_large");
  engineModelTypeCombo_->addItem("16k_multi_lang(多语种大模型)",
                                 "16k_multi_lang");
  engineModelTypeCombo_->addItem("16k_zh(中文普通话通用)", "16k_zh");
  engineModelTypeCombo_->addItem("16k_en(英语)", "16k_en");
  engineModelTypeCombo_->addItem("16k_en_large(英语大模型)", "16k_en_large");
  engineModelTypeCombo_->addItem("16k_yue(粤语)", "16k_yue");
  engineModelTypeCombo_->addItem("16k_zh-PY(中英粤混合)", "16k_zh-PY");
  engineModelTypeCombo_->addItem("16k_zh-TW(中文繁体)", "16k_zh-TW");
  engineModelTypeCombo_->addItem("16k_ja(日语)", "16k_ja");
  engineModelTypeCombo_->addItem("16k_ko(韩语)", "16k_ko");
  engineModelTypeCombo_->addItem("16k_vi(越南语)", "16k_vi");
  engineModelTypeCombo_->addItem("16k_ms(马来语)", "16k_ms");
  engineModelTypeCombo_->addItem("16k_id(印度尼西亚语)", "16k_id");
  engineModelTypeCombo_->addItem("16k_fil(菲律宾语)", "16k_fil");
  engineModelTypeCombo_->addItem("16k_th(泰语)", "16k_th");
  engineModelTypeCombo_->addItem("16k_pt(葡萄牙语)", "16k_pt");
  engineModelTypeCombo_->addItem("16k_tr(土耳其语)", "16k_tr");
  engineModelTypeCombo_->addItem("16k_ar(阿拉伯语)", "16k_ar");
  engineModelTypeCombo_->addItem("16k_es(西班牙语)", "16k_es");
  engineModelTypeCombo_->addItem("16k_hi(印地语)", "16k_hi");
  engineModelTypeCombo_->addItem("16k_fr(法语)", "16k_fr");
  engineModelTypeCombo_->addItem("16k_de(德语)", "16k_de");
  engineModelTypeCombo_->addItem("16k_zh_medical(中文医疗)", "16k_zh_medical");
  engineModelTypeCombo_->setObjectName("ConfigComboBox");
  asrLayout->addWidget(engineModelTypeCombo_);

  asrLayout->addStretch();
  asrScrollArea->setWidget(asrPage);
  stackedWidget_->addWidget(asrScrollArea);

  // ------------------------------------------------------------------------
  // Subtitle Settings Page
  // ------------------------------------------------------------------------
  auto *subtitleScrollArea = new QScrollArea();
  subtitleScrollArea->setWidgetResizable(true);
  subtitleScrollArea->setFrameShape(QFrame::NoFrame);
  auto *subtitlePage = new QWidget();
  auto *subLayout = new QVBoxLayout(subtitlePage);
  subLayout->setContentsMargins(24, 16, 24, 24);
  subLayout->setSpacing(12);

  // 1. 默认字体样式 Group Box
  fontStyleGroup_ = new QGroupBox(tr("默认字体样式"), subtitlePage);
  auto *fontStyleLayout = new QVBoxLayout(fontStyleGroup_);
  fontStyleLayout->setContentsMargins(16, 18, 16, 16);
  fontStyleLayout->setSpacing(12);

  auto *fontGrid = new QGridLayout();
  fontGrid->setSpacing(10);

  auto *fontFamilyLabel = new QLabel(tr("字体族"), subtitlePage);
  subtitleFontFamilyLabel_ = fontFamilyLabel;
  fontFamilyLabel->setObjectName("ConfigFieldLabel");
  fontGrid->addWidget(fontFamilyLabel, 0, 0);

  subtitleFontFamilyCombo_ = new QComboBox(subtitlePage);
  subtitleFontFamilyCombo_->setFixedHeight(32);
  for (const auto &family : QFontDatabase::families()) {
    subtitleFontFamilyCombo_->addItem(family);
  }
  fontGrid->addWidget(subtitleFontFamilyCombo_, 1, 0);

  auto *fontSizeLabel = new QLabel(tr("字号"), subtitlePage);
  subtitleFontSizeLabel_ = fontSizeLabel;
  fontSizeLabel->setObjectName("ConfigFieldLabel");
  fontGrid->addWidget(fontSizeLabel, 0, 1);

  subtitleFontSizeSpin_ = new QSpinBox(subtitlePage);
  subtitleFontSizeSpin_->setFixedHeight(32);
  subtitleFontSizeSpin_->setRange(8, 72);
  subtitleFontSizeSpin_->setValue(24);
  fontGrid->addWidget(subtitleFontSizeSpin_, 1, 1);

  fontGrid->setColumnStretch(0, 3);
  fontGrid->setColumnStretch(1, 1);
  fontStyleLayout->addLayout(fontGrid);

  // 下半排：对齐方式 + 样式属性
  auto *bottomRowLayout = new QHBoxLayout();
  bottomRowLayout->setSpacing(20);

  auto *alignCol = new QVBoxLayout();
  alignCol->setSpacing(6);
  auto *alignmentLabel = new QLabel(tr("对齐方式"), subtitlePage);
  subtitleAlignmentLabel_ = alignmentLabel;
  alignmentLabel->setObjectName("ConfigFieldLabel");
  alignCol->addWidget(alignmentLabel);

  subtitleAlignmentCombo_ = new QComboBox(subtitlePage);
  subtitleAlignmentCombo_->setFixedHeight(32);
  subtitleAlignmentCombo_->addItem(tr("左对齐"), 0x81);
  subtitleAlignmentCombo_->addItem(tr("居中"), 0x84);
  subtitleAlignmentCombo_->addItem(tr("右对齐"), 0x82);
  alignCol->addWidget(subtitleAlignmentCombo_);
  bottomRowLayout->addLayout(alignCol, 1);

  auto *styleCol = new QVBoxLayout();
  styleCol->setSpacing(6);
  auto *styleGroupLabel = new QLabel(tr("样式属性"), subtitlePage);
  subtitleStyleGroupLabel_ = styleGroupLabel;
  styleGroupLabel->setObjectName("ConfigFieldLabel");
  styleCol->addWidget(styleGroupLabel);

  auto *styleLayout = new QHBoxLayout();
  styleLayout->setSpacing(12);

  subtitleBoldLabel_ = new QLabel(tr("粗体"), subtitlePage);
  styleLayout->addWidget(subtitleBoldLabel_);
  subtitleBoldCheck_ = new QCheckBox(subtitlePage);
  styleLayout->addWidget(subtitleBoldCheck_);

  subtitleItalicLabel_ = new QLabel(tr("斜体"), subtitlePage);
  styleLayout->addWidget(subtitleItalicLabel_);
  subtitleItalicCheck_ = new QCheckBox(subtitlePage);
  styleLayout->addWidget(subtitleItalicCheck_);

  subtitleUnderlineLabel_ = new QLabel(tr("下划线"), subtitlePage);
  styleLayout->addWidget(subtitleUnderlineLabel_);
  subtitleUnderlineCheck_ = new QCheckBox(subtitlePage);
  styleLayout->addWidget(subtitleUnderlineCheck_);
  styleLayout->addStretch();

  styleCol->addLayout(styleLayout);
  bottomRowLayout->addLayout(styleCol, 1);

  fontStyleLayout->addLayout(bottomRowLayout);
  subLayout->addWidget(fontStyleGroup_);

  // 2. 默认排版位置 Group Box
  positionGroup_ = new QGroupBox(tr("默认排版位置"), subtitlePage);
  auto *positionLayout = new QVBoxLayout(positionGroup_);
  positionLayout->setContentsMargins(16, 18, 16, 16);
  positionLayout->setSpacing(12);

  auto *rectLayout = new QGridLayout();
  rectLayout->setContentsMargins(0, 0, 0, 0);
  rectLayout->setHorizontalSpacing(16);
  rectLayout->setVerticalSpacing(10);
  rectLayout->setColumnStretch(0, 0);
  rectLayout->setColumnStretch(1, 1);
  rectLayout->setColumnStretch(2, 0);
  rectLayout->setColumnStretch(3, 0);
  rectLayout->setColumnStretch(4, 1);
  rectLayout->setColumnMinimumWidth(2, 40);

  subtitleRectXLabel_ = new QLabel(tr("X (%):"), subtitlePage);
  subtitleRectXLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  rectLayout->addWidget(subtitleRectXLabel_, 0, 0);
  subtitleRectXSpin_ = new QSpinBox(subtitlePage);
  subtitleRectXSpin_->setFixedHeight(32);
  subtitleRectXSpin_->setRange(0, 100);
  subtitleRectXSpin_->setValue(10);
  rectLayout->addWidget(subtitleRectXSpin_, 0, 1);

  subtitleRectYLabel_ = new QLabel(tr("Y (%):"), subtitlePage);
  subtitleRectYLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  rectLayout->addWidget(subtitleRectYLabel_, 0, 3);
  subtitleRectYSpin_ = new QSpinBox(subtitlePage);
  subtitleRectYSpin_->setFixedHeight(32);
  subtitleRectYSpin_->setRange(0, 100);
  subtitleRectYSpin_->setValue(75);
  rectLayout->addWidget(subtitleRectYSpin_, 0, 4);

  subtitleRectWLabel_ = new QLabel(tr("宽度 (%):"), subtitlePage);
  subtitleRectWLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  rectLayout->addWidget(subtitleRectWLabel_, 1, 0);
  subtitleRectWSpin_ = new QSpinBox(subtitlePage);
  subtitleRectWSpin_->setFixedHeight(32);
  subtitleRectWSpin_->setRange(0, 100);
  subtitleRectWSpin_->setValue(80);
  rectLayout->addWidget(subtitleRectWSpin_, 1, 1);

  subtitleRectHLabel_ = new QLabel(tr("高度 (%):"), subtitlePage);
  subtitleRectHLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  rectLayout->addWidget(subtitleRectHLabel_, 1, 3);
  subtitleRectHSpin_ = new QSpinBox(subtitlePage);
  subtitleRectHSpin_->setFixedHeight(32);
  subtitleRectHSpin_->setRange(0, 100);
  subtitleRectHSpin_->setValue(20);
  rectLayout->addWidget(subtitleRectHSpin_, 1, 4);

  subtitleRotationLabel_ = new QLabel(tr("旋转 (°):"), subtitlePage);
  subtitleRotationLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  rectLayout->addWidget(subtitleRotationLabel_, 2, 0);
  subtitleRotationSpin_ = new QSpinBox(subtitlePage);
  subtitleRotationSpin_->setFixedHeight(32);
  subtitleRotationSpin_->setRange(-180, 180);
  subtitleRotationSpin_->setValue(0);
  rectLayout->addWidget(subtitleRotationSpin_, 2, 1);

  positionLayout->addLayout(rectLayout);
  subLayout->addWidget(positionGroup_);

  // 3. 说话人设置 Group Box
  speakerGroup_ = new QGroupBox(tr("说话人设置"), subtitlePage);
  auto *speakerLayout = new QVBoxLayout(speakerGroup_);
  speakerLayout->setContentsMargins(16, 18, 16, 16);
  speakerLayout->setSpacing(12);

  speakerBgFolderLabel_ = new QLabel(tr("背景图文件夹"), subtitlePage);
  speakerBgFolderLabel_->setObjectName("ConfigFieldLabel");
  speakerLayout->addWidget(speakerBgFolderLabel_);

  auto *folderLayout = new QHBoxLayout();
  folderLayout->setSpacing(8);
  speakerBgFolderEdit_ = new QLineEdit(subtitlePage);
  speakerBgFolderEdit_->setObjectName("SpeakerFolderEdit");
  speakerBgFolderEdit_->setFixedHeight(32);
  folderLayout->addWidget(speakerBgFolderEdit_);
  speakerBgFolderBtn_ = new QPushButton(tr("浏览..."), subtitlePage);
  speakerBgFolderBtn_->setObjectName("SpeakerBrowseButton");
  speakerBgFolderBtn_->setFixedHeight(32);
  connect(speakerBgFolderBtn_, &QPushButton::clicked, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("选择背景图文件夹"), speakerBgFolderEdit_->text());
    if (!dir.isEmpty()) {
      speakerBgFolderEdit_->setText(dir);
      checkDirtyState();
    }
  });
  folderLayout->addWidget(speakerBgFolderBtn_);
  speakerLayout->addLayout(folderLayout);

  auto *marginLabel = new QLabel(tr("九宫格边距"), subtitlePage);
  marginLabel->setObjectName("ConfigFieldLabel");
  speakerLayout->addWidget(marginLabel);

  auto *marginLayout = new QGridLayout();
  marginLayout->setContentsMargins(0, 0, 0, 0);
  marginLayout->setHorizontalSpacing(16);
  marginLayout->setVerticalSpacing(10);
  marginLayout->setColumnStretch(0, 0);
  marginLayout->setColumnStretch(1, 1);
  marginLayout->setColumnStretch(2, 0);
  marginLayout->setColumnStretch(3, 0);
  marginLayout->setColumnStretch(4, 1);
  marginLayout->setColumnMinimumWidth(2, 40);

  speakerMarginLeftLabel_ = new QLabel(tr("左:"), subtitlePage);
  speakerMarginLeftLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  marginLayout->addWidget(speakerMarginLeftLabel_, 0, 0);
  speakerMarginLeftSpin_ = new QSpinBox(subtitlePage);
  speakerMarginLeftSpin_->setFixedHeight(32);
  speakerMarginLeftSpin_->setRange(0, 100);
  speakerMarginLeftSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginLeftSpin_, 0, 1);

  speakerMarginTopLabel_ = new QLabel(tr("上:"), subtitlePage);
  speakerMarginTopLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  marginLayout->addWidget(speakerMarginTopLabel_, 0, 3);
  speakerMarginTopSpin_ = new QSpinBox(subtitlePage);
  speakerMarginTopSpin_->setFixedHeight(32);
  speakerMarginTopSpin_->setRange(0, 100);
  speakerMarginTopSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginTopSpin_, 0, 4);

  speakerMarginRightLabel_ = new QLabel(tr("右:"), subtitlePage);
  speakerMarginRightLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  marginLayout->addWidget(speakerMarginRightLabel_, 1, 0);
  speakerMarginRightSpin_ = new QSpinBox(subtitlePage);
  speakerMarginRightSpin_->setFixedHeight(32);
  speakerMarginRightSpin_->setRange(0, 100);
  speakerMarginRightSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginRightSpin_, 1, 1);

  speakerMarginBottomLabel_ = new QLabel(tr("下:"), subtitlePage);
  speakerMarginBottomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  marginLayout->addWidget(speakerMarginBottomLabel_, 1, 3);
  speakerMarginBottomSpin_ = new QSpinBox(subtitlePage);
  speakerMarginBottomSpin_->setFixedHeight(32);
  speakerMarginBottomSpin_->setRange(0, 100);
  speakerMarginBottomSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginBottomSpin_, 1, 4);
  speakerLayout->addLayout(marginLayout);

  subLayout->addWidget(speakerGroup_);

  subLayout->addStretch();
  subtitleScrollArea->setWidget(subtitlePage);
  stackedWidget_->addWidget(subtitleScrollArea);

  contentLayout->addWidget(stackedWidget_);

  mainLayout->addWidget(contentWidget);

  auto *footer = new QWidget(this);
  footer->setObjectName("ConfigFooter");
  footer->setFixedHeight(60);
  auto *footerLayout = new QHBoxLayout(footer);
  footerLayout->setContentsMargins(20, 0, 20, 0);

  dirtyLabel_ = new QLabel(tr("有未保存的更改"), footer);
  dirtyLabel_->setObjectName("ConfigDirtyLabel");
  dirtyLabel_->setVisible(false);
  footerLayout->addWidget(dirtyLabel_);

  footerLayout->addStretch();

  btnCancel_ = new QPushButton(tr("取消"), footer);
  btnCancel_->setObjectName("ConfigCancelButton");
  btnApply_ = new QPushButton(tr("应用"), footer);
  btnApply_->setObjectName("ConfigApplyButton");
  btnOk_ = new QPushButton(tr("确定"), footer);
  btnOk_->setObjectName("ConfigOkButton");

  footerLayout->addWidget(btnCancel_);
  footerLayout->addWidget(btnApply_);
  footerLayout->addWidget(btnOk_);

  mainLayout->addWidget(footer);

  connect(sidebarList_, &QListWidget::currentRowChanged, stackedWidget_,
          &QStackedWidget::setCurrentIndex);
  connect(sidebarList_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (auto *item = sidebarList_->item(row)) {
      titleRightLabel->setText(item->text());
    }
  });
  sidebarList_->setCurrentRow(0);
}

void ConfigDialog::updateStorageFields(const QString &provider) {
  if (provider == "aliyun_oss") {
    ossBucketEdit_->setText(tempAliBucket_);
    ossRegionEdit_->setText(tempAliRegion_);
    ossAccessKeyEdit_->setText(tempAliAk_);
    ossSecretKeyEdit_->setText(tempAliSk_);
  } else if (provider == "tencent_cos") {
    ossBucketEdit_->setText(tempCosBucket_);
    ossRegionEdit_->setText(tempCosRegion_);
    ossAccessKeyEdit_->setText(tempCosAk_);
    ossSecretKeyEdit_->setText(tempCosSk_);
  }
}

void ConfigDialog::updateStorageLabels(const QString &provider) {
  if (provider == "aliyun_oss") {
    akLabel_->setText(tr("访问密钥 ID (Access Key ID)"));
    skLabel_->setText(tr("访问密钥 (Access Key Secret)"));
  } else if (provider == "tencent_cos") {
    akLabel_->setText(tr("密钥 ID (Secret ID)"));
    skLabel_->setText(tr("密钥 (Secret Key)"));
  }
}

void ConfigDialog::onStorageProviderChanged(const QString &) {
  QString newProvider = storageProviderCombo_->currentData().toString();
  if (newProvider == currentProvider_) {
    return;
  }
  currentProvider_ = newProvider;

  // 切换存储提供商时，重置密钥框为密文显示，小眼睛为闭眼
  ossSecretKeyEdit_->setEchoMode(QLineEdit::Password);
  if (ossEyeAction_) {
    ossEyeAction_->setIcon(createEyeIcon(false));
  }

  updateStorageFields(currentProvider_);
  updateStorageLabels(currentProvider_);
  checkDirtyState();
}
