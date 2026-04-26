# 音视频预览功能实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 为字幕编辑器添加音视频预览功能，支持拖入文件后即时预览视频画面、播放音频、接近帧级 seek、字幕实时叠加。

**Architecture:** 采用三线程模型（主线程 GUI + 解码线程 + 音频播放线程），FFmpeg 软解 + swscale 转 RGBA + QPainter 渲染 + QAudioSink 音频输出。通过抽象接口预留硬件加速和 GPU 渲染扩展能力。

**Tech Stack:** C++17, Qt6, FFmpeg 8.0, QWindowKit

---
## 文件清单

| 文件 | 操作 | 职责 |
|------|------|------|
| `include/FFmpegDecoder.h` | 创建 | FFmpeg 解封装 + 解码器接口与实现声明 |
| `src/FFmpegDecoder.cpp` | 创建 | FFmpegDecoder 实现（解封装、解码、seek） |
| `include/QtAudioOutput.h` | 创建 | Qt QAudioSink 音频输出实现 |
| `src/QtAudioOutput.cpp` | 创建 | QtAudioOutput 实现 |
| `include/SoftwareVideoRenderer.h` | 创建 | 软件视频渲染器（swscale 转 RGBA + QPainter） |
| `src/SoftwareVideoRenderer.cpp` | 创建 | SoftwareVideoRenderer 实现 |
| `include/MediaPlayer.h` | 创建 | 播放控制器，状态机，音视频同步 |
| `src/MediaPlayer.cpp` | 创建 | MediaPlayer 实现 |
| `include/VideoPreviewPanel.h` | 修改 | 增加播放控制信号、字幕叠加接口、MediaPlayer 连接 |
| `src/VideoPreviewPanel.cpp` | 修改 | 替换占位符视频区，接入 MediaPlayer，字幕叠加绘制 |
| `include/TimelinePanel.h` | 修改 | 增加 mediaFileDropped(QString) 信号 |
| `src/TimelinePanel.cpp` | 修改 | dropEvent 中 emit mediaFileDropped 信号 |
| `include/AppWindow.h` | 修改 | 增加 MediaPlayer 成员指针 |
| `src/AppWindow.cpp` | 修改 | 初始化 MediaPlayer，新增跨面板信号连接 |
| `CMakeLists.txt` | 修改 | 添加新源文件到构建 |

---
## Task 1: FFmpegDecoder -- 核心解码器

**目标：** 实现 FFmpeg 解封装和视频/音频解码，支持文件打开、关闭、seek。解码线程独立运行，通过 queue 向主线程输出帧。

**Files:**
- Create: `include/FFmpegDecoder.h`
- Create: `src/FFmpegDecoder.cpp`

---

- [ ] **Step 1.1: 创建 FFmpegDecoder 头文件**

Create `include/FFmpegDecoder.h`:

Key design points:
- DecodedVideoFrame: ptsMs, width, height, rgbaData (QByteArray, RGBA32)
- DecodedAudioFrame: ptsMs, pcmData (S16 interleaved), sampleRate, channels
- FFmpegDecoder extends QThread
- Thread-safe controls: std::atomic<bool> for running_, playing_, seekRequested_; std::atomic<qint64> for seekTargetMs_
- Frame queues: QMutex + QQueue<DecodedVideoFrame/AudioFrame>
- Queue limits: MAX_VIDEO_QUEUE_SIZE = 3, MAX_AUDIO_QUEUE_MS = 500
- Signals: decodeError(QString), endOfStream()
- Metadata getters: durationMs(), fps(), videoSize(), hasVideo(), hasAudio(), audioSampleRate(), audioChannels()

- [ ] **Step 1.2: 创建 FFmpegDecoder 实现骨架**

Create `src/FFmpegDecoder.cpp` with minimal stubs that compile.

Stub implementations needed:
- Constructor calling QThread(parent), destructor calling close()
- open(path): return false stub
- close(): stop() + wait(5000)
- requestSeek(targetMs): atomic store to seekTargetMs_ and seekRequested_
- setPlaying(playing): atomic store to playing_
- stop(): atomic store false to running_ and playing_
- dequeueVideoFrame()/dequeueAudioFrame(): QMutexLocker, return nullopt if empty else dequeue
- videoQueueSize()/audioQueueSize(): QMutexLocker, return size
- run(): empty
- performSeek(): empty
- clearQueues(): empty
- decodeVideoPacket(): return false
- decodeAudioPacket(): return false
- convertAudioFrame(): empty

