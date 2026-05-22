# Dynamic Subtitle Background Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为字幕编辑器实现动态字幕背景图功能。根据说话人（Speaker）的不同，在视频预览中为字幕贴上不同的背景图。包含 ASR 说话人识别、说话人管理对话框、字幕列表内联说话人切换、九宫格/固定长度背景图渲染等完整链路。

**Design Spec:** `docs/superpowers/specs/2026-05-23-dynamic-subtitle-bg-design.md`

**Architecture:**
1. 扩展数据模型（`SubtitleItem`, `AsrServiceBase::TranscriptSegment`）加入 `speakerId` 字段。
2. 在 `SubtitleTrack` 中管理 `SpeakerInfo` 映射表和全局统一设置（背景图文件夹、四向九宫格边距），统一设置持久化到 `config.ini`。
3. 扩展 `SubtitleListModel` 以暴露 `SpeakerIdRole`。
4. 修改 `SubtitleListDelegate` 在时间码与文本之间新增说话人列；修改 `SubtitleListPanel` 表头同步对齐。
5. 创建 `SpeakerManagerDialog` 说话人管理对话框（动态主题支持）。
6. 修改 `SoftwareVideoRenderer` 增加背景图缓存与九宫格/固定长度绘制逻辑。
7. 修改 `VideoPreviewPanel` 在字幕叠加时传递说话人背景配置给渲染器。
8. 扩展 `TencentAsrService` 开启说话人分离并解析 SpeakerId。

**Tech Stack:** C++17, Qt 6 (QDialog, QPainter, QSettings, QFileDialog, QDir)

---

### Task 1: 扩展数据模型 — SubtitleItem 与 TranscriptSegment

**Files:**
- Modify: `include/SubtitleItem.h`
- Modify: `include/AsrServiceBase.h`

- [ ] **Step 1: 在 SubtitleItem 中添加 speakerId 字段**

Modify `include/SubtitleItem.h`:
```cpp
#pragma once

#include <QString>

struct SubtitleItem {
  QString id;            // UUID
  QString text;          // 字幕文本
  qint64 startMs = 0;    // 开始时间（毫秒）
  qint64 endMs = 0;      // 结束时间（毫秒）
  bool selected = false; // 选中状态
  int speakerId = -1;    // 说话人 ID (-1 表示未分配)
};
```

- [ ] **Step 2: 在 TranscriptSegment 中添加 speakerId 字段**

Modify `include/AsrServiceBase.h`，在 `TranscriptSegment` 结构体中添加：
```cpp
  struct TranscriptSegment {
    QString text;
    qint64 startMs = 0;
    qint64 endMs = 0;
    int speakerId = -1;  // 说话人 ID (来自 ASR 的 SpeakerDiarization)
  };
```

- [ ] **Step 3: Commit**
```bash
git add include/SubtitleItem.h include/AsrServiceBase.h
git commit -m "feat(speaker): add speakerId field to SubtitleItem and TranscriptSegment"
```

---

### Task 2: 扩展 SubtitleTrack — SpeakerInfo 管理与统一设置持久化

**Files:**
- Modify: `include/SubtitleTrack.h`
- Modify: `src/SubtitleTrack.cpp`

- [ ] **Step 1: 在头文件中定义 SpeakerInfo 结构体与新增接口**

Modify `include/SubtitleTrack.h`：
```cpp
#pragma once

#include "SubtitleItem.h"
#include <QList>
#include <QMap>
#include <QMargins>
#include <QObject>
#include <QString>

struct SpeakerInfo {
  int id = -1;
  QString name;
  QString bgImageFile;   // 文件名 (如 "alice.png")，配合 globalBgFolder_ 使用
  bool is9Patch = true;
};

class SubtitleTrack : public QObject {
  Q_OBJECT

public:
  explicit SubtitleTrack(QObject *parent = nullptr);

  // --- 原有接口保持不变 ---
  void clear();
  void addItem(const SubtitleItem &item);
  void removeItem(const QString &id);
  void updateItem(const QString &id, const SubtitleItem &newItem);
  void selectItem(const QString &id);
  const QList<SubtitleItem> &items() const;
  const SubtitleItem *selectedItem() const;
  const SubtitleItem *findItem(const QString &id) const;
  void splitItem(const QString &id, int cursorPosition = -1,
                 const QString &currentText = QString());
  void mergeItems(const QString &id1, const QString &id2);
  void addGapItem(qint64 startMs, qint64 endMs);

  // --- 说话人管理接口 ---
  void setSpeakerInfo(int id, const SpeakerInfo &info);
  SpeakerInfo speakerInfo(int id) const;
  QList<SpeakerInfo> allSpeakers() const;
  void clearSpeakers();
  void autoRegisterSpeaker(int speakerId);

  // --- 全局统一设置 ---
  QString globalBgFolder() const;
  void setGlobalBgFolder(const QString &path);
  QMargins unifiedBorderMargins() const;
  void setUnifiedBorderMargins(const QMargins &margins);

  // --- 持久化（仅保存全局统一设置到 config.ini）---
  void loadGlobalSettings();
  void saveGlobalSettings();

signals:
  void itemAdded(const SubtitleItem &item);
  void itemRemoved(const QString &id);
  void itemUpdated(const QString &id);
  void itemSelected(const QString &id);
  void dataChanged();
  void speakersChanged();

private:
  QList<SubtitleItem> items_;
  QString selectedId_;
  QMap<int, SpeakerInfo> speakers_;
  QString globalBgFolder_;
  QMargins unifiedBorderMargins_{15, 15, 15, 15};
};
```

