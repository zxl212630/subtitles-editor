# 音视频预览功能设计文档

**日期**: 2026-04-26  
**功能**: 音视频预览（支持帧级精确 seek、流畅播放、字幕叠加）  
**状态**: 已审核待实现

---

## 1. 背景与目标

当前 `VideoPreviewPanel` 仅有占位符 UI（黑色矩形、静态控件），无法预览拖入的音视频文件。`TimelinePanel` 支持拖入文件并触发 ASR，但 ASR 完成前用户无法看到/听到任何内容。

### 目标
- 拖入音视频后，视频画面立即显示，音频可播放
- **帧级精确 seek**：点击时间轴任意位置，画面精确跳转到对应帧
- **流畅 seek**：拖动 playhead 或连续 seek 时不卡顿
- **高效播放**：软解方案在 macOS M 系列上可流畅播放 4K H264/H265
- **字幕叠加**：播放时实时显示当前时间对应的字幕文字
- **可演进**：当前软解软渲，但架构预留硬件加速（VideoToolbox）和 GPU 渲染接口

### 非目标（后续迭代）
- 8K 高码率硬解（架构预留接口，本次不实现）
- GPU 渲染（OpenGL/Metal，架构预留接口）
- 音频波形图（纯音频文件时可选，本次仅显示黑色背景）
- 导出带字幕的合成视频

---

## 2. 总体架构

### 2.1 分层设计

采用**接口隔离 + 分层抽象**，当前实现位于 "Software" 层，后续升级只需替换对应层实现，上层（MediaPlayer、UI）零改动。

```
┌─────────────────────────────────────────────────────────────┐
│                      VideoPreviewPanel                        │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │  播放控制栏   │  │  视频显示区   │  │  字幕叠加层 (QPainter)│  │
│  │ Play/Pause  │  │ (Software   │  │                     │  │
│  │ Seek/Time   │  │  VideoRenderer│  │                     │  │
│  │ 逐帧前进后退  │  │  via QPainter)│  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────┴─────────┐
                    ▼                   ▼
          ┌─────────────────┐  ┌─────────────────┐
          │  MediaPlayer    │  │  SubtitleTrack  │
          │  (播放控制器)    │  │  (字幕数据模型)   │
          │  · 状态机管理    │  │                 │
          │  · 播放/暂停/seek│  │                 │
          │  · 逐帧控制     │  │                 │
          └────────┬────────┘  └─────────────────┘
                   │
         ┌─────────┴─────────┐
         ▼                   ▼
┌─────────────────┐  ┌─────────────────┐
│ FFmpegDecoder   │  │ QtAudioOutput   │
│ (当前: Software │  │ (当前实现)       │
│  decode)        │  │                 │
│ 预留: HW Decoder│  │ 预留: SdlAudio  │
│  interface      │  │  Output interface│
└─────────────────┘  └─────────────────┘
         │
         ▼
┌─────────────────┐
│ VideoRenderer   │
│ (当前: Software │
│  Renderer via   │
│  QPainter+QImage)│
│ 预留: OpenGL/   │
│ Metal Renderer  │
└─────────────────┘
```

### 2.2 核心抽象接口

