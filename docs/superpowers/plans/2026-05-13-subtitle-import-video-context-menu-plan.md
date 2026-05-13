# 字幕导入 + 视频轨道右键菜单 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 SRT 字幕导入、视频轨道右键菜单（属性/ASR）、ASR 前置覆盖确认

**Architecture:** 复用现有 `SubtitleTrack` 数据模型和信号体系，`srtparser.h` 单头文件解析 SRT，`TimelinePanel` 负责拖拽分流和右键菜单触发，`AppWindow` 集中处理业务逻辑和弹窗

**Tech Stack:** C++17, Qt6 Widgets, FFmpeg, srtparser.h

---

## 文件结构

| 文件 | 操作 | 职责 |
|---|---|---|
| `include/srtparser.h` | 创建 | 外部 SRT 解析库（单头文件） |
| `include/VideoPropertyDialog.h` | 创建 | 视频属性弹窗头文件 |
| `src/VideoPropertyDialog.cpp` | 创建 | 视频属性弹窗实现 |
| `include/TimelinePanel.h` | 修改 | 新增信号、getter、contextMenuEvent 声明 |
| `src/TimelinePanel.cpp` | 修改 | 拖拽分流、右键菜单、路径存储 |
| `include/AppWindow.h` | 修改 | 新增私有槽函数声明 |
| `src/AppWindow.cpp` | 修改 | 信号连接、SRT 导入、ASR 触发、属性弹窗 |
| `include/MediaPlayer.h` | 修改 | 新增视频元数据 getter |
| `src/MediaPlayer.cpp` | 修改 | 实现视频元数据 getter |

---

## Task 1: 复制 srtparser.h

**Files:**
- Create: `include/srtparser.h`

- [ ] **Step 1: 复制外部库到项目**

Run: `cp /Users/zxl/Projects/Mgtv/flowcut/src/srtparser.h include/srtparser.h`

- [ ] **Step 2: 提交**

```bash
git add include/srtparser.h
git commit -m "chore: add srtparser.h for SRT subtitle parsing"
```

---

## Task 2: 创建 VideoPropertyDialog

**Files:**
- Create: `include/VideoPropertyDialog.h`
- Create: `src/VideoPropertyDialog.cpp`

- [ ] **Step 1: 编写头文件**

`include/VideoPropertyDialog.h`：
- 继承 `QDialog`
- 构造函数接收 `QMap<QString, QString> properties`
- `setupUi()` 私有方法构建暗色键值对布局

- [ ] **Step 2: 编写实现**

`src/VideoPropertyDialog.cpp`：
- 窗口标题 `"视频属性"`
- 背景色 `#1e1e1e`
- 标题 label：14px 粗体 `#d1d5db`
- 分割线 `#333333`
- 遍历 properties，每行：左侧 key（100px 固定宽，`#9ca3af`，右对齐），右侧 value（`#d1d5db`，可换行）
- 底部 `QDialogButtonBox::Ok` 按钮，样式贴合主题

- [ ] **Step 3: 提交**

```bash
git add include/VideoPropertyDialog.h src/VideoPropertyDialog.cpp
git commit -m "feat: add VideoPropertyDialog for displaying video metadata"
```

---

## Task 3: 修改 TimelinePanel.h

**Files:**
- Modify: `include/TimelinePanel.h`

- [ ] **Step 1: signals 段添加三个新信号**

```cpp
  void subtitleFileDropped(const QString &path);
  void videoAsrRequested();
  void videoPropertyRequested();
```

- [ ] **Step 2: public 段添加 getter**

```cpp
  qint64 totalDuration() const { return totalDurationMs_; }
  QString mediaFilePath() const { return mediaFilePath_; }
```

- [ ] **Step 3: protected 段添加 contextMenuEvent 声明**

```cpp
  void contextMenuEvent(QContextMenuEvent *event) override;
```

- [ ] **Step 4: private 段末尾添加成员变量**

```cpp
  QString mediaFilePath_;  // 视频完整路径
```

- [ ] **Step 5: 提交**

```bash
git add include/TimelinePanel.h
git commit -m "feat(timeline): add signals, getters, and context menu declaration"
```

---

## Task 4: 修改 TimelinePanel.cpp — 路径存储和拖拽分流

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: 修改 setMediaFilePath 存储完整路径**