- [ ] **Step 1.3: 实现 open() 方法**

Replace open() stub with full FFmpeg initialization:

1. avformat_open_input(&fmtCtx_, path.toUtf8().constData(), nullptr, nullptr)
   - On failure: av_strerror, LOG_DEC(critical), emit decodeError, return false
2. avformat_find_stream_info(fmtCtx_, nullptr)
   - On failure: log, close input, return false
3. Loop through streams to find first video (AVMEDIA_TYPE_VIDEO) and audio (AVMEDIA_TYPE_AUDIO)
4. For video stream: avcodec_find_decoder -> avcodec_alloc_context3 -> avcodec_parameters_to_context -> avcodec_open2
5. For audio stream: same codec setup
6. Extract metadata: durationMs from fmtCtx->duration or stream duration, fps from avg_frame_rate, resolution from codec context, audio format from codec context
7. Store videoTimeBase_ and audioTimeBase_ from streams
8. LOG_DEC all metadata
9. Return true on success

- [ ] **Step 1.4: 实现 close() 资源释放**

Replace close() stub:
1. stop()
2. wait(5000) for thread to finish
3. clearQueues()
4. sws_freeContext(swsCtx_) if set
5. avcodec_free_context(&videoCodecCtx_) if set
6. avcodec_free_context(&audioCodecCtx_) if set
7. avformat_close_input(&fmtCtx_) if set
8. Reset all metadata fields to defaults
9. LOG_DEC(info, close() complete)

- [ ] **Step 1.5: 实现 run() 解码主循环**

Replace run() stub with the decoder loop:

1. Set running_ = true, playing_ = true
2. Allocate AVPacket* packet = av_packet_alloc()
3. Allocate AVFrame* frame = av_frame_alloc()
4. Main loop while running_.load():
   a. If seekRequested_.load(): performSeek(seekTargetMs_.load()), then seekRequested_.store(false)
   b. If !playing_.load(): QThread::msleep(50); continue
   c. Check queue capacity:
      - bool videoFull = !hasVideo_ || videoQueueSize() >= MAX_VIDEO_QUEUE_SIZE
      - bool audioFull = !hasAudio_ || audioQueueSize() >= MAX_AUDIO_QUEUE_MS
      - If both full: QThread::msleep(5); continue
   d. int ret = av_read_frame(fmtCtx_, packet)
   e. If ret < 0:
      - If AVERROR_EOF: LOG_DEC(info, EOF), emit endOfStream()
      - Else: LOG_DEC(warning, error)
      - Break loop
   f. If packet->stream_index == videoStreamIdx_: decodeVideoPacket(packet)
   g. Else if packet->stream_index == audioStreamIdx_: decodeAudioPacket(packet)
   h. av_packet_unref(packet)
5. Flush decoders: send nullptr packet to drain remaining frames
6. av_packet_free(&packet), av_frame_free(&frame)
7. LOG_DEC(info, decoder thread stopped)

- [ ] **Step 1.6: 实现 performSeek()**

Add performSeek() implementation:

1. Convert targetMs to stream timestamp:
   - int64_t target = static_cast<int64_t>(targetMs / 1000.0 / av_q2d(videoTimeBase_))
2. av_seek_frame(fmtCtx_, videoStreamIdx_, target, AVSEEK_FLAG_BACKWARD)
3. If videoCodecCtx_: avcodec_flush_buffers(videoCodecCtx_)
4. If audioCodecCtx_: avcodec_flush_buffers(audioCodecCtx_)
5. clearQueues()
6. LOG_DEC(info, seek complete target= << targetMs << ms)

- [ ] **Step 1.7: 实现 decodeVideoPacket()**

Add decodeVideoPacket() implementation:

