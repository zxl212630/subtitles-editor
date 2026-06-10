#include "MediaPlayer.h"
#include "ConfigManager.h"
#include "FFmpegDecoder.h"
#include "QtAudioOutput.h"
#include "SeekDecoder.h"
#include "SoftwareVideoRenderer.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

#include <thread>

#define PROFILE_TIMING 1

#define LOG_MP_info(msg) qInfo() << "[MediaPlayer]" << msg
#define LOG_MP_warning(msg) qWarning() << "[MediaPlayer]" << msg
#define LOG_MP_critical(msg) qCritical() << "[MediaPlayer]" << msg
#define LOG_MP_debug(msg) qDebug() << "[MediaPlayer]" << msg
#define LOG_MP(level, msg) LOG_MP_##level(msg)

MediaPlayer::MediaPlayer(QObject *parent)
    : QObject(parent), totalDurationLimitMs_(0) {
  volume_ = ConfigManager::instance().volume();
  isMuted_ = ConfigManager::instance().muted();

  decoder_ = new FFmpegDecoder(this);
  seekDecoder_ = new SeekDecoder(this);
  audioOutput_ = new QtAudioOutput(this);
  audioOutput_->setVolume(isMuted_ ? 0.0 : volume_);
  playbackTimer_ = new QTimer(this);

  seekCoalesceTimer_ = new QTimer(this);
  seekCoalesceTimer_->setSingleShot(true);
  seekCoalesceTimer_->setInterval(15); // 15ms 合并延迟窗口

  connect(decoder_, &FFmpegDecoder::decodeError, this,
          &MediaPlayer::onDecoderError);
  connect(decoder_, &FFmpegDecoder::endOfStream, this,
          &MediaPlayer::onEndOfStream);
  connect(playbackTimer_, &QTimer::timeout, this,
          &MediaPlayer::onPlaybackTimer);
  connect(seekCoalesceTimer_, &QTimer::timeout, this,
          &MediaPlayer::executePendingSeek);
  connect(seekDecoder_, &SeekDecoder::frameReady, this,
          &MediaPlayer::onSeekFrameReady, Qt::QueuedConnection);
}

MediaPlayer::~MediaPlayer() {
  if (loadConn_) {
    disconnect(loadConn_);
    loadConn_ = {};
  }
  if (decoder_)
    decoder_->setCancelOpen(true);
  if (seekDecoder_)
    seekDecoder_->setCancelOpen(true);

  if (loadThread_.joinable()) {
    loadThread_.join();
  }
  stop();
  playbackTimer_->stop();
  playbackTimerRunning_ = false;
  seekCoalesceTimer_->stop();

  if (decoder_) {
    decoder_->close();
  }
  if (seekDecoder_) {
    seekDecoder_->close();
  }
  if (audioOutput_) {
    audioOutput_->close();
  }
}

void MediaPlayer::setVideoRenderer(SoftwareVideoRenderer *renderer) {
  videoRenderer_ = renderer;
}

void MediaPlayer::ensureSeekDecoderOpen() {
  if (currentPath_.isEmpty())
    return;
  if (seekDecoder_ && !seekDecoder_->isRunning()) {
    LOG_MP(info, "ensureSeekDecoderOpen() lazy loading SeekDecoder path="
                     << currentPath_);
    bool ok = seekDecoder_->open(currentPath_);
    if (ok) {
      if (videoRenderer_) {
        seekDecoder_->setOutputSize(videoRenderer_->size());
      }
      LOG_MP(info, "ensureSeekDecoderOpen() lazy loaded successfully");
    } else {
      LOG_MP(warning, "ensureSeekDecoderOpen() lazy load failed");
    }
  }
}

