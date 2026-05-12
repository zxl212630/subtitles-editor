# Timeline Drag-to-Seek Preview Design

## Context

当前 TimelinePanel 仅支持点击跳转（mousePressEvent 直接 seek），不支持拖拽指针进行实时视频预览。用户需要拖拽指针时能实时预览视频画面，且指针必须跟手、无延迟。

## Requirements

1. 往前和往后拖拽都能实时预览视频画面
2. 拖拽过程中预览要流畅
3. 指针（playhead）必须跟手，不能有延迟
4. 拖拽结束后停在最终位置（不恢复播放）

## Architecture

### 交互三阶段

| 阶段 | 事件 | 指针行为 | 视频行为 |
|------|------|----------|----------|
| 开始 | mousePressEvent (左键) | 记录起点，进入拖拽模式 | 如果正在播放，先暂停 |
| 拖拽 | mouseMoveEvent | 每帧更新 currentTimeMs_，立即重绘 | 按 1000/fps 间隔调用 previewSeek |
| 结束 | mouseReleaseEvent | 确认最终位置 | 执行一次精确 seek() |

点击 vs 拖拽区分：按下时记录位置，mouseMove 超过 3px 阈值才进入拖拽模式，否则 mouseUp 时按点击处理（保持现有行为）。

### 视频预览机制

#### TimelinePanel 侧节流

- mouseMove 时立即更新 `currentTimeMs_` 和重绘指针
- 视频预览按帧率节流：间隔 = `1000 / videoFps_` 毫秒
- 25fps → 40ms, 30fps → 33ms, 50fps → 20ms, 60fps → 16ms
- `videoFps_` 从 `FFmpegDecoder::fps()` 获取，视频加载后缓存

#### MediaPlayer::previewSeek(qint64 ms)

新增槽函数，区别于普通 `seek()`：

- 普通 `seek()`：暂停 → 清空 → requestSeek → 重启播放，重量级操作
- `previewSeek()`：仅更新原子变量 `seekTargetMs_`，不清空队列、不暂停/重启，轻量级
- 解码线程在循环顶部检测到 `seekRequested_` 后执行 `performSeek`，解码出第一帧直接渲染（不追帧到精确位置）
- 第一次调用时需要初始化 seekPreviewMode（设置 decoder 为 playing、启动定时器），后续调用只更新目标

#### 信号链路

```
TimelinePanel::mouseMove
  → 指针立即更新 + 重绘
  → (节流间隔到达) emit previewSeekRequested(ms)
    → MediaPlayer::previewSeek(ms)
      → 更新 seekTargetMs_ 原子变量（自然合并快速拖拽）
        → 解码线程 performSeek + 解码第一帧 → 入队
          → MediaPlayer 渲染循环取帧 → renderFrame
```

### 解码优化

#### Layer 2: 跳过追帧

当前 seekPreviewMode 会逐帧比较 pts >= target 才渲染。拖拽模式改为：performSeek 后解码出的第一帧直接渲染，不等待精确位置。

改动点：`MediaPlayer::onPlaybackTimer()` 中 seekPreviewMode 分支，拖拽时跳过 pts 比较，直接渲染第一个可用帧。

#### Layer 3: 帧池复用

- `FFmpegDecoder` 持有复用 `AVFrame*` 成员，解码循环中 `av_frame_unref` 后重用，不再每次 alloc/free
- RGBA 数据用预分配的 `QByteArray` 缓冲区
- `SoftwareVideoRenderer` 减少 `.copy()` 深拷贝

### 帧乱序防护

快速拖拽时，新 seek 可能在旧 seek 解码完成前触发。防护机制：

1. `requestSeek` 时同时清空视频队列（丢弃旧 seek 产出的帧）
2. 解码循环检测到 `seekRequested_` 后执行 `performSeek`，从新位置开始解码
3. 因为队列已清空，取到的帧一定是最新 seek 的产出，无需 generation 检查

如果解码速度跟不上拖拽速度（极端情况），画面会比指针"慢半拍"，但不会冻住、不会闪回。拖慢后视频会追上指针位置。

### 拖拽结束

- mouseUp 时执行一次精确 `seek(currentTimeMs_)`
- 退出拖拽模式
- 不恢复之前的播放状态

## Files to Modify

| 文件 | 改动 |
|------|------|
| `include/TimelinePanel.h` | 新增 `isDragging_`、`dragStartX_`、`lastPreviewMs_`、`videoFps_`，声明 mouseMoveEvent/mouseReleaseEvent，新增信号 `previewSeekRequested(qint64)` |
| `src/TimelinePanel.cpp` | 实现拖拽三阶段逻辑，setMouseTracking(true)，节流控制，修改 mousePressEvent 区分点击/拖拽 |
| `include/MediaPlayer.h` | 新增 `previewSeek(qint64 ms)` 槽函数 |
| `src/MediaPlayer.cpp` | 实现 previewSeek（轻量级 seek），修改 seekPreviewMode 为"显示第一帧"策略 |
| `include/FFmpegDecoder.h` | 新增复用 `AVFrame*` 成员 |
| `src/FFmpegDecoder.cpp` | 解码循环复用 frame，seek 时清空视频队列 |
| `src/AppWindow.cpp` | 连接 TimelinePanel::previewSeekRequested 到 MediaPlayer::previewSeek |

## Out of Scope

- 硬件加速解码（VideoToolbox）— 留待后续优化
- skip_frame 跳过 B 帧 — 留待与硬解一起优化
- 关键帧缩略图预缓存
- 低分辨率预览解码器

## Verification

1. 加载一个视频文件
2. 在时间线任意位置点击，验证现有点击跳转仍正常
3. 按住左键拖拽指针，验证：
   - 指针实时跟随鼠标（无延迟）
   - 视频画面持续更新（不冻住、不闪回）
   - 往前和往后拖拽都能预览
4. 快速拖拽，验证画面跟上（可能略慢于指针但持续更新）
5. 松开鼠标，验证视频停在最终位置
6. 拖拽过程中播放按钮应处于暂停状态
7. 构建通过：`cmake --build cmake-build-debug`