- [ ] **Step 2: 实现说话人管理和统一设置持久化**

Modify `src/SubtitleTrack.cpp`，在文件末尾添加新方法实现（原有方法保持不变）：
```cpp
// ---- Speaker management ----

void SubtitleTrack::setSpeakerInfo(int id, const SpeakerInfo &info) {
  speakers_[id] = info;
  speakers_[id].id = id;
  emit speakersChanged();
}

SpeakerInfo SubtitleTrack::speakerInfo(int id) const {
  return speakers_.value(id);
}

QList<SpeakerInfo> SubtitleTrack::allSpeakers() const {
  return speakers_.values();
}

void SubtitleTrack::clearSpeakers() {
  speakers_.clear();
  emit speakersChanged();
}

void SubtitleTrack::autoRegisterSpeaker(int speakerId) {
  if (speakerId >= 0 && !speakers_.contains(speakerId)) {
    SpeakerInfo info;
    info.id = speakerId;
    info.name = QString("Speaker %1").arg(speakerId);
    speakers_[speakerId] = info;
    emit speakersChanged();
  }
}

// ---- Global settings ----

QString SubtitleTrack::globalBgFolder() const { return globalBgFolder_; }

void SubtitleTrack::setGlobalBgFolder(const QString &path) {
  globalBgFolder_ = path;
}

QMargins SubtitleTrack::unifiedBorderMargins() const {
  return unifiedBorderMargins_;
}

void SubtitleTrack::setUnifiedBorderMargins(const QMargins &margins) {
  unifiedBorderMargins_ = margins;
}

void SubtitleTrack::loadGlobalSettings() {
  QSettings settings;
  globalBgFolder_ = settings.value("speaker/bgFolder").toString();
  int left = settings.value("speaker/marginLeft", 15).toInt();
  int top = settings.value("speaker/marginTop", 15).toInt();
  int right = settings.value("speaker/marginRight", 15).toInt();
  int bottom = settings.value("speaker/marginBottom", 15).toInt();
  unifiedBorderMargins_ = QMargins(left, top, right, bottom);
}

void SubtitleTrack::saveGlobalSettings() {
  QSettings settings;
  settings.setValue("speaker/bgFolder", globalBgFolder_);
  settings.setValue("speaker/marginLeft", unifiedBorderMargins_.left());
  settings.setValue("speaker/marginTop", unifiedBorderMargins_.top());
  settings.setValue("speaker/marginRight", unifiedBorderMargins_.right());
  settings.setValue("speaker/marginBottom", unifiedBorderMargins_.bottom());
}
```

注意：需要在 `SubtitleTrack.cpp` 顶部添加 `#include <QSettings>`。

- [ ] **Step 3: Commit**
```bash
git add include/SubtitleTrack.h src/SubtitleTrack.cpp
git commit -m "feat(speaker): add SpeakerInfo management and global settings to SubtitleTrack"
```

---

### Task 3: 扩展 SubtitleListModel — 添加 SpeakerIdRole

**Files:**
- Modify: `include/SubtitleListModel.h`
- Modify: `src/SubtitleListModel.cpp`

- [ ] **Step 1: 在 Roles 枚举中添加 SpeakerIdRole**

Modify `include/SubtitleListModel.h`：
```cpp
  enum Roles {
    IdRole = Qt::UserRole + 1,
    TextRole,
    StartMsRole,
    EndMsRole,
    SelectedRole,
    StartTimeRole,
    EndTimeRole,
    SpeakerIdRole  // 新增
  };
```

- [ ] **Step 2: 在 data() 和 setData() 中支持 SpeakerIdRole**

Modify `src/SubtitleListModel.cpp`：

在 `data()` 的 switch 中添加：
```cpp
  case SpeakerIdRole:
    return item.speakerId;
```

