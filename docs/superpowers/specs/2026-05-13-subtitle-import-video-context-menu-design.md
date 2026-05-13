# 字幕导入 + 视频轨道右键菜单设计文档

日期：2026-05-13

---

## 1. 背景与目标

当前导入视频时会自动触发 ASR，现改为：
- 导入视频 **不再** 自动触发 ASR
- 视频轨道右键菜单手动选择 ASR
- 新增 SRT 字幕文件导入功能

## 2. 需求清单

### 2.1 字幕导入
- 支持 `.srt` 格式，使用 `srtparser.h`（单头文件 C++ 库）
- 拖拽到时间线面板，按扩展名自动分流：视频 vs 字幕
- Empty State 点击导入也支持选择 `.srt`
- 导入前检查字幕轨道是否已有内容，非空则弹窗确认覆盖
- 仅清空字幕轨道数据（`SubtitleTrack::clear()`），视频轨道不受影响
- 字幕导入后 `SubtitleTrack::dataChanged()` 自动同步字幕列表面板和视频预览字幕叠加
- 当前无视频时，以字幕最晚结束时间作为总时长；有视频时取 `max(视频时长, 字幕最晚时间)`
- 导入后播放头跳到 0
- SRT 解析失败弹 `QMessageBox::critical`

### 2.2 视频轨道右键菜单
- 右键触发区域：视频轨道上实际蓝色 clip 条形区域
- 菜单项：属性、智能语音识别
- 菜单样式：暗色，贴合现有主题

### 2.3 视频属性弹窗
- 自定义 `QDialog`，暗色主题
- 展示字段：
  - 文件路径
  - 分辨率（宽×高）
  - 帧率
  - 时长
  - 音频采样率 / 通道数
  - 视频编码格式
  - 文件大小

### 2.4 智能语音识别（ASR）
- 通过视频轨道右键菜单触发
- 执行前检查字幕轨道是否已有内容，非空则弹窗确认覆盖
- 确认后调用 `startAsrPipeline`
- ASR 生成的字幕写入字幕轨道，同样触发 `dataChanged()` 同步 UI

## 3. 架构设计

### 3.1 新增信号

**TimelinePanel 新增信号：**
```cpp
void subtitleFileDropped(const QString &path);
void videoAsrRequested();           // 右键 → ASR
void videoPropertyRequested();      // 右键 → 属性
```

### 3.2 数据流

```
用户拖入 .srt
  → TimelinePanel::dropEvent
    → 识别扩展名 .srt
      → emit subtitleFileDropped(path)
        → AppWindow 接收
          → 确认覆盖（如有）
            → SubtitleTrack::clear()
            → srtparser 解析
            → SubtitleTrack::addItem() 逐条添加
            → 更新 totalDurationMs_ = max(视频, 字幕最晚)
            → seekTo(0)

用户右键视频 clip
  → TimelinePanel::contextMenuEvent (在 clip 区域内)
    → QMenu 弹出
      → 属性
        → emit videoPropertyRequested()
          → AppWindow 弹出 VideoPropertyDialog
      → 智能语音识别
        → emit videoAsrRequested()
          → AppWindow 确认覆盖（如有）
            → TimelinePanel::startAsrPipeline(path)
```

### 3.3 模块职责

| 模块 | 职责 |
|---|---|
| `TimelinePanel` | 拖拽接收、右键菜单触发、信号发射、视频属性存储 |
| `AppWindow` | 信号连接、弹窗处理、调用 ASR/SRT 导入逻辑 |
| `SubtitleTrack` | 数据模型，`clear()`/`addItem()` 驱动 UI 同步 |
| `VideoPropertyDialog` | 自定义弹窗，展示视频元数据 |
| `srtparser.h` | SRT 文件解析（外部库，单头文件） |

### 3.4 总时长管理

```cpp
// TimelinePanel 中维护
void updateTotalDuration(qint64 videoDuration, qint64 subtitleMaxEnd) {
    totalDurationMs_ = qMax(videoDuration, subtitleMaxEnd);
    updateScrollBar();
    canvas_->update();
}
```

- 导入视频时：更新 `videoDurationMs_`，重新计算 `totalDurationMs_`
- 导入字幕时：计算字幕最晚结束时间，重新计算 `totalDurationMs_`

## 4. UI 规范

### 4.1 右键菜单
- 暗色背景 `#1e1e1e`
- 文字色 `#d1d5db`
- 悬停色 `#2a2a2a`
- 字号 13px
- 无分割线（单列表项）

### 4.2 属性弹窗
- 窗口背景 `#1e1e1e`
- 键值对左右对齐布局
- 标签 `#9ca3af`，值 `#d1d5db`
- 底部确认按钮（仅关闭）
- 窗口标题：`视频属性`

### 4.3 覆盖确认弹窗
- `QMessageBox::question`
- 标题：`确认覆盖`
- 内容：`字幕轨道已有内容，继续导入将清空现有字幕，是否继续？`
- 按钮：`继续` / `取消`

## 5. 边界情况

| 场景 | 处理 |
|---|---|
| 拖入非视频非 srt 文件 | 忽略 |
| SRT 解析失败 | `QMessageBox::critical("字幕文件格式错误")` |
| 字幕导入时无视频 | 以字幕最晚时间作为总时长，视频轨道仍为空 |
| ASR 时字幕轨道为空 | 直接执行，不弹窗 |
| 右键非 clip 区域 | 不弹出菜单 |
| 无视频时右键 clip | 理论上 clip 不存在，此场景不发生 |

## 6. 文件改动清单

### 新增
- `include/srtparser.h` — 复制外部库
- `include/VideoPropertyDialog.h`
- `src/VideoPropertyDialog.cpp`

### 修改
- `include/TimelinePanel.h` — 新增信号、成员
- `src/TimelinePanel.cpp` — 拖拽分流、右键菜单、属性存储
- `src/AppWindow.cpp` — 信号连接、弹窗处理、SRT 导入逻辑
- `src/AppWindow.h` — 新增槽函数声明（如有需要）

## 7. 关键接口

### TimelinePanel 新增
```cpp
signals:
    void subtitleFileDropped(const QString &path);
    void videoAsrRequested();
    void videoPropertyRequested();

public:
    void startAsrPipeline(const QString &localPath);
    void setMediaFilePath(const QString &path);  // 已存在，需存完整路径

private:
    QString mediaFilePath_;        // 新增：完整路径（原 mediaFileName_ 仅文件名）
```

### AppWindow 新增处理
```cpp
private slots:
    void onSubtitleFileDropped(const QString &path);
    void onVideoAsrRequested();
    void onVideoPropertyRequested();
```