```cpp
void TimelinePanel::setMediaFilePath(const QString &path) {
  mediaFilePath_ = path;
  QFileInfo info(path);
  mediaFileName_ = info.fileName();
}
```

- [ ] **Step 2: 修改 dropEvent 按扩展名分流**

将原来的 `emit mediaFileDropped(localPath)` 替换为：

```cpp
  QString ext = QFileInfo(localPath).suffix().toLower();
  if (ext == "srt") {
    emit subtitleFileDropped(localPath);
  } else {
    emit mediaFileDropped(localPath);
  }
```

同时删除 `TODO: ASR temporarily disabled` 注释块。

- [ ] **Step 3: 提交**

```bash
git add src/TimelinePanel.cpp
git commit -m "feat(timeline): store full path and dispatch drag by file extension"
```

---

## Task 5: 修改 TimelinePanel.cpp — 右键菜单

**Files:**
- Modify: `src/TimelinePanel.cpp`

- [ ] **Step 1: 顶部添加 `#include <QMenu>`**

- [ ] **Step 2: 添加 contextMenuEvent 实现**

```cpp
void TimelinePanel::contextMenuEvent(QContextMenuEvent *event) {
  if (totalDurationMs_ <= 0 || mediaFilePath_.isEmpty())
    return;

  int y = event->pos().y();
  int videoTrackY = RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT;
  if (y < videoTrackY || y >= videoTrackY + VIDEO_TRACK_HEIGHT)
    return;

  int x = event->pos().x();
  int videoX = timeToX(0);
  int videoEndX = timeToX(totalDurationMs_);
  if (x < videoX || x > videoEndX)
    return;

  QMenu menu(this);
  menu.setStyleSheet(R"(
      QMenu {
          background-color: #1e1e1e;
          border: 1px solid #333333;
          padding: 4px;
      }
      QMenu::item {
          color: #d1d5db;
          padding: 8px 24px;
          font-size: 13px;
      }
      QMenu::item:selected {
          background-color: #2a2a2a;
      }
  )");

  QAction *propAction = menu.addAction("属性");
  QAction *asrAction = menu.addAction("智能语音识别");

  QAction *selected = menu.exec(event->globalPos());
  if (selected == propAction) {
    emit videoPropertyRequested();
  } else if (selected == asrAction) {
    emit videoAsrRequested();
  }
}
```

- [ ] **Step 3: 提交**

```bash
git add src/TimelinePanel.cpp
git commit -m "feat(timeline): add right-click context menu on video clip"
```

---

## Task 6: 修改 AppWindow.h

**Files:**
- Modify: `include/AppWindow.h`

- [ ] **Step 1: private 段添加三个槽函数声明**

```cpp
  void onSubtitleFileDropped(const QString &path);
  void onVideoAsrRequested();
  void onVideoPropertyRequested();
```

- [ ] **Step 2: 提交**

```bash
git add include/AppWindow.h
git commit -m "feat(app): add slot declarations for subtitle/ASR/property handlers"
```

---

## Task 7: 修改 AppWindow.cpp — 信号连接和导入对话框

**Files:**
- Modify: `src/AppWindow.cpp`

- [ ] **Step 1: 顶部添加 include**

```cpp
#include "VideoPropertyDialog.h"
#include "srtparser.h"
```

- [ ] **Step 2: 修改导入按钮的文件对话框**

找到 `importVideoBtn` 的 clicked lambda（约 148 行）：

过滤器改为：
```cpp
"媒体文件 (*.mp4 *.mkv *.avi *.mov *.srt);;视频文件 (*.mp4 *.mkv *.avi *.mov);;字幕文件 (*.srt);;所有文件 (*)"
```

lambda 体改为：
```cpp
            if (!path.isEmpty()) {
              QString ext = QFileInfo(path).suffix().toLower();
              if (ext == "srt") {
                onSubtitleFileDropped(path);
              } else if (d->mediaPlayer) {
                d->timelinePanel->setMediaFilePath(path);
                d->mediaPlayer->load(path);
              }
            }
```

- [ ] **Step 3: 连接 TimelinePanel 新信号**

在 `setupSplitterLayout()` 末尾（现有连接之后）添加：
```cpp
  // 12. TimelinePanel subtitle drop -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::subtitleFileDropped, this,
          &AppWindow::onSubtitleFileDropped);

  // 13. TimelinePanel video ASR -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::videoAsrRequested, this,
          &AppWindow::onVideoAsrRequested);

  // 14. TimelinePanel video property -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::videoPropertyRequested, this,
          &AppWindow::onVideoPropertyRequested);
```