```cpp
// ============================================================
// MediaDecoder 抽象 — 负责解封装 + 解码
// 当前实现: FFmpegDecoder (avformat + avcodec)
// 预留实现: VideoToolboxDecoder / VADecoder
// ============================================================
class MediaDecoder {
public:
    virtual ~MediaDecoder() = default;

    virtual bool open(const QString& path) = 0;
    virtual void close() = 0;

    // 精确 seek 到 targetMs（毫秒）
    // 内部自动寻找最近关键帧，返回实际 seek 到的位置
    virtual qint64 seek(qint64 targetMs) = 0;

    // 解码一帧视频，返回 std::nullopt 表示 EOS 或需要更多 packet
    virtual std::optional<DecodedVideoFrame> decodeVideoFrame() = 0;

    // 解码一帧音频（返回音频 samples）
    virtual std::optional<DecodedAudioFrame> decodeAudioFrame() = 0;

    // 元数据查询
    virtual qint64 durationMs() const = 0;
    virtual double fps() const = 0;
    virtual QSize videoSize() const = 0;
    virtual bool hasVideo() const = 0;
    virtual bool hasAudio() const = 0;
};

// ============================================================
// VideoRenderer 抽象 — 负责将解码后的帧显示到屏幕
// 当前实现: SoftwareVideoRenderer (swscale → QImage → QPainter)
// 预留实现: OpenGLVideoRenderer / MetalVideoRenderer
// ============================================================
class VideoRenderer {
public:
    virtual ~VideoRenderer() = default;

    // 渲染一帧到关联的 QWidget 上
    virtual void renderFrame(const DecodedVideoFrame& frame) = 0;

    // 通知渲染器显示区域尺寸变化
    virtual void onViewportResized(int width, int height) = 0;

    // 返回内部使用的 QWidget，用于嵌入到 VideoPreviewPanel 布局中
    virtual QWidget* widget() = 0;

    // 清空显示（显示黑色背景）
    virtual void clear() = 0;
};

// ============================================================
// AudioOutput 抽象 — 负责音频播放
// 当前实现: QtAudioOutput (QAudioSink)
// 预留实现: SdlAudioOutput / MiniaudioOutput
// ============================================================
class AudioOutput {
public:
    virtual ~AudioOutput() = default;

    // 打开音频设备
    virtual bool open(int sampleRate, int channels, AVSampleFormat format) = 0;
    virtual void close() = 0;

    // 写入音频数据（会被播放线程调用）
    virtual void write(const uint8_t* data, int size) = 0;

    // 获取已播放的样本数（用于计算 audioClock）
    virtual qint64 samplesPlayed() const = 0;

    // 清空缓冲（seek 时调用）
    virtual void flush() = 0;

    virtual void setVolume(float volume) = 0;
    virtual bool isOpen() const = 0;
};
```

---

## 3. 音视频同步方案

### 3.1 策略：音频主时钟（Audio Master Clock）

音频是连续流不可跳帧，视频是离散帧可丢帧。因此**以音频播放进度为时间基准，视频帧根据偏差决定显示时机**。

```
┌─────────────────────────────────────────────────────────────┐
│                     音频主时钟同步流程                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   解码线程 (独立 QThread)         播放控制 (主线程)           │
│                                                             │
│   ┌──────────────┐              ┌──────────────────────┐     │
│   │ FFmpeg 解封装  │              │ AudioOutput 播放中    │     │
│   │ 分离音/视频包  │              │                      │     │
│   └──────┬───────┘              │  audioClock =         │     │
│          │                      │  samplesPlayed /      │     │
│          ▼                      │  sampleRate           │     │
│   ┌──────────────┐              │  = 当前播放时间(秒)    │     │
│   │  视频帧队列   │              │                      │     │
│   │  (最多3帧)   │◄─────────────│  定时查询 audioClock   │     │
│   └──────┬───────┘              └──────────────────────┘     │
│          │                                                  │
│          ▼                                                  │
│   ┌──────────────────────────────────────────┐              │
│   │  同步决策 (每帧调用)                        │              │
│   │                                          │              │
│   │  delayMs = (videoPts - audioClock) * 1000 │              │
│   │                                          │              │
│   │  if delayMs > 5:     视频太早 → usleep    │              │
│   │  if delayMs < -40:   视频太晚 → 丢帧      │              │
│   │  else:               立即渲染             │              │
│   └──────────────────────────────────────────┘              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 关键公式

```cpp
// 音频时钟（秒）
double audioClock = audioOutput_->samplesPlayed() / static_cast<double>(sampleRate);

// 视频帧 PTS（秒）
double videoPts = av_frame_get_best_effort_timestamp(frame) * av_q2d(videoStreamTimeBase);

// 同步偏差（毫秒）
double delayMs = (videoPts - audioClock) * 1000.0;

