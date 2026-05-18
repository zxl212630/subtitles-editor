#include "ConfigDialog.h"
#include "ConfigManager.h"
#include "ThemeManager.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QDebug>
#include "PaletteSelectors.h"
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

ConfigDialog::ConfigDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("设置"));
    setMinimumSize(800, 560);
    
    // Set object name for QSS
    setObjectName("ConfigDialog");

    windowAgent = new QWK::WidgetWindowAgent(this);
    windowAgent->setup(this);

    setupTitleBar();
    setupUi();
    loadConfig();

    connect(btnCancel_, &QPushButton::clicked, this, &ConfigDialog::onCancel);
    connect(btnApply_, &QPushButton::clicked, this, &ConfigDialog::onApply);
    connect(btnOk_, &QPushButton::clicked, this, &ConfigDialog::onOk);

    // Track changes
    connect(themeSelector_, &ThemeSelectorWidget::themeSelected, this, &ConfigDialog::checkDirtyState);
    connect(colorSelector_, &ColorSelectorWidget::colorSelected, this, &ConfigDialog::checkDirtyState);
    connect(langCombo_, &QComboBox::currentTextChanged, this, &ConfigDialog::checkDirtyState);
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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Left part of title bar (above sidebar)
    auto *leftContainer = new QWidget(titleBar);
    leftContainer->setFixedWidth(180);
    leftContainer->setObjectName("TitleLeftContainer");
    auto *leftLayout = new QHBoxLayout(leftContainer);
    leftLayout->setContentsMargins(12, 0, 12, 0);
    titleLeftLabel = new QLabel(tr("设置"), leftContainer);
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
    rightLayout->addWidget(titleRightLabel);
    layout->addWidget(rightContainer);
}

void ConfigDialog::loadConfig() {
    auto &cfg = ConfigManager::instance();
    initialConfig_["language"] = cfg.getString("", "language");
    if (initialConfig_["language"].toString().isEmpty()) initialConfig_["language"] = "zh_CN";
    
    initialConfig_["theme"] = cfg.getString("", "theme");
    if (initialConfig_["theme"].toString() == "light" || initialConfig_["theme"].toString().isEmpty()) {
        initialConfig_["theme"] = "dark";
    }

    initialConfig_["primary_color"] = cfg.getString("", "primary_color");
    if (initialConfig_["primary_color"].toString().isEmpty()) initialConfig_["primary_color"] = "blue";

    langCombo_->setCurrentIndex(langCombo_->findData(initialConfig_["language"]));
    themeSelector_->setCurrentTheme(initialConfig_["theme"].toString());
    colorSelector_->setCurrentColor(initialConfig_["primary_color"].toString());

    initialConfig_["oss_bucket"] = cfg.getString("aliyun_oss", "bucket");
    initialConfig_["oss_region"] = cfg.getString("aliyun_oss", "region");
    initialConfig_["oss_ak"] = cfg.getString("aliyun_oss", "access_key_id");
    initialConfig_["oss_sk"] = cfg.getString("aliyun_oss", "access_key_secret");

    initialConfig_["tc_appid"] = cfg.getString("tencent_asr", "app_id");
    initialConfig_["tc_sid"] = cfg.getString("tencent_asr", "secret_id");
    initialConfig_["tc_skey"] = cfg.getString("tencent_asr", "secret_key");

    ossBucketEdit_->setText(initialConfig_["oss_bucket"].toString());
    ossRegionEdit_->setText(initialConfig_["oss_region"].toString());
    ossAccessKeyEdit_->setText(initialConfig_["oss_ak"].toString());
    ossSecretKeyEdit_->setText(initialConfig_["oss_sk"].toString());
    tencentAppIdEdit_->setText(initialConfig_["tc_appid"].toString());
    tencentSecretIdEdit_->setText(initialConfig_["tc_sid"].toString());
    tencentSecretKeyEdit_->setText(initialConfig_["tc_skey"].toString());

    // Sync title
    if (sidebarList_->currentItem()) {
        titleRightLabel->setText(sidebarList_->currentItem()->text());
    }

    checkDirtyState();
}

