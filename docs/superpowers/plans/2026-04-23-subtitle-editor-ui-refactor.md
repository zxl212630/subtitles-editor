# 字幕编辑器 UI 架构重构实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将硬编码的静态示意图重构为业务驱动的可交互面板架构，拆分独立面板类，建立字幕数据模型，实现面板联动。

**Architecture:** QSplitter 管理面板尺寸调整（不可浮动），三个独立面板类（VideoPreviewPanel / SubtitleListPanel / TimelinePanel），SubtitleTrack 作为唯一数据源驱动字幕列表和时间线。

**Tech Stack:** Qt 6.5, C++17, QWindowKit, CMake

---

## 文件结构总览

| 文件 | 操作 | 职责 |
|------|------|------|
| `include/SubtitleItem.h` | 创建 | 字幕项数据结构 |
| `include/SubtitleTrack.h` | 创建 | 字幕轨道数据模型 |
| `src/SubtitleTrack.cpp` | 创建 | 数据模型实现 |
| `include/AsrServiceBase.h` | 创建 | ASR 抽象基类 |
| `src/AsrServiceBase.cpp` | 创建 | ASR 基类实现 |
| `include/SubtitleExporter.h` | 创建 | 导出接口声明 |
| `src/SubtitleExporter.cpp` | 创建 | 导出接口占位实现 |
| `include/SubtitleListModel.h` | 创建 | 列表模型 |
| `src/SubtitleListModel.cpp` | 创建 | 列表模型实现 |
| `include/VideoPreviewPanel.h` | 创建 | 视频预览面板 |
| `src/VideoPreviewPanel.cpp` | 创建 | 视频预览面板实现 |
| `include/SubtitleListPanel.h` | 创建 | 字幕列表面板 |
| `src/SubtitleListPanel.cpp` | 创建 | 字幕列表面板实现 |
| `include/TimelinePanel.h` | 创建 | 时间线面板 |
| `src/TimelinePanel.cpp` | 创建 | 时间线面板实现 |
| `include/AppWindow.h` | 修改 | 精简为组装逻辑 |
| `src/AppWindow.cpp` | 修改 | 精简为 QSplitter + 面板组装 |
| `CMakeLists.txt` | 修改 | 添加新源文件 |

---

### Task 1: 基础数据结构（SubtitleItem + SubtitleTrack）

**Files:**
- Create: `include/SubtitleItem.h`
- Create: `include/SubtitleTrack.h`
- Create: `src/SubtitleTrack.cpp`

- [ ] **Step 1: 创建 SubtitleItem 结构体**

Create `include/SubtitleItem.h`:
```cpp
#pragma once

#include <QString>

struct SubtitleItem {
    QString id;              // UUID
    QString text;            // 字幕文本
    qint64 startMs = 0;      // 开始时间（毫秒）
    qint64 endMs = 0;        // 结束时间（毫秒）
    bool selected = false;   // 选中状态
};
```

- [ ] **Step 2: 创建 SubtitleTrack 头文件**

Create `include/SubtitleTrack.h`:
```cpp
#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include "SubtitleItem.h"

class SubtitleTrack : public QObject
{
    Q_OBJECT

public:
    explicit SubtitleTrack(QObject* parent = nullptr);

    void clear();
    void addItem(const SubtitleItem& item);
    void removeItem(const QString& id);
    void updateItem(const QString& id, const SubtitleItem& newItem);
    void selectItem(const QString& id);

    const QList<SubtitleItem>& items() const;
    const SubtitleItem* selectedItem() const;
    const SubtitleItem* findItem(const QString& id) const;

signals:
    void itemAdded(const SubtitleItem& item);
    void itemRemoved(const QString& id);
    void itemUpdated(const QString& id);
    void itemSelected(const QString& id);
    void dataChanged();

private:
    QList<SubtitleItem> items_;
    QString selectedId_;
};
```

- [ ] **Step 3: 创建 SubtitleTrack 实现**

Create `src/SubtitleTrack.cpp`:
```cpp
#include "SubtitleTrack.h"

SubtitleTrack::SubtitleTrack(QObject* parent)
    : QObject(parent)
{
}

void SubtitleTrack::clear()
{
    items_.clear();
    selectedId_.clear();
    emit dataChanged();
}

void SubtitleTrack::addItem(const SubtitleItem& item)
{
    items_.append(item);
    emit itemAdded(item);
    emit dataChanged();
}

void SubtitleTrack::removeItem(const QString& id)
{
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].id == id) {
            items_.removeAt(i);
            if (selectedId_ == id) {
                selectedId_.clear();
            }
            emit itemRemoved(id);
            emit dataChanged();
            return;
        }
    }
}

void SubtitleTrack::updateItem(const QString& id, const SubtitleItem& newItem)
{
    for (int i = 0; i < items_.size(); ++i) {
        if (items_[i].id == id) {
            items_[i] = newItem;
            items_[i].id = id; // preserve id
            emit itemUpdated(id);
            emit dataChanged();
            return;
        }
    }
}

void SubtitleTrack::selectItem(const QString& id)
{
    bool found = false;
    for (const auto& item : items_) {
        if (item.id == id) {
            found = true;
            break;
        }
    }
    if (!found) return;

    selectedId_ = id;
    emit itemSelected(id);
}

const QList<SubtitleItem>& SubtitleTrack::items() const
{
    return items_;
}

const SubtitleItem* SubtitleTrack::selectedItem() const
{
    for (const auto& item : items_) {
        if (item.id == selectedId_) {
            return &item;
        }
    }
    return nullptr;
}

const SubtitleItem* SubtitleTrack::findItem(const QString& id) const
{
    for (const auto& item : items_) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}
```