if (delayMs > 5.0) {
    // 精确 sleep 到显示时机，使用 QElapsedTimer 做 busy-wait（精度更高）
    qint64 sleepUs = static_cast<qint64>(delayMs * 1000);
    // busy-wait loop for microsecond precision
} else if (delayMs < -40.0) {
    // 超过 40ms 偏差，丢帧
    dropCount_++;
    return; // 不渲染
} else {
    // 在 ±5ms 容差内，立即渲染
    videoRenderer_->renderFrame(decodedFrame);
}
```

### 3.3 特殊场景处理

| 场景 | 处理方式 |
|------|---------|
| **Seek 后** | 清空音视频解码缓冲队列；重置 `audioClock = seekTarget`；音频 `flush()`；重新从关键帧开始解码 |
| **逐帧前进/后退** | 暂停音频输出，直接解码并显示目标帧，`audioClock` 强制等于 `videoPts` |
| **暂停** | 音频输出停止写入，解码线程暂停；恢复时从暂停位置继续 |
| **纯音频文件（无视频）** | 视频显示区域保持黑色背景，字幕正常叠加（如有） |
| **纯视频文件（无音频）** | `audioClock` 退化为 `QElapsedTimer` 系统计时器，按固定 `1/fps` 间隔显示帧 |
| **播放结束** | 发送 `playbackFinished()` 信号，状态切换为 `Stopped` |

---

## 4. 拖入文件到播放的数据流

```
用户拖入文件
    │
    ▼
TimelinePanel::dropEvent(const QMimeData*)
    │
    ├───► 提取文件路径 localPath
    │         │
    │         ▼
    │    同步通知 AppWindow
    │         │
    │         ▼
    │    AppWindow::onMediaFileDropped(localPath)
    │         │
    │         ├───► mediaPlayer_->load(localPath)
    │         │           │
    │         │           ▼
    │         │    ┌─────────────────┐
    │         │    │  MediaPlayer    │
    │         │    │  · 调用 FFmpeg  │
    │         │    │    Decoder 打开  │
    │         │    │  · 提取时长/尺寸 │
    │         │    │  · 初始化 Audio  │
    │         │    │    Output        │
    │         │    │  · 状态: Ready   │
    │         │    └───────┬─────────┘
    │         │            │
    │         │            ▼
    │         │    发送 mediaLoaded(durationMs, videoSize)
    │         │            │
    │         ▼            ▼
    │    timelinePanel_->setTotalDuration(durationMs)
    │    videoPreviewPanel_->setVideoSize(videoSize)
    │    videoPreviewPanel_->seekTo(0)  // 显示首帧
    │    // 可选：自动开始播放，或等待用户点击 Play
    │
    └───► 启动 ASR Pipeline（现有逻辑不变，后台运行）
                  │
                  ▼
            AudioTranscoder → OssUploader → TencentAsrService
                  │
                  ▼
            ASR 完成后：SubtitleTrack 更新，字幕自动出现在时间轴
```

### 4.1 关键设计点

- **ASR 和预览并行**：拖入文件后预览立即开始，ASR 在后台独立运行。用户可以在 ASR 完成前预览视频，但此时字幕列表为空
- **状态管理**：`MediaPlayer` 维护 `Stopped → Loading → Ready → Playing ↔ Paused → Stopped` 状态机
- **文件格式支持**：所有 FFmpeg 支持的格式（mp4, mov, mkv, avi, mp3, wav, flac, m4a 等）

---

## 5. UI 联动设计

### 5.1 信号连接矩阵

| 用户操作 | 触发信号 | 受影响组件 | 行为 |
|---------|---------|-----------|------|
| 点击 Timeline 某位置 | `timeClicked(qint64 ms)` | MediaPlayer | `seek(ms)` |
| 点击 Timeline 某位置 | `timeClicked(qint64 ms)` | SubtitleListPanel | 高亮对应时间段字幕 |
| 点击 SubtitleList 某条 | `itemSeekRequested(QString id, qint64 startMs)` | TimelinePanel | `setCurrentTime(startMs)` |
| 点击 SubtitleList 某条 | `itemSeekRequested(QString id, qint64 startMs)` | MediaPlayer | `seek(startMs)` |
| Video 播放中（每帧/定时） | `timeChanged(qint64 ms)` | TimelinePanel | `setCurrentTime(ms)`，playhead 跟随 |
| Video 播放中 | `timeChanged(qint64 ms)` | SubtitleListPanel | `scrollToTime(ms)`，自动滚动到当前字幕 |
| 拖动 Timeline playhead | `timeClicked(qint64 ms)` | MediaPlayer | `seek(ms)` |
| 字幕文本编辑完成 | `SubtitleTrack::dataChanged` | VideoPreviewPanel | 当前帧字幕文字刷新 |
| ASR 完成 | `asrSucceeded()` | SubtitleTrack | 批量添加字幕，所有面板更新 |

### 5.2 AppWindow 中的连接代码

```cpp
// 在 AppWindow::setupSplitterLayout() 中新增：