bool ConfigDialog::isDirty() const {
    return (langCombo_->currentData().toString() != initialConfig_["language"].toString()) ||
           (themeSelector_->currentTheme() != initialConfig_["theme"].toString()) ||
           (colorSelector_->currentColor() != initialConfig_["primary_color"].toString()) ||
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
    cfg.setValue("", "language", langCombo_->currentData().toString());
    cfg.setValue("", "theme", themeSelector_->currentTheme());
    cfg.setValue("", "primary_color", colorSelector_->currentColor());
    
    cfg.setValue("aliyun_oss", "bucket", ossBucketEdit_->text());
    cfg.setValue("aliyun_oss", "region", ossRegionEdit_->text());
    cfg.setValue("aliyun_oss", "access_key_id", ossAccessKeyEdit_->text());
    cfg.setValue("aliyun_oss", "access_key_secret", ossSecretKeyEdit_->text());

    cfg.setValue("tencent_asr", "app_id", tencentAppIdEdit_->text());
    cfg.setValue("tencent_asr", "secret_id", tencentSecretIdEdit_->text());
    cfg.setValue("tencent_asr", "secret_key", tencentSecretKeyEdit_->text());
    
    cfg.sync();
    
    // Update initialConfig_ directly to reflect saved state
    initialConfig_["language"] = langCombo_->currentData().toString();
    initialConfig_["theme"] = themeSelector_->currentTheme();
    initialConfig_["primary_color"] = colorSelector_->currentColor();
    initialConfig_["oss_bucket"] = ossBucketEdit_->text();
    initialConfig_["oss_region"] = ossRegionEdit_->text();
    initialConfig_["oss_ak"] = ossAccessKeyEdit_->text();
    initialConfig_["oss_sk"] = ossSecretKeyEdit_->text();
    initialConfig_["tc_appid"] = tencentAppIdEdit_->text();
    initialConfig_["tc_sid"] = tencentSecretIdEdit_->text();
    initialConfig_["tc_skey"] = tencentSecretKeyEdit_->text();

    checkDirtyState();
}

void ConfigDialog::onApply() { 
    saveConfig(); 
    ThemeManager::instance().applyTheme();
}

void ConfigDialog::onOk() { 
    saveConfig(); 
    ThemeManager::instance().applyTheme();
    accept(); 
}