在 `setData()` 中添加对 `SpeakerIdRole` 的处理（在现有 `if (role == Qt::EditRole || role == TextRole)` 块之前）：
```cpp
  if (role == SpeakerIdRole) {
    SubtitleItem newItem = item;
    newItem.speakerId = value.toInt();
    disconnect(track_, &SubtitleTrack::dataChanged, this,
               &SubtitleListModel::onDataChanged);
    track_->updateItem(item.id, newItem);
    connect(track_, &SubtitleTrack::dataChanged, this,
            &SubtitleListModel::onDataChanged);
    emit dataChanged(index, index, {SpeakerIdRole});
    return true;
  }
```

在 `roleNames()` 中添加：
```cpp
  roles[SpeakerIdRole] = "speakerId";
```

- [ ] **Step 3: Commit**
```bash
git add include/SubtitleListModel.h src/SubtitleListModel.cpp
git commit -m "feat(speaker): add SpeakerIdRole to SubtitleListModel"
```

---

### Task 4: 修改 SubtitleListDelegate — 新增说话人列绘制与下拉菜单

**Files:**
- Modify: `include/SubtitleListDelegate.h`
- Modify: `src/SubtitleListDelegate.cpp`

- [ ] **Step 1: 在头文件中添加新的信号和方法**

Modify `include/SubtitleListDelegate.h`：
- 添加 `class SubtitleTrack;` 前向声明
- 添加 `void setTrack(SubtitleTrack *track);` 公有方法
- 添加 `QRect speakerRect(const QStyleOptionViewItem &option) const;` 公有方法
- 添加信号 `void speakerChangeRequested(const QString &id, int newSpeakerId);`
- 添加信号 `void manageSpeakersRequested();`
- 添加私有成员 `SubtitleTrack *track_ = nullptr;`

```cpp
signals:
  void deleteClicked(const QString &id);
  void splitClicked(const QString &id, int cursorPosition);
  void splitClickedWithData(const QString &id, int cursorPosition,
                            const QString &text);
  void speakerChangeRequested(const QString &id, int newSpeakerId);
  void manageSpeakersRequested();
```

- [ ] **Step 2: 修改 paint() 实现四列布局 — 在时间码和文本之间插入说话人药丸标签列**

Modify `src/SubtitleListDelegate.cpp` 的 `paint()` 方法：

核心改动：
```cpp
  // Timecode area (left, 宽 100)
  QRect timeRect(rect.left() + 12, rect.top() + 10, 100, 36);
  painter->setPen(QColor("#858e9f"));
  QFont timeFont = painter->font();
  timeFont.setFamily("Inter");
  timeFont.setPointSize(11);
  painter->setFont(timeFont);
  painter->drawText(timeRect.left(), timeRect.top() + 12, startTime);
  painter->drawText(timeRect.left(), timeRect.top() + 26, endTime);

  // Speaker pill label (新增，宽 80)
  QRect spkRect(timeRect.right() + 12, rect.top(), 80, rect.height());
  {
    int speakerId = index.data(SubtitleListModel::SpeakerIdRole).toInt();
    QString speakerLabel;
    if (speakerId >= 0 && track_) {
      SpeakerInfo info = track_->speakerInfo(speakerId);
      speakerLabel = info.name.isEmpty()
                         ? QString("Speaker %1").arg(speakerId)
                         : info.name;
    } else {
      speakerLabel = tr("未分配");
    }
    // 绘制圆角药丸背景
    QFont spkFont = painter->font();
    spkFont.setPointSize(9);
    painter->setFont(spkFont);
    QFontMetrics fm(spkFont);
    int textW = fm.horizontalAdvance(speakerLabel) + 16;
    int pillW = qMin(textW, 76);
    int pillH = 20;
    int pillX = spkRect.left() + (spkRect.width() - pillW) / 2;
    int pillY = spkRect.top() + (spkRect.height() - pillH) / 2;
    QRect pillRect(pillX, pillY, pillW, pillH);

    QColor primary = ThemeManager::instance().getPrimaryColor();
    primary.setAlpha(speakerId >= 0 ? 50 : 25);
    painter->setPen(Qt::NoPen);
    painter->setBrush(primary);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawRoundedRect(pillRect, pillH / 2, pillH / 2);

    QColor pillTextColor = speakerId >= 0
                               ? ThemeManager::instance().getPrimaryColor()
                               : QColor("#6b7280");
    painter->setPen(pillTextColor);
    painter->drawText(pillRect, Qt::AlignCenter, speakerLabel);
  }

  // Text area (middle) — 起点变更为 spkRect.right() + 12
  int textLeft = spkRect.right() + 12;
  int textRight = rect.right() - 12 - 36 - 12;
  QRect textRect(textLeft, rect.top(), qMax(50, textRight - textLeft),
                 rect.height());
  // ... 后续绘制不变 ...
```

- [ ] **Step 3: 实现 speakerRect() 方法**