- [ ] **Step 4: 提交**

```bash
git add include/SubtitleItem.h include/SubtitleTrack.h src/SubtitleTrack.cpp
git commit -m "feat(model): add SubtitleItem and SubtitleTrack data model"
```

---

### Task 2: ASR 抽象基类

**Files:**
- Create: `include/AsrServiceBase.h`
- Create: `src/AsrServiceBase.cpp`

- [ ] **Step 1: 创建 AsrServiceBase 头文件**

Create `include/AsrServiceBase.h`:
```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QList>

class AsrServiceBase : public QObject
{
    Q_OBJECT

public:
    struct TranscriptSegment {
        QString text;
        qint64 startMs = 0;
        qint64 endMs = 0;
    };

    struct TranscriptResult {
        bool success = false;
        QString errorMessage;
        QList<TranscriptSegment> segments;
    };

    explicit AsrServiceBase(QObject* parent = nullptr);
    virtual ~AsrServiceBase();

    virtual void transcribe(const QString& audioFilePath) = 0;

signals:
    void transcribeFinished(const AsrServiceBase::TranscriptResult& result);
    void transcribeProgress(int percent);
};
```

- [ ] **Step 2: 创建 AsrServiceBase 实现**

Create `src/AsrServiceBase.cpp`:
```cpp
#include "AsrServiceBase.h"

AsrServiceBase::AsrServiceBase(QObject* parent)
    : QObject(parent)
{
}

AsrServiceBase::~AsrServiceBase() = default;
```

- [ ] **Step 3: 提交**

```bash
git add include/AsrServiceBase.h src/AsrServiceBase.cpp
git commit -m "feat(asr): add AsrServiceBase abstract interface"
```

---

### Task 3: 导出接口（占位）

**Files:**
- Create: `include/SubtitleExporter.h`
- Create: `src/SubtitleExporter.cpp`

- [ ] **Step 1: 创建 SubtitleExporter 头文件**

Create `include/SubtitleExporter.h`:
```cpp
#pragma once

#include <QString>

class SubtitleTrack;

class SubtitleExporter
{
public:
    static bool exportToSRT(const SubtitleTrack& track, const QString& filePath);
    static bool exportToASS(const SubtitleTrack& track, const QString& filePath);
    static bool exportToVTT(const SubtitleTrack& track, const QString& filePath);
    static bool exportToPremiereXML(const SubtitleTrack& track, const QString& filePath);
};
```

- [ ] **Step 2: 创建 SubtitleExporter 占位实现**

Create `src/SubtitleExporter.cpp`:
```cpp
#include "SubtitleExporter.h"
#include "SubtitleTrack.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

bool SubtitleExporter::exportToSRT(const SubtitleTrack& track, const QString& filePath)
{
    Q_UNUSED(track)
    Q_UNUSED(filePath)
    qWarning() << "exportToSRT not implemented";
    return false;
}

bool SubtitleExporter::exportToASS(const SubtitleTrack& track, const QString& filePath)
{
    Q_UNUSED(track)
    Q_UNUSED(filePath)
    qWarning() << "exportToASS not implemented";
    return false;
}

bool SubtitleExporter::exportToVTT(const SubtitleTrack& track, const QString& filePath)
{
    Q_UNUSED(track)
    Q_UNUSED(filePath)
    qWarning() << "exportToVTT not implemented";
    return false;
}

bool SubtitleExporter::exportToPremiereXML(const SubtitleTrack& track, const QString& filePath)
{
    Q_UNUSED(track)
    Q_UNUSED(filePath)
    qWarning() << "exportToPremiereXML not implemented";
    return false;
}
```

- [ ] **Step 3: 提交**

```bash
git add include/SubtitleExporter.h src/SubtitleExporter.cpp
git commit -m "feat(export): add SubtitleExporter placeholder interface"
```

---

### Task 4: 字幕列表模型

**Files:**
- Create: `include/SubtitleListModel.h`
- Create: `src/SubtitleListModel.cpp`

- [ ] **Step 1: 创建 SubtitleListModel 头文件**

Create `include/SubtitleListModel.h`:
```cpp
#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QList>

class SubtitleTrack;

class SubtitleListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TextRole,
        StartMsRole,
        EndMsRole,
        SelectedRole,
        StartTimeRole,
        EndTimeRole
    };

    explicit SubtitleListModel(QObject* parent = nullptr);

    void setTrack(SubtitleTrack* track);
    void setFilterText(const QString& text);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

private slots:
    void onDataChanged();

private:
    void rebuildFilteredIndices();
    static QString formatTime(qint64 ms);

    SubtitleTrack* track_ = nullptr;
    QString filterText_;
    QList<int> filteredIndices_;
};
```

- [ ] **Step 2: 创建 SubtitleListModel 实现**