// 1. Timeline 点击/拖动 → 视频 seek
connect(d->timelinePanel, &TimelinePanel::timeClicked,
        d->mediaPlayer, &MediaPlayer::seek);

// 2. 播放时间推进 → Timeline playhead 跟随
connect(d->mediaPlayer, &MediaPlayer::timeChanged,
        d->timelinePanel, &TimelinePanel::setCurrentTime);

// 3. 播放时间推进 → SubtitleList 自动滚动
connect(d->mediaPlayer, &MediaPlayer::timeChanged,
        d->subtitleListPanel, &SubtitleListPanel::scrollToTime);

// 4. SubtitleList 点击 → 视频 seek + Timeline 移动
connect(d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested,
        d->mediaPlayer, &MediaPlayer::seek);
connect(d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested,
        d->timelinePanel, &TimelinePanel::setCurrentTime);

// 5. 字幕数据变化 → 视频区字幕刷新
connect(d->subtitleTrack, &SubtitleTrack::dataChanged,
        d->videoPreviewPanel, QOverload<>::of(&VideoPreviewPanel::update));

// 6. 拖入文件 → 加载到 MediaPlayer（TimelinePanel dropEvent 中触发）
connect(d->timelinePanel, &TimelinePanel::mediaFileDropped,
        d->mediaPlayer, &MediaPlayer::load);
connect(d->mediaPlayer, &MediaPlayer::mediaLoaded,
        d->timelinePanel, &TimelinePanel::setTotalDuration);
connect(d->mediaPlayer, &MediaPlayer::mediaLoaded,
        d->videoPreviewPanel, &VideoPreviewPanel::onMediaLoaded);