```cpp
QRect SubtitleListDelegate::speakerRect(
    const QStyleOptionViewItem &option) const {
  const QRect rect = option.rect;
  QRect timeRect(rect.left() + 12, rect.top() + 10, 100, 36);
  return QRect(timeRect.right() + 12, rect.top(), 80, rect.height());
}
```

- [ ] **Step 4: 修改 updateEditorGeometry 的 textLeft 对齐**

Modify `updateEditorGeometry()`：
```cpp
void SubtitleListDelegate::updateEditorGeometry(
    QWidget *editor, const QStyleOptionViewItem &option,
    const QModelIndex & /*index*/) const {
  const QRect rect = option.rect;
  QRect timeRect(rect.left() + 12, rect.top() + 10, 100, 36);
  QRect spkRect(timeRect.right() + 12, rect.top(), 80, rect.height());
  int textLeft = spkRect.right() + 12;
  int textRight = rect.right() - 12 - 36 - 12;
  int textWidth = qMax(50, textRight - textLeft);
  QRect editRect(textLeft, rect.top() + 8, textWidth, rect.height() - 16);
  editor->setGeometry(editRect);
}
```

- [ ] **Step 5: 实现 setTrack() 与说话人点击菜单逻辑**

在 `SubtitleListPanel::eventFilter` 中，检测鼠标点击位置是否在 `speakerRect` 区域内：
- 如是，弹出 `QMenu`，包含已配置的说话人、`未分配`、`+ 新建说话人...`、`⚙️ 管理说话人...`。
- 选择后通过 `SubtitleListModel::setData(... SpeakerIdRole ...)` 修改。

这部分逻辑放在 Task 6（SubtitleListPanel）中实现更合适，因为 Panel 拥有 eventFilter。

- [ ] **Step 6: Commit**
```bash
git add include/SubtitleListDelegate.h src/SubtitleListDelegate.cpp
git commit -m "feat(speaker): add speaker column rendering and pill label to SubtitleListDelegate"
```

---

### Task 5: 修改 SubtitleListPanel — 表头对齐与说话人下拉菜单

**Files:**
- Modify: `include/SubtitleListPanel.h`
- Modify: `src/SubtitleListPanel.cpp`

- [ ] **Step 1: 在头文件中添加 headerSpeaker_ 成员与新方法**

Modify `include/SubtitleListPanel.h`：
```cpp
  // 在 private 成员中添加：
  QLabel *headerSpeaker_ = nullptr;
  void showSpeakerMenu(const QModelIndex &index, const QPoint &globalPos);
```

- [ ] **Step 2: 修改 setupUi() — 表头添加 Speaker 列并调整对齐**

Modify `src/SubtitleListPanel.cpp` 的 `setupUi()` 中的表头部分：

原有的 `hlLayout` spacing 从 `80` 改为 `12`：
```cpp
  hlLayout->setSpacing(12);
```

将 `headerTime_` 固定宽度设为 100：
```cpp
  headerTime_ = new QLabel(tr("Timecode"), headerLeft);
  headerTime_->setObjectName("SubtitleHeaderLabel");
  headerTime_->setFixedWidth(100);
  hlLayout->addWidget(headerTime_);
```

在 `headerTime_` 和 `headerText_` 之间插入 `headerSpeaker_`：
```cpp
  headerSpeaker_ = new QLabel(tr("Speaker"), headerLeft);
  headerSpeaker_->setObjectName("SubtitleHeaderLabel");
  headerSpeaker_->setFixedWidth(80);
  hlLayout->addWidget(headerSpeaker_);

  headerText_ = new QLabel(tr("Subtitle"), headerLeft);
  headerText_->setObjectName("SubtitleHeaderLabel");
  hlLayout->addWidget(headerText_);
```

- [ ] **Step 3: 在 setTrack() 中将 track 传递给 delegate**

```cpp
void SubtitleListPanel::setTrack(SubtitleTrack *track) {
  track_ = track;
  model_->setTrack(track);
  delegate_->setTrack(track);  // 新增
}
```

- [ ] **Step 4: 在 eventFilter 中添加说话人列点击检测与 QMenu 弹出**

在 `SubtitleListPanel::eventFilter` 方法中，检测鼠标点击事件：

```cpp
// 在 eventFilter 的 MouseButtonRelease 处理中增加：
if (event->type() == QEvent::MouseButtonRelease) {
  auto *me = static_cast<QMouseEvent *>(event);
  QModelIndex index = listView_->indexAt(me->pos());
  if (index.isValid()) {
    QStyleOptionViewItem option;
    option.initFrom(listView_);
    option.rect = listView_->visualRect(index);
    QRect spkRect = delegate_->speakerRect(option);
    if (spkRect.contains(me->pos())) {
      showSpeakerMenu(index, me->globalPosition().toPoint());
      return true;
    }
  }
}
```

