# 字幕编辑器工程文件功能实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为字幕编辑器添加工程文件支持，允许用户保存和打开 .sedit 工程文件，同时添加菜单栏和配置页面。

**Architecture:** 采用渐进式实现方案，分 4 个阶段：1) 工程文件序列化/反序列化，2) 菜单栏和标题栏，3) 字幕设置配置页面，4) 自动保存和快捷键。使用 JSON 格式存储工程数据，新增 SubtitleProject 和 ProjectManager 两个核心类。

**Tech Stack:** C++17, Qt6 (Core, Widgets), QJsonDocument, QJsonArray, QJsonObject

---

## 文件结构

### 新建文件

| 文件 | 职责 |
|------|------|
| `include/SubtitleProject.h` | 工程文件序列化/反序列化 |
| `src/SubtitleProject.cpp` | 实现 |
| `include/ProjectManager.h` | 工程管理（保存/加载/自动保存） |
| `src/ProjectManager.cpp` | 实现 |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| `include/SubtitleTrack.h` | 添加 toJsonObject/fromJsonObject 方法 |
| `src/SubtitleTrack.cpp` | 实现序列化方法 |
| `include/AppWindow.h` | 添加菜单栏、标题栏、快捷键 |
| `src/AppWindow.cpp` | 实现菜单和快捷键逻辑 |
| `include/ConfigDialog.h` | 添加"字幕设置"页面 |
| `src/ConfigDialog.cpp` | 实现配置页面 |
| `CMakeLists.txt` | 添加新源文件 |

---

## Task 1: SubtitleTrack 序列化方法

**Files:**
- Modify: `include/SubtitleTrack.h:116`
- Modify: `src/SubtitleTrack.cpp`

- [ ] **Step 1: 在 SubtitleTrack.h 添加序列化方法声明**

在 `SubtitleTrack` 类的 `private` 部分之前添加：

```cpp
  // --- 序列化 ---
  QJsonObject toJsonObject() const;
  void fromJsonObject(const QJsonObject &obj);
```

- [ ] **Step 2: 在 SubtitleTrack.cpp 添加序列化实现**

在文件末尾添加：

```cpp
QJsonObject SubtitleTrack::toJsonObject() const {
  QJsonObject root;

  // 字幕数据
  QJsonArray subtitlesArray;
  for (const auto &item : items_) {
    QJsonObject itemObj;
    itemObj["id"] = item.id;
    itemObj["text"] = item.text;
    itemObj["startMs"] = item.startMs;
    itemObj["endMs"] = item.endMs;
    itemObj["speakerId"] = item.speakerId;

    QJsonObject styleObj;
    styleObj["fontFamily"] = item.fontFamily;
    styleObj["fontSize"] = item.fontSize;
    styleObj["bold"] = item.bold;
    styleObj["italic"] = item.italic;
    styleObj["underline"] = item.underline;
    styleObj["alignment"] = item.alignment;
    itemObj["style"] = styleObj;

    QJsonObject posObj;
    posObj["x"] = item.rectX;
    posObj["y"] = item.rectY;
    posObj["width"] = item.rectW;
    posObj["height"] = item.rectH;
    posObj["rotation"] = item.rotation;
    itemObj["position"] = posObj;

    subtitlesArray.append(itemObj);
  }
  root["subtitles"] = subtitlesArray;

  // 说话人数据
  QJsonArray speakersArray;
  for (auto it = speakers_.constBegin(); it != speakers_.constEnd(); ++it) {
    const auto &speaker = it.value();
    QJsonObject speakerObj;
    speakerObj["id"] = speaker.id;
    speakerObj["name"] = speaker.name;
    speakerObj["bgImageFile"] = speaker.bgImageFile;
    speakerObj["is9Patch"] = speaker.is9Patch;
    speakersArray.append(speakerObj);
  }
  root["speakers"] = speakersArray;

  return root;
}

void SubtitleTrack::fromJsonObject(const QJsonObject &obj) {
  clear();

  // 字幕数据
  QJsonArray subtitlesArray = obj["subtitles"].toArray();
  for (const auto &itemVal : subtitlesArray) {
    QJsonObject itemObj = itemVal.toObject();
    SubtitleItem item;
    item.id = itemObj["id"].toString();
    item.text = itemObj["text"].toString();
    item.startMs = itemObj["startMs"].toVariant().toLongLong();
    item.endMs = itemObj["endMs"].toVariant().toLongLong();
    item.speakerId = itemObj["speakerId"].toInt(-1);

    QJsonObject styleObj = itemObj["style"].toObject();
    item.fontFamily = styleObj["fontFamily"].toString("Arial");
    item.fontSize = styleObj["fontSize"].toInt(24);
    item.bold = styleObj["bold"].toBool(false);
    item.italic = styleObj["italic"].toBool(false);
    item.underline = styleObj["underline"].toBool(false);
    item.alignment = styleObj["alignment"].toInt(0x84);

    QJsonObject posObj = itemObj["position"].toObject();
    item.rectX = posObj["x"].toDouble(0.1);
    item.rectY = posObj["y"].toDouble(0.75);
    item.rectW = posObj["width"].toDouble(0.8);
    item.rectH = posObj["height"].toDouble(0.2);
    item.rotation = posObj["rotation"].toDouble(0.0);

    addItem(item);
  }

  // 说话人数据
  QJsonArray speakersArray = obj["speakers"].toArray();
  for (const auto &speakerVal : speakersArray) {
    QJsonObject speakerObj = speakerVal.toObject();
    SpeakerInfo speaker;
    speaker.id = speakerObj["id"].toInt(-1);
    speaker.name = speakerObj["name"].toString();
    speaker.bgImageFile = speakerObj["bgImageFile"].toString();
    speaker.is9Patch = speakerObj["is9Patch"].toBool(true);
    setSpeakerInfo(speaker.id, speaker);
  }
}
```

- [ ] **Step 3: 添加必要的头文件**