1. avcodec_send_packet(videoCodecCtx_, packet)
2. AVFrame* frame = av_frame_alloc()
3. While avcodec_receive_frame(videoCodecCtx_, frame) == 0:
   a. qint64 ptsMs = frame->pts * av_q2d(videoTimeBase_) * 1000
   b. If frame->pts == AV_NOPTS_VALUE: use frame->best_effort_timestamp
   c. int w = frame->width, h = frame->height
   d. If !swsCtx_ || size changed: sws_getContext(w, h, (AVPixelFormat)frame->format, w, h, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr)
   e. QByteArray rgbaData(w * h * 4, 0)
   f. uint8_t* dstData[4] = { (uint8_t*)rgbaData.data(), nullptr, nullptr, nullptr }
   g. int dstLinesize[4] = { w * 4, 0, 0, 0 }
   h. sws_scale(swsCtx_, frame->data, frame->linesize, 0, h, dstData, dstLinesize)
   i. DecodedVideoFrame vframe; vframe.ptsMs = ptsMs; vframe.width = w; vframe.height = h; vframe.rgbaData = std::move(rgbaData)
   j. Lock videoQueueMutex_, enqueue if queue size < MAX_VIDEO_QUEUE_SIZE
   k. av_frame_unref(frame)
4. av_frame_free(&frame)
5. Return true

- [ ] **Step 1.8: 实现 decodeAudioPacket()**

Add decodeAudioPacket() implementation:

1. avcodec_send_packet(audioCodecCtx_, packet)
2. AVFrame* frame = av_frame_alloc()
3. While avcodec_receive_frame(audioCodecCtx_, frame) == 0:
   a. qint64 ptsMs = frame->pts * av_q2d(audioTimeBase_) * 1000
   b. If frame->pts == AV_NOPTS_VALUE: use frame->best_effort_timestamp
   c. DecodedAudioFrame aframe
   d. convertAudioFrame(frame, aframe)
   e. Lock audioQueueMutex_, enqueue
   f. av_frame_unref(frame)
4. av_frame_free(&frame)
5. Return true

- [ ] **Step 1.9: 实现 convertAudioFrame()**

Add convertAudioFrame() implementation:

1. Input: AVFrame* frame (could be planar like fltp, or packed like s16)
2. Output: DecodedAudioFrame with S16 interleaved pcmData
3. Use SwrContext (swr_alloc_set_opts2 + swr_init + swr_convert) to convert from frame->format to AV_SAMPLE_FMT_S16
4. Or simple copy if format is already S16 and packed
5. For planar formats: interleave channels using swr_convert
6. Set out.ptsMs, out.sampleRate = frame->sample_rate, out.channels = frame->ch_layout.nb_channels
7. Store converted data in out.pcmData (QByteArray)

- [ ] **Step 1.10: 编译验证**

Run: `cmake --build cmake-build-debug`
Expected: Compiles without errors

- [ ] **Step 1.11: Commit**

```bash
git add include/FFmpegDecoder.h src/FFmpegDecoder.cpp
git commit -m "feat: add FFmpegDecoder with demuxing and decoding"
```

---
## Task 2: QtAudioOutput -- 音频输出

**目标：** 使用 Qt6 QAudioSink 实现音频播放。封装为可替换接口，预留 SDL2/Miniaudio 升级路径。

**Files:**
- Create: `include/QtAudioOutput.h`
- Create: `src/QtAudioOutput.cpp`

---

- [ ] **Step 2.1: 创建 QtAudioOutput 头文件**

Create `include/QtAudioOutput.h`:

Class design:
- Extends QObject
- open(sampleRate, channels): configures QAudioFormat (Int16), creates QAudioSink
- close(): stops and deletes QAudioSink
- write(data, size): writes PCM data to QAudioSink IO device
- flush(): resets QAudioSink, clears buffer
- samplesPlayed(): returns totalBytesWritten_ / (channels * sizeof(int16))
- setVolume(volume): calls audioSink_->setVolume()
- isOpen(): returns audioSink_ != nullptr
- Members: QAudioFormat format_, QAudioSink* audioSink_, QIODevice* ioDevice_, qint64 totalBytesWritten_, int sampleRate_, int channels_

- [ ] **Step 2.2: 创建 QtAudioOutput 实现**

Create `src/QtAudioOutput.cpp`:

