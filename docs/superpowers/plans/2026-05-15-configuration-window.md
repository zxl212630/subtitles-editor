# Configuration Window Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a native configuration dialog with dirty-state management, theme/language infrastructure, and ASR/OSS provider settings.

**Architecture:** We will extend `ConfigManager` to support saving values. We'll set up `QTest` infrastructure for TDD. `ConfigDialog` will use `QStackedWidget` for category pages and a persistent footer for "Unsaved changes" status. Theme switching will use dynamically loaded QSS, and language switching will use `QTranslator`.

**Tech Stack:** C++17, Qt6 (Widgets, Test, Core), CMake.

---

### Task 1: Setup Test Infrastructure (QTest)

The project currently lacks a test suite. We must set it up to enable TDD.

**Files:**
- Modify: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `tests/TestConfigManager.cpp`

- [ ] **Step 1: Write the failing test for ConfigManager**

```cpp
// tests/TestConfigManager.cpp
#include <QtTest>
#include "ConfigManager.h"

class TestConfigManager : public QObject {
    Q_OBJECT
private slots:
    void testSetAndGetValue() {
        ConfigManager::instance().setValue("test_group", "test_key", "hello_world");
        QCOMPARE(ConfigManager::instance().getString("test_group", "test_key"), QString("hello_world"));
    }
};

QTEST_MAIN(TestConfigManager)
#include "TestConfigManager.moc"
```

- [ ] **Step 2: Add tests to CMake build system**

Modify `CMakeLists.txt` (append to end):
```cmake
# ============================================================
# Tests
# ============================================================
enable_testing()
add_subdirectory(tests)
```

Create `tests/CMakeLists.txt`:
```cmake
find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(TestConfigManager TestConfigManager.cpp ../src/ConfigManager.cpp)
target_include_directories(TestConfigManager PRIVATE ../include)
target_link_libraries(TestConfigManager PRIVATE Qt::Core Qt::Test)
add_test(NAME TestConfigManager COMMAND TestConfigManager)
```

- [ ] **Step 3: Run test to verify it fails to compile**

Run: `cmake -B build && cmake --build build --target TestConfigManager`
Expected: FAIL (no `setValue` member in `ConfigManager`).

- [ ] **Step 4: Write minimal implementation in ConfigManager**

Modify `include/ConfigManager.h` (add public methods):
```cpp
  // ... existing public methods
  void setValue(const QString &group, const QString &key, const QVariant &value);
  void sync();
  
  QString theme() const;
  QString language() const;
```

Modify `src/ConfigManager.cpp`:
```cpp
// Add new methods
void ConfigManager::setValue(const QString &group, const QString &key, const QVariant &value) {
    settings_.beginGroup(group);
    settings_.setValue(key, value);
    settings_.endGroup();
}

void ConfigManager::sync() {
    settings_.sync();
}

QString ConfigManager::theme() const {
    return getString("general", "theme");
}

QString ConfigManager::language() const {
    return getString("general", "language");
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build --target TestConfigManager && ctest --test-dir build -R TestConfigManager -V`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt tests/ include/ConfigManager.h src/ConfigManager.cpp
git commit -m "test: setup QTest and add setValue to ConfigManager"
```

---

### Task 2: Theme and Language Infrastructure

**Files:**
- Create: `resources/themes/dark.qss`
- Create: `resources/themes/light.qss`
- Create: `translations/zh_CN.ts`
- Modify: `resources/resources.qrc`
- Modify: `src/main.cpp`
- Modify: `src/AppWindow.cpp`

- [ ] **Step 1: Create QSS files**

```css
/* resources/themes/dark.qss */
QMainWindow { background-color: #151515; }
QFrame#TitleBar { background-color: #262626; border: none; }
QLabel { background: transparent; }
/* Title label specifically */
QLabel#AppTitleLabel {
    color: #9ca3af;
    font-family: Inter, sans-serif;
    font-size: 12px;
    font-weight: normal;
}
QSplitter::handle { background-color: #0a0a0a; }
QWidget#CentralWidget { background-color: #0a0a0a; }
```

```css
/* resources/themes/light.qss */
/* Placeholder for light theme */
```

- [ ] **Step 2: Create placeholder translation file**

```xml
<!-- translations/zh_CN.ts -->
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
</TS>
```

- [ ] **Step 3: Add to resources.qrc**

Modify `resources/resources.qrc` (inside `<qresource>`):
```xml
        <file>themes/dark.qss</file>
        <file>themes/light.qss</file>
```

- [ ] **Step 4: Load theme in AppWindow**

Modify `src/AppWindow.cpp`. Remove hardcoded `setStyleSheet` calls for QMainWindow, TitleBar, QSplitter, etc. and load from QRC instead. 

Add to `AppWindow::setupUi()`:
```cpp
    d->titleLabel->setObjectName("AppTitleLabel");
    central->setObjectName("CentralWidget");

    QString theme = ConfigManager::instance().theme();
    if (theme.isEmpty()) theme = "dark";
    
    QFile f(QString(":/themes/%1.qss").arg(theme));
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(QTextStream(&f).readAll());
    }