在 `SubtitleTrack.cpp` 顶部添加：

```cpp
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
```

- [ ] **Step 4: 编译测试**

Run: `cmake --build cmake-build-debug`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add include/SubtitleTrack.h src/SubtitleTrack.cpp
git commit -m "feat: 添加 SubtitleTrack 序列化方法"
```

---

## Task 2: SubtitleProject 类

**Files:**
- Create: `include/SubtitleProject.h`
- Create: `src/SubtitleProject.cpp`

- [ ] **Step 1: 创建 SubtitleProject.h**

```cpp
#pragma once

#include "SubtitleTrack.h"
#include <QJsonObject>
#include <QString>
#include <QDateTime>

class SubtitleProject {
public:
  SubtitleProject();

  // 文件操作
  bool load(const QString &filePath);
  bool save(const QString &filePath);
  bool save(const QString &filePath, const SubtitleTrack &track);

  // 获取数据
  QString videoPath() const;
  double videoFps() const;
  QJsonObject settings() const;
  QDateTime created() const;
  QDateTime modified() const;

  // 设置数据
  void setVideoPath(const QString &path);
  void setVideoFps(double fps);
  void setSettings(const QJsonObject &settings);

  // 工具方法
  QString resolveVideoPath(const QString &projectDir) const;
  bool isDirty() const;
  void setDirty(bool dirty);

  // 最近文件
  static QStringList recentFiles();
  static void addRecentFile(const QString &filePath);
  static void clearRecentFiles();

private:
  QString filePath_;
  QString videoPath_;
  double videoFps_ = 25.0;
  QJsonObject settings_;
  QDateTime created_;
  QDateTime modified_;
  bool dirty_ = false;

  static const int kMaxRecentFiles = 10;
};
```

- [ ] **Step 2: 创建 SubtitleProject.cpp**

```cpp
#include "SubtitleProject.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

SubtitleProject::SubtitleProject()
    : created_(QDateTime::currentDateTime()),
      modified_(QDateTime::currentDateTime()) {}

bool SubtitleProject::load(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QByteArray data = file.readAll();
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    return false;
  }

  QJsonObject root = doc.object();

  // 元数据
  created_ = QDateTime::fromString(root["created"].toString(), Qt::ISODate);
  modified_ = QDateTime::fromString(root["modified"].toString(), Qt::ISODate);

  // 视频信息
  QJsonObject videoObj = root["video"].toObject();
  videoPath_ = videoObj["path"].toString();
  videoFps_ = videoObj["fps"].toDouble(25.0);

  // 设置
  settings_ = root["settings"].toObject();

  filePath_ = filePath;
  dirty_ = false;

  return true;
}

bool SubtitleProject::save(const QString &filePath, const SubtitleTrack &track) {
  QJsonObject root;

  // 元数据
  root["version"] = "1.0";
  root["created"] = created_.toString(Qt::ISODate);
  root["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  // 视频信息
  QJsonObject videoObj;
  videoObj["path"] = videoPath_;
  videoObj["fps"] = videoFps_;
  root["video"] = videoObj;

  // 字幕和说话人数据
  QJsonObject trackData = track.toJsonObject();
  root["subtitles"] = trackData["subtitles"];
  root["speakers"] = trackData["speakers"];

  // 设置
  root["settings"] = settings_;

  // 写入文件
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));

  filePath_ = filePath;
  modified_ = QDateTime::currentDateTime();
  dirty_ = false;

  return true;
}

QString SubtitleProject::videoPath() const { return videoPath_; }

double SubtitleProject::videoFps() const { return videoFps_; }

QJsonObject SubtitleProject::settings() const { return settings_; }

QDateTime SubtitleProject::created() const { return created_; }

QDateTime SubtitleProject::modified() const { return modified_; }

void SubtitleProject::setVideoPath(const QString &path) {
  videoPath_ = path;
  dirty_ = true;
}

void SubtitleProject::setVideoFps(double fps) {
  videoFps_ = fps;
  dirty_ = true;
}

void SubtitleProject::setSettings(const QJsonObject &settings) {
  settings_ = settings;
  dirty_ = true;
}

QString SubtitleProject::resolveVideoPath(const QString &projectDir) const {
  if (QFileInfo::exists(videoPath_)) {
    return videoPath_;
  }

  // 尝试相对路径
  QString relativePath = QDir(projectDir).filePath(videoPath_);
  if (QFileInfo::exists(relativePath)) {
    return relativePath;
  }

  return videoPath_;
}

bool SubtitleProject::isDirty() const { return dirty_; }

void SubtitleProject::setDirty(bool dirty) { dirty_ = dirty; }

QStringList SubtitleProject::recentFiles() {
  QSettings settings;
  return settings.value("recentFiles").toStringList();
}

void SubtitleProject::addRecentFile(const QString &filePath) {
  QSettings settings;
  QStringList files = settings.value("recentFiles").toStringList();
  files.removeAll(filePath);
  files.prepend(filePath);

  while (files.size() > kMaxRecentFiles) {
    files.removeLast();
  }

  settings.setValue("recentFiles", files);
}