- [ ] **Step 4: 提交**

```bash
git add src/AppWindow.cpp include/AppWindow.h
git commit -m "feat(app): connect timeline signals and update import dialog filter"
```

---

## Task 8: 实现 onSubtitleFileDropped

**Files:**
- Modify: `src/AppWindow.cpp`

- [ ] **Step 1: 在文件末尾添加实现**

```cpp
void AppWindow::onSubtitleFileDropped(const QString &path) {
  if (!d->subtitleTrack)
    return;

  if (!d->subtitleTrack->items().isEmpty()) {
    int ret = QMessageBox::question(
        this, "确认覆盖",
        "字幕轨道已有内容，继续导入将清空现有字幕，是否继续？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
      return;
    d->subtitleTrack->clear();
  }

  try {
    SubtitleParserFactory parserFactory(path.toStdString());
    SubtitleParser *parser = parserFactory.getParser();
    auto subtitles = parser->getSubtitles();

    qint64 maxEndMs = 0;
    for (auto *sub : subtitles) {
      if (!sub)
        continue;
      SubtitleItem item;
      item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      item.text = QString::fromStdString(sub->getText());
      item.startMs = static_cast<qint64>(sub->getStartTime());
      item.endMs = static_cast<qint64>(sub->getEndTime());
      d->subtitleTrack->addItem(item);
      if (item.endMs > maxEndMs)
        maxEndMs = item.endMs;
    }

    delete parser;

    if (maxEndMs > 0 && d->timelinePanel) {
      d->timelinePanel->setTotalDuration(
          qMax(d->timelinePanel->totalDuration(), maxEndMs));
    }

    if (d->mediaPlayer)
      d->mediaPlayer->seek(0);

  } catch (...) {
    QMessageBox::critical(this, "字幕文件格式错误",
                          "无法解析字幕文件，请检查文件格式。");
  }
}
```

- [ ] **Step 2: 提交**

```bash
git add src/AppWindow.cpp
git commit -m "feat(app): implement SRT subtitle import with overwrite confirmation"
```

---

## Task 9: 修改 MediaPlayer — 添加元数据 getter

**Files:**
- Modify: `include/MediaPlayer.h`
- Modify: `src/MediaPlayer.cpp`

- [ ] **Step 1: MediaPlayer.h 添加声明**

```cpp
  QSize videoSize() const;
  qint64 durationMs() const;
  QString videoCodecName() const;
  int audioSampleRate() const;
  int audioChannels() const;
```

- [ ] **Step 2: MediaPlayer.cpp 添加实现**

```cpp
QSize MediaPlayer::videoSize() const {
  return decoder_ ? decoder_->videoSize() : QSize();
}

qint64 MediaPlayer::durationMs() const {
  return decoder_ ? decoder_->durationMs() : 0;
}

QString MediaPlayer::videoCodecName() const {
  return decoder_ ? decoder_->videoCodecName() : QString();
}

int MediaPlayer::audioSampleRate() const {
  return decoder_ ? decoder_->audioSampleRate() : 0;
}

int MediaPlayer::audioChannels() const {
  return decoder_ ? decoder_->audioChannels() : 0;
}
```

**注意：** `FFmpegDecoder` 目前可能没有 `videoCodecName()`。需要同步添加。在 `FFmpegDecoder.h` 添加声明，在 `FFmpegDecoder.cpp` 的 `open()` 中读取 `videoCodecCtx_->codec->name` 存入 `videoCodecName_` 成员。

- [ ] **Step 3: 提交**

```bash
git add include/MediaPlayer.h src/MediaPlayer.cpp include/FFmpegDecoder.h src/FFmpegDecoder.cpp
git commit -m "feat(mediaplayer): add video metadata getters for property dialog"
```

---

## Task 10: 实现 onVideoAsrRequested

**Files:**
- Modify: `src/AppWindow.cpp`

- [ ] **Step 1: 在 onSubtitleFileDropped 之后添加**

