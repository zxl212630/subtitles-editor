# 语音识别(ASR)进度反馈功能设计文档

**文档日期**: 2026-05-19  
**相关特性**: 语音识别进度展示、三阶段动画、异常处理与任务取消

## 1. 需求背景
目前软件在进行语音识别时（从视频提取音频 -> 上传至 OSS -> 腾讯云 ASR 识别），界面缺乏明确的进度反馈。用户点击“识别”后无法感知当前处于哪个阶段，也无法中途取消任务，体验上容易产生“卡死”的错觉。

本设计的目的是实现一个全局的进度对话框（`AsrProgressDialog`），通过动态视觉隐喻和明确的异常处理逻辑，提升语音识别过程的透明度和可控性。

## 2. UI 架构与动效设计

### 2.1 整体布局 (`AsrProgressDialog`)
基于 `QDialog` 实现，采用应用统一定义的 ThemeManager 样式 (`@primary_color`, `@bg_panel` 等)。
对话框主要由三个区域组成：
1. **顶层 - 步骤指示器 (Step Indicator)**：显示 `1. Extraction` -> `2. Upload` -> `3. Recognition`，让用户明确整体流程。
2. **中层 - 动画展示区 (Animation Area)**：核心视觉区域，根据不同阶段展示不同的 SVG/UI 动画。
3. **底层 - 状态信息与操作区**：显示当前阶段的文本描述（支持国际化 `tr()`），以及一个 `Cancel` 按钮。

### 2.2 三阶段视觉隐喻 (Visual Metaphor)
为了避免生硬的文字和不准确的百分比，采用 **“[来源] -> [流转] -> [目标]”** 的视觉结构。

#### Stage 1: 音频提取 (Extraction)
*   **动作**: `AudioTranscoder` 使用 FFmpeg 从视频中提取音频流。
*   **来源 (Left)**: 一个带有胶片孔和播放箭头的**视频图标**，播放箭头呈呼吸闪烁状态。
*   **流转 (Middle)**: 一组细碎的蓝色数据粒子（`p-dot`）从左向右流动。
*   **目标 (Right)**: **音频波形**（由5根高度交替变化的线条组成）正在逐渐形成。

#### Stage 2: 音频上传 (Upload)
*   **动作**: `OssUploader` 将提取的音频上传至云端对象存储。
*   **来源 (Left)**: 继承自 Stage 1 的**音频波形**图标。
*   **流转 (Middle)**: 同样的数据粒子流，保持视觉连贯性。
*   **目标 (Right)**: 一个对称的**云朵图标**（基于 SVG 路径绘制），伴随整体的呼吸缩放和外发光。

#### Stage 3: 字幕识别 (Recognition)
*   **动作**: `TencentAsrService` 创建识别任务并轮询状态。
*   **来源 (Left)**: 继承自 Stage 2 的云朵，但此时云朵内部包裹着跳动的**音频波形**。
*   **流转 (Middle)**: 粒子流效果。
*   **目标 (Right)**: 一个正在生成的**字幕文本块**，内部线条采用打字机（Typewriter）效果逐行显现，并伴随从上到下的扫描光束。

## 3. C++ 接口与逻辑设计

### 3.1 进度管线拆分
需要将现有的串行 Lambda 回调拆分或注入进度信号，以便 UI 层捕获状态。

1.  **AudioTranscoder**: 需确认/添加阶段开始和完成事件。由于转码极快，可不用细分百分比，只需抛出 `started` 和 `finished`。
2.  **OssUploader**: 监听底层的 `QNetworkReply::uploadProgress`（由于采用动画方案，此项为备用，主要监听完成信号）。
3.  **TencentAsrService**: 已有的 `transcribeProgress` 可以保留用于状态触发，但 UI 层面主要依赖轮询的回调来驱动 Stage 3 动画保持活跃。

### 3.2 错误处理 (Error Handling)
当任何一个管线节点发出 `error` / `failed` 信号时：
1.  `AsrProgressDialog` 捕获该错误。
2.  **UI 更新**：
    *   停止所有动画 (Animation Group `pause()`或`stop()`)。
    *   当前阶段图标、步骤节点颜色变为**红色** (`#ef4444`)。
    *   底部状态文本更新为具体的错误字符串（如“OSS 上传失败：权限不足”）。
    *   `Cancel` 按钮文本变为 `Close`。
3.  **逻辑处理**：停止触发下一阶段任务，释放资源，等待用户手动点击 `Close` 关闭窗口。

### 3.3 取消机制 (Cancellation Logic)
用户点击对话框的 `Cancel` 按钮时：
1.  **UI 更新**：立即关闭 `AsrProgressDialog`。
2.  **后台清理**：
    *   对话框发出 `canceled()` 信号。
    *   `TimelinePanel::startAsrPipeline` 捕获后，调用当前正在运行的组件的 `abort()` 方法。
    *   `AudioTranscoder::abort()`: 调用 `QProcess::kill()` 终止 FFmpeg。
    *   `OssUploader::abort()`: 调用 `QNetworkReply::abort()`。
    *   `TencentAsrService::abort()`: 停止轮询 `QTimer`。
    *   最后对所有组件调用 `deleteLater()` 防止内存泄漏。

## 4. 技术实现细节 (Qt)
*   动画控制：使用 `QPropertyAnimation` 对 Widget 的 `pos`, `opacity`, `size` 等进行控制。
*   粒子效果：可使用定时器触发重绘，或者多个组合的 `QPropertyAnimation` 实现。若为了极致性能，可直接在自定义 Widget 的 `paintEvent` 中根据 `QTime::currentTime()` 计算偏移量进行绘制。
*   主题适配：所有关于蓝色（`#4a9eff`）的硬编码均需通过 `ThemeManager::getPrimaryColor()` 动态获取，并在切换主题时触发重绘。

## 5. 未决事项 (TBD) / 需要跟进
*   在 `AudioTranscoder` 和 `OssUploader` 类中是否已经实现了健壮的 `abort()` 逻辑？需要在开发阶段确认并补全。