void SubtitleProject::clearRecentFiles() {
  QSettings settings;
  settings.remove("recentFiles");
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

在 `add_executable` 部分添加新文件：

```cmake
    src/SubtitleProject.cpp
```

- [ ] **Step 4: 编译测试**

Run: `cmake --build cmake-build-debug`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add include/SubtitleProject.h src/SubtitleProject.cpp CMakeLists.txt
git commit -m "feat: 添加 SubtitleProject 类"
```

---

## Task 3: ProjectManager 类

**Files:**
- Create: `include/ProjectManager.h`
- Create: `src/ProjectManager.cpp`

- [ ] **Step 1: 创建 ProjectManager.h**

```cpp
#pragma once

#include "SubtitleProject.h"
#include "SubtitleTrack.h"
#include <QObject>
#include <QTimer>

class ProjectManager : public QObject {
  Q_OBJECT

public:
  explicit ProjectManager(SubtitleTrack *track, QObject *parent = nullptr);

  // 文件操作
  bool newProject();
  bool openProject(const QString &filePath);
  bool saveProject();
  bool saveProjectAs(const QString &filePath);

  // 状态查询
  QString currentFilePath() const;
  QString currentProjectName() const;
  bool hasProject() const;
  bool isDirty() const;

  // 自动保存
  void enableAutoSave(bool enable);
  void setAutoSaveInterval(int seconds);

signals:
  void projectChanged(const QString &filePath);
  void dirtyStateChanged(bool dirty);
  void autoSaveTriggered();

private slots:
  void onAutoSave();

private:
  void updateTitle();
  void markDirty();

  SubtitleTrack *track_;
  SubtitleProject project_;
  QString currentFilePath_;
  bool hasProject_ = false;

  QTimer autoSaveTimer_;
  bool autoSaveEnabled_ = false;
};
```

- [ ] **Step 2: 创建 ProjectManager.cpp**

```cpp
#include "ProjectManager.h"
#include <QDir>
#include <QFileInfo>

ProjectManager::ProjectManager(SubtitleTrack *track, QObject *parent)
    : QObject(parent), track_(track) {
  connect(&autoSaveTimer_, &QTimer::timeout, this, &ProjectManager::onAutoSave);
}

bool ProjectManager::newProject() {
  if (isDirty()) {
    // TODO: 提示用户保存
  }

  track_->clear();
  project_ = SubtitleProject();
  currentFilePath_.clear();
  hasProject_ = false;

  emit projectChanged(QString());
  return true;
}

bool ProjectManager::openProject(const QString &filePath) {
  if (!project_.load(filePath)) {
    return false;
  }

  // 加载字幕数据
  QFile file(filePath);
  if (file.open(QIODevice::ReadOnly)) {
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();
    track_->fromJsonObject(root);
  }

  currentFilePath_ = filePath;
  hasProject_ = true;

  SubtitleProject::addRecentFile(filePath);
  emit projectChanged(filePath);

  return true;
}

bool ProjectManager::saveProject() {
  if (currentFilePath_.isEmpty()) {
    return false;
  }
  return project_.save(currentFilePath_, *track_);
}

bool ProjectManager::saveProjectAs(const QString &filePath) {
  if (!project_.save(filePath, *track_)) {
    return false;
  }

  currentFilePath_ = filePath;
  hasProject_ = true;

  SubtitleProject::addRecentFile(filePath);
  emit projectChanged(filePath);

  return true;
}

QString ProjectManager::currentFilePath() const { return currentFilePath_; }

QString ProjectManager::currentProjectName() const {
  if (currentFilePath_.isEmpty()) {
    return QString();
  }
  return QFileInfo(currentFilePath_).completeBaseName();
}

bool ProjectManager::hasProject() const { return hasProject_; }

bool ProjectManager::isDirty() const { return project_.isDirty(); }

void ProjectManager::enableAutoSave(bool enable) {
  autoSaveEnabled_ = enable;
  if (enable) {
    autoSaveTimer_.start();
  } else {
    autoSaveTimer_.stop();
  }
}

void ProjectManager::setAutoSaveInterval(int seconds) {
  autoSaveTimer_.setInterval(seconds * 1000);
}

void ProjectManager::onAutoSave() {
  if (hasProject_ && !currentFilePath_.isEmpty() && project_.isDirty()) {
    saveProject();
    emit autoSaveTriggered();
  }
}

void ProjectManager::markDirty() {
  project_.setDirty(true);
  emit dirtyStateChanged(true);
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

在 `add_executable` 部分添加新文件：

```cmake
    src/ProjectManager.cpp
```

- [ ] **Step 4: 编译测试**

Run: `cmake --build cmake-build-debug`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add include/ProjectManager.h src/ProjectManager.cpp CMakeLists.txt
git commit -m "feat: 添加 ProjectManager 类"
```

---

## Task 4: 修改 AppWindow 添加菜单栏

**Files:**
- Modify: `include/AppWindow.h`
- Modify: `src/AppWindow.cpp`

- [ ] **Step 1: 在 AppWindow.h 添加成员变量**

在 `Private` 结构体中添加：

```cpp
  // 菜单栏
  QMenuBar *menuBar = nullptr;
  QMenu *fileMenu = nullptr;
  QMenu *editMenu = nullptr;
  QMenu *settingsMenu = nullptr;
  QMenu *helpMenu = nullptr;
  QMenu *recentFilesMenu = nullptr;

  // ProjectManager
  ProjectManager *projectManager = nullptr;
```

在 `AppWindow` 类中添加槽函数声明：

```cpp
private slots:
  void onNewProject();
  void onOpenProject();
  void onSaveProject();
  void onSaveProjectAs();
  void onOpenRecentFile(const QString &filePath);
  void onClearRecentFiles();
  void onExportSrt();
  void onExportAss();
  void onExportTxt();
  void onSelectAll();
  void onDeleteSelected();
  void onAbout();
```

- [ ] **Step 2: 在 AppWindow.cpp 添加菜单栏创建**

在 `setupUi()` 函数中，在 `setCentralWidget(central);` 之后添加：

```cpp
  // 创建菜单栏
  setupMenuBar();

  // 创建 ProjectManager
  d->projectManager = new ProjectManager(d->subtitleTrack, this);
```

添加新函数 `setupMenuBar()`：

```cpp
void AppWindow::setupMenuBar() {
  d->menuBar = new QMenuBar(this);
  setMenuBar(d->menuBar);

  // 文件菜单
  d->fileMenu = d->menuBar->addMenu(tr("文件"));

  QAction *newAction = d->fileMenu->addAction(tr("新建工程"));
  newAction->setShortcut(QKeySequence::New);
  connect(newAction, &QAction::triggered, this, &AppWindow::onNewProject);

  QAction *openAction = d->fileMenu->addAction(tr("打开工程..."));
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &AppWindow::onOpenProject);

  QAction *saveAction = d->fileMenu->addAction(tr("保存工程"));
  saveAction->setShortcut(QKeySequence::Save);
  connect(saveAction, &QAction::triggered, this, &AppWindow::onSaveProject);

  QAction *saveAsAction = d->fileMenu->addAction(tr("另存为..."));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  connect(saveAsAction, &QAction::triggered, this, &AppWindow::onSaveProjectAs);

  d->fileMenu->addSeparator();

  // 最近打开
  d->recentFilesMenu = d->fileMenu->addMenu(tr("最近打开"));
  connect(d->recentFilesMenu, &QMenu::aboutToShow, this, [this]() {
    d->recentFilesMenu->clear();
    QStringList recentFiles = SubtitleProject::recentFiles();
    if (recentFiles.isEmpty()) {
      d->recentFilesMenu->addAction(tr("无最近文件"))->setEnabled(false);
    } else {
      for (const auto &file : recentFiles) {
        QAction *action = d->recentFilesMenu->addAction(
            QFileInfo(file).fileName());
        action->setData(file);
        connect(action, &QAction::triggered, this, [this, action]() {
          onOpenRecentFile(action->data().toString());
        });
      }
      d->recentFilesMenu->addSeparator();
      QAction *clearAction = d->recentFilesMenu->addAction(tr("清除最近"));
      connect(clearAction, &QAction::triggered, this,
              &AppWindow::onClearRecentFiles);
    }
  });

  d->fileMenu->addSeparator();

  // 导出字幕
  QMenu *exportMenu = d->fileMenu->addMenu(tr("导出字幕"));
  QAction *srtAction = exportMenu->addAction("SRT");
  connect(srtAction, &QAction::triggered, this, &AppWindow::onExportSrt);
  QAction *assAction = exportMenu->addAction("ASS");
  connect(assAction, &QAction::triggered, this, &AppWindow::onExportAss);
  QAction *txtAction = exportMenu->addAction("TXT");
  connect(txtAction, &QAction::triggered, this, &AppWindow::onExportTxt);

  d->fileMenu->addSeparator();

  QAction *exitAction = d->fileMenu->addAction(tr("退出"));
  connect(exitAction, &QAction::triggered, this, &QWidget::close);

  // 编辑菜单
  d->editMenu = d->menuBar->addMenu(tr("编辑"));

  QAction *undoAction = d->editMenu->addAction(tr("撤销"));
  undoAction->setShortcut(QKeySequence::Undo);
  undoAction->setEnabled(false); // 预留

  QAction *redoAction = d->editMenu->addAction(tr("重做"));
  redoAction->setShortcut(QKeySequence::Redo);
  redoAction->setEnabled(false); // 预留

  d->editMenu->addSeparator();

  QAction *cutAction = d->editMenu->addAction(tr("剪切"));
  cutAction->setShortcut(QKeySequence::Cut);
  cutAction->setEnabled(false); // 预留

  QAction *copyAction = d->editMenu->addAction(tr("复制"));
  copyAction->setShortcut(QKeySequence::Copy);
  copyAction->setEnabled(false); // 预留

  QAction *pasteAction = d->editMenu->addAction(tr("粘贴"));
  pasteAction->setShortcut(QKeySequence::Paste);
  pasteAction->setEnabled(false); // 预留

  d->editMenu->addSeparator();

  QAction *selectAllAction = d->editMenu->addAction(tr("全选"));
  selectAllAction->setShortcut(QKeySequence::SelectAll);
  connect(selectAllAction, &QAction::triggered, this, &AppWindow::onSelectAll);

  QAction *deleteAction = d->editMenu->addAction(tr("删除选中"));
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this, &AppWindow::onDeleteSelected);

  // 设置菜单
  d->settingsMenu = d->menuBar->addMenu(tr("设置"));

  QAction *configAction = d->settingsMenu->addAction(tr("配置..."));
  connect(configAction, &QAction::triggered, this, [this]() {
    ConfigDialog dlg(this);
    connect(&dlg, &ConfigDialog::configApplied, this,
            &AppWindow::onConfigApplied);
    dlg.exec();
  });

  QMenu *langMenu = d->settingsMenu->addMenu(tr("语言"));
  QAction *zhAction = langMenu->addAction(tr("中文"));
  connect(zhAction, &QAction::triggered, this, []() {
    TranslationManager::instance().loadLanguage("zh_CN");
  });
  QAction *enAction = langMenu->addAction("English");
  connect(enAction, &QAction::triggered, this, []() {
    TranslationManager::instance().loadLanguage("en_US");
  });

  // 帮助菜单
  d->helpMenu = d->menuBar->addMenu(tr("帮助"));

  QAction *aboutAction = d->helpMenu->addAction(tr("关于"));
  connect(aboutAction, &QAction::triggered, this, &AppWindow::onAbout);
}
```

- [ ] **Step 3: 添加槽函数实现**

```cpp
void AppWindow::onNewProject() {
  if (d->projectManager && d->projectManager->isDirty()) {
    int ret = AppMessageBox::question(
        this, tr("确认新建"),
        tr("当前工程有未保存的更改，是否继续新建？"));
    if (ret != AppMessageBox::Yes)
      return;
  }
  if (d->projectManager) {
    d->projectManager->newProject();
  }
  setWindowTitle(tr("字幕编辑"));
}