```

- [ ] **Step 5: Add Settings SVG icon**

Create `resources/icons/settings.svg`:
```svg
<svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
  <path d="M12 15C13.6569 15 15 13.6569 15 12C15 10.3431 13.6569 9 12 9C10.3431 9 9 10.3431 9 12C9 13.6569 10.3431 15 12 15Z" stroke="#D1D5DB" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z" stroke="#D1D5DB" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
```
Add `<file>icons/settings.svg</file>` to `resources/resources.qrc`.

- [ ] **Step 6: Build and Commit**

Run: `cmake --build build`
```bash
git add resources/ src/AppWindow.cpp translations/
git commit -m "feat: infrastructure for dynamic themes and i18n"
```

---

### Task 3: ConfigDialog Skeleton & General Page

**Files:**
- Create: `include/ConfigDialog.h`
- Create: `src/ConfigDialog.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write UI logic in `ConfigDialog.h`**

```cpp
#pragma once
#include <QDialog>
#include <QVariantMap>

class QListWidget;
class QStackedWidget;
class QComboBox;
class QLabel;
class QPushButton;

class ConfigDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigDialog(QWidget *parent = nullptr);
    ~ConfigDialog() override = default;

private slots:
    void onApply();
    void onOk();
    void onCancel();
    void checkDirtyState();

private:
    void setupUi();
    void loadConfig();
    void saveConfig();
    bool isDirty() const;

    QListWidget *sidebarList_;
    QStackedWidget *stackedWidget_;
    
    // General Page
    QComboBox *langCombo_;
    QComboBox *themeCombo_;

    // Footer
    QLabel *dirtyLabel_;
    QPushButton *btnApply_;
    QPushButton *btnOk_;
    QPushButton *btnCancel_;

    QVariantMap initialConfig_;
};
```

- [ ] **Step 2: Add to CMakeLists.txt**

Add `include/ConfigDialog.h` to `HEADERS` and `src/ConfigDialog.cpp` to `SOURCES`.

- [ ] **Step 3: Implement ConfigDialog.cpp Setup**

```cpp
#include "ConfigDialog.h"
#include "ConfigManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QComboBox>
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
}
```

- [ ] **Step 4: Implement Logic**

```cpp
void ConfigDialog::loadConfig() {
    auto &cfg = ConfigManager::instance();
    initialConfig_["language"] = cfg.language().isEmpty() ? "zh_CN" : cfg.language();
    initialConfig_["theme"] = cfg.theme().isEmpty() ? "dark" : cfg.theme();

    langCombo_->setCurrentIndex(langCombo_->findData(initialConfig_["language"]));
    themeCombo_->setCurrentIndex(themeCombo_->findData(initialConfig_["theme"]));
}

bool ConfigDialog::isDirty() const {
    return (langCombo_->currentData().toString() != initialConfig_["language"]) ||
           (themeCombo_->currentData().toString() != initialConfig_["theme"]);
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
        if (reply == QMessageBox::Cancel) { return; }
    }
    reject();
}
```

- [ ] **Step 5: Build and Commit**

Run: `cmake -B build && cmake --build build`
```bash
git add include/ConfigDialog.h src/ConfigDialog.cpp CMakeLists.txt
git commit -m "feat: setup ConfigDialog with General page and dirty state"
```

---

### Task 4: Storage & ASR Pages