Implementation details:
- Constructor: initialize members to nullptr/0
- Destructor: call close()
- open(sampleRate, channels):
  1. LOG_AUDIO(info, open() sampleRate=... channels=...)
  2. format_.setSampleRate(sampleRate)
  3. format_.setChannelCount(channels)
  4. format_.setSampleFormat(QAudioFormat::Int16)
  5. QAudioDevice device = QMediaDevices::defaultAudioOutput()
  6. If !device.isFormatSupported(format_): log warning, try to use nearest format
  7. audioSink_ = new QAudioSink(device, format_)
  8. ioDevice_ = audioSink_->start()
  9. sampleRate_ = sampleRate, channels_ = channels
  10. totalBytesWritten_ = 0
  11. Return ioDevice_ != nullptr
- close():
  1. If audioSink_: audioSink_->stop()
  2. Delete audioSink_, set to nullptr
  3. ioDevice_ = nullptr
  4. LOG_AUDIO(info, close())
- write(data, size):
  1. If !ioDevice_: return
  2. qint64 written = ioDevice_->write((const char*)data, size)
  3. totalBytesWritten_ += written
  4. LOG_AUDIO(debug, write() bytes=... totalPlayed=...)
- flush():
  1. If audioSink_: audioSink_->reset()
  2. totalBytesWritten_ = 0
  3. LOG_AUDIO(debug, flush() cleared)
- samplesPlayed():
  1. return totalBytesWritten_ / (channels_ * sizeof(int16_t))
- setVolume(volume): if audioSink_, audioSink_->setVolume(volume)
- isOpen(): return audioSink_ != nullptr

- [ ] **Step 2.3: 编译验证**

Run: `cmake --build cmake-build-debug`
Expected: Compiles without errors

- [ ] **Step 2.4: Commit**

```bash
git add include/QtAudioOutput.h src/QtAudioOutput.cpp
git commit -m "feat: add QtAudioOutput using QAudioSink"
```

---
## Task 3: SoftwareVideoRenderer -- 软件视频渲染器

**目标：** 将 FFmpegDecoder 输出的 RGBA 帧通过 QPainter 渲染到 QWidget 上。

**Files:**
- Create: 
- Create: 

---

- [ ] **Step 3.1: 创建 SoftwareVideoRenderer 头文件**

Create :

Class design:
- Extends QWidget (so it can be embedded directly)
- renderFrame(const DecodedVideoFrame& frame): converts rgbaData to QImage and stores it
- clear(): sets hasFrame_ = false, calls update()
- paintEvent override: draws the stored QImage scaled to widget size (keep aspect ratio, black bars)
- Members: QImage currentImage_, bool hasFrame_, QMutex imageMutex_

- [ ] **Step 3.2: 创建 SoftwareVideoRenderer 实现**

Create :

Implementation details:
- Constructor: setAttribute(Qt::WA_OpaquePaintEvent), set minimum size 320x180
- renderFrame(frame):
  1. QMutexLocker lock(&imageMutex_)
  2. currentImage_ = QImage((const uchar*)frame.rgbaData.constData(), frame.width, frame.height, QImage::Format_RGBA8888).copy()
  3. hasFrame_ = true
  4. QMetaObject::invokeMethod(this, &QWidget::update, Qt::QueuedConnection)
  5. LOG_RENDER(debug, renderFrame() size= << frame.width << x << frame.height)
- clear():
  1. QMutexLocker lock(&imageMutex_)
  2. hasFrame_ = false
  3. update()
- paintEvent(event):
  1. QPainter painter(this)
  2. painter.fillRect(rect(), Qt::black)
  3. QMutexLocker lock(&imageMutex_)
  4. If !hasFrame_: return
  5. Calculate scaled rect keeping aspect ratio:
     - widgetRatio = width() / height()
     - imageRatio = currentImage_.width() / currentImage_.height()
     - If widgetRatio > imageRatio: newHeight = height(), newWidth = height() * imageRatio
     - Else: newWidth = width(), newHeight = width() / imageRatio
     - Center the rect in widget
  6. painter.drawImage(targetRect, currentImage_)
  7. LOG_RENDER(debug, paintEvent() cost=...ms)

- [ ] **Step 3.3: 编译验证**