- [ ] **Step 5: 实现 showSpeakerMenu() 方法**

```cpp
void SubtitleListPanel::showSpeakerMenu(const QModelIndex &index,
                                        const QPoint &globalPos) {
  if (!track_) return;

  QMenu menu(this);
  // 未分配
  QAction *unassignAction = menu.addAction(tr("未分配"));
  unassignAction->setData(-1);

  menu.addSeparator();

  // 已配置的说话人
  for (const auto &spk : track_->allSpeakers()) {
    QString label = spk.name.isEmpty()
                        ? QString("Speaker %1").arg(spk.id)
                        : QString("%1 (%2)").arg(spk.name).arg(spk.id);
    QAction *action = menu.addAction(label);
    action->setData(spk.id);
  }

  menu.addSeparator();
  QAction *newAction = menu.addAction(tr("+ 新建说话人..."));
  QAction *manageAction = menu.addAction(tr("⚙️ 管理说话人..."));

  QAction *chosen = menu.exec(globalPos);
  if (!chosen) return;

  if (chosen == manageAction) {
    // 弹出 SpeakerManagerDialog（Task 7）
    // SpeakerManagerDialog dlg(track_, this);
    // dlg.exec();
    return;
  }
  if (chosen == newAction) {
    // 新建一个说话人，使用下一个可用 ID
    int nextId = 0;
    for (const auto &spk : track_->allSpeakers()) {
      if (spk.id >= nextId) nextId = spk.id + 1;
    }
    track_->autoRegisterSpeaker(nextId);
    model_->setData(index, nextId, SubtitleListModel::SpeakerIdRole);
    return;
  }

  // 选择了某个说话人或未分配
  int speakerId = chosen->data().toInt();
  model_->setData(index, speakerId, SubtitleListModel::SpeakerIdRole);
}
```

- [ ] **Step 6: 在 retranslateUi() 中添加 headerSpeaker_ 翻译**

```cpp
  if (headerSpeaker_)
    headerSpeaker_->setText(tr("Speaker"));
```

- [ ] **Step 7: Commit**
```bash
git add include/SubtitleListPanel.h src/SubtitleListPanel.cpp
git commit -m "feat(speaker): add Speaker column header and inline speaker menu to SubtitleListPanel"
```

---

### Task 6: 扩展 SoftwareVideoRenderer — 背景图缓存与九宫格/固定长度绘制

**Files:**
- Modify: `include/SoftwareVideoRenderer.h`
- Modify: `src/SoftwareVideoRenderer.cpp`

- [ ] **Step 1: 在头文件中添加背景图相关接口与成员**

Modify `include/SoftwareVideoRenderer.h`：
```cpp
  // 在 public 区域添加：
  void setSubtitleBg(const QString &imagePath, bool is9Patch,
                     const QMargins &margins);
  void clearSubtitleBg();

  // 在 private 区域添加：
  QString bgImagePath_;
  bool bgIs9Patch_ = false;
  QMargins bgMargins_;
  QHash<QString, QImage> bgCache_;
  QMutex bgMutex_;

  void drawNinePatch(QPainter &painter, const QImage &src,
                     const QRect &target, const QMargins &margins);
```

- [ ] **Step 2: 实现 setSubtitleBg / clearSubtitleBg**

```cpp
void SoftwareVideoRenderer::setSubtitleBg(const QString &imagePath,
                                           bool is9Patch,
                                           const QMargins &margins) {
  {
    QMutexLocker lock(&bgMutex_);
    bgImagePath_ = imagePath;
    bgIs9Patch_ = is9Patch;
    bgMargins_ = margins;
  }
  update();
}

void SoftwareVideoRenderer::clearSubtitleBg() {
  {
    QMutexLocker lock(&bgMutex_);
    bgImagePath_.clear();
  }
  update();
}
```

- [ ] **Step 3: 实现 drawNinePatch() 九宫格绘制算法**