```

---

## 6. 字幕叠加渲染

### 6.1 渲染位置

字幕叠加在 `VideoPreviewPanel` 的视频显示区域上，使用 `QPainter` 在每一帧渲染后绘制。

```
┌─────────────────────────────────────────┐
│  videoArea_ (透明背景 QFrame)            │
│  ┌─────────────────────────────────────┐│
│  │  videoFrame (SoftwareRenderer 绘制)  ││
│  │  · 黑色背景（无视频时）               ││
│  │  · 或当前视频帧                      ││
│  │                                     ││
│  │    ┌──────────────────────────┐     ││
│  │    │  "当前时间对应的字幕文字"  │     ││ ← QPainter::drawText
│  │    │  font: 用户选择字体/大小   │     ││    底部居中
│  │    │  黑色描边 + 白色填充        │     ││
│  │    └──────────────────────────┘     ││
│  │                                     ││
│  └─────────────────────────────────────┘│
└─────────────────────────────────────────┘
```

### 6.2 渲染逻辑

```cpp
void VideoPreviewPanel::paintSubtitle(QPainter& painter, qint64 currentTimeMs) {
    // 查询当前激活的字幕
    SubtitleItem currentItem = subtitleTrack_->itemAtTime(currentTimeMs);
    if (currentItem.text.isEmpty()) return;

    // 字体设置（使用用户选择的字体和大小）
    QFont font(fontCombo_->currentText(), sizeCombo_->currentText().toInt());
    painter.setFont(font);

    QRect textRect = videoArea_->rect().adjusted(40, 0, -40, -20);

    // 黑色描边（保证任何视频背景下都清晰可见）
    painter.setPen(QPen(Qt::black, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, currentItem.text);

    // 白色填充文字
    painter.setPen(Qt::white);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignBottom, currentItem.text);
}
```

### 6.3 无视频时的字幕预览

当拖入纯音频文件时，视频显示区域显示纯黑色背景，字幕仍然正常叠加显示。这样用户可以在只听音频的情况下看到字幕效果。

---

## 7. 日志与可观测性

由于视频预览涉及多线程、FFmpeg 底层操作，且开发环境无法实际播放视频进行验证，**必须通过详尽日志确保每一步执行正确**。

### 7.1 日志分级与模块

使用 Qt `qDebug()` / `qInfo()` / `qWarning()` / `qCritical()`，统一格式：`[Module] message key=value`。

```cpp
// 日志宏（放在公共头文件中）
#define LOG_MP(level, msg)  q##level() << "[MediaPlayer]" << msg
#define LOG_DEC(level, msg) q##level() << "[FFmpegDecoder]" << msg
#define LOG_SYNC(level, msg) q##level() << "[SyncEngine]" << msg
#define LOG_AUDIO(level, msg) q##level() << "[AudioOutput]" << msg
#define LOG_RENDER(level, msg) q##level() << "[VideoRenderer]" << msg
#define LOG_SUB(level, msg)   q##level() << "[SubtitleOverlay]" << msg
```

### 7.2 关键路径日志规范

**文件加载流程：**
```
[MediaPlayer] load() started path=/Users/xxx/video.mp4
[FFmpegDecoder] avformat_open_input() ret=0
[FFmpegDecoder] streams video=0 audio=1 subtitle=-1
[FFmpegDecoder] metadata duration=123456ms fps=29.970 size=1920x1080 codec=h264
[FFmpegDecoder] audio format sampleRate=48000 channels=2 sampleFmt=fltp
[MediaPlayer] load() success state=Ready duration=123456ms
[AudioOutput] open() sampleRate=48000 channels=2 format=fltp
[VideoRenderer] setViewport size=1920x1080
```

**Seek 操作：**
```
[MediaPlayer] seek() request target=5000ms
[FFmpegDecoder] av_seek_frame() target=5000ms ret=0 actual=4967ms
[AudioOutput] flush() cleared
[SyncEngine] reset audioClock=4967.000
[MediaPlayer] seek() complete displayedPts=5000ms
```

**播放循环（每帧，Debug 级别）：**
```
[FFmpegDecoder] decodeVideoFrame() pts=5000.033 dts=5000.033
[SyncEngine] sync videoPts=5000.033 audioClock=5000.012 delay=0.021ms action=render
[VideoRenderer] renderFrame() cost=2.1ms
[AudioOutput] write() bytes=3840 totalPlayed=240000 samplesClock=5000.000
```

**字幕叠加：**
```
[SubtitleOverlay] active id={uuid} start=4000 end=7000 text="这是字幕内容"
[SubtitleOverlay] font=Arial size=24 rect=(40,560,1240,40)
```

**错误：**
```
[FFmpegDecoder] avcodec_receive_frame() ret=-11 EAGAIN
[SyncEngine] drift detected videoPts=10000.500 audioClock=9998.200 drift=2.3ms
[MediaPlayer] recover attempt=1/3
[FFmpegDecoder] recover() flush decoder re-seek to currentTime
```

### 7.3 验证检查清单

通过分析日志可验证以下功能：

| 验证项 | 期望日志模式 |
|--------|-------------|
| 文件正常打开 | `[FFmpegDecoder] avformat_open_input() ret=0` |
| 音视频流识别正确 | `streams video=N audio=M` |
| 时长信息正确 | `duration=XXXms` 与文件实际时长一致 |
| Seek 精确 | `seek() request target=T` → `displayedPts=T` |
| 同步正常 | `delay` 值在 ±5ms 范围内，绝大多数为 `action=render` |
| 无持续丢帧 | `action=drop` 不连续出现，dropCount 增长缓慢 |
| 字幕正确叠加 | `[SubtitleOverlay] active` 的时间范围与字幕数据一致 |
| 播放流畅 | 相邻帧时间戳间隔 ≈ 1/fps |

---

## 8. 错误处理

| 场景 | 检测方式 | 处理方式 |
|------|---------|---------|
| FFmpeg 无法打开文件 | `avformat_open_input() < 0` | `MediaPlayer::loadFailed(QString error)` → `QMessageBox::critical` |
| 文件无视频流 | `videoStreamIdx == -1` | 视频区显示黑色背景，音频正常播放，字幕正常叠加 |
| 文件无音频流 | `audioStreamIdx == -1` | `audioClock` 回退到 `QElapsedTimer`，按固定 fps 播放 |
| 解码连续失败 | `avcodec_receive_frame()` 连续 10 次返回错误 | 自动 `recover()`：flush decoder → re-seek → 重试，最多 3 次后报错停止 |
| Seek 到不可解码位置 | `av_seek_frame()` 失败 | 回退到文件开头，尝试寻找第一个关键帧 |
| 音频设备打开失败 | `QAudioSink` 构造失败 | 静默降级：仅视频预览，无音频播放，日志告警 |
| 4K+ 视频播放卡顿 | `renderFrame() cost > 33ms` 持续出现 | 日志 warning："帧渲染耗时过高，建议后续启用硬件加速" |
| 播放结束 | 解码器返回 EOF | 状态切换 `Playing → Stopped`，playhead 停在最后一帧 |

---

## 9. 状态机

```
                    ┌──────────────┐
                    │   Stopped    │
                    └──────┬───────┘
                           │ load(path)
                           ▼
                    ┌──────────────┐
         ┌─────────│   Loading    │
         │         └──────┬───────┘
         │                │ load success
         │                ▼
  close()│         ┌──────────────┐
         │         │    Ready     │◄──── seek(ms)
         │         └──────┬───────┘
         │                │ play()
         │                ▼
         │         ┌──────────────┐
         └────────►│   Playing    │
                   └──────┬───────┘
                          │ pause()
                          ▼
                   ┌──────────────┐
                   │    Paused    │
                   └──────┬───────┘
                          │ play()
                          └────────► Playing