Run: ninja: no work to do.
Expected: Compiles without errors

- [ ] **Step 3.4: Commit**



---
## Task 4: MediaPlayer -- 播放控制器

**目标：** 连接 FFmpegDecoder、QtAudioOutput、SoftwareVideoRenderer，实现播放/暂停/seek/音视频同步。

**Files:**
- Create: 
- Create: 

---

- [ ] **Step 4.1: 创建 MediaPlayer 头文件**

Create :

Class design:
- Extends QObject
- State enum: Stopped, Loading, Ready, Playing, Paused
- load(path): opens file, starts decoder thread, initializes audio/video renderers
- play(), pause(), stop(): state transitions
- seek(ms): requests seek to decoder, flushes audio, resets sync
- stepForward(), stepBackward(): advances/retreats one frame (pauses audio)
- Signals:
  - mediaLoaded(durationMs, videoSize): emitted when file is ready
  - timeChanged(ms): emitted periodically during playback
  - playbackFinished(): emitted when stream ends
  - playbackError(error): emitted on errors
- Members:
  - FFmpegDecoder* decoder_
  - QtAudioOutput* audioOutput_
  - SoftwareVideoRenderer* videoRenderer_
  - QTimer* playbackTimer_ (for triggering frame display)
  - qint64 currentTimeMs_
  - State state_
  - int droppedFrames_, int renderedFrames_

- [ ] **Step 4.2: 创建 MediaPlayer 实现骨架**

Create  with stubs:
- Constructor: create decoder_, audioOutput_, playbackTimer_
- Destructor: stop(), delete members
- load(path): stub returning false
- play(), pause(), stop(): stubs
- seek(ms): stub
- stepForward(), stepBackward(): stubs
- onPlaybackTimer(): empty slot
- onDecoderError(): empty slot
- onEndOfStream(): empty slot

- [ ] **Step 4.3: 实现 load() 方法**

Replace load() stub:
1. If state_ != Stopped: stop() first
2. state_ = Loading, emit stateChanged(Loading)
3. LOG_MP(info, load() started path= << path)
4. decoder_->open(path)
5. If failed: state_ = Stopped, emit playbackError(), return false
6. If decoder_->hasAudio(): audioOutput_->open(decoder_->audioSampleRate(), decoder_->audioChannels())
7. If decoder_->hasVideo(): videoRenderer_->clear()
8. decoder_->start() (starts QThread)
9. state_ = Ready
10. currentTimeMs_ = 0
11. emit mediaLoaded(decoder_->durationMs(), decoder_->videoSize())
12. LOG_MP(info, load() success state=Ready duration= << decoder_->durationMs())
13. Return true

- [ ] **Step 4.4: 实现 play() / pause() / stop()**

Replace stubs:
- play():
  1. If state_ == Ready || state_ == Paused:
  2. state_ = Playing
  3. decoder_->setPlaying(true)
  4. playbackTimer_->start(16) // ~60fps check interval
  5. LOG_MP(info, play() state=Playing)
- pause():
  1. If state_ == Playing:
  2. state_ = Paused
  3. decoder_->setPlaying(false)
  4. playbackTimer_->stop()
  5. LOG_MP(info, pause() state=Paused)
- stop():
  1. playbackTimer_->stop()
  2. decoder_->stop()
  3. decoder_->wait(5000)
  4. audioOutput_->close()
  5. videoRenderer_->clear()
  6. state_ = Stopped
  7. currentTimeMs_ = 0
  8. LOG_MP(info, stop() state=Stopped)

- [ ] **Step 4.5: 实现 seek() 方法**

Replace seek() stub:
1. LOG_MP(info, seek() request target= << ms << ms)
2. qint64 oldState = state_
3. If state_ == Playing: pause() temporarily
4. decoder_->requestSeek(ms)
5. audioOutput_->flush()
6. currentTimeMs_ = ms
7. If oldState == Playing: play()
8. LOG_MP(info, seek() complete)

- [ ] **Step 4.6: 实现 onPlaybackTimer() -- 音视频同步核心**

Replace onPlaybackTimer() stub:

1. Process audio frames:
   a. While auto aframe = decoder_->dequeueAudioFrame():
   b. audioOutput_->write(aframe->pcmData.constData(), aframe->pcmData.size())
   c. LOG_AUDIO(debug, write() bytes= << aframe->pcmData.size())