**Files:**
- Modify: `include/ConfigDialog.h`
- Modify: `src/ConfigDialog.cpp`

- [ ] **Step 1: Add Fields to Header**

```cpp
    // In ConfigDialog.h private section:
    // Storage Page
    QComboBox *storageProviderCombo_;
    QLineEdit *ossBucketEdit_;
    QLineEdit *ossRegionEdit_;
    QLineEdit *ossAccessKeyEdit_;
    QLineEdit *ossSecretKeyEdit_;

    // ASR Page
    QComboBox *asrProviderCombo_;
    QLineEdit *tencentAppIdEdit_;
    QLineEdit *tencentSecretIdEdit_;
    QLineEdit *tencentSecretKeyEdit_;
```

- [ ] **Step 2: Setup Storage and ASR UI**

Modify `ConfigDialog::setupUi`:
```cpp
    // Add to sidebar
    sidebarList_->addItem(tr("存储"));
    sidebarList_->addItem(tr("语音识别"));

    // Storage Page
    auto *storagePage = new QWidget();
    auto *storageLayout = new QVBoxLayout(storagePage);
    storageLayout->setAlignment(Qt::AlignTop);
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

    // Connect textChanged to checkDirtyState
    auto lineEdits = {ossBucketEdit_, ossRegionEdit_, ossAccessKeyEdit_, ossSecretKeyEdit_, tencentAppIdEdit_, tencentSecretIdEdit_, tencentSecretKeyEdit_};
    for(auto *le : lineEdits) {
        connect(le, &QLineEdit::textChanged, this, &ConfigDialog::checkDirtyState);
    }
```

- [ ] **Step 3: Update `loadConfig`, `saveConfig`, and `isDirty`**

Update `loadConfig`:
```cpp
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
```

Update `isDirty` to include checking all line edits against `initialConfig_`.

Update `saveConfig`:
```cpp
    cfg.setValue("aliyun_oss", "bucket", ossBucketEdit_->text());
    cfg.setValue("aliyun_oss", "region", ossRegionEdit_->text());
    cfg.setValue("aliyun_oss", "access_key_id", ossAccessKeyEdit_->text());
    cfg.setValue("aliyun_oss", "access_key_secret", ossSecretKeyEdit_->text());

    cfg.setValue("tencent_asr", "app_id", tencentAppIdEdit_->text());
    cfg.setValue("tencent_asr", "secret_id", tencentSecretIdEdit_->text());
    cfg.setValue("tencent_asr", "secret_key", tencentSecretKeyEdit_->text());
```

- [ ] **Step 4: Build and Commit**

Run: `cmake --build build`
```bash
git commit -am "feat: implement Storage and ASR pages in ConfigDialog"
```

---

### Task 5: AppWindow Integration

**Files:**
- Modify: `src/AppWindow.cpp`
- Modify: `include/AppWindow.h`

- [ ] **Step 1: Add Settings Button to Title Bar**

Modify `src/AppWindow.cpp` in `setupTitleBar()`:
```cpp
    auto *rightLayout = new QHBoxLayout();
    rightLayout->setSpacing(4);
    rightLayout->setContentsMargins(0,0,0,0);

    auto *btnSettings = new QPushButton(d->titleBar);
    btnSettings->setIcon(QIcon(":/icons/settings.svg"));
    btnSettings->setFixedSize(28, 28);
    btnSettings->setStyleSheet("QPushButton { border: none; background: transparent; } QPushButton:hover { background: #3b3b3b; border-radius: 4px; }");
    rightLayout->addWidget(btnSettings);

    layout->addLayout(rightLayout);
    
    connect(btnSettings, &QPushButton::clicked, this, &AppWindow::onSettingsRequested);
```

- [ ] **Step 2: Implement Slot**

In `AppWindow.h`:
```cpp
  void onSettingsRequested();
```

In `AppWindow.cpp`:
```cpp
#include "ConfigDialog.h"

void AppWindow::onSettingsRequested() {
    ConfigDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // Handle post-save updates (e.g. prompt for restart if language changed)
    }
}
```

- [ ] **Step 3: Run and Commit**

Run: `cmake --build build`
```bash
git commit -am "feat: integrate ConfigDialog into AppWindow"
```