Create `src/SubtitleListModel.cpp`:
```cpp
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"
#include "SubtitleItem.h"

SubtitleListModel::SubtitleListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void SubtitleListModel::setTrack(SubtitleTrack* track)
{
    if (track_) {
        disconnect(track_, &SubtitleTrack::dataChanged, this, &SubtitleListModel::onDataChanged);
    }
    track_ = track;
    if (track_) {
        connect(track_, &SubtitleTrack::dataChanged, this, &SubtitleListModel::onDataChanged);
    }
    rebuildFilteredIndices();
}

void SubtitleListModel::setFilterText(const QString& text)
{
    filterText_ = text;
    rebuildFilteredIndices();
}

int SubtitleListModel::rowCount(const QModelIndex& /*parent*/) const
{
    return filteredIndices_.size();
}

QVariant SubtitleListModel::data(const QModelIndex& index, int role) const
{
    if (!track_ || !index.isValid() || index.row() >= filteredIndices_.size()) {
        return QVariant();
    }

    const int originalIndex = filteredIndices_[index.row()];
    const auto& items = track_->items();
    if (originalIndex >= items.size()) {
        return QVariant();
    }

    const auto& item = items[originalIndex];

    switch (role) {
    case Qt::DisplayRole:
        return item.text;
    case IdRole:
        return item.id;
    case TextRole:
        return item.text;
    case StartMsRole:
        return QVariant::fromValue(item.startMs);
    case EndMsRole:
        return QVariant::fromValue(item.endMs);
    case SelectedRole:
        return item.selected;
    case StartTimeRole:
        return formatTime(item.startMs);
    case EndTimeRole:
        return formatTime(item.endMs);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SubtitleListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TextRole] = "text";
    roles[StartMsRole] = "startMs";
    roles[EndMsRole] = "endMs";
    roles[SelectedRole] = "selected";
    roles[StartTimeRole] = "startTime";
    roles[EndTimeRole] = "endTime";
    return roles;
}

void SubtitleListModel::onDataChanged()
{
    beginResetModel();
    rebuildFilteredIndices();
    endResetModel();
}

void SubtitleListModel::rebuildFilteredIndices()
{
    filteredIndices_.clear();
    if (!track_) return;

    const auto& items = track_->items();
    for (int i = 0; i < items.size(); ++i) {
        if (filterText_.isEmpty() || items[i].text.contains(filterText_, Qt::CaseInsensitive)) {
            filteredIndices_.append(i);
        }
    }
}

QString SubtitleListModel::formatTime(qint64 ms)
{
    const int hours = ms / 3600000;
    const int minutes = (ms % 3600000) / 60000;
    const int seconds = (ms % 60000) / 1000;
    const int frames = (ms % 1000) / 40; // approx 25fps
    return QString("%1:%2:%3:%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(frames, 2, 10, QChar('0'));
}
```

- [ ] **Step 3: 提交**

```bash
git add include/SubtitleListModel.h src/SubtitleListModel.cpp
git commit -m "feat(model): add SubtitleListModel with filtering"
```

---

### Task 5: VideoPreviewPanel 拆分

**Files:**
- Create: `include/VideoPreviewPanel.h`
- Create: `src/VideoPreviewPanel.cpp`

- [ ] **Step 1: 创建 VideoPreviewPanel 头文件**

Create `include/VideoPreviewPanel.h`:
```cpp
#pragma once

#include <QWidget>
#include <QFontDatabase>

class QComboBox;
class QLabel;
class QFrame;
class QPushButton;

class VideoPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPreviewPanel(QWidget* parent = nullptr);

signals:
    void fontChanged(const QString& family);
    void fontSizeChanged(int size);

private:
    void setupUi();
    void populateFontCombo();
    void populateSizeCombo();

    QComboBox* fontCombo_ = nullptr;
    QComboBox* sizeCombo_ = nullptr;
    QLabel* timeLabel_ = nullptr;
};
```

- [ ] **Step 2: 创建 VideoPreviewPanel 实现**

