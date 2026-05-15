#include "ConfigDialog.h"
#include "ConfigManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
    setupUi();
    loadConfig();
    checkDirtyState();
}

void ConfigDialog::setupUi() {
    setWindowTitle(tr("配置"));
    resize(700, 500);
    setStyleSheet("QDialog { background-color: #1e1e1e; color: #d1d5db; }");

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar
    sidebarList_ = new QListWidget(this);
    sidebarList_->setFixedWidth(160);
    sidebarList_->setStyleSheet("QListWidget { background-color: #262626; border-right: 1px solid #0a0a0a; outline: none; } QListWidget::item { padding: 12px 16px; color: #9ca3af; } QListWidget::item:selected { background-color: #3b3b3b; color: #fff; }");
    sidebarList_->addItem(tr("通用"));
    sidebarList_->addItem(tr("存储"));
    sidebarList_->addItem(tr("语音识别"));
    
    // Stacked Widget
    stackedWidget_ = new QStackedWidget(this);
    
    // General Page
    auto *generalPage = new QWidget();
    auto *generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setAlignment(Qt::AlignTop);
    generalLayout->setContentsMargins(25, 25, 25, 25);
    
    auto *titleLabel = new QLabel(tr("通用配置"), generalPage);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #fff; margin-bottom: 20px;");
    generalLayout->addWidget(titleLabel);

    langCombo_ = new QComboBox(generalPage);
    langCombo_->addItem("简体中文", "zh_CN");
    langCombo_->addItem("English", "en_US");
    generalLayout->addWidget(new QLabel(tr("语言 (Language)")));
    generalLayout->addWidget(langCombo_);

    themeCombo_ = new QComboBox(generalPage);
    themeCombo_->addItem(tr("深色 (Dark)"), "dark");
    themeCombo_->addItem(tr("浅色 (Light)"), "light");
    generalLayout->addWidget(new QLabel(tr("主题 (Theme)")));
    generalLayout->addWidget(themeCombo_);

    stackedWidget_->addWidget(generalPage);

    // Storage Page
    auto *storagePage = new QWidget();
    auto *storageLayout = new QVBoxLayout(storagePage);
    storageLayout->setAlignment(Qt::AlignTop);
    storageLayout->setContentsMargins(25, 25, 25, 25);

    auto *storageTitleLabel = new QLabel(tr("存储配置"), storagePage);
    storageTitleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #fff; margin-bottom: 20px;");
    storageLayout->addWidget(storageTitleLabel);

    storageLayout->addWidget(new QLabel(tr("存储提供商")));
    storageProviderCombo_ = new QComboBox(storagePage);
    storageProviderCombo_->addItem("阿里云 OSS", "aliyun_oss");
    storageLayout->addWidget(storageProviderCombo_);
    
    ossBucketEdit_ = new QLineEdit(storagePage);
    storageLayout->addWidget(new QLabel("Bucket"));
    storageLayout->addWidget(ossBucketEdit_);
    
    ossRegionEdit_ = new QLineEdit(storagePage);
    storageLayout->addWidget(new QLabel("Region"));
    storageLayout->addWidget(ossRegionEdit_);

    ossAccessKeyEdit_ = new QLineEdit(storagePage);
    storageLayout->addWidget(new QLabel("Access Key ID"));
    storageLayout->addWidget(ossAccessKeyEdit_);

    ossSecretKeyEdit_ = new QLineEdit(storagePage);
    ossSecretKeyEdit_->setEchoMode(QLineEdit::Password);
    storageLayout->addWidget(new QLabel("Access Key Secret"));
    storageLayout->addWidget(ossSecretKeyEdit_);
    
    stackedWidget_->addWidget(storagePage);

    // ASR Page
    auto *asrPage = new QWidget();
    auto *asrLayout = new QVBoxLayout(asrPage);
    asrLayout->setAlignment(Qt::AlignTop);
    asrLayout->setContentsMargins(25, 25, 25, 25);

    auto *asrTitleLabel = new QLabel(tr("语音识别配置"), asrPage);
    asrTitleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #fff; margin-bottom: 20px;");
    asrLayout->addWidget(asrTitleLabel);

    asrLayout->addWidget(new QLabel(tr("ASR 服务提供商")));
    asrProviderCombo_ = new QComboBox(asrPage);
    asrProviderCombo_->addItem("腾讯云", "tencent");
    asrLayout->addWidget(asrProviderCombo_);

    tencentAppIdEdit_ = new QLineEdit(asrPage);
    asrLayout->addWidget(new QLabel("App ID"));
    asrLayout->addWidget(tencentAppIdEdit_);

    tencentSecretIdEdit_ = new QLineEdit(asrPage);
    asrLayout->addWidget(new QLabel("Secret ID"));
    asrLayout->addWidget(tencentSecretIdEdit_);

    tencentSecretKeyEdit_ = new QLineEdit(asrPage);
    tencentSecretKeyEdit_->setEchoMode(QLineEdit::Password);
    asrLayout->addWidget(new QLabel("Secret Key"));
    asrLayout->addWidget(tencentSecretKeyEdit_);

    stackedWidget_->addWidget(asrPage);

    // Right side layout (Stack + Footer)
    auto *rightLayout = new QVBoxLayout();
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(stackedWidget_);

    // Footer
    auto *footer = new QWidget(this);
    footer->setStyleSheet("background-color: #262626; border-top: 1px solid #333;");
    auto *footerLayout = new QHBoxLayout(footer);
    
    dirtyLabel_ = new QLabel(tr("● 有未保存的更改"), footer);
    dirtyLabel_->setStyleSheet("color: #eab308; font-size: 12px;");
    footerLayout->addWidget(dirtyLabel_);
    footerLayout->addStretch();

    btnCancel_ = new QPushButton(tr("取消"), footer);
    btnApply_ = new QPushButton(tr("应用"), footer);
    btnOk_ = new QPushButton(tr("确定"), footer);
    
    QString btnStyle = "QPushButton { background-color: #444; color: #eee; border: none; padding: 7px 18px; border-radius: 4px; } QPushButton:hover { background-color: #555; }";
    QString primaryBtnStyle = "QPushButton { background-color: #0284c7; color: #fff; border: none; padding: 7px 18px; border-radius: 4px; } QPushButton:hover { background-color: #0369a1; }";
    
    btnCancel_->setStyleSheet(btnStyle);
    btnApply_->setStyleSheet(primaryBtnStyle);
    btnOk_->setStyleSheet(primaryBtnStyle);

    footerLayout->addWidget(btnCancel_);
    footerLayout->addWidget(btnApply_);
    footerLayout->addWidget(btnOk_);
    rightLayout->addWidget(footer);

    mainLayout->addWidget(sidebarList_);
    mainLayout->addLayout(rightLayout);

    connect(sidebarList_, &QListWidget::currentRowChanged, stackedWidget_, &QStackedWidget::setCurrentIndex);
    connect(btnCancel_, &QPushButton::clicked, this, &ConfigDialog::onCancel);
    connect(btnApply_, &QPushButton::clicked, this, &ConfigDialog::onApply);
    connect(btnOk_, &QPushButton::clicked, this, &ConfigDialog::onOk);
    
    connect(langCombo_, &QComboBox::currentTextChanged, this, &ConfigDialog::checkDirtyState);
    connect(themeCombo_, &QComboBox::currentTextChanged, this, &ConfigDialog::checkDirtyState);
    connect(storageProviderCombo_, &QComboBox::currentTextChanged, this, &ConfigDialog::checkDirtyState);
    connect(asrProviderCombo_, &QComboBox::currentTextChanged, this, &ConfigDialog::checkDirtyState);

    auto lineEdits = {ossBucketEdit_, ossRegionEdit_, ossAccessKeyEdit_, ossSecretKeyEdit_, tencentAppIdEdit_, tencentSecretIdEdit_, tencentSecretKeyEdit_};
    for(auto *le : lineEdits) {
        connect(le, &QLineEdit::textChanged, this, &ConfigDialog::checkDirtyState);
    }
}