```cpp
void SoftwareVideoRenderer::drawNinePatch(QPainter &painter, const QImage &src,
                                           const QRect &target,
                                           const QMargins &m) {
  int sw = src.width();
  int sh = src.height();
  int tw = target.width();
  int th = target.height();

  int ml = m.left(), mr = m.right(), mt = m.top(), mb = m.bottom();

  // Clamp margins to source size
  ml = qMin(ml, sw / 2);
  mr = qMin(mr, sw / 2);
  mt = qMin(mt, sh / 2);
  mb = qMin(mb, sh / 2);

  // Source rects (9 regions)
  QRect sTL(0, 0, ml, mt);
  QRect sTC(ml, 0, sw - ml - mr, mt);
  QRect sTR(sw - mr, 0, mr, mt);
  QRect sML(0, mt, ml, sh - mt - mb);
  QRect sMC(ml, mt, sw - ml - mr, sh - mt - mb);
  QRect sMR(sw - mr, mt, mr, sh - mt - mb);
  QRect sBL(0, sh - mb, ml, mb);
  QRect sBC(ml, sh - mb, sw - ml - mr, mb);
  QRect sBR(sw - mr, sh - mb, mr, mb);

  int tx = target.x(), ty = target.y();

  // Target rects
  QRect dTL(tx, ty, ml, mt);
  QRect dTC(tx + ml, ty, tw - ml - mr, mt);
  QRect dTR(tx + tw - mr, ty, mr, mt);
  QRect dML(tx, ty + mt, ml, th - mt - mb);
  QRect dMC(tx + ml, ty + mt, tw - ml - mr, th - mt - mb);
  QRect dMR(tx + tw - mr, ty + mt, mr, th - mt - mb);
  QRect dBL(tx, ty + th - mb, ml, mb);
  QRect dBC(tx + ml, ty + th - mb, tw - ml - mr, mb);
  QRect dBR(tx + tw - mr, ty + th - mb, mr, mb);

  // Draw 9 patches
  painter.drawImage(dTL, src, sTL);
  painter.drawImage(dTC, src, sTC);
  painter.drawImage(dTR, src, sTR);
  painter.drawImage(dML, src, sML);
  painter.drawImage(dMC, src, sMC);
  painter.drawImage(dMR, src, sMR);
  painter.drawImage(dBL, src, sBL);
  painter.drawImage(dBC, src, sBC);
  painter.drawImage(dBR, src, sBR);
}
```

- [ ] **Step 4: 修改 paintEvent() — 在字幕文字绘制前插入背景绘制**

Modify `src/SoftwareVideoRenderer.cpp` 的 `paintEvent()`，在字幕文字绘制逻辑前：
```cpp
  // Draw subtitle overlay (clipped to video area)
  QString text;
  QFont font;
  QString bgPath;
  bool is9Patch = false;
  QMargins bgMargins;
  {
    QMutexLocker lock(&subtitleMutex_);
    text = subtitleText_;
    font = subtitleFont_;
  }
  {
    QMutexLocker lock(&bgMutex_);
    bgPath = bgImagePath_;
    is9Patch = bgIs9Patch_;
    bgMargins = bgMargins_;
  }

  if (!text.isEmpty()) {
    painter.setFont(font);
    QRect textRect = targetRect.adjusted(40, 0, -40, -20);

    // 绘制背景图（如果有配置）
    if (!bgPath.isEmpty()) {
      // 从缓存中获取或加载背景图
      QImage bgImage;
      if (bgCache_.contains(bgPath)) {
        bgImage = bgCache_[bgPath];
      } else {
        bgImage = QImage(bgPath);
        if (!bgImage.isNull()) {
          bgCache_[bgPath] = bgImage;
        }
      }

      if (!bgImage.isNull()) {
        QFontMetrics fm(font);
        QRect textBounding = fm.boundingRect(
            textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
        // 向外扩展 padding
        QRect bgRect = textBounding.adjusted(-20, -10, 20, 10);

        if (is9Patch) {
          drawNinePatch(painter, bgImage, bgRect, bgMargins);
        } else {
          // 固定长度：居中在文字底部
          int imgX = textBounding.center().x() - bgImage.width() / 2;
          int imgY = textBounding.center().y() - bgImage.height() / 2;
          painter.drawImage(imgX, imgY, bgImage);
        }
      }
    }

    // 绘制字幕文字（黑色描边 + 白色填充，原有逻辑不变）
    painter.setPen(
        QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
    painter.setPen(Qt::white);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, text);
  }
```

- [ ] **Step 5: Commit**
```bash
git add include/SoftwareVideoRenderer.h src/SoftwareVideoRenderer.cpp
git commit -m "feat(speaker): add subtitle background rendering with 9-patch and fixed-size modes"
```

---

### Task 7: 修改 VideoPreviewPanel — 传递说话人背景配置给渲染器

**Files:**
- Modify: `include/VideoPreviewPanel.h`
- Modify: `src/VideoPreviewPanel.cpp`

- [ ] **Step 1: 修改 updateSubtitleOverlay() 传递背景配置**

Modify `src/VideoPreviewPanel.cpp` 的 `updateSubtitleOverlay()`：