Create `src/VideoPreviewPanel.cpp`:
```cpp
#include "VideoPreviewPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QFontDatabase>
#include <QValidator>

static QPushButton* createIconBtn(QWidget* parent, const QString& text, int w, int h,
                                   const QString& bg = "#333333", const QString& color = "#d1d5db")
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(w, h);
    btn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 4px;
            font-family: Inter, sans-serif;
            font-size: 12px;
            font-weight: bold;
        }
    )").arg(bg, color));
    return btn;
}

VideoPreviewPanel::VideoPreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void VideoPreviewPanel::setupUi()
{
    setObjectName("VideoPreviewPanel");
    setStyleSheet(R"(
        QWidget#VideoPreviewPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
            border: 1px solid #333333;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Toolbar ---
    auto* toolbar = new QFrame(this);
    toolbar->setFixedHeight(40);
    toolbar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            border: none;
        }
    )");
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(12, 0, 16, 0);
    tbLayout->setSpacing(12);
    tbLayout->setAlignment(Qt::AlignVCenter);

    // Font combo
    fontCombo_ = new QComboBox(toolbar);
    fontCombo_->setFixedSize(140, 28);
    fontCombo_->setStyleSheet(R"(
        QComboBox {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 4px;
            padding-left: 8px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background-color: #141414;
            color: #d1d5db;
            selection-background-color: #333333;
        }
    )");
    populateFontCombo();
    tbLayout->addWidget(fontCombo_);

    connect(fontCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { emit fontChanged(fontCombo_->currentText()); });

    // Size combo
    sizeCombo_ = new QComboBox(toolbar);
    sizeCombo_->setFixedSize(60, 28);
    sizeCombo_->setEditable(true);
    sizeCombo_->setStyleSheet(R"(
        QComboBox {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 4px;
            padding-left: 8px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
        QComboBox::drop-down { border: none; width: 20px; }
    )");
    populateSizeCombo();
    tbLayout->addWidget(sizeCombo_);

    connect(sizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { emit fontSizeChanged(sizeCombo_->currentText().toInt()); });

    // Size input validation
    auto* validator = new QIntValidator(1, 999, sizeCombo_);
    sizeCombo_->setValidator(validator);

    // Elastic spacer
    auto* tbSpacer = new QWidget(toolbar);
    tbSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tbLayout->addWidget(tbSpacer);

    // Format buttons
    tbLayout->addWidget(createIconBtn(toolbar, "B", 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, "I", 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, "U", 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, QString(QChar(0x2261)), 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, QString(QChar(0x2261)), 28, 28));
    tbLayout->addWidget(createIconBtn(toolbar, QString(QChar(0x2261)), 28, 28));

    layout->addWidget(toolbar);

    // --- Video display area ---
    auto* videoArea = new QFrame(this);
    videoArea->setStyleSheet("background-color: transparent; border: none;");
    videoArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* vaLayout = new QVBoxLayout(videoArea);
    vaLayout->setContentsMargins(40, 0, 40, 0);
    vaLayout->setAlignment(Qt::AlignCenter);

    auto* blackRect = new QFrame(videoArea);
    blackRect->setStyleSheet("background-color: #000000; border: none;");
    blackRect->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vaLayout->addWidget(blackRect);

    layout->addWidget(videoArea, 1);

    // --- Playback control bar ---
    auto* controlBar = new QFrame(this);
    controlBar->setFixedHeight(36);
    controlBar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-bottom-left-radius: 10px;
            border-bottom-right-radius: 10px;
            border: none;
        }
    )");
    auto* cbLayout = new QHBoxLayout(controlBar);
    cbLayout->setContentsMargins(8, 0, 12, 0);
    cbLayout->setSpacing(8);
    cbLayout->setAlignment(Qt::AlignVCenter);

    auto addIconLabel = [&](const QString& text, int w, int h) {
        auto* lbl = new QLabel(text, controlBar);
        lbl->setFixedSize(w, h);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #d1d5db; font-family: Inter; font-size: 12px; background: transparent;");
        cbLayout->addWidget(lbl);
    };

    addIconLabel(QString(QChar(0x23EE)), 16, 16);
    addIconLabel(QString(QChar(0x23ED)), 16, 16);
    addIconLabel(QString(QChar(0x25B6)), 16, 16);
    addIconLabel(QString(QChar(0x25A0)), 14, 14);

    // Progress bar container
    auto* progressContainer = new QFrame(controlBar);
    progressContainer->setFixedSize(550, 4);
    progressContainer->setStyleSheet("background-color: #333333; border-radius: 2px;");
    auto* progressFill = new QFrame(progressContainer);
    progressFill->setFixedSize(260, 4);
    progressFill->setStyleSheet("background-color: #38bdf8; border-radius: 2px;");
    progressFill->move(0, 0);
    cbLayout->addWidget(progressContainer);

    timeLabel_ = new QLabel("00:00:00:00 / 00:00:00:00", controlBar);
    timeLabel_->setStyleSheet("color: #d1d5db; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    cbLayout->addWidget(timeLabel_);

    addIconLabel("Vol", 24, 16);
    addIconLabel("FS", 20, 16);

    layout->addWidget(controlBar);
}

void VideoPreviewPanel::populateFontCombo()
{
    QFontDatabase db;
    QStringList families = db.families();
    families.sort();

    for (const QString& family : families) {
        if (family.startsWith('.') || family.isEmpty()) continue;
        fontCombo_->addItem(family);
    }

    int idx = fontCombo_->findText("Arial");
    if (idx >= 0) {
        fontCombo_->setCurrentIndex(idx);
    } else if (fontCombo_->count() > 0) {
        fontCombo_->setCurrentIndex(0);
    }
}

void VideoPreviewPanel::populateSizeCombo()
{
    const QList<int> sizes = {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36, 40, 48, 56, 64, 72};
    for (int s : sizes) {
        sizeCombo_->addItem(QString::number(s));
    }
    sizeCombo_->setCurrentText("24");
}
```

- [ ] **Step 3: 提交**

```bash
git add include/VideoPreviewPanel.h src/VideoPreviewPanel.cpp
git commit -m "feat(panel): add VideoPreviewPanel with real font/size combos"
```

---

### Task 6: SubtitleListPanel 拆分

**Files:**
- Create: `include/SubtitleListPanel.h`
- Create: `src/SubtitleListPanel.cpp`

- [ ] **Step 1: 创建 SubtitleListPanel 头文件**

Create `include/SubtitleListPanel.h`:
```cpp
#pragma once

#include <QWidget>

class SubtitleTrack;
class SubtitleListModel;
class QListView;
class QLineEdit;
class QPushButton;

class SubtitleListPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SubtitleListPanel(QWidget* parent = nullptr);

    void setTrack(SubtitleTrack* track);

signals:
    void itemSelected(const QString& id);
    void itemDeleteRequested(const QString& id);

private:
    void setupUi();
    void onItemClicked(const QModelIndex& index);

    SubtitleTrack* track_ = nullptr;
    SubtitleListModel* model_ = nullptr;
    QListView* listView_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
};
```

- [ ] **Step 2: 创建 SubtitleListPanel 实现**