void MediaPlayer::load(const QString &path) {
  // If a previous async load is still running, interrupt it first to avoid
  // blocking the main thread
  if (decoder_)
    decoder_->setCancelOpen(true);
  if (seekDecoder_)
    seekDecoder_->setCancelOpen(true);

  if (loadThread_.joinable()) {
    loadThread_.join();
  }

  // Reset cancel state for the new load task
  if (decoder_)
    decoder_->setCancelOpen(false);
  if (seekDecoder_)
    seekDecoder_->setCancelOpen(false);

  // Use clear() to fully reset state without triggering seek on old video
  clear();

  isLoading_.store(true);
  {
    std::lock_guard<std::mutex> lock(loadMutex_);
    loadDone_ = false;
    ++loadGeneration_;
  }
  state_ = Loading;
  emit stateChanged(Loading);

  LOG_MP(info, "load() started (async) path=" << path);
  LOG_MP(info, "load() launching background thread...");

  // Connect loadFinished signal to onLoadFinished slot (auto-queued since
  // signal is emitted from worker thread, slot runs on main thread)
  loadConn_ = connect(this, &MediaPlayer::loadFinished, this,
                      &MediaPlayer::onLoadFinished, Qt::QueuedConnection);

  // Capture generation before starting thread
  int gen = loadGeneration_;

  // Launch background thread: opens both decoders in parallel
  loadThread_ = std::thread([this, path, gen]() {
    bool decoderOk = false;
    bool seekDecoderOk = false;

    std::thread decoderThread(
        [this, &path, &decoderOk]() { decoderOk = decoder_->open(path); });
    std::thread seekThread([this, &path, &seekDecoderOk]() {
      seekDecoderOk = seekDecoder_->open(path);
    });

    decoderThread.join();
    seekThread.join();

    // Mark thread as done before emitting signal
    {
      std::lock_guard<std::mutex> lock(loadMutex_);
      loadDone_ = true;
    }

    // Emit signal → Qt queues onLoadFinished on main thread
    emit loadFinished(path, decoderOk, seekDecoderOk, gen);
  });
}

void MediaPlayer::onLoadFinished(const QString &path, bool decoderOk,
                                 bool seekDecoderOk, int generation) {
  // Discard stale callback from a previous load that was superseded
  {
    std::lock_guard<std::mutex> lock(loadMutex_);
    if (generation != loadGeneration_) {
      LOG_MP(info, "onLoadFinished discarded (stale generation "
                       << generation << " != current " << loadGeneration_
                       << ")");
      return;
    }
  }

  LOG_MP(info, "onLoadFinished called decoderOk="
                   << decoderOk << " seekDecoderOk=" << seekDecoderOk);
  isLoading_.store(false);

  if (!decoderOk) {
    seekDecoder_->close();
    state_ = Stopped;
    emit stateChanged(Stopped);
    emit playbackError("Failed to open media file: " + path);
    return;
  }

  currentPath_ = path;

  if (!seekDecoderOk) {
    LOG_MP(info,
           "SeekDecoder is not opened yet, will be lazy loaded on demand");
  }

  if (decoder_->hasAudio()) {
    audioOutput_->open(decoder_->audioSampleRate(), decoder_->audioChannels());
  }

  if (decoder_->hasVideo()) {
    videoRenderer_->clear();
    if (seekDecoderOk && videoRenderer_) {
      seekDecoder_->setOutputSize(videoRenderer_->size());
    }
  }

  decoder_->start();

  state_ = Ready;
  currentTimeMs_ = 0;

  emit mediaLoaded(decoder_->durationMs(), decoder_->videoSize());
  LOG_MP(info,
         "load() success state=Ready duration=" << decoder_->durationMs());
}

