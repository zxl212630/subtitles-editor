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
#include <QFile>
#include <QApplication>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
    setupUi();
    loadConfig();
    checkDirtyState();
}

void ConfigDialog::setupUi() {
    setWindowTitle(tr("配置"));
    resize(700, 560);
    setMinimumSize(600, 500);

    windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);

    setupTitleBar();

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(titleBar);

    auto *contentWidget = new QWidget(this);
    contentWidget->setObjectName("ConfigContentWidget");
    rootLayout->addWidget(contentWidget);

    auto *mainLayout = new QHBoxLayout(contentWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar
    sidebarList_ = new QListWidget(contentWidget);
    sidebarList_->setFixedWidth(160);
    sidebarList_->setObjectName("ConfigSidebar");
    sidebarList_->addItem(tr("通用"));
    sidebarList_->addItem(tr("存储"));
    sidebarList_->addItem(tr("语音识别"));
    
    // Stacked Widget
    stackedWidget_ = new QStackedWidget(contentWidget);
    stackedWidget_->setObjectName("ConfigStackedWidget");
    
    // General Page
    auto *generalPage = new QWidget();
    auto *generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setAlignment(Qt::AlignTop);
    generalLayout->setContentsMargins(25, 25, 25, 25);
    generalLayout->setSpacing(15);
    
    auto *titleLabelPage = new QLabel(tr("通用配置"), generalPage);
    titleLabelPage->setObjectName("ConfigPageTitle");
    generalLayout->addWidget(titleLabelPage);

    auto *langLabel = new QLabel(tr("语言 (Language)"), generalPage);
    langLabel->setObjectName("ConfigFieldLabel");
    generalLayout->addWidget(langLabel);
    langCombo_ = new QComboBox(generalPage);
    langCombo_->addItem("简体中文", "zh_CN");
    langCombo_->addItem("English", "en_US");
    generalLayout->addWidget(langCombo_);

    auto *themeLabel = new QLabel(tr("主题 (Theme)"), generalPage);
    themeLabel->setObjectName("ConfigFieldLabel");
    generalLayout->addWidget(themeLabel);
    themeCombo_ = new QComboBox(generalPage);
    themeCombo_->addItem(tr("深色 (Dark)"), "dark");
    themeCombo_->addItem(tr("浅色 (Light)"), "light");
    generalLayout->addWidget(themeCombo_);

    stackedWidget_->addWidget(generalPage);

    // Storage Page
    auto *storagePage = new QWidget();
    auto *storageLayout = new QVBoxLayout(storagePage);
    storageLayout->setAlignment(Qt::AlignTop);
    storageLayout->setContentsMargins(25, 25, 25, 25);
    storageLayout->setSpacing(15);

    auto *storageTitleLabel = new QLabel(tr("存储配置"), storagePage);
    storageTitleLabel->setObjectName("ConfigPageTitle");
    storageLayout->addWidget(storageTitleLabel);

    auto *storageProvLabel = new QLabel(tr("存储提供商"), storagePage);
    storageProvLabel->setObjectName("ConfigFieldLabel");
    storageLayout->addWidget(storageProvLabel);
    storageProviderCombo_ = new QComboBox(storagePage);
    storageProviderCombo_->addItem(tr("阿里云 OSS"), "aliyun_oss");
    storageLayout->addWidget(storageProviderCombo_);
    
    auto *bucketLabel = new QLabel(tr("存储桶 (Bucket)"), storagePage);
    bucketLabel->setObjectName("ConfigFieldLabel");
    storageLayout->addWidget(bucketLabel);
    ossBucketEdit_ = new QLineEdit(storagePage);
    storageLayout->addWidget(ossBucketEdit_);
    
    auto *regionLabel = new QLabel(tr("地域 (Region)"), storagePage);
    regionLabel->setObjectName("ConfigFieldLabel");
    storageLayout->addWidget(regionLabel);
    ossRegionEdit_ = new QLineEdit(storagePage);
    storageLayout->addWidget(ossRegionEdit_);

    auto *akLabel = new QLabel(tr("访问密钥 ID (Access Key ID)"), storagePage);
    akLabel->setObjectName("ConfigFieldLabel");
    storageLayout->addWidget(akLabel);
    ossAccessKeyEdit_ = new QLineEdit(storagePage);
    storageLayout->addWidget(ossAccessKeyEdit_);

    auto *skLabel = new QLabel(tr("访问密钥密钥 (Access Key Secret)"), storagePage);
    skLabel->setObjectName("ConfigFieldLabel");
    storageLayout->addWidget(skLabel);
    ossSecretKeyEdit_ = new QLineEdit(storagePage);
    ossSecretKeyEdit_->setEchoMode(QLineEdit::Password);
    storageLayout->addWidget(ossSecretKeyEdit_);
    
    stackedWidget_->addWidget(storagePage);

    // ASR Page
    auto *asrPage = new QWidget();
    auto *asrLayout = new QVBoxLayout(asrPage);
    asrLayout->setAlignment(Qt::AlignTop);
    asrLayout->setContentsMargins(25, 25, 25, 25);
    asrLayout->setSpacing(15);

    auto *asrTitleLabel = new QLabel(tr("语音识别配置"), asrPage);
    asrTitleLabel->setObjectName("ConfigPageTitle");
    asrLayout->addWidget(asrTitleLabel);

    auto *asrProvLabel = new QLabel(tr("ASR 服务提供商"), asrPage);
    asrProvLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(asrProvLabel);
    asrProviderCombo_ = new QComboBox(asrPage);
    asrProviderCombo_->addItem(tr("腾讯云"), "tencent");
    asrLayout->addWidget(asrProviderCombo_);

    auto *appIdLabel = new QLabel(tr("应用 ID (App ID)"), asrPage);
    appIdLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(appIdLabel);
    tencentAppIdEdit_ = new QLineEdit(asrPage);
    asrLayout->addWidget(tencentAppIdEdit_);

    auto *sidLabel = new QLabel(tr("密钥 ID (Secret ID)"), asrPage);
    sidLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(sidLabel);
    tencentSecretIdEdit_ = new QLineEdit(asrPage);
    asrLayout->addWidget(tencentSecretIdEdit_);

    auto *skeyLabel = new QLabel(tr("密钥密码 (Secret Key)"), asrPage);
    skeyLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(skeyLabel);
    tencentSecretKeyEdit_ = new QLineEdit(asrPage);
    tencentSecretKeyEdit_->setEchoMode(QLineEdit::Password);
    asrLayout->addWidget(tencentSecretKeyEdit_);

    stackedWidget_->addWidget(asrPage);

    // Right side layout (Stack + Footer)
    auto *rightSideLayout = new QVBoxLayout();
    rightSideLayout->setContentsMargins(0, 0, 0, 0);
    rightSideLayout->setSpacing(0);
    rightSideLayout->addWidget(stackedWidget_);

    // Footer
    auto *footer = new QWidget(contentWidget);
    footer->setObjectName("ConfigFooter");
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(15, 10, 15, 10);
    
    dirtyLabel_ = new QLabel(tr("● 有未保存的更改"), footer);
    dirtyLabel_->setObjectName("ConfigDirtyLabel");
    footerLayout->addWidget(dirtyLabel_);
    footerLayout->addStretch();

    btnCancel_ = new QPushButton(tr("取消"), footer);
    btnApply_ = new QPushButton(tr("应用"), footer);
    btnOk_ = new QPushButton(tr("确定"), footer);
    
    btnCancel_->setObjectName("ConfigCancelButton");
    btnApply_->setObjectName("ConfigApplyButton");
    btnOk_->setObjectName("ConfigOkButton");

    footerLayout->addWidget(btnCancel_);
    footerLayout->addWidget(btnApply_);
    footerLayout->addWidget(btnOk_);
    rightSideLayout->addWidget(footer);

    mainLayout->addWidget(sidebarList_);
    mainLayout->addLayout(rightSideLayout);

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

    windowAgent->setTitleBar(titleBar);
}

void ConfigDialog::setupTitleBar() {
    titleBar = new QFrame(this);
    titleBar->setFixedHeight(36);
    titleBar->setObjectName("TitleBar");

    auto *layout = new QHBoxLayout(titleBar);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);

    auto *leftSpacer = new QWidget(titleBar);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(leftSpacer);

    titleLabel = new QLabel(tr("配置"), titleBar);
    titleLabel->setObjectName("AppTitleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto *rightSpacer = new QWidget(titleBar);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(rightSpacer);
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