void ConfigDialog::loadConfig() {
    auto &cfg = ConfigManager::instance();
    initialConfig_["language"] = cfg.getString("general", "language");
    if (initialConfig_["language"].toString().isEmpty()) initialConfig_["language"] = "zh_CN";
    
    initialConfig_["theme"] = cfg.getString("general", "theme");
    if (initialConfig_["theme"].toString().isEmpty()) initialConfig_["theme"] = "dark";

    langCombo_->setCurrentIndex(langCombo_->findData(initialConfig_["language"]));
    themeCombo_->setCurrentIndex(themeCombo_->findData(initialConfig_["theme"]));

    initialConfig_["oss_bucket"] = cfg.ossBucket();
    ossBucketEdit_->setText(cfg.ossBucket());
    initialConfig_["oss_region"] = cfg.ossRegion();
    ossRegionEdit_->setText(cfg.ossRegion());
    initialConfig_["oss_ak"] = cfg.ossAccessKeyId();
    ossAccessKeyEdit_->setText(cfg.ossAccessKeyId());
    initialConfig_["oss_sk"] = cfg.ossAccessKeySecret();
    ossSecretKeyEdit_->setText(cfg.ossAccessKeySecret());

    initialConfig_["tc_appid"] = cfg.tencentAppId();
    tencentAppIdEdit_->setText(cfg.tencentAppId());
    initialConfig_["tc_sid"] = cfg.tencentSecretId();
    tencentSecretIdEdit_->setText(cfg.tencentSecretId());
    initialConfig_["tc_skey"] = cfg.tencentSecretKey();
    tencentSecretKeyEdit_->setText(cfg.tencentSecretKey());
}