在原有的 `videoRenderer_->setSubtitleText(activeItem->text)` 调用之后添加：
```cpp
  // 根据说话人查找并传递背景图配置
  if (activeItem && activeItem->speakerId >= 0 && subtitleTrack_) {
    SpeakerInfo info = subtitleTrack_->speakerInfo(activeItem->speakerId);
    QString bgFolder = subtitleTrack_->globalBgFolder();
    if (!bgFolder.isEmpty() && !info.bgImageFile.isEmpty()) {
      QString fullPath = QDir(bgFolder).filePath(info.bgImageFile);
      QMargins margins = subtitleTrack_->unifiedBorderMargins();
      videoRenderer_->setSubtitleBg(fullPath, info.is9Patch, margins);
    } else {
      videoRenderer_->clearSubtitleBg();
    }
  } else {
    videoRenderer_->clearSubtitleBg();
  }
```

需要在文件顶部添加 `#include <QDir>` 和 `#include "SubtitleTrack.h"`（如果未包含 SpeakerInfo 定义）。

- [ ] **Step 2: Commit**
```bash
git add src/VideoPreviewPanel.cpp
git commit -m "feat(speaker): pass speaker background config to renderer in updateSubtitleOverlay"
```

---

### Task 8: 创建 SpeakerManagerDialog — 说话人管理对话框

**Files:**
- Create: `include/SpeakerManagerDialog.h`
- Create: `src/SpeakerManagerDialog.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 创建头文件**

Create `include/SpeakerManagerDialog.h`：
```cpp
#pragma once

#include <QDialog>

class SubtitleTrack;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QListWidget;
class QComboBox;
class QLabel;

class SpeakerManagerDialog : public QDialog {
  Q_OBJECT

public:
  explicit SpeakerManagerDialog(SubtitleTrack *track,
                                 QWidget *parent = nullptr);
  ~SpeakerManagerDialog() override;

private slots:
  void onSpeakerSelectionChanged();
  void onAddSpeaker();
  void onRemoveSpeaker();
  void onBrowseFolder();
  void onImageFileChanged(const QString &fileName);
  void onSaveAndApply();
  void updateTheme();

private:
  void setupUi();
  void populateSpeakerList();
  void populateImageCombo();
  void updatePreviewImage();

  SubtitleTrack *track_ = nullptr;

  // 统一设置区
  QLineEdit *bgFolderEdit_ = nullptr;
  QPushButton *browseFolderBtn_ = nullptr;
  QSpinBox *marginLeft_ = nullptr;
  QSpinBox *marginRight_ = nullptr;
  QSpinBox *marginTop_ = nullptr;
  QSpinBox *marginBottom_ = nullptr;

  // 说话人列表
  QListWidget *speakerList_ = nullptr;
  QPushButton *addBtn_ = nullptr;
  QPushButton *removeBtn_ = nullptr;

  // 右侧属性
  QLineEdit *nameEdit_ = nullptr;
  QComboBox *imageCombo_ = nullptr;
  QLabel *previewLabel_ = nullptr;
  QComboBox *drawModeCombo_ = nullptr;

  // 底部按钮
  QPushButton *saveBtn_ = nullptr;
  QPushButton *cancelBtn_ = nullptr;
};
```

- [ ] **Step 2: 创建实现文件 — setupUi()**

Create `src/SpeakerManagerDialog.cpp`：

完整实现包含：
- `setupUi()` 构建 ASCII Art 中描述的布局（标题栏、统一设置区、左侧列表+右侧属性、底部按钮）。
- 连接 `ThemeManager::instance().themeChanged()` 信号到 `updateTheme()` 槽函数以支持动态主题。
- `populateSpeakerList()` 从 `track_->allSpeakers()` 填充列表。
- `populateImageCombo()` 扫描 `bgFolderEdit_` 路径下的所有图片文件（`*.png`, `*.jpg`, `*.jpeg`, `*.bmp`）。
- `onSaveAndApply()` 将所有修改写回 `track_`，调用 `track_->saveGlobalSettings()`，然后 `accept()`。
- `updateTheme()` 读取 ThemeManager 颜色重新设置样式表。

（此步骤实现较长，在编码阶段根据具体的 QSS 风格来实现。）

- [ ] **Step 3: 修改 CMakeLists.txt 添加新文件**

在 `SOURCES` 列表中添加 `src/SpeakerManagerDialog.cpp`，在 `HEADERS` 列表中添加 `include/SpeakerManagerDialog.h`。

- [ ] **Step 4: 在 SubtitleListPanel 中连接管理菜单项**

在 `showSpeakerMenu()` 中，取消 Task 5 中的注释：
```cpp
  if (chosen == manageAction) {
    SpeakerManagerDialog dlg(track_, this);
    dlg.exec();
    return;
  }
```

并添加 `#include "SpeakerManagerDialog.h"` 到 `SubtitleListPanel.cpp`。

- [ ] **Step 5: Commit**
```bash
git add include/SpeakerManagerDialog.h src/SpeakerManagerDialog.cpp CMakeLists.txt src/SubtitleListPanel.cpp
git commit -m "feat(speaker): create SpeakerManagerDialog with dynamic theme support"
```