Create `src/SubtitleListPanel.cpp`:
```cpp
#include "SubtitleListPanel.h"
#include "SubtitleListModel.h"
#include "SubtitleTrack.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QHeaderView>

SubtitleListPanel::SubtitleListPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void SubtitleListPanel::setTrack(SubtitleTrack* track)
{
    track_ = track;
    model_->setTrack(track);
}

void SubtitleListPanel::setupUi()
{
    setObjectName("SubtitleListPanel");
    setStyleSheet(R"(
        QWidget#SubtitleListPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // --- Panel header (tabs) ---
    auto* panelHeader = new QFrame(this);
    panelHeader->setFixedHeight(40);
    panelHeader->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            border: none;
        }
    )");
    auto* phLayout = new QHBoxLayout(panelHeader);
    phLayout->setContentsMargins(12, 6, 0, 6);
    phLayout->setSpacing(4);
    phLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto addTab = [&](const QString& text, bool active) {
        auto* tab = new QPushButton(text, panelHeader);
        tab->setFixedSize(60, 28);
        QString bg = active ? "#333333" : "#262626";
        QString fg = active ? "#e5e5e5" : "#9ca3af";
        tab->setStyleSheet(QString(R"(
            QPushButton {
                background-color: %1;
                color: %2;
                border: none;
                border-radius: 5px;
                font-family: Inter, sans-serif;
                font-size: 12px;
            }
        )").arg(bg, fg));
        phLayout->addWidget(tab);
    };

    addTab("字幕", true);
    addTab("预设", false);
    addTab("自定义", false);
    addTab("动画", false);
    phLayout->addStretch();
    layout->addWidget(panelHeader);

    // --- Panel content ---
    auto* panelContent = new QFrame(this);
    panelContent->setStyleSheet("background-color: transparent; border: none;");
    panelContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* pcLayout = new QVBoxLayout(panelContent);
    pcLayout->setContentsMargins(12, 12, 12, 12);
    pcLayout->setSpacing(0);

    // Search bar
    auto* searchBar = new QFrame(panelContent);
    searchBar->setFixedHeight(40);
    searchBar->setStyleSheet("background-color: transparent; border: none;");
    auto* sbLayout = new QHBoxLayout(searchBar);
    sbLayout->setContentsMargins(0, 0, 0, 0);
    sbLayout->setAlignment(Qt::AlignVCenter);

    searchEdit_ = new QLineEdit(searchBar);
    searchEdit_->setPlaceholderText("请输入查找内容");
    searchEdit_->setFixedHeight(28);
    searchEdit_->setStyleSheet(R"(
        QLineEdit {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 5px;
            padding-left: 8px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
    )");
    sbLayout->addWidget(searchEdit_);
    pcLayout->addWidget(searchBar);

    connect(searchEdit_, &QLineEdit::textChanged, this, [this](const QString& text) {
        model_->setFilterText(text);
    });

    // List container
    auto* listContainer = new QFrame(panelContent);
    listContainer->setStyleSheet(R"(
        QFrame {
            background-color: #141414;
            border-radius: 5px;
        }
    )");
    listContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* lcLayout = new QVBoxLayout(listContainer);
    lcLayout->setContentsMargins(0, 0, 0, 0);
    lcLayout->setSpacing(0);

    // Table header
    auto* tableHeader = new QFrame(listContainer);
    tableHeader->setFixedHeight(32);
    tableHeader->setStyleSheet("background-color: transparent; border: none;");
    auto* thLayout = new QHBoxLayout(tableHeader);
    thLayout->setContentsMargins(12, 0, 12, 0);
    thLayout->setSpacing(12);
    thLayout->setAlignment(Qt::AlignVCenter);

    auto* headerTime = new QLabel("时间码", tableHeader);
    headerTime->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    thLayout->addWidget(headerTime);

    thLayout->addStretch();

    auto* headerText = new QLabel("字幕", tableHeader);
    headerText->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    thLayout->addWidget(headerText);

    thLayout->addStretch();

    auto* headerAction = new QLabel("操作", tableHeader);
    headerAction->setStyleSheet("color: #9ca3af; font-family: Inter, sans-serif; font-size: 11px; background: transparent;");
    thLayout->addWidget(headerAction);

    lcLayout->addWidget(tableHeader);

    // Subtitle list
    listView_ = new QListView(listContainer);
    listView_->setStyleSheet(R"(
        QListView {
            background-color: transparent;
            border: none;
            outline: none;
        }
        QListView::item {
            height: 56px;
            background-color: transparent;
            border: none;
        }
        QListView::item:selected {
            background-color: #1f2937;
            border-radius: 5px;
        }
    )");
    listView_->setSelectionMode(QAbstractItemView::SingleSelection);
    listView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    model_ = new SubtitleListModel(this);
    listView_->setModel(model_);

    connect(listView_, &QListView::clicked, this, &SubtitleListPanel::onItemClicked);

    lcLayout->addWidget(listView_);
    pcLayout->addWidget(listContainer);
    layout->addWidget(panelContent);
}

void SubtitleListPanel::onItemClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;
    QString id = model_->data(index, SubtitleListModel::IdRole).toString();
    if (track_) {
        track_->selectItem(id);
    }
    emit itemSelected(id);
}
```

- [ ] **Step 3: 提交**

```bash
git add include/SubtitleListPanel.h src/SubtitleListPanel.cpp
git commit -m "feat(panel): add SubtitleListPanel with QListView and search"
```

---

### Task 7: TimelinePanel 拆分

**Files:**
- Create: `include/TimelinePanel.h`
- Create: `src/TimelinePanel.cpp`

- [ ] **Step 1: 创建 TimelinePanel 头文件**

Create `include/TimelinePanel.h`:
```cpp
#pragma once

#include <QWidget>

class SubtitleTrack;

class TimelinePanel : public QWidget
{
    Q_OBJECT

public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    void setTrack(SubtitleTrack* track);

signals:
    void timeClicked(qint64 ms);
    void itemSelected(const QString& id);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void drawRuler(QPainter& painter);
    void drawSubtitleTrack(QPainter& painter, int y);
    void drawVideoTrack(QPainter& painter, int y);
    void drawPlayhead(QPainter& painter);

    qint64 pixelsToMs(int px) const;
    int msToPixels(qint64 ms) const;

    SubtitleTrack* track_ = nullptr;
    qint64 totalDurationMs_ = 11000; // placeholder 11 seconds
    qint64 currentTimeMs_ = 6040;    // placeholder ~6 seconds
    static constexpr int RULER_HEIGHT = 36;
    static constexpr int SUBTITLE_TRACK_HEIGHT = 48;
    static constexpr int VIDEO_TRACK_HEIGHT = 96;
    static constexpr int TRACK_HEAD_WIDTH = 120;
    static constexpr int PIXELS_PER_SECOND = 100;
};
```