void MediaPlayer::play() {
  if (isLoading_.load())
    return;
  if (state_ == Ready || state_ == Paused || state_ == Stopped) {
    if (state_ == Stopped) {
      if (decoder_->hasAudio()) {
        audioOutput_->open(decoder_->audioSampleRate(),
                           decoder_->audioChannels());
      }
      decoder_->requestSeek(currentTimeMs_);
      decoder_->clearAllQueues();
      decoder_->start();
    }

    seekPreviewMode_ = false;
    state_ = Playing;
    emit stateChanged(Playing);

    decoder_->setPlaying(true);
    playbackStartTimeMs_ = currentTimeMs_;
    playbackElapsedTimer_.restart();
    playbackTimerRunning_ = true;
    driftStartMs_ = -1;
    playbackTimer_->start(16);
    if (audioOutput_) {
      audioSessionStartUSecs_ = -1;
      audioOutput_->resume();
    }
    LOG_MP(info, "play() state=Playing startTime=" << playbackStartTimeMs_);
  }
}

void MediaPlayer::pause() {
  if (state_ == Playing) {
    decoder_->setPlaying(false);
    playbackTimer_->stop();
    playbackTimerRunning_ = false;
    audioOutput_->suspend();

    state_ = Paused;
    driftStartMs_ = -1;
    emit stateChanged(Paused);
    LOG_MP(info, "pause() state=Paused time=" << currentTimeMs_);
  }
}

void MediaPlayer::stop() {
  playbackTimer_->stop();
  playbackTimerRunning_ = false;

  if (decoder_ && decoder_->hasVideo()) {
    decoder_->setPlaying(false);
    decoder_->requestSeek(0);
    decoder_->clearAllQueues();
  }

  audioOutput_->suspend();

  state_ = Stopped;
  currentTimeMs_ = 0;
  pendingVideoFrame_ = std::nullopt;
  driftStartMs_ = -1;

  if (seekDecoder_ && seekDecoder_->isRunning()) {
    seekPreviewMode_ = true;
    seekDecoder_->requestSeek(0);
  }

  emit timeChanged(0);
  emit stateChanged(Stopped);
  LOG_MP(info, "stop() state=Stopped");
}

void MediaPlayer::clear() {
  // Disconnect load signal before joining to prevent queued callback from
  // executing on a partially-reset object
  if (loadConn_) {
    disconnect(loadConn_);
    loadConn_ = {};
  }

  // Interrupt the loading thread immediately
  if (decoder_)
    decoder_->setCancelOpen(true);
  if (seekDecoder_)
    seekDecoder_->setCancelOpen(true);

  if (loadThread_.joinable()) {
    loadThread_.join();
    loadThread_ = {};
  }

  isLoading_.store(false);
  stop();
  playbackTimer_->stop();
  playbackTimerRunning_ = false;
  seekPreviewMode_ = false;
  isPreviewDragging_ = false;
  hasPendingSeek_ = false;
  seekCoalesceTimer_->stop();
  driftStartMs_ = -1;
  currentPath_.clear();
  audioSessionStartUSecs_ = 0;

  if (decoder_) {
    decoder_->close();
  }
  if (seekDecoder_) {
    seekDecoder_->close();
  }
  if (audioOutput_) {
    audioOutput_->close();
  }
  if (videoRenderer_) {
    videoRenderer_->clear();
  }
  currentTimeMs_ = 0;
  emit timeChanged(0);
}

void MediaPlayer::onPlaybackFinished() {
  playbackTimer_->stop();
  playbackTimerRunning_ = false;

  if (decoder_ && decoder_->hasVideo()) {
    decoder_->setPlaying(false);
  }
  if (audioOutput_) {
    audioOutput_->suspend();
  }
  state_ = Stopped;
  emit stateChanged(Stopped);
  emit playbackFinished();
}

void MediaPlayer::setTotalDurationLimit(qint64 ms) {
  totalDurationLimitMs_ = ms;
}

qint64 MediaPlayer::totalDurationLimit() const { return totalDurationLimitMs_; }