---

### Task 9: 扩展 TencentAsrService — 开启说话人分离与解析 SpeakerId

**Files:**
- Modify: `src/TencentAsrService.cpp`

- [ ] **Step 1: 在 payload() 中添加 SpeakerDiarization 参数**

Modify `src/TencentAsrService.cpp` 的 `payload()` 方法：
```cpp
QJsonObject TencentAsrService::payload(const QString &audioUrl) {
  QJsonObject obj;
  obj["ChannelNum"] = 1;
  obj["EngineModelType"] = "16k_zh";
  obj["ResTextFormat"] = 3;
  obj["Url"] = audioUrl;
  obj["SourceType"] = 0; // 0=URL
  obj["SpeakerDiarization"] = 1;    // 开启说话人分离
  obj["SpeakerNumber"] = 0;         // 0=自动检测说话人数量
  return obj;
}
```

- [ ] **Step 2: 在 parseResultDetail() 中解析 SpeakerId**

Modify `src/TencentAsrService.cpp` 的 `parseResultDetail()`：
```cpp
void TencentAsrService::parseResultDetail(const QJsonArray &resultDetail,
                                          QList<TranscriptSegment> &segments) {
  for (const QJsonValue &val : resultDetail) {
    QJsonObject sentence = val.toObject();
    TranscriptSegment seg;
    seg.text = sentence["FinalSentence"].toString();
    seg.startMs = sentence["StartMs"].toVariant().toLongLong();
    seg.endMs = sentence["EndMs"].toVariant().toLongLong();
    seg.speakerId = sentence["SpeakerId"].toInt(-1);  // 新增：解析说话人ID
    if (!seg.text.isEmpty()) {
      segments.append(seg);
    }
  }
}
```

- [ ] **Step 3: 在 TimelinePanel 的 ASR 结果处理中填充 speakerId 并自动注册说话人**

Modify `src/TimelinePanel.cpp` 中 `transcribeFinished` 回调里创建 SubtitleItem 的循环：
```cpp
  track_->clear();
  for (const auto &seg : result.segments) {
    SubtitleItem item;
    item.id = QUuid::createUuid().toString();
    item.text = seg.text;
    item.startMs = seg.startMs;
    item.endMs = seg.endMs;
    item.speakerId = seg.speakerId;  // 新增
    track_->addItem(item);
    track_->autoRegisterSpeaker(seg.speakerId);  // 新增：自动注册说话人
  }
```

- [ ] **Step 4: Commit**
```bash
git add src/TencentAsrService.cpp src/TimelinePanel.cpp
git commit -m "feat(speaker): enable speaker diarization in ASR and auto-register speakers"
```

---

### Task 10: 编译验证与手动测试

- [ ] **Step 1: 编译项目**

```bash
cd /Users/zxl/Projects/cpp/subtitles-editor
cmake --build build --target subtitles-editor -j$(sysctl -n hw.ncpu)
```

确保编译无错误、无警告。

- [ ] **Step 2: 运行 clang-format 格式化**

```bash
find include/ src/ -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```

- [ ] **Step 3: 手动验证清单**

1. **说话人列在列表中显示**：启动应用后，添加字幕或加载已有字幕。确认列表表头显示 `Timecode | Speaker | Subtitle | Action` 四列对齐。
2. **说话人药丸标签交互**：点击列表项中的说话人药丸标签，确认弹出包含"未分配"、已有说话人、"新建"和"管理"选项的菜单。
3. **说话人管理窗口**：点击"管理说话人"，确认 `SpeakerManagerDialog` 正确弹出，可以选择背景图文件夹、编辑说话人名称、选择背景图、调整九宫格边距。
4. **动态主题**：在设置中切换主题或主色调，确认 `SpeakerManagerDialog` 及说话人药丸标签的颜色实时更新。
5. **九宫格绘制**：为说话人配置九宫格背景图，在视频预览中确认字幕背景正确跟随文字宽度拉伸、边角保持不变形。
6. **固定长度绘制**：将绘制模式切换为"固定长度"，输入较长字幕，确认背景图保持原尺寸居中且文字溢出。
7. **ASR 说话人识别**：使用 ASR 识别包含多个说话人的视频，确认不同句子的 `speakerId` 正确分配、列表中显示对应的药丸标签。
8. **全局设置持久化**：修改统一九宫格边距和背景图文件夹后保存，重启应用确认这两个值被恢复。

- [ ] **Step 4: Final Commit**
```bash
git add -A
git commit -m "feat(speaker): dynamic subtitle background with speaker management — complete"
```

---

Plan complete and saved to `docs/superpowers/plans/2026-05-23-dynamic-subtitle-bg.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration
**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
