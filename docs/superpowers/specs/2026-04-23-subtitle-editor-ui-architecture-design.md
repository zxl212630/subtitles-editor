# 字幕编辑器 UI 架构设计

**日期：** 2026-04-23  
**范围：** 将静态示意图重构为业务驱动的可交互面板架构（视频预览暂为占位区，后续迭代）

---

## 1. 背景与目标

当前 `AppWindow.cpp` 包含 748 行硬编码的静态界面（假字幕文本、假时间码、绝对定位的轨道条），所有面板逻辑混杂在一起，没有数据模型支撑。

本次设计的目标：
1. **拆分独立面板类**，每个面板有自己的职责和文件
2. **建立字幕数据模型**，作为单一数据源驱动字幕列表和时间线
3. **实现面板联动**：字幕列表选中 ↔ 时间线高亮 ↔ 视频预览跳转
4. **支持尺寸调整**：视频预览 ↔ 字幕列表（左右）、上方面板 ↔ 时间线（上下），不可浮动
5. **视频预览暂保留黑色占位区**，解码渲染后续迭代

---

## 2. 整体架构

```
AppWindow (QMainWindow)
├── titleBar (QFrame) — QWindowKit 托管，不变
├── centralWidget
│   └── verticalSplitter (QSplitter, Qt::Vertical)
│       ├── topSplitter (QSplitter, Qt::Horizontal)
│       │   ├── videoPreviewPanel (VideoPreviewPanel)
│       │   └── subtitleListPanel (SubtitleListPanel)
│       └── timelinePanel (TimelinePanel)
```

- `topSplitter`：左右可调，视频预览最小宽度 400px，字幕列表最小宽度 300px
- `verticalSplitter`：上下可调，时间线最小高度 150px，最大高度 400px
- 所有 QSplitter 分隔条自定义为 1px 暗色线（`#0a0a0a`），无凸起效果

---

## 3. 面板拆分

### 3.1 VideoPreviewPanel

**文件：** `src/VideoPreviewPanel.h`, `src/VideoPreviewPanel.cpp`

**职责：**
- 顶部工具栏：字体选择、字号选择、B/I/U、左/中/右对齐
- 中部视频显示区：当前为 `#000000` 黑色占位矩形（后续替换为视频渲染）
- 底部控制条：播放/暂停/上一帧/下一帧、进度条、时间显示、音量、全屏

**当前占位行为：**
- 点击播放按钮：无实际视频，仅切换图标状态
- 进度条：可拖拽，但无实际视频进度同步
- 时间显示：`00:00:00:00 / 00:00:00:00`（后续接入视频时长）

### 3.2 SubtitleListPanel

**文件：** `src/SubtitleListPanel.h`, `src/SubtitleListPanel.cpp`

**职责：**
- 标签页头：字幕 / 预设 / 自定义 / 动画（仅"字幕"页有效，其余为占位按钮）
- 搜索栏：QLineEdit，过滤字幕列表（placeholder："请输入查找内容"）
- 字幕列表：`QListView` + 自定义 `QAbstractListModel`（`SubtitleListModel`）

**列表项显示：**
- 左侧：时间码（开始 / 结束）
- 中间：字幕文本
- 右侧：分割按钮、删除按钮

**交互：**
- 单击行 → `SubtitleTrack::selectItem(id)` → 触发跨面板联动
- 双击行 → 进入编辑模式（后续迭代）
- 搜索过滤 → `SubtitleListModel::setFilterText()` → 模型刷新

### 3.3 TimelinePanel

**文件：** `src/TimelinePanel.h`, `src/TimelinePanel.cpp`

**职责：**
- 时间刻度尺：根据视频总时长动态计算主刻度/次刻度密度（当前固定 12 个主刻度占位）
- 轨道区域：
  - 字幕轨道：显示 `SubtitleTrack` 中所有字幕项为蓝色条，长度按时间比例
  - 视频轨道：显示已导入视频片段（当前固定蓝色条占位）
- 播放头：橙色竖线，位置 = 当前时间 / 总时长 * 轨道宽度

**绘制方式：** `paintEvent` 自定义绘制，不使用子控件摆放轨道条。

**交互：**
- 点击轨道区域 → 跳转到对应时间点 → 更新播放头位置
- 点击字幕条 → `SubtitleTrack::selectItem(id)` → 跨面板联动

---

## 4. 数据模型

### 4.1 SubtitleItem

```cpp
struct SubtitleItem {
    QString id;              // UUID
    QString text;            // 字幕文本
    qint64 startMs = 0;      // 开始时间（毫秒）
    qint64 endMs = 0;        // 结束时间（毫秒）
    bool selected = false;   // 选中状态
};
```

### 4.2 SubtitleTrack

```cpp
class SubtitleTrack : public QObject {
    Q_OBJECT
public:
    void clear();
    void addItem(const SubtitleItem& item);
    void removeItem(const QString& id);
    void updateItem(const QString& id, const SubtitleItem& newItem);
    void selectItem(const QString& id);
    
    const QList<SubtitleItem>& items() const;
    const SubtitleItem* selectedItem() const;

signals:
    void itemAdded(const SubtitleItem& item);
    void itemRemoved(const QString& id);
    void itemUpdated(const QString& id);
    void itemSelected(const QString& id);
    void dataChanged();          // 批量变更后发射
};
```