2. Calculate audioClock:
   a. double audioClock = 0
   b. If decoder_->hasAudio() && audioOutput_->isOpen():
      audioClock = audioOutput_->samplesPlayed() / double(decoder_->audioSampleRate())
   c. Else: use QElapsedTimer fallback (or simple increment by timer interval)

3. Process video frame (sync):
   a. auto vframe = decoder_->dequeueVideoFrame()
   b. If !vframe: return (no frame ready yet)
   c. double videoPts = vframe->ptsMs / 1000.0
   d. double delayMs = (videoPts - audioClock) * 1000.0
   e. If decoder_->hasAudio():
      - If delayMs > 5.0: QThread::msleep(static_cast<int>(delayMs)); render frame
      - If delayMs < -40.0: drop frame, increment droppedFrames_
      - Else: render frame immediately
   f. Else (no audio): render frame immediately

4. Render frame:
   a. videoRenderer_->renderFrame(*vframe)
   b. currentTimeMs_ = vframe->ptsMs
   c. emit timeChanged(currentTimeMs_)
   d. renderedFrames_++
   e. LOG_SYNC(debug, sync videoPts= << videoPts << audioClock= << audioClock << delay= << delayMs << action=render)

5. Check for end of stream:
   a. If decoder_->videoQueueSize() == 0 && decoder_->audioQueueSize() == 0 && decoder is finished:
   b. stop(), emit playbackFinished()

- [ ] **Step 4.7: 实现 stepForward() / stepBackward()**

Replace stubs:
- stepForward(): seek(currentTimeMs_ + (1000.0 / decoder_->fps()))
- stepBackward(): seek(currentTimeMs_ - (1000.0 / decoder_->fps()))

- [ ] **Step 4.8: 实现错误处理和 EOS 槽**

- onDecoderError(error): LOG_MP(critical, ...), emit playbackError(error), stop()
- onEndOfStream(): LOG_MP(info, EOS), emit playbackFinished(), stop()

- [ ] **Step 4.9: 编译验证**

Run: ninja: no work to do.
Expected: Compiles without errors

- [ ] **Step 4.10: Commit**



---
## Task 5: VideoPreviewPanel 改造

**目标：** 替换占位符视频区，接入 MediaPlayer，添加字幕叠加绘制。

**Files:**
- Modify: 
- Modify: 

---

- [ ] **Step 5.1: 修改 VideoPreviewPanel 头文件**

Modify :

Add forward declarations:
- class MediaPlayer
- class SoftwareVideoRenderer
- class SubtitleTrack

Add public methods:
- void setMediaPlayer(MediaPlayer* player)
- void setSubtitleTrack(SubtitleTrack* track)
- void onMediaLoaded(qint64 durationMs, QSize videoSize)
- void seekTo(qint64 ms)
- void updateSubtitleOverlay()

Add signals:
- void playRequested()
- void pauseRequested()
- void seekRequested(qint64 ms)
- void stepForwardRequested()
- void stepBackwardRequested()

Add private members:
- MediaPlayer* mediaPlayer_ = nullptr
- SoftwareVideoRenderer* videoRenderer_ = nullptr
- SubtitleTrack* subtitleTrack_ = nullptr
- QPushButton* playBtn_ = nullptr
- QPushButton* pauseBtn_ = nullptr
- QPushButton* stepFwdBtn_ = nullptr
- QPushButton* stepBwdBtn_ = nullptr
- QLabel* currentTimeLabel_ = nullptr
- bool isPlaying_ = false

Remove or repurpose:
- QList<QFrame*> handles_ (drag handles, keep for now but not used)
- blackRect placeholder

- [ ] **Step 5.2: 修改 VideoPreviewPanel 实现 -- 替换视频显示区**

Modify :

In setupUi(), video display area section:
1. Remove blackRect placeholder
2. Create SoftwareVideoRenderer* videoRenderer_ = new SoftwareVideoRenderer(videoArea_)
3. Add videoRenderer_ to vaLayout (stretch 1, align center)
4. Remove or hide drag handles (handles_ list)