void AppWindow::onOpenProject() {
  QString filePath = QFileDialog::getOpenFileName(
      this, tr("打开工程"), QString(),
      tr("字幕编辑工程 (*.sedit);;所有文件 (*)"));

  if (filePath.isEmpty())
    return;

  if (d->projectManager && d->projectManager->openProject(filePath)) {
    setWindowTitle(tr("字幕编辑 - %1").arg(d->projectManager->currentProjectName()));
  } else {
    AppMessageBox::critical(this, tr("打开失败"),
                            tr("无法打开工程文件，请检查文件格式。"));
  }
}

void AppWindow::onSaveProject() {
  if (d->projectManager && d->projectManager->currentFilePath().isEmpty()) {
    onSaveProjectAs();
    return;
  }
  if (d->projectManager) {
    d->projectManager->saveProject();
  }
}

void AppWindow::onSaveProjectAs() {
  QString filePath = QFileDialog::getSaveFileName(
      this, tr("另存为"), QString(),
      tr("字幕编辑工程 (*.sedit)"));

  if (filePath.isEmpty())
    return;

  if (!filePath.endsWith(".sedit")) {
    filePath += ".sedit";
  }

  if (d->projectManager && d->projectManager->saveProjectAs(filePath)) {
    setWindowTitle(tr("字幕编辑 - %1").arg(d->projectManager->currentProjectName()));
  } else {
    AppMessageBox::critical(this, tr("保存失败"),
                            tr("无法保存工程文件，请检查磁盘空间。"));
  }
}