```cpp
void AppWindow::onVideoAsrRequested() {
  if (!d->subtitleTrack || !d->timelinePanel)
    return;

  QString videoPath = d->timelinePanel->mediaFilePath();
  if (videoPath.isEmpty())
    return;

  if (!d->subtitleTrack->items().isEmpty()) {
    int ret = QMessageBox::question(
        this, "确认覆盖",
        "字幕轨道已有内容，语音识别将清空现有字幕，是否继续？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
      return;
  }

  d->timelinePanel->startAsrPipeline(videoPath);
}
```

- [ ] **Step 2: 提交**

```bash
git add src/AppWindow.cpp
git commit -m "feat(app): implement ASR trigger from video context menu with confirmation"
```

---

## Task 11: 实现 onVideoPropertyRequested

**Files:**
- Modify: `src/AppWindow.cpp`

- [ ] **Step 1: 在 onVideoAsrRequested 之后添加**

```cpp
void AppWindow::onVideoPropertyRequested() {
  if (!d->mediaPlayer || !d->timelinePanel)
    return;

  QString path = d->timelinePanel->mediaFilePath();
  if (path.isEmpty())
    return;

  QFileInfo fi(path);
  QMap<QString, QString> props;
  props.insert("文件路径", path);
  props.insert("文件大小",
               QString("%1 MB").arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 2));

  QSize size = d->mediaPlayer->videoSize();
  if (size.isValid())
    props.insert("分辨率", QString("%1x%2").arg(size.width()).arg(size.height()));

  double fps = d->mediaPlayer->decoderFps();
  if (fps > 0.0)
    props.insert("帧率", QString("%1 fps").arg(fps, 0, 'f', 2));

  qint64 duration = d->mediaPlayer->durationMs();
  if (duration > 0)
    props.insert("时长", QTime::fromMSecsSinceStartOfDay(static_cast<int>(duration))
                          .toString("hh:mm:ss.zzz"));

  QString codec = d->mediaPlayer->videoCodecName();
  if (!codec.isEmpty())
    props.insert("视频编码", codec);

  int sampleRate = d->mediaPlayer->audioSampleRate();
  if (sampleRate > 0)
    props.insert("音频采样率", QString("%1 Hz").arg(sampleRate));

  int channels = d->mediaPlayer->audioChannels();
  if (channels > 0)
    props.insert("音频通道", QString("%1").arg(channels));

  VideoPropertyDialog dialog(props, this);
  dialog.exec();
}
```

- [ ] **Step 2: 提交**

```bash
git add src/AppWindow.cpp
git commit -m "feat(app): implement video property dialog from context menu"
```

---

## Task 12: 编译验证

**Files:**
- All modified files

- [ ] **Step 1: 格式化代码**

```bash
clang-format -i include/*.h src/*.cpp
```

- [ ] **Step 2: 编译**

```bash
cmake --build cmake-build-debug
```

Expected: 编译成功，无新增 warning（除现有已知的 QFontDatabase deprecated）。

- [ ] **Step 3: 提交（如编译通过）**

```bash
git add -A
git commit -m "style: format code for subtitle import and context menu feature"
```

---

## Self-Review Checklist

### Spec 覆盖度

| Spec 需求 | 对应 Task |
|---|---|
| SRT 字幕导入 | Task 1, 8 |
| 拖拽分流（视频/srt） | Task 4 |
| Empty State 点击导入 srt | Task 7 |
| 导入前覆盖确认 | Task 8, 10 |
| 仅清空字幕轨道 | Task 8, 10（只调 `subtitleTrack->clear()`） |
| 字幕导入后同步 UI | Task 8（`SubtitleTrack::dataChanged()` 自动触发） |
| 无视频时以字幕时长为总时长 | Task 8（`setTotalDuration(qMax(...))`） |
| 播放头跳到 0 | Task 8 |
| SRT 解析失败弹窗 | Task 8 |
| 视频轨道右键菜单 | Task 5 |
| 右键触发区域 = clip 条形区 | Task 5（检查 x/y 范围） |
| 属性弹窗 | Task 2, 11 |
| 属性字段（8 个） | Task 11 |
| ASR 前置确认 | Task 10 |

### Placeholder 扫描
- 无 TBD / TODO
- 无 "implement later"
- 所有代码已给出或明确描述

### 类型一致性
- `mediaFilePath_` 在 TimelinePanel.h 定义，TimelinePanel.cpp 使用，AppWindow.cpp 通过 `mediaFilePath()` getter 访问
- `totalDuration()` getter 返回 `qint64`，与 `totalDurationMs_` 类型一致