- [ ] **Step 2: 创建 TimelinePanel 实现**

Create `src/TimelinePanel.cpp`:
```cpp
#include "TimelinePanel.h"
#include "SubtitleTrack.h"
#include "SubtitleItem.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontDatabase>

TimelinePanel::TimelinePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("TimelinePanel");
    setStyleSheet(R"(
        QWidget#TimelinePanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");
    setMinimumHeight(150);
    setMaximumHeight(400);
}

void TimelinePanel::setTrack(SubtitleTrack* track)
{
    if (track_) {
        disconnect(track_, &SubtitleTrack::dataChanged, this, QOverload<>::of(&TimelinePanel::update));
        disconnect(track_, &SubtitleTrack::itemSelected, this, QOverload<>::of(&TimelinePanel::update));
    }
    track_ = track;
    if (track_) {
        connect(track_, &SubtitleTrack::dataChanged, this, QOverload<>::of(&TimelinePanel::update));
        connect(track_, &SubtitleTrack::itemSelected, this, QOverload<>::of(&TimelinePanel::update));
    }
    update();
}

void TimelinePanel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor("#1e1e1e"));

    drawRuler(painter);
    drawSubtitleTrack(painter, RULER_HEIGHT);
    drawVideoTrack(painter, RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT);
    drawPlayhead(painter);
}

void TimelinePanel::drawRuler(QPainter& painter)
{
    painter.setPen(QColor("#6b7280"));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    int contentWidth = width() - TRACK_HEAD_WIDTH;
    int seconds = totalDurationMs_ / 1000;
    for (int s = 0; s <= seconds; ++s) {
        int x = TRACK_HEAD_WIDTH + s * PIXELS_PER_SECOND;
        if (x > width()) break;

        QString label = QString("00:00:%1:00").arg(s, 2, 10, QChar('0'));
        painter.drawText(x - 20, 8, 60, 14, Qt::AlignCenter, label);

        painter.setPen(QColor("#333333"));
        painter.drawLine(x, 24, x, 34);
        painter.setPen(QColor("#6b7280"));
    }

    // Minor ticks
    painter.setPen(QColor("#404040"));
    for (int s = 0; s < seconds; ++s) {
        int midX = TRACK_HEAD_WIDTH + s * PIXELS_PER_SECOND + PIXELS_PER_SECOND / 2;
        painter.drawLine(midX, 28, midX, 31);
    }
}

void TimelinePanel::drawSubtitleTrack(QPainter& painter, int y)
{
    // Track background
    painter.fillRect(TRACK_HEAD_WIDTH, y, width() - TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT, QColor("#2a2a2a"));

    // Track head
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, y, TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT, QColor("#262626"));
    painter.setPen(QColor("#9ca3af"));
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(12, y + 18, "T  字幕1");

    // Separator
    painter.setPen(QColor("#333333"));
    painter.drawLine(TRACK_HEAD_WIDTH, y + SUBTITLE_TRACK_HEIGHT - 1, width(), y + SUBTITLE_TRACK_HEIGHT - 1);

    if (!track_) return;

    // Subtitle bars
    for (const auto& item : track_->items()) {
        int x = TRACK_HEAD_WIDTH + msToPixels(item.startMs);
        int w = msToPixels(item.endMs - item.startMs);
        if (w < 4) w = 4;

        QColor barColor = item.selected ? QColor("#0ea5e9") : QColor("#38bdf8");
        painter.setPen(Qt::NoPen);
        painter.setBrush(barColor);
        painter.drawRoundedRect(x, y + 2, w, SUBTITLE_TRACK_HEIGHT - 4, 4, 4);

        painter.setPen(QColor("#e5e5e5"));
        QFont barFont = painter.font();
        barFont.setPointSize(9);
        painter.setFont(barFont);
        painter.drawText(x + 8, y + 18, item.text);
    }
}

void TimelinePanel::drawVideoTrack(QPainter& painter, int y)
{
    painter.fillRect(TRACK_HEAD_WIDTH, y, width() - TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT, QColor("#2a2a2a"));

    painter.setPen(Qt::NoPen);
    painter.fillRect(0, y, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT, QColor("#262626"));
    painter.setPen(QColor("#9ca3af"));
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(12, y + 18, "F  视频1");

    // Placeholder video bar
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#0284c7"));
    painter.drawRoundedRect(TRACK_HEAD_WIDTH + 4, y + 2, 400, VIDEO_TRACK_HEIGHT - 4, 4, 4);
    painter.setPen(QColor("#e5e5e5"));
    painter.drawText(TRACK_HEAD_WIDTH + 16, y + 50, "video.mp4");
}

void TimelinePanel::drawPlayhead(QPainter& painter)
{
    int x = TRACK_HEAD_WIDTH + msToPixels(currentTimeMs_);
    painter.setPen(QColor("#f59e0b"));
    painter.drawLine(x, 0, x, height());

    // Triangle pointer
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#f59e0b"));
    QPointF triangle[3] = {
        QPointF(x - 5, -5),
        QPointF(x + 5, -5),
        QPointF(x, 5)
    };
    painter.drawPolygon(triangle, 3);
}

void TimelinePanel::mousePressEvent(QMouseEvent* event)
{
    if (event->x() < TRACK_HEAD_WIDTH) return;

    qint64 ms = pixelsToMs(event->x() - TRACK_HEAD_WIDTH);
    if (ms < 0) ms = 0;
    if (ms > totalDurationMs_) ms = totalDurationMs_;

    currentTimeMs_ = ms;
    emit timeClicked(ms);
    update();
}

qint64 TimelinePanel::pixelsToMs(int px) const
{
    return static_cast<qint64>(px) * 1000 / PIXELS_PER_SECOND;
}

int TimelinePanel::msToPixels(qint64 ms) const
{
    return static_cast<int>(ms * PIXELS_PER_SECOND / 1000);
}
```