void AppWindow::onOpenRecentFile(const QString &filePath) {
  if (d->projectManager && d->projectManager->openProject(filePath)) {
    setWindowTitle(tr("字幕编辑 - %1").arg(d->projectManager->currentProjectName()));
  }
}

void AppWindow::onClearRecentFiles() {
  SubtitleProject::clearRecentFiles();
}

void AppWindow::onExportSrt() {
  if (!d->subtitleTrack)
    return;

  QString filePath = QFileDialog::getSaveFileName(
      this, tr("导出 SRT"), QString(), tr("SRT 字幕 (*.srt)"));

  if (filePath.isEmpty())
    return;

  if (SubtitleExporter::exportToSRT(*d->subtitleTrack, filePath)) {
    AppMessageBox::information(this, tr("导出成功"),
                               tr("字幕已导出到：%1").arg(filePath));
  } else {
    AppMessageBox::critical(this, tr("导出失败"),
                            tr("无法导出字幕文件。"));
  }
}

void AppWindow::onExportAss() {
  if (!d->subtitleTrack)
    return;

  QString filePath = QFileDialog::getSaveFileName(
      this, tr("导出 ASS"), QString(), tr("ASS 字幕 (*.ass)"));

  if (filePath.isEmpty())
    return;

  if (SubtitleExporter::exportToASS(*d->subtitleTrack, filePath)) {
    AppMessageBox::information(this, tr("导出成功"),
                               tr("字幕已导出到：%1").arg(filePath));
  } else {
    AppMessageBox::critical(this, tr("导出失败"),
                            tr("无法导出字幕文件。"));
  }
}

void AppWindow::onExportTxt() {
  if (!d->subtitleTrack)
    return;

  QString filePath = QFileDialog::getSaveFileName(
      this, tr("导出 TXT"), QString(), tr("TXT 文本 (*.txt)"));

  if (filePath.isEmpty())
    return;

  if (SubtitleExporter::exportToTXT(*d->subtitleTrack, filePath)) {
    AppMessageBox::information(this, tr("导出成功"),
                               tr("字幕已导出到：%1").arg(filePath));
  } else {
    AppMessageBox::critical(this, tr("导出失败"),
                            tr("无法导出字幕文件。"));
  }
}

void AppWindow::onSelectAll() {
  if (d->subtitleTrack) {
    QSet<QString> allIds;
    for (const auto &item : d->subtitleTrack->items()) {
      allIds.insert(item.id);
    }
    d->subtitleTrack->setSelectedItems(allIds);
  }
}

void AppWindow::onDeleteSelected() {
  if (!d->subtitleTrack)
    return;

  auto selected = d->subtitleTrack->selectedItem();
  if (selected) {
    d->subtitleTrack->removeItem(selected->id);
  }
}