void ConfigDialog::onCancel() {
    if (themeSelector_->currentTheme() != initialConfig_["theme"].toString() || 
        colorSelector_->currentColor() != initialConfig_["primary_color"].toString()) {
        ConfigManager::instance().setValue("", "theme", initialConfig_["theme"]);
        ConfigManager::instance().setValue("", "primary_color", initialConfig_["primary_color"]);
        ThemeManager::instance().applyTheme();
    }
    reject();
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
    sidebarList_->addItem(tr("常规"));
    sidebarList_->addItem(tr("存储"));
    sidebarList_->addItem(tr("语音识别"));
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
    langLabel->setObjectName("ConfigFieldLabel");
    genLayout->addWidget(langLabel);
    langCombo_ = new QComboBox(generalPage);
    langCombo_->setFixedHeight(32);
    langCombo_->addItem("简体中文", "zh_CN");
    langCombo_->addItem("English", "en_US");
    genLayout->addWidget(langCombo_);

    auto *themeLabel = new QLabel(tr("主题模式 (Theme)"), generalPage);
    themeLabel->setObjectName("ConfigFieldLabel");
    genLayout->addWidget(themeLabel);
    themeSelector_ = new ThemeSelectorWidget(generalPage);
    themeSelector_->addTheme("dark", "#151515", "#1e1e1e", "#3b82f6");
    themeSelector_->addTheme("oled", "#0a0a0a", "#121212", "#10b981");
    themeSelector_->addTheme("midnight", "#111218", "#171821", "#6366f1");
    genLayout->addWidget(themeSelector_);

    auto *colorLabel = new QLabel(tr("主色调 (Primary Color)"), generalPage);
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
    stProvLabel->setObjectName("ConfigFieldLabel");
    stLayout->addWidget(stProvLabel);
    storageProviderCombo_ = new QComboBox(storagePage);
    storageProviderCombo_->setFixedHeight(32);
    storageProviderCombo_->addItem("阿里云 OSS", "aliyun_oss");
    stLayout->addWidget(storageProviderCombo_);

    auto *bucketLabel = new QLabel(tr("存储桶 (Bucket)"), storagePage);
    bucketLabel->setObjectName("ConfigFieldLabel");
    stLayout->addWidget(bucketLabel);
    ossBucketEdit_ = new QLineEdit(storagePage);
    ossBucketEdit_->setFixedHeight(32);
    stLayout->addWidget(ossBucketEdit_);

    auto *regionLabel = new QLabel(tr("地域 (Region)"), storagePage);
    regionLabel->setObjectName("ConfigFieldLabel");
    stLayout->addWidget(regionLabel);
    ossRegionEdit_ = new QLineEdit(storagePage);
    ossRegionEdit_->setFixedHeight(32);
    stLayout->addWidget(ossRegionEdit_);

    auto *akLabel = new QLabel(tr("访问密钥 ID (Access Key ID)"), storagePage);
    akLabel->setObjectName("ConfigFieldLabel");
    stLayout->addWidget(akLabel);
    ossAccessKeyEdit_ = new QLineEdit(storagePage);
    ossAccessKeyEdit_->setFixedHeight(32);
    stLayout->addWidget(ossAccessKeyEdit_);

    auto *skLabel = new QLabel(tr("访问密钥 (Access Key Secret)"), storagePage);
    skLabel->setObjectName("ConfigFieldLabel");
    stLayout->addWidget(skLabel);
    ossSecretKeyEdit_ = new QLineEdit(storagePage);
    ossSecretKeyEdit_->setFixedHeight(32);
    ossSecretKeyEdit_->setEchoMode(QLineEdit::Password);
    stLayout->addWidget(ossSecretKeyEdit_);

    stLayout->addStretch();
    stackedWidget_->addWidget(storagePage);

    // ------------------------------------------------------------------------
    // ASR Page
    // ------------------------------------------------------------------------
    auto *asrPage = new QWidget();
    auto *asrLayout = new QVBoxLayout(asrPage);
    asrLayout->setContentsMargins(30, 25, 30, 30);
    asrLayout->setSpacing(15);
    
    auto *asrProvLabel = new QLabel(tr("识别提供商"), asrPage);
    asrProvLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(asrProvLabel);
    asrProviderCombo_ = new QComboBox(asrPage);
    asrProviderCombo_->setFixedHeight(32);
    asrProviderCombo_->addItem("腾讯云 ASR", "tencent_asr");
    asrLayout->addWidget(asrProviderCombo_);

    auto *appIdLabel = new QLabel(tr("应用 ID (App ID)"), asrPage);
    appIdLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(appIdLabel);
    tencentAppIdEdit_ = new QLineEdit(asrPage);
    tencentAppIdEdit_->setFixedHeight(32);
    asrLayout->addWidget(tencentAppIdEdit_);

    auto *sidLabel = new QLabel(tr("密钥 ID (Secret ID)"), asrPage);
    sidLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(sidLabel);
    tencentSecretIdEdit_ = new QLineEdit(asrPage);
    tencentSecretIdEdit_->setFixedHeight(32);
    asrLayout->addWidget(tencentSecretIdEdit_);

    auto *skeyLabel = new QLabel(tr("密钥 (Secret Key)"), asrPage);
    skeyLabel->setObjectName("ConfigFieldLabel");
    asrLayout->addWidget(skeyLabel);
    tencentSecretKeyEdit_ = new QLineEdit(asrPage);
    tencentSecretKeyEdit_->setFixedHeight(32);
    tencentSecretKeyEdit_->setEchoMode(QLineEdit::Password);
    asrLayout->addWidget(tencentSecretKeyEdit_);

    asrLayout->addStretch();
    stackedWidget_->addWidget(asrPage);

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

    connect(sidebarList_, &QListWidget::currentRowChanged, stackedWidget_, &QStackedWidget::setCurrentIndex);
    connect(sidebarList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (auto *item = sidebarList_->item(row)) {
            titleRightLabel->setText(item->text());
        }
    });
    sidebarList_->setCurrentRow(0);
}