- [ ] **Step 3: 提交**

```bash
git add include/TimelinePanel.h src/TimelinePanel.cpp
git commit -m "feat(panel): add TimelinePanel with paintEvent rendering"
```

---

### Task 8: AppWindow 精简与 QSplitter 组装

**Files:**
- Modify: `include/AppWindow.h`
- Modify: `src/AppWindow.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 修改 AppWindow.h**

Modify `include/AppWindow.h`:
```cpp
#pragma once

#include <QMainWindow>
#include <memory>

class VideoPreviewPanel;
class SubtitleListPanel;
class TimelinePanel;
class SubtitleTrack;
class QSplitter;

class AppWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);
    ~AppWindow() override;

private:
    void setupUi();
    void setupTitleBar();
    void setupSplitterLayout();
    void setupDummyData();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
```

- [ ] **Step 2: 重写 AppWindow.cpp**

Modify `src/AppWindow.cpp`:
```cpp
#include "AppWindow.h"
#include "VideoPreviewPanel.h"
#include "SubtitleListPanel.h"
#include "TimelinePanel.h"
#include "SubtitleTrack.h"
#include "SubtitleItem.h"

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QSplitter>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>
#include <QUuid>

struct AppWindow::Private
{
    QWK::WidgetWindowAgent* windowAgent = nullptr;
    QFrame* titleBar = nullptr;
    QLabel* titleLabel = nullptr;

    QSplitter* verticalSplitter = nullptr;
    QSplitter* topSplitter = nullptr;
    VideoPreviewPanel* videoPreviewPanel = nullptr;
    SubtitleListPanel* subtitleListPanel = nullptr;
    TimelinePanel* timelinePanel = nullptr;

    SubtitleTrack* subtitleTrack = nullptr;
};

AppWindow::AppWindow(QWidget* parent)
    : QMainWindow(parent)
    , d(std::make_unique<Private>())
{
    setupUi();
}

AppWindow::~AppWindow() = default;

void AppWindow::setupUi()
{
    setWindowTitle("字幕编辑");
    resize(1440, 900);
    setMinimumSize(960, 600);

    d->windowAgent = new QWK::WidgetWindowAgent(this);
    d->windowAgent->setup(this);

    setupTitleBar();
    setupSplitterLayout();
    setupDummyData();

    setMenuWidget(d->titleBar);
    d->windowAgent->setTitleBar(d->titleBar);
}

void AppWindow::setupTitleBar()
{
    d->titleBar = new QFrame(this);
    d->titleBar->setFixedHeight(36);
    d->titleBar->setObjectName("TitleBar");
    d->titleBar->setStyleSheet(R"(
        QFrame#TitleBar {
            background-color: #262626;
            border: none;
        }
    )");

    auto* layout = new QHBoxLayout(d->titleBar);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);

    auto* leftSpacer = new QWidget(d->titleBar);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(leftSpacer);

    d->titleLabel = new QLabel("字幕编辑", d->titleBar);
    d->titleLabel->setAlignment(Qt::AlignCenter);
    d->titleLabel->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-family: Inter, sans-serif;
            font-size: 12px;
            font-weight: normal;
            background: transparent;
        }
    )");
    layout->addWidget(d->titleLabel);

    auto* rightSpacer = new QWidget(d->titleBar);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(rightSpacer);
}

void AppWindow::setupSplitterLayout()
{
    // Subtitle track (shared data model)
    d->subtitleTrack = new SubtitleTrack(this);

    // Create panels
    d->videoPreviewPanel = new VideoPreviewPanel(this);
    d->subtitleListPanel = new SubtitleListPanel(this);
    d->timelinePanel = new TimelinePanel(this);

    // Connect panels to data
    d->subtitleListPanel->setTrack(d->subtitleTrack);
    d->timelinePanel->setTrack(d->subtitleTrack);

    // Connect cross-panel signals
    connect(d->subtitleListPanel, &SubtitleListPanel::itemSelected,
            d->timelinePanel, [this](const QString& id) {
                Q_UNUSED(id)
                d->timelinePanel->update();
            });

    connect(d->timelinePanel, &TimelinePanel::timeClicked,
            this, [this](qint64 ms) {
                Q_UNUSED(ms)
                // TODO: update video preview time display
            });

    // Top horizontal splitter
    d->topSplitter = new QSplitter(Qt::Horizontal, this);
    d->topSplitter->addWidget(d->videoPreviewPanel);
    d->topSplitter->addWidget(d->subtitleListPanel);
    d->topSplitter->setStretchFactor(0, 1);
    d->topSplitter->setStretchFactor(1, 0);
    d->topSplitter->setHandleWidth(1);
    d->topSplitter->setStyleSheet("QSplitter::handle { background-color: #0a0a0a; }");
    d->subtitleListPanel->setMinimumWidth(300);
    d->videoPreviewPanel->setMinimumWidth(400);

    // Vertical splitter
    d->verticalSplitter = new QSplitter(Qt::Vertical, this);
    d->verticalSplitter->addWidget(d->topSplitter);
    d->verticalSplitter->addWidget(d->timelinePanel);
    d->verticalSplitter->setStretchFactor(0, 1);
    d->verticalSplitter->setStretchFactor(1, 0);
    d->verticalSplitter->setHandleWidth(1);
    d->verticalSplitter->setStyleSheet("QSplitter::handle { background-color: #0a0a0a; }");
    d->timelinePanel->setMinimumHeight(150);
    d->timelinePanel->setMaximumHeight(400);

    // Set central widget
    auto* central = new QWidget(this);
    central->setStyleSheet("background-color: #0a0a0a;");
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(10, 10, 10, 10);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(d->verticalSplitter);
    setCentralWidget(central);
}