void AppWindow::onAbout() {
  AppMessageBox::information(
      this, tr("关于"),
      tr("字幕编辑器 v1.0\n\n一个简单易用的视频字幕编辑工具。"));
}
```

- [ ] **Step 4: 添加必要的头文件**

在 `AppWindow.cpp` 顶部添加：

```cpp
#include <QFileDialog>
#include <QMenuBar>
#include <QMenu>
#include <QKeySequence>
```

- [ ] **Step 5: 编译测试**

Run: `cmake --build cmake-build-debug`
Expected: 编译成功

- [ ] **Step 6: 提交**

```bash
git add include/AppWindow.h src/AppWindow.cpp
git commit -m "feat: 添加菜单栏和标题栏"
```

---

## Task 5: 添加"字幕设置"配置页面

**Files:**
- Modify: `include/ConfigDialog.h`
- Modify: `src/ConfigDialog.cpp`

- [ ] **Step 1: 在 ConfigDialog.h 添加成员变量**

```cpp
  // 字幕设置页面
  QComboBox *subtitleFontFamilyCombo_;
  QSpinBox *subtitleFontSizeSpin_;
  QCheckBox *subtitleBoldCheck_;
  QCheckBox *subtitleItalicCheck_;
  QCheckBox *subtitleUnderlineCheck_;
  QComboBox *subtitleAlignmentCombo_;
  QDoubleSpinBox *subtitleRectXSpin_;
  QDoubleSpinBox *subtitleRectYSpin_;
  QDoubleSpinBox *subtitleRectWSpin_;
  QDoubleSpinBox *subtitleRectHSpin_;
  QDoubleSpinBox *subtitleRotationSpin_;
  QLineEdit *speakerBgFolderEdit_;
  QPushButton *speakerBgFolderBtn_;
  QSpinBox *speakerMarginLeftSpin_;
  QSpinBox *speakerMarginTopSpin_;
  QSpinBox *speakerMarginRightSpin_;
  QSpinBox *speakerMarginBottomSpin_;

  QLabel *subtitleFontFamilyLabel_;
  QLabel *subtitleFontSizeLabel_;
  QLabel *subtitleBoldLabel_;
  QLabel *subtitleItalicLabel_;
  QLabel *subtitleUnderlineLabel_;
  QLabel *subtitleAlignmentLabel_;
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
```

- [ ] **Step 2: 在 ConfigDialog.cpp 添加页面创建**

在 `setupUi()` 函数中，在 ASR 页面之后添加：

```cpp
  // ------------------------------------------------------------------------
  // 字幕设置页面
  // ------------------------------------------------------------------------
  auto *subtitlePage = new QWidget();
  auto *subLayout = new QVBoxLayout(subtitlePage);
  subLayout->setContentsMargins(30, 25, 30, 30);
  subLayout->setSpacing(15);

  // 默认字体样式
  auto *fontStyleLabel = new QLabel(tr("默认字体样式"), subtitlePage);
  fontStyleLabel->setObjectName("ConfigSectionLabel");
  subLayout->addWidget(fontStyleLabel);

  auto *fontFamilyLabel = new QLabel(tr("字体族"), subtitlePage);
  subtitleFontFamilyLabel_ = fontFamilyLabel;
  fontFamilyLabel->setObjectName("ConfigFieldLabel");
  subLayout->addWidget(fontFamilyLabel);

  subtitleFontFamilyCombo_ = new QComboBox(subtitlePage);
  subtitleFontFamilyCombo_->setFixedHeight(32);
  QFontDatabase fontDb;
  for (const auto &family : fontDb.families()) {
    subtitleFontFamilyCombo_->addItem(family);
  }
  subLayout->addWidget(subtitleFontFamilyCombo_);

  auto *fontSizeLabel = new QLabel(tr("字号"), subtitlePage);
  subtitleFontSizeLabel_ = fontSizeLabel;
  fontSizeLabel->setObjectName("ConfigFieldLabel");
  subLayout->addWidget(fontSizeLabel);

  subtitleFontSizeSpin_ = new QSpinBox(subtitlePage);
  subtitleFontSizeSpin_->setRange(8, 72);
  subtitleFontSizeSpin_->setValue(24);
  subLayout->addWidget(subtitleFontSizeSpin_);

  auto *styleLayout = new QHBoxLayout();
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
  subLayout->addLayout(styleLayout);

  auto *alignmentLabel = new QLabel(tr("对齐方式"), subtitlePage);
  subtitleAlignmentLabel_ = alignmentLabel;
  alignmentLabel->setObjectName("ConfigFieldLabel");
  subLayout->addWidget(alignmentLabel);

  subtitleAlignmentCombo_ = new QComboBox(subtitlePage);
  subtitleAlignmentCombo_->setFixedHeight(32);
  subtitleAlignmentCombo_->addItem(tr("左对齐"), 0x81);
  subtitleAlignmentCombo_->addItem(tr("居中"), 0x84);
  subtitleAlignmentCombo_->addItem(tr("右对齐"), 0x82);
  subLayout->addWidget(subtitleAlignmentCombo_);

  // 默认排版位置
  auto *positionLabel = new QLabel(tr("默认排版位置"), subtitlePage);
  positionLabel->setObjectName("ConfigSectionLabel");
  subLayout->addWidget(positionLabel);

  auto *rectLayout = new QGridLayout();
  subtitleRectXLabel_ = new QLabel(tr("X:"), subtitlePage);
  rectLayout->addWidget(subtitleRectXLabel_, 0, 0);
  subtitleRectXSpin_ = new QDoubleSpinBox(subtitlePage);
  subtitleRectXSpin_->setRange(0.0, 1.0);
  subtitleRectXSpin_->setSingleStep(0.05);
  subtitleRectXSpin_->setValue(0.1);
  rectLayout->addWidget(subtitleRectXSpin_, 0, 1);

  subtitleRectYLabel_ = new QLabel(tr("Y:"), subtitlePage);
  rectLayout->addWidget(subtitleRectYLabel_, 0, 2);
  subtitleRectYSpin_ = new QDoubleSpinBox(subtitlePage);
  subtitleRectYSpin_->setRange(0.0, 1.0);
  subtitleRectYSpin_->setSingleStep(0.05);
  subtitleRectYSpin_->setValue(0.75);
  rectLayout->addWidget(subtitleRectYSpin_, 0, 3);

  subtitleRectWLabel_ = new QLabel(tr("宽度:"), subtitlePage);
  rectLayout->addWidget(subtitleRectWLabel_, 1, 0);
  subtitleRectWSpin_ = new QDoubleSpinBox(subtitlePage);
  subtitleRectWSpin_->setRange(0.0, 1.0);
  subtitleRectWSpin_->setSingleStep(0.05);
  subtitleRectWSpin_->setValue(0.8);
  rectLayout->addWidget(subtitleRectWSpin_, 1, 1);

  subtitleRectHLabel_ = new QLabel(tr("高度:"), subtitlePage);
  rectLayout->addWidget(subtitleRectHLabel_, 1, 2);
  subtitleRectHSpin_ = new QDoubleSpinBox(subtitlePage);
  subtitleRectHSpin_->setRange(0.0, 1.0);
  subtitleRectHSpin_->setSingleStep(0.05);
  subtitleRectHSpin_->setValue(0.2);
  rectLayout->addWidget(subtitleRectHSpin_, 1, 3);
  subLayout->addLayout(rectLayout);

  auto *rotationLayout = new QHBoxLayout();
  subtitleRotationLabel_ = new QLabel(tr("旋转:"), subtitlePage);
  rotationLayout->addWidget(subtitleRotationLabel_);
  subtitleRotationSpin_ = new QDoubleSpinBox(subtitlePage);
  subtitleRotationSpin_->setRange(-180.0, 180.0);
  subtitleRotationSpin_->setSingleStep(1.0);
  subtitleRotationSpin_->setValue(0.0);
  subtitleRotationSpin_->setSuffix("°");
  rotationLayout->addWidget(subtitleRotationSpin_);
  rotationLayout->addStretch();
  subLayout->addLayout(rotationLayout);

  // 说话人设置
  auto *speakerLabel = new QLabel(tr("说话人设置"), subtitlePage);
  speakerLabel->setObjectName("ConfigSectionLabel");
  subLayout->addWidget(speakerLabel);

  speakerBgFolderLabel_ = new QLabel(tr("背景图文件夹"), subtitlePage);
  speakerBgFolderLabel_->setObjectName("ConfigFieldLabel");
  subLayout->addWidget(speakerBgFolderLabel_);

  auto *folderLayout = new QHBoxLayout();
  speakerBgFolderEdit_ = new QLineEdit(subtitlePage);
  folderLayout->addWidget(speakerBgFolderEdit_);
  speakerBgFolderBtn_ = new QPushButton(tr("浏览..."), subtitlePage);
  connect(speakerBgFolderBtn_, &QPushButton::clicked, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("选择背景图文件夹"), speakerBgFolderEdit_->text());
    if (!dir.isEmpty()) {
      speakerBgFolderEdit_->setText(dir);
      checkDirtyState();
    }
  });
  folderLayout->addWidget(speakerBgFolderBtn_);
  subLayout->addLayout(folderLayout);

  auto *marginLabel = new QLabel(tr("九宫格边距"), subtitlePage);
  marginLabel->setObjectName("ConfigFieldLabel");
  subLayout->addWidget(marginLabel);

  auto *marginLayout = new QGridLayout();
  speakerMarginLeftLabel_ = new QLabel(tr("左:"), subtitlePage);
  marginLayout->addWidget(speakerMarginLeftLabel_, 0, 0);
  speakerMarginLeftSpin_ = new QSpinBox(subtitlePage);
  speakerMarginLeftSpin_->setRange(0, 100);
  speakerMarginLeftSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginLeftSpin_, 0, 1);

  speakerMarginTopLabel_ = new QLabel(tr("上:"), subtitlePage);
  marginLayout->addWidget(speakerMarginTopLabel_, 0, 2);
  speakerMarginTopSpin_ = new QSpinBox(subtitlePage);
  speakerMarginTopSpin_->setRange(0, 100);
  speakerMarginTopSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginTopSpin_, 0, 3);

  speakerMarginRightLabel_ = new QLabel(tr("右:"), subtitlePage);
  marginLayout->addWidget(speakerMarginRightLabel_, 1, 0);
  speakerMarginRightSpin_ = new QSpinBox(subtitlePage);
  speakerMarginRightSpin_->setRange(0, 100);
  speakerMarginRightSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginRightSpin_, 1, 1);

  speakerMarginBottomLabel_ = new QLabel(tr("下:"), subtitlePage);
  marginLayout->addWidget(speakerMarginBottomLabel_, 1, 2);
  speakerMarginBottomSpin_ = new QSpinBox(subtitlePage);
  speakerMarginBottomSpin_->setRange(0, 100);
  speakerMarginBottomSpin_->setValue(15);
  marginLayout->addWidget(speakerMarginBottomSpin_, 1, 3);
  subLayout->addLayout(marginLayout);

  subLayout->addStretch();
  stackedWidget_->addWidget(subtitlePage);