**设计原则：**
- `SubtitleTrack` 是**唯一可变数据源**，所有面板的字幕变更必须通过它
- 面板间不直接通信，通过 `SubtitleTrack` 的信号解耦

### 4.3 SubtitleListModel

```cpp
class SubtitleListModel : public QAbstractListModel {
    Q_OBJECT
public:
    void setTrack(SubtitleTrack* track);
    void setFilterText(const QString& text);
    
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    
private:
    SubtitleTrack* track_ = nullptr;
    QString filterText_;
    QList<int> filteredIndices_;  // 搜索过滤后的索引映射
};
```

---

## 5. 数据流

### 5.1 ASR 导入

```
用户触发导入（文件选择 / 拖拽 / ASR API 调用）
    ↓
AppWindow 调用 ASR Service（大模型接口）
    ↓
返回 JSON：{segments: [{text, start, end}, ...]}
    ↓
AppWindow::importSubtitles(json)
    ↓
SubtitleTrack::clear() + 循环 addItem()
    ↓
SubtitleTrack::dataChanged()
    ↓
├─→ SubtitleListModel 刷新 → QListView 更新
├─→ TimelinePanel::update() → 重绘轨道条
└─→ VideoPreviewPanel 更新总时长显示（占位）
```

### 5.2 选中联动

```
用户在 SubtitleListPanel 点击第 N 行
    ↓
SubtitleListModel::data(index, SelectionRole) → 获取 id
    ↓
SubtitleTrack::selectItem(id)
    ↓
SubtitleTrack::itemSelected(id)
    ↓
├─→ SubtitleListPanel：确保该行可见（滚动到视口）
├─→ TimelinePanel：高亮对应字幕条，重绘
└─→ VideoPreviewPanel：时间显示跳转到 startMs（占位）
```

### 5.3 删除字幕

```
用户点击某行的删除按钮
    ↓
SubtitleTrack::removeItem(id)
    ↓
SubtitleTrack::itemRemoved(id)
    ↓
├─→ SubtitleListModel 移除对应行
└─→ TimelinePanel 移除对应轨道条
```

---

## 6. 导出接口

```cpp
// include/SubtitleExporter.h
class SubtitleExporter {
public:
    static bool exportToSRT(const SubtitleTrack& track, const QString& filePath);
    static bool exportToASS(const SubtitleTrack& track, const QString& filePath);
    static bool exportToVTT(const SubtitleTrack& track, const QString& filePath);
    static bool exportToPremiereXML(const SubtitleTrack& track, const QString& filePath);
};
```

**设计原则：**
- 纯静态工具类，无状态
- 输入 `const SubtitleTrack&`，输出文件
- 错误处理：返回 `bool`，失败时打印 `qWarning`
- 格式细节后续按各格式规范实现

---

## 7. 类文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `include/AppWindow.h` | 修改 | 移除面板 setup 方法，改为持有 QSplitter 和面板指针 |
| `src/AppWindow.cpp` | 修改 | 精简为组装逻辑 |
| `include/SubtitleItem.h` | 新增 | 字幕项数据结构 |
| `include/SubtitleTrack.h` | 新增 | 字幕轨道数据模型 |
| `src/SubtitleTrack.cpp` | 新增 | 数据模型实现 |
| `include/SubtitleListModel.h` | 新增 | 列表模型 |
| `src/SubtitleListModel.cpp` | 新增 | 列表模型实现 |
| `include/VideoPreviewPanel.h` | 新增 | 视频预览面板 |
| `src/VideoPreviewPanel.cpp` | 新增 | 视频预览面板实现（含占位视频区） |
| `include/SubtitleListPanel.h` | 新增 | 字幕列表面板 |
| `src/SubtitleListPanel.cpp` | 新增 | 字幕列表面板实现 |
| `include/TimelinePanel.h` | 新增 | 时间线面板 |
| `src/TimelinePanel.cpp` | 新增 | 时间线面板实现 |
| `include/SubtitleExporter.h` | 新增 | 导出接口声明 |
| `src/SubtitleExporter.cpp` | 新增 | 导出接口实现（占位） |

---

## 8. 不在本次范围（后续迭代）

| 功能 | 说明 |
|------|------|
| 视频解码与渲染 | VideoPreviewPanel 保留黑色占位区，后续接入 FFmpeg/QMediaPlayer |
| 实际播放控制 | 播放/暂停按钮仅切换 UI 状态，不与视频同步 |
| ASR API 接入 | 提供 `importSubtitles(json)` 接口，实际网络调用后续实现 |
| 字幕编辑（双击改文本/时间码） | 列表项编辑功能后续实现 |
| 导出格式完整实现 | 导出接口框架搭建，各格式细节后续按规范实现 |
| 预设/自定义/动画标签页 | 目前仅为占位按钮 |

---

## 9. Self-Review

- [x] 无 TBD / TODO 占位符
- [x] 数据流方向一致：SubtitleTrack 是唯一可变源
- [x] 面板间无直接耦合，通过信号通信
- [x] 范围明确：不含视频渲染、ASR 网络、完整导出
- [x] 无歧义：QSplitter 不可浮动，尺寸限制已说明