void MediaPlayer::seek(qint64 ms) {
  if (isLoading_.load())
    return;

  if (sender()) {
    qInfo() << "[DEBUG_SEEK] seek requested by signal sender:"
            << sender()->metaObject()->className()
            << " name:" << sender()->objectName();
  } else {
    qInfo() << "[DEBUG_SEEK] seek requested by direct function call";
  }
  LOG_MP(info, "seek() request target=" << ms << "ms");

  if (state_ == Playing) {
    pause();
  }

  currentTimeMs_ = ms;
  seekTargetMs_ = ms;
  pendingVideoFrame_ = std::nullopt;
  previewFrameRendered_ = false;
  driftStartMs_ = -1;
  isPreviewDragging_ = false;

  seekPreviewMode_ = true;
  ensureSeekDecoderOpen();
  if (seekDecoder_) {
    seekDecoder_->requestSeek(ms, true);
  }
  if (decoder_) {
    decoder_->requestSeek(ms);
    decoder_->clearAllQueues();
  }
  if (audioOutput_) {
    audioOutput_->flush();
  }

  emit timeChanged(currentTimeMs_);
  LOG_MP(info, "seek() complete");
}

void MediaPlayer::previewSeek(qint64 ms) {
  if (isLoading_.load())
    return;
  if (!decoder_)
    return;

  currentTimeMs_ = ms;
  driftStartMs_ = -1;
  emit timeChanged(currentTimeMs_);

  if (!decoder_->hasVideo())
    return;

  if (!isPreviewDragging_) {
    if (state_ == Playing) {
      pause();
    }
    isPreviewDragging_ = true;
    seekPreviewMode_ = true;
    seekPreviewTimer_.start();
    LOG_MP(info, "previewSeek() drag started target=" << ms);
  }

  ensureSeekDecoderOpen();

  pendingSeekMs_ = ms;
  hasPendingSeek_ = true;
  if (!seekCoalesceTimer_->isActive()) {
    seekCoalesceTimer_->start();
  }
}

void MediaPlayer::executePendingSeek() {
  if (!hasPendingSeek_)
    return;
  hasPendingSeek_ = false;
  seekTargetMs_ = pendingSeekMs_;

  ensureSeekDecoderOpen();
  if (seekDecoder_ && seekDecoder_->isRunning()) {
    seekDecoder_->requestSeek(pendingSeekMs_,
                              true); // 恢复精确 Seek（解码 P 帧）
  }
}

void MediaPlayer::onSeekFrameReady(DecodedVideoFrame frame) {
  if (!isPreviewDragging_ && frame.targetMs != seekTargetMs_) {
    return;
  }

  if (!isPreviewDragging_ && !seekPreviewMode_)
    return;

  if (videoRenderer_) {
    videoRenderer_->renderFrame(frame);
  }
  emit previewFrameRendered(frame.ptsMs, frame.targetMs);

  if (!isPreviewDragging_) {
    seekPreviewMode_ = false;
  }
}

void MediaPlayer::stopPreviewDragging() {
  if (!isPreviewDragging_)
    return;

  seekCoalesceTimer_->stop();
  hasPendingSeek_ = false;

  // Perform a full exact seek to ensure the final frame is accurately rendered.
  // The seek() method will also safely reset isPreviewDragging_.
  seek(currentTimeMs_);
}

double MediaPlayer::decoderFps() const {
  return decoder_ ? decoder_->fps() : 25.0;
}

void MediaPlayer::stepForward() {
  double fps = decoder_->fps();
  if (fps > 0.0) {
    seek(currentTimeMs_ + static_cast<qint64>(1000.0 / fps));
  }
}

void MediaPlayer::stepBackward() {
  double fps = decoder_->fps();
  if (fps > 0.0) {
    seek(currentTimeMs_ - static_cast<qint64>(1000.0 / fps));
  }
}