```

在侧边栏添加项目：

```cpp
  sidebarList_->addItem(tr("字幕设置"));
```

- [ ] **Step 3: 在 loadConfig() 添加加载逻辑**

```cpp
  // 字幕设置
  subtitleFontFamilyCombo_->setCurrentText(
      cfg.getString("subtitle", "fontFamily", "Arial"));
  subtitleFontSizeSpin_->setValue(
      cfg.getInt("subtitle", "fontSize", 24));
  subtitleBoldCheck_->setChecked(
      cfg.getBool("subtitle", "bold", false));
  subtitleItalicCheck_->setChecked(
      cfg.getBool("subtitle", "italic", false));
  subtitleUnderlineCheck_->setChecked(
      cfg.getBool("subtitle", "underline", false));
  subtitleAlignmentCombo_->setCurrentIndex(
      subtitleAlignmentCombo_->findData(
          cfg.getInt("subtitle", "alignment", 0x84)));
  subtitleRectXSpin_->setValue(
      cfg.getDouble("subtitle", "rectX", 0.1));
  subtitleRectYSpin_->setValue(
      cfg.getDouble("subtitle", "rectY", 0.75));
  subtitleRectWSpin_->setValue(
      cfg.getDouble("subtitle", "rectW", 0.8));
  subtitleRectHSpin_->setValue(
      cfg.getDouble("subtitle", "rectH", 0.2));
  subtitleRotationSpin_->setValue(
      cfg.getDouble("subtitle", "rotation", 0.0));
  speakerBgFolderEdit_->setText(
      cfg.getString("speaker", "bgFolder"));
  speakerMarginLeftSpin_->setValue(
      cfg.getInt("speaker", "marginLeft", 15));
  speakerMarginTopSpin_->setValue(
      cfg.getInt("speaker", "marginTop", 15));
  speakerMarginRightSpin_->setValue(
      cfg.getInt("speaker", "marginRight", 15));
  speakerMarginBottomSpin_->setValue(
      cfg.getInt("speaker", "marginBottom", 15));