In control bar section:
1. Replace icon labels with actual QPushButton controls:
   - stepBwdBtn_: step backward one frame
   - playBtn_: play/pause toggle
   - stepFwdBtn_: step forward one frame
   - Add currentTimeLabel_: show current time / total duration
2. Connect buttons to internal slots that emit signals
3. Keep existing progress bar container as visual indicator (or remove if Timeline handles it)

- [ ] **Step 5.3: 添加字幕叠加绘制**

Add paintSubtitle() private method:

1. Query current subtitle from subtitleTrack_ at current time (use mediaPlayer_->currentTimeMs())
2. If no active subtitle: return
3. Create QPainter on videoRenderer_
4. Set font from fontCombo_ and sizeCombo_
5. Calculate text rect: bottom center of videoRenderer_, with 40px side margins, 20px bottom margin
6. Draw black outline (3px) first for visibility
7. Draw white text on top
8. LOG_SUB(debug, active id=... text=...)

Call paintSubtitle() from a slot connected to mediaPlayer_->timeChanged()

- [ ] **Step 5.4: 实现公开方法**

- setMediaPlayer(player): store pointer, connect timeChanged to update time label and subtitle overlay
- setSubtitleTrack(track): store pointer
- onMediaLoaded(duration, size): update time label format, maybe adjust video renderer size
- seekTo(ms): pass to mediaPlayer_->seek(ms)
- updateSubtitleOverlay(): force redraw subtitle

- [ ] **Step 5.5: 编译验证**

Run: ninja: no work to do.
Expected: Compiles without errors

- [ ] **Step 5.6: Commit**



---
## Task 6: TimelinePanel 和 AppWindow 集成

**目标：** 将拖入的文件路径传递给 MediaPlayer，建立跨面板信号连接。

**Files:**
- Modify: 
- Modify: 
- Modify: 
- Modify: 

---

- [ ] **Step 6.1: 修改 TimelinePanel 头文件**

Modify :

Add signal:
- void mediaFileDropped(const QString& path)

Add method:
- void setTotalDuration(qint64 ms)

Remove or adjust:
- totalDurationMs_ default value should be 0 instead of 11000
- currentTimeMs_ default value should be 0 instead of 6040

- [ ] **Step 6.2: 修改 TimelinePanel 实现**

Modify :

In dropEvent():
1. After extracting localPath, emit mediaFileDropped(localPath) BEFORE starting ASR pipeline
2. Keep existing ASR pipeline start (startAsrPipeline) unchanged

Add setTotalDuration():
1. totalDurationMs_ = ms
2. update()

In drawVideoTrack():
1. Replace placeholder video bar with actual duration-based bar
2. If totalDurationMs_ > 0: video bar width = msToPixels(totalDurationMs_)
3. Else: keep placeholder

- [ ] **Step 6.3: 修改 AppWindow 头文件**

Modify :

Add forward declaration:
- class MediaPlayer

Add to Private struct:
- MediaPlayer* mediaPlayer = nullptr

- [ ] **Step 6.4: 修改 AppWindow 实现 -- 初始化和连接**

Modify :

In setupSplitterLayout(), after creating panels:
1. d->mediaPlayer = new MediaPlayer(this)
2. d->videoPreviewPanel->setMediaPlayer(d->mediaPlayer)
3. d->videoPreviewPanel->setSubtitleTrack(d->subtitleTrack)

Add signal connections:
1. Timeline -> MediaPlayer:
   connect(d->timelinePanel, &TimelinePanel::mediaFileDropped, d->mediaPlayer, &MediaPlayer::load)
2. MediaPlayer -> Timeline (duration):
   connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->timelinePanel, &TimelinePanel::setTotalDuration)
3. MediaPlayer -> VideoPreview (seek display):
   connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->videoPreviewPanel, &VideoPreviewPanel::onMediaLoaded)
4. MediaPlayer -> Timeline (time sync):
   connect(d->mediaPlayer, &MediaPlayer::timeChanged, d->timelinePanel, &TimelinePanel::setCurrentTime)
5. Timeline click -> MediaPlayer seek:
   connect(d->timelinePanel, &TimelinePanel::timeClicked, d->mediaPlayer, &MediaPlayer::seek)