void MediaPlayer::onPlaybackTimer() {
  if (seekPreviewMode_) {
    while (decoder_->audioQueueSize() > 0) {
      decoder_->dequeueAudioFrame();
    }

    if (seekPreviewTimer_.elapsed() > 3000) {
      seekPreviewMode_ = false;
      playbackTimer_->stop();
      playbackTimerRunning_ = false;
      LOG_MP(warning, "seek preview timed out");
    }
    return;
  }

  qint64 videoDuration =
      (decoder_ && decoder_->hasVideo()) ? decoder_->durationMs() : 0;
  bool noVideoOrOut = (videoDuration <= 0) || (currentTimeMs_ >= videoDuration);

  if (noVideoOrOut) {
    if (playbackTimerRunning_) {
      currentTimeMs_ = playbackStartTimeMs_ + playbackElapsedTimer_.elapsed();
    }
    qint64 maxLimit = qMax(videoDuration, totalDurationLimitMs_);
    if (currentTimeMs_ >= maxLimit) {
      currentTimeMs_ = maxLimit;
      onPlaybackFinished();
      emit timeChanged(currentTimeMs_);
      return;
    }
    emit timeChanged(currentTimeMs_);
    return;
  }

  // 1. Process audio frames
  while (decoder_->audioQueueSize() > 0) {
    // When queue is very long (>10), be conservative and check buffer space
    // This prevents the queue from growing unbounded if audio device is slow
    if (decoder_->audioQueueSize() > 10) {
      qint64 bytesFree = audioOutput_->bytesFree();
      if (bytesFree < 4096) { // Conservative minimum
        break;
      }
    }

    auto aframe = decoder_->dequeueAudioFrame();
    if (!aframe) {
      break;
    }

    // Skip audio frames that are before the current playback position.
    // This happens after a seek when the decoder starts from a keyframe
    // that is earlier than the seek target.
    if (aframe->ptsMs < playbackStartTimeMs_) {
      continue;
    }

    // write() handles partial writes internally and blocks if buffer is full
    audioOutput_->write(aframe->pcmData.constData(), aframe->pcmData.size());
  }

  // 2. Calculate playback clock based on audio master clock or elapsed real
  // time
  double audioClock = 0.0;
  if (playbackTimerRunning_) {
    if (decoder_->hasAudio() && audioOutput_ && audioOutput_->isOpen()) {
      if (audioSessionStartUSecs_ < 0) {
        audioSessionStartUSecs_ = audioOutput_->playedUSecs();
      }
      qint64 sessionPlayedMs =
          qMax(0LL, audioOutput_->playedUSecs() - audioSessionStartUSecs_) /
          1000;
      audioClock = (playbackStartTimeMs_ + sessionPlayedMs) / 1000.0;
    } else {
      audioClock =
          (playbackStartTimeMs_ + playbackElapsedTimer_.elapsed()) / 1000.0;
    }
  } else {
    audioClock = currentTimeMs_ / 1000.0;
  }

  // 3. Process video frame (sync)
  if (!pendingVideoFrame_) {
    pendingVideoFrame_ = decoder_->dequeueVideoFrame();
  }

  // Filter out any video frames that are strictly older than the playback start
  // time to prevent backward visual jumps after an exact seek.
  while (pendingVideoFrame_ &&
         pendingVideoFrame_->ptsMs < playbackStartTimeMs_) {
    pendingVideoFrame_ = decoder_->dequeueVideoFrame();
  }

  if (!pendingVideoFrame_) {
    // Check for end of stream
    if (decoder_->videoQueueSize() == 0 && decoder_->audioQueueSize() == 0 &&
        decoder_->isFinished()) {
      qint64 videoDuration =
          (decoder_ && decoder_->hasVideo()) ? decoder_->durationMs() : 0;
      if (totalDurationLimitMs_ > videoDuration) {
        currentTimeMs_ = videoDuration;
        playbackStartTimeMs_ = videoDuration;
        playbackElapsedTimer_.restart();
      } else {
        qint64 current = currentTimeMs_;
        if (playbackTimerRunning_) {
          if (driftStartMs_ < 0) {
            driftStartMs_ = currentTimeMs_;
            driftTimer_.start();
          }
          current = driftStartMs_ + driftTimer_.elapsed();
        }
        if (current >= videoDuration) {
          currentTimeMs_ = videoDuration;
          onPlaybackFinished();
          emit timeChanged(currentTimeMs_);
        } else {
          currentTimeMs_ = current;
          emit timeChanged(currentTimeMs_);
        }
      }
    }
    return;
  }

  double videoPts = pendingVideoFrame_->ptsMs / 1000.0;
  double delayMs = (videoPts - audioClock) * 1000.0;

  bool shouldRender = true;
  bool dropFrame = false;
  if (decoder_->hasAudio()) {
    if (delayMs > 20.0) {
      // Video is ahead, keep frame for next tick (do not block UI thread)
      shouldRender = false;
    } else if (delayMs < -40.0) {
      dropFrame = true;
      shouldRender = false;
      droppedFrames_++;
    }
  }

  // 4. Render frame
  if (shouldRender) {
    if (videoRenderer_) {
#if PROFILE_TIMING
      QElapsedTimer renderPerfTimer;
      renderPerfTimer.start();
#endif
      videoRenderer_->renderFrame(*pendingVideoFrame_);
#if PROFILE_TIMING
      static int renderLogCounter = 0;
      if (++renderLogCounter % 60 == 0) {
        qInfo() << "[TIMING:playback_render] frame#" << renderedFrames_
                << " pts=" << pendingVideoFrame_->ptsMs
                << " render_us=" << (renderPerfTimer.nsecsElapsed() / 1000)
                << " dropped=" << droppedFrames_;
      }
#endif
    }
    currentTimeMs_ = pendingVideoFrame_->ptsMs;
    emit timeChanged(currentTimeMs_);
    renderedFrames_++;
  }

  if (shouldRender || dropFrame) {
    pendingVideoFrame_ = std::nullopt;
  }
}