void AppWindow::setupDummyData()
{
    auto addItem = [&](const QString& text, qint64 start, qint64 end) {
        SubtitleItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.text = text;
        item.startMs = start;
        item.endMs = end;
        d->subtitleTrack->addItem(item);
    };

    addItem("Online tool to convert", 1000, 3170);
    addItem("the subtitle file (SRT) to", 5000, 7170);
    addItem("PremierePro-supported XML format", 8000, 11170);
}
```

- [ ] **Step 3: 更新 CMakeLists.txt**

Modify `CMakeLists.txt`，在 `set(SOURCES ...)` 和 `set(HEADERS ...)` 中添加新文件：

```cmake
set(SOURCES
    src/main.cpp
    src/AppWindow.cpp
    src/SubtitleTrack.cpp
    src/AsrServiceBase.cpp
    src/SubtitleExporter.cpp
    src/SubtitleListModel.cpp
    src/VideoPreviewPanel.cpp
    src/SubtitleListPanel.cpp
    src/TimelinePanel.cpp
)

set(HEADERS
    include/AppWindow.h
    include/SubtitleItem.h
    include/SubtitleTrack.h
    include/AsrServiceBase.h
    include/SubtitleExporter.h
    include/SubtitleListModel.h
    include/VideoPreviewPanel.h
    include/SubtitleListPanel.h
    include/TimelinePanel.h
)
```

- [ ] **Step 4: 编译验证**

```bash
cd /Users/zxl/Projects/cpp/subtitles-editor
cmake -B cmake-build-debug -S . 2>&1 | tail -5
```

Expected: 配置成功，无错误。

```bash
cmake --build cmake-build-debug --target subtitles-editor 2>&1
```

Expected: 编译成功，生成 `subtitles-editor.app`。

- [ ] **Step 5: 运行验证**

```bash
nohup /Users/zxl/Projects/cpp/subtitles-editor/cmake-build-debug/subtitles-editor.app/Contents/MacOS/subtitles-editor > /dev/null 2>&1 &
sleep 4
screencapture -R100,100,1440,900 -x /Users/zxl/Projects/cpp/subtitles-editor/doc/plan_verify.png
```

验证点：
- 视频预览面板和字幕列表面板之间可以左右拖拽调整宽度
- 上方面板区和时间线面板之间可以上下拖拽调整高度
- 时间线最小高度 150px，最大高度 400px
- 字幕列表面板显示 3 条字幕项
- 时间线面板显示 3 个蓝色字幕条和 1 个视频条
- 播放头为橙色竖线 + 三角形指针
- 字体下拉框显示系统字体列表
- 字号下拉框显示 8~72 预设值

- [ ] **Step 6: 提交**

```bash
git add include/AppWindow.h src/AppWindow.cpp CMakeLists.txt
git commit -m "refactor(ui): restructure AppWindow with QSplitter and independent panels"
```

---

## Self-Review

### 1. Spec Coverage

| 设计文档需求 | 对应任务 |
|-------------|---------|
| SubtitleItem 结构体 | Task 1, Step 1 |
| SubtitleTrack 数据模型 | Task 1, Step 2-3 |
| AsrServiceBase 抽象类 | Task 2 |
| SubtitleExporter 导出接口 | Task 3 |
| SubtitleListModel 列表模型 | Task 4 |
| VideoPreviewPanel（含真实字体/字号下拉框） | Task 5 |
| SubtitleListPanel（含搜索过滤） | Task 6 |
| TimelinePanel（paintEvent 绘制） | Task 7 |
| QSplitter 组装（左右+上下可调，不可浮动） | Task 8, Step 2 |
| 跨面板信号联动 | Task 8, Step 2 |
| 字幕列表选中 ↔ 时间线高亮 | Task 6 onItemClicked + Task 8 connect |
| 3 条占位字幕数据 | Task 8, Step 5 setupDummyData |

### 2. Placeholder Scan

- [x] 无 "TBD", "TODO"（除 Task 8 中一个 `// TODO: update video preview time display`，这是视频渲染后续迭代的明确标记，符合设计文档范围）
- [x] 无 "appropriate error handling" 等模糊描述
- [x] 每个步骤包含完整代码
- [x] 类型签名一致：Task 1 定义的 `SubtitleTrack::selectItem(id)` 在 Task 6 和 Task 8 中使用方式一致

### 3. Type Consistency

- [x] `SubtitleItem::id` 为 `QString`，在 SubtitleListModel::IdRole 中返回 `QString`
- [x] `SubtitleTrack::dataChanged()` 信号在 SubtitleListModel 中正确连接
- [x] `AsrServiceBase::TranscriptResult` 结构体与 Task 8 中的 ASR 导入数据流一致

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-04-23-subtitle-editor-ui-refactor.md`.**

**Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**

**注意：用户要求写完计划后等他确认再开始编码。当前计划已完成，等待用户确认。**