```

- **Stopped**: 未加载或已关闭，视频区黑色背景
- **Loading**: 正在打开文件、初始化解码器
- **Ready**: 文件已加载，显示首帧，等待播放
- **Playing**: 正在播放，解码线程和音频输出活跃
- **Paused**: 暂停，画面静止在当前帧，音频停止

---

## 10. 新增/修改的文件清单

| 文件 | 类型 | 说明 |
|------|------|------|
| `include/MediaPlayer.h` | 新增 | 播放控制器，状态机，信号管理 |
| `src/MediaPlayer.cpp` | 新增 | MediaPlayer 实现 |
| `include/FFmpegDecoder.h` | 新增 | FFmpeg 解封装 + 解码器 |
| `src/FFmpegDecoder.cpp` | 新增 | FFmpegDecoder 实现 |
| `include/SoftwareVideoRenderer.h` | 新增 | 软件渲染器（swscale + QPainter） |
| `src/SoftwareVideoRenderer.cpp` | 新增 | SoftwareVideoRenderer 实现 |
| `include/QtAudioOutput.h` | 新增 | Qt QAudioSink 音频输出实现 |
| `src/QtAudioOutput.cpp` | 新增 | QtAudioOutput 实现 |
| `include/VideoPreviewPanel.h` | 修改 | 增加播放控制信号、字幕叠加接口 |
| `src/VideoPreviewPanel.cpp` | 修改 | 替换占位符，接入 MediaPlayer |
| `include/TimelinePanel.h` | 修改 | 增加 `mediaFileDropped` 信号 |
| `src/TimelinePanel.cpp` | 修改 | dropEvent 中触发预览加载 |
| `include/AppWindow.h` | 修改 | 增加 MediaPlayer 成员 |
| `src/AppWindow.cpp` | 修改 | 新增信号连接、MediaPlayer 初始化 |
| `CMakeLists.txt` | 修改 | 新增源文件 |

---

## 11. 演进路线图

| 阶段 | 内容 | 前提条件 |
|------|------|---------|
| **Phase 1（本次）** | 软解（FFmpeg）+ 软渲（QPainter）+ QtAudioOutput | 完成 |
| **Phase 2** | 硬件解码：FFmpeg + VideoToolbox（macOS）/ D3D11VA（Windows） | Phase 1 稳定运行 |
| **Phase 3** | GPU 渲染：QOpenGLWidget + Shader 渲染 YUV 帧 | Phase 2 完成 |
| **Phase 4** | 音频替换：QtAudioOutput → SDL2 Audio 或 Miniaudio | 出现音频质量问题时 |

---

*文档结束。待用户审核确认后进入 implementation plan 阶段。*