void MediaPlayer::onDecoderError(const QString &error) {
  LOG_MP(critical, "Decoder error: " << error);
  emit playbackError(error);
  stop();
}

void MediaPlayer::onEndOfStream() {
  LOG_MP(info, "EOS from decoder, letting playback drain remaining queues");
}

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

QString MediaPlayer::audioCodecName() const {
  return decoder_ ? decoder_->audioCodecName() : QString();
}

qint64 MediaPlayer::videoBitRate() const {
  return decoder_ ? decoder_->videoBitRate() : 0;
}

qint64 MediaPlayer::audioBitRate() const {
  return decoder_ ? decoder_->audioBitRate() : 0;
}

int MediaPlayer::audioBitDepth() const {
  return decoder_ ? decoder_->audioBitDepth() : 0;
}

QString MediaPlayer::mediaCreationTime() const {
  return decoder_ ? decoder_->mediaCreationTime() : QString();
}

void MediaPlayer::setVolume(qreal volume) {
  volume_ = qBound(0.0, volume, 1.0);
  ConfigManager::instance().setVolume(volume_);

  if (volume_ == 0.0) {
    if (!isMuted_) {
      isMuted_ = true;
      ConfigManager::instance().setMuted(true);
    }
  } else {
    if (isMuted_) {
      isMuted_ = false;
      ConfigManager::instance().setMuted(false);
    }
  }

  if (audioOutput_) {
    audioOutput_->setVolume(isMuted_ ? 0.0 : volume_);
  }
  emit volumeChanged(volume_, isMuted_);
}

void MediaPlayer::setMuted(bool muted) {
  if (isMuted_ == muted)
    return;
  isMuted_ = muted;
  ConfigManager::instance().setMuted(isMuted_);

  if (!isMuted_ && volume_ <= 0.001) {
    volume_ = 0.5;
    ConfigManager::instance().setVolume(volume_);
  }

  if (audioOutput_) {
    audioOutput_->setVolume(isMuted_ ? 0.0 : volume_);
  }
  emit volumeChanged(volume_, isMuted_);
}