6. SubtitleList -> MediaPlayer seek:
   connect(d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested, d->mediaPlayer, [this](const QString&, qint64 ms) { d->mediaPlayer->seek(ms); })
7. SubtitleList -> Timeline sync:
   connect(d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested, d->timelinePanel, &TimelinePanel::setCurrentTime)
8. VideoPreview play -> MediaPlayer:
   connect(d->videoPreviewPanel, &VideoPreviewPanel::playRequested, d->mediaPlayer, &MediaPlayer::play)
9. VideoPreview pause -> MediaPlayer:
   connect(d->videoPreviewPanel, &VideoPreviewPanel::pauseRequested, d->mediaPlayer, &MediaPlayer::pause)
10. VideoPreview step -> MediaPlayer:
    connect(d->videoPreviewPanel, &VideoPreviewPanel::stepForwardRequested, d->mediaPlayer, &MediaPlayer::stepForward)
    connect(d->videoPreviewPanel, &VideoPreviewPanel::stepBackwardRequested, d->mediaPlayer, &MediaPlayer::stepBackward)
11. SubtitleTrack data change -> VideoPreview subtitle refresh:
    connect(d->subtitleTrack, &SubtitleTrack::dataChanged, d->videoPreviewPanel, &VideoPreviewPanel::updateSubtitleOverlay)

- [ ] **Step 6.5: 编译验证**

Run: ninja: no work to do.
Expected: Compiles without errors

- [ ] **Step 6.6: Commit**



---
## Task 7: CMakeLists.txt 更新和最终构建

**目标：** 将所有新源文件添加到 CMake 构建中，确保完整项目编译通过。

**Files:**
- Modify: `CMakeLists.txt`

---

- [ ] **Step 7.1: 更新 CMakeLists.txt**

Modify `CMakeLists.txt`:

Add to SOURCES list:
```cmake
    src/FFmpegDecoder.cpp
    src/QtAudioOutput.cpp
    src/SoftwareVideoRenderer.cpp
    src/MediaPlayer.cpp
```

Add to HEADERS list:
```cmake
    include/FFmpegDecoder.h
    include/QtAudioOutput.h
    include/SoftwareVideoRenderer.h
    include/MediaPlayer.h
```

Verify FFmpeg library linkage (should already exist):
- avcodec, avformat, avutil, swscale
- If swresample is needed for audio conversion, add it

- [ ] **Step 7.2: 最终编译验证**

Run:
```bash
cmake -B cmake-build-debug -S .
cmake --build cmake-build-debug
```

Expected:
- Clean build with no errors
- No linker errors for FFmpeg symbols
- No undefined reference to QAudioSink

- [ ] **Step 7.3: 代码格式化**

Run: `clang-format -i src/*.cpp include/*.h`

- [ ] **Step 7.4: 最终 Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add video preview components to CMakeLists.txt"
```

---

## 验证清单（通过日志确认）

功能实现后，运行程序并拖入视频文件，检查日志输出：

- [ ] `[FFmpegDecoder] open() started path=...` -- 文件拖入后触发
- [ ] `[FFmpegDecoder] streams video=0 audio=1` -- 流识别正确
- [ ] `[FFmpegDecoder] metadata duration=... fps=... size=...` -- 元数据提取正确
- [ ] `[MediaPlayer] load() success state=Ready` -- 加载成功
- [ ] `[AudioOutput] open() sampleRate=... channels=...` -- 音频设备打开
- [ ] 点击播放后: `[MediaPlayer] play() state=Playing`
- [ ] `[SyncEngine] sync videoPts=... audioClock=... delay=... action=render` -- 同步工作
- [ ] `[VideoRenderer] renderFrame() size=...` -- 视频帧渲染
- [ ] 点击时间轴 seek: `[MediaPlayer] seek() request target=...ms`
- [ ] `[FFmpegDecoder] seek complete target=...ms` -- seek 成功
- [ ] `[SubtitleOverlay] active id=... text=...` -- 字幕叠加正确
- [ ] 播放结束: `[MediaPlayer] playbackFinished()`

---

*计划结束。执行时推荐使用 subagent-driven-development skill，每个 Task 分配一个子代理执行。*
