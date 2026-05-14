# 时间线字幕 Clip 拖拽调整

## 概述

在 TimelinePanel 上支持字幕 clip 的拖拽移动和边缘缩放。

## 交互模式

三种交互状态，通过鼠标按下位置区分：

| 鼠标按下位置 | 进入状态 | 行为 |
|------------|---------|------|
| clip 中间区域 | ClipMove | 整体左右移动 clip |
| clip 左边缘 6px | ClipResizeLeft | 调整 startMs |
| clip 右边缘 6px | ClipResizeRight | 调整 endMs |
| 刻度尺区域（Y<36） | Seek | 现有播放头跳转行为 |
| 轨道空白区域 | 无操作 | - |

## 光标规则

- clip 左边缘 → 自定义光标 `resize-left.svg`
- clip 右边缘 → 自定义光标 `resize-right.svg`
- clip 中间 → 默认箭头
- 其他 → 保持默认

新增图标需加入 `resources/resources.qrc`。

## 碰撞规则

所有时间边界使用严格不等式（不允许等于）：

- **左移**：`newStartMs > 前一个 clip 的 endMs`
- **右移**：`newEndMs < 后一个 clip 的 startMs`
- **左边缘缩放**：`newStartMs > 前一个 clip 的 endMs`
- **右边缘缩放**：`newEndMs < 后一个 clip 的 startMs`

## 最小持续时间

clip 缩放时强制最小持续时间 100ms：`endMs - startMs >= 100`。

## 边界处理

- `newStartMs >= 0`
- `newEndMs <= totalDurationMs - 1`

## 提交策略

拖拽过程中**不调用 `track_->updateItem()`**，只在 `mouseReleaseEvent` 时一次性提交。拖拽过程中通过本地变量存储临时时间值，绘制时使用临时值提供视觉预览。

## 相邻 clip 查找

拖拽时需要查找当前 clip 的时间顺序上的前后相邻 clip。SubtitleTrack 中的 items 无排序保证，需每次查找时遍历确定相邻关系。

## 涉及文件

- `src/TimelinePanel.cpp` — 核心交互逻辑
- `include/TimelinePanel.h` — 新增状态枚举、成员变量、方法声明
- `resources/resources.qrc` — 新增两个光标 SVG