bool ConfigDialog::isDirty() const {
    return (langCombo_->currentData().toString() != initialConfig_["language"]) ||
           (themeCombo_->currentData().toString() != initialConfig_["theme"]) ||
           (ossBucketEdit_->text() != initialConfig_["oss_bucket"].toString()) ||
           (ossRegionEdit_->text() != initialConfig_["oss_region"].toString()) ||
           (ossAccessKeyEdit_->text() != initialConfig_["oss_ak"].toString()) ||
           (ossSecretKeyEdit_->text() != initialConfig_["oss_sk"].toString()) ||
           (tencentAppIdEdit_->text() != initialConfig_["tc_appid"].toString()) ||
           (tencentSecretIdEdit_->text() != initialConfig_["tc_sid"].toString()) ||
           (tencentSecretKeyEdit_->text() != initialConfig_["tc_skey"].toString());
}

void ConfigDialog::checkDirtyState() {
    bool dirty = isDirty();
    dirtyLabel_->setVisible(dirty);
    btnApply_->setEnabled(dirty);
}

void ConfigDialog::saveConfig() {
    auto &cfg = ConfigManager::instance();
    cfg.setValue("general", "language", langCombo_->currentData().toString());
    cfg.setValue("general", "theme", themeCombo_->currentData().toString());
    
    cfg.setValue("aliyun_oss", "bucket", ossBucketEdit_->text());
    cfg.setValue("aliyun_oss", "region", ossRegionEdit_->text());
    cfg.setValue("aliyun_oss", "access_key_id", ossAccessKeyEdit_->text());
    cfg.setValue("aliyun_oss", "access_key_secret", ossSecretKeyEdit_->text());

    cfg.setValue("tencent_asr", "app_id", tencentAppIdEdit_->text());
    cfg.setValue("tencent_asr", "secret_id", tencentSecretIdEdit_->text());
    cfg.setValue("tencent_asr", "secret_key", tencentSecretKeyEdit_->text());
    
    cfg.sync();
    loadConfig(); // Reset initial state to current
    checkDirtyState();
}

void ConfigDialog::onApply() { saveConfig(); }
void ConfigDialog::onOk() { saveConfig(); accept(); }

void ConfigDialog::onCancel() {
    if (isDirty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("未保存的更改"),
            tr("配置已修改，是否在离开前保存？"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) { saveConfig(); accept(); return; }
        if (reply == QMessageBox::No) { reject(); return; }
        if (reply == QMessageBox::Cancel) { return; }
    }
    reject();
}