```

- [ ] **Step 4: 在 saveConfig() 添加保存逻辑**

```cpp
  // 字幕设置
  cfg.setValue("subtitle", "fontFamily",
               subtitleFontFamilyCombo_->currentText());
  cfg.setValue("subtitle", "fontSize", subtitleFontSizeSpin_->value());
  cfg.setValue("subtitle", "bold", subtitleBoldCheck_->isChecked());
  cfg.setValue("subtitle", "italic", subtitleItalicCheck_->isChecked());
  cfg.setValue("subtitle", "underline", subtitleUnderlineCheck_->isChecked());
  cfg.setValue("subtitle", "alignment",
               subtitleAlignmentCombo_->currentData().toInt());
  cfg.setValue("subtitle", "rectX", subtitleRectXSpin_->value());
  cfg.setValue("subtitle", "rectY", subtitleRectYSpin_->value());
  cfg.setValue("subtitle", "rectW", subtitleRectWSpin_->value());
  cfg.setValue("subtitle", "rectH", subtitleRectHSpin_->value());
  cfg.setValue("subtitle", "rotation", subtitleRotationSpin_->value());
  cfg.setValue("speaker", "bgFolder", speakerBgFolderEdit_->text());
  cfg.setValue("speaker", "marginLeft", speakerMarginLeftSpin_->value());
  cfg.setValue("speaker", "marginTop", speakerMarginTopSpin_->value());
  cfg.setValue("speaker", "marginRight", speakerMarginRightSpin_->value());
  cfg.setValue("speaker", "marginBottom", speakerMarginBottomSpin_->value());
```

- [ ] **Step 5: 在 retranslateUi() 添加翻译**

```cpp
  // 字幕设置页面
  if (auto *item = sidebarList_->item(3))
    item->setText(tr("字幕设置"));

  subtitleFontFamilyLabel_->setText(tr("字体族"));
  subtitleFontSizeLabel_->setText(tr("字号"));
  subtitleBoldLabel_->setText(tr("粗体"));
  subtitleItalicLabel_->setText(tr("斜体"));
  subtitleUnderlineLabel_->setText(tr("下划线"));
  subtitleAlignmentLabel_->setText(tr("对齐方式"));
  subtitleAlignmentCombo_->setItemText(0, tr("左对齐"));
  subtitleAlignmentCombo_->setItemText(1, tr("居中"));
  subtitleAlignmentCombo_->setItemText(2, tr("右对齐"));
  subtitleRectXLabel_->setText(tr("X:"));
  subtitleRectYLabel_->setText(tr("Y:"));
  subtitleRectWLabel_->setText(tr("宽度:"));
  subtitleRectHLabel_->setText(tr("高度:"));
  subtitleRotationLabel_->setText(tr("旋转:"));
  speakerBgFolderLabel_->setText(tr("背景图文件夹"));
  speakerBgFolderBtn_->setText(tr("浏览..."));
  speakerMarginLeftLabel_->setText(tr("左:"));
  speakerMarginTopLabel_->setText(tr("上:"));
  speakerMarginRightLabel_->setText(tr("右:"));
  speakerMarginBottomLabel_->setText(tr("下:"));
```

- [ ] **Step 6: 编译测试**

Run: `cmake --build cmake-build-debug`
Expected: 编译成功

- [ ] **Step 7: 提交**

```bash
git add include/ConfigDialog.h src/ConfigDialog.cpp
git commit -m "feat: 添加字幕设置配置页面"
```

---

## Task 6: 集成自动保存

**Files:**
- Modify: `src/AppWindow.cpp`

- [ ] **Step 1: 在 AppWindow 构造函数中启用自动保存**

在 `setupMenuBar()` 调用之后添加：

```cpp
  // 启用自动保存
  d->projectManager->enableAutoSave(true);
  d->projectManager->setAutoSaveInterval(60); // 1 分钟
```

- [ ] **Step 2: 连接字幕修改信号**

在构造函数中添加：

```cpp
  // 连接字幕修改信号到 ProjectManager
  connect(d->subtitleTrack, &SubtitleTrack::dataChanged, this, [this]() {
    if (d->projectManager) {
      d->projectManager->setDirty(true);
    }
  });
```

- [ ] **Step 3: 连接自动保存信号**

```cpp
  connect(d->projectManager, &ProjectManager::autoSaveTriggered, this,
          [this]() {
            statusBar()->showMessage(tr("工程已自动保存"), 2000);
          });
```

- [ ] **Step 4: 编译测试**

Run: `cmake --build cmake-build-debug`
Expected: 编译成功

- [ ] **Step 5: 提交**

```bash
git add src/AppWindow.cpp
git commit -m "feat: 集成自动保存功能"
```

---

## Task 7: 动态主题和多语言支持

**Files:**
- Modify: `src/AppWindow.cpp`
- Modify: `src/ConfigDialog.cpp`

- [ ] **Step 1: 在 AppWindow 添加 changeEvent 处理**

```cpp
void AppWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QMainWindow::changeEvent(event);
}

void AppWindow::retranslateUi() {
  setWindowTitle(tr("字幕编辑"));
  // 菜单栏会自动更新，因为使用了 tr()
}
```

- [ ] **Step 2: 在 ConfigDialog 添加 changeEvent 处理**

确保 `retranslateUi()` 方法正确更新所有标签文本。

- [ ] **Step 3: 编译测试**

Run: `cmake --build cmake-build-debug`
Expected: 编译成功

- [ ] **Step 4: 测试主题切换**

手动测试：切换主题，确认所有 UI 元素正确更新。

- [ ] **Step 5: 测试语言切换**

手动测试：切换语言，确认所有菜单和标签正确更新。

- [ ] **Step 6: 提交**

```bash
git add src/AppWindow.cpp src/ConfigDialog.cpp
git commit -m "feat: 完善动态主题和多语言支持"
```

---

## 验证清单

完成所有任务后，验证以下功能：

1. **工程文件保存/加载**
   - [ ] 新建工程并保存
   - [ ] 打开已保存的工程
   - [ ] 验证字幕数据完整保存
   - [ ] 验证说话人信息保存

2. **菜单栏**
   - [ ] 所有菜单项正常显示
   - [ ] 快捷键正常工作
   - [ ] 最近文件列表正常

3. **字幕设置**
   - [ ] 配置页面正常显示
   - [ ] 保存设置到 config.ini
   - [ ] 动态主题切换正常
   - [ ] 动态语言切换正常

4. **自动保存**
   - [ ] 修改字幕后自动保存
   - [ ] 备份文件正确创建

5. **错误处理**
   - [ ] 打开不存在的视频文件显示警告
   - [ ] 打开损坏的工程文件显示错误
