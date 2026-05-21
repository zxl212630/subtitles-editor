#include "MediaPlayer.h"
#include "FFmpegDecoder.h"
#include "QtAudioOutput.h"
#include "SeekDecoder.h"
#include "SoftwareVideoRenderer.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

#define PROFILE_TIMING 1

#define LOG_MP_info(msg) qInfo() << "[MediaPlayer]" << msg
#define LOG_MP_warning(msg) qWarning() << "[MediaPlayer]" << msg
#define LOG_MP_critical(msg) qCritical() << "[MediaPlayer]" << msg
#define LOG_MP_debug(msg) qDebug() << "[MediaPlayer]" << msg
#define LOG_MP(level, msg) LOG_MP_##level(msg)

MediaPlayer::MediaPlayer(QObject *parent) : QObject(parent) {
  decoder_ = new FFmpegDecoder(this);
  seekDecoder_ = new SeekDecoder(this);
  audioOutput_ = new QtAudioOutput(this);
  playbackTimer_ = new QTimer(this);

  seekCoalesceTimer_ = new QTimer(this);
  seekCoalesceTimer_->setSingleShot(true);
  seekCoalesceTimer_->setInterval(8); // 8ms 合并延迟窗口

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
  stop();
  delete audioOutput_;
  delete decoder_;
  delete seekDecoder_;
}

void MediaPlayer::setVideoRenderer(SoftwareVideoRenderer *renderer) {
  videoRenderer_ = renderer;
}

bool MediaPlayer::load(const QString &path) {
  if (state_ != Stopped) {
    stop();
  }

  state_ = Loading;
  emit stateChanged(Loading);

  LOG_MP(info, "load() started path=" << path);

  if (!decoder_->open(path)) {
    state_ = Stopped;
    emit playbackError("Failed to open media file: " + path);
    return false;
  }

  if (decoder_->hasAudio()) {
    audioOutput_->open(decoder_->audioSampleRate(), decoder_->audioChannels());
  }

  if (decoder_->hasVideo()) {
    videoRenderer_->clear();
    seekDecoder_->open(path);
    if (videoRenderer_) {
      seekDecoder_->setOutputSize(videoRenderer_->size());
    }
  }

  decoder_->start();

  state_ = Ready;
  currentTimeMs_ = 0;

  emit mediaLoaded(decoder_->durationMs(), decoder_->videoSize());
  LOG_MP(info,
         "load() success state=Ready duration=" << decoder_->durationMs());

  return true;
}

void MediaPlayer::play() {
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

    decoder_->clearAllQueues();
    pendingVideoFrame_ = std::nullopt;

    decoder_->setPlaying(true);
    playbackStartTimeMs_ = currentTimeMs_;
    playbackElapsedTimer_.restart();
    playbackTimerRunning_ = true;
    playbackTimer_->start(16);
    audioOutput_->resume();
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
    emit stateChanged(Paused);
    LOG_MP(info, "pause() state=Paused time=" << currentTimeMs_);
  }
}

void MediaPlayer::stop() {
  playbackTimer_->stop();
  playbackTimerRunning_ = false;
  decoder_->stop();
  decoder_->wait(5000);
  seekDecoder_->close();
  audioOutput_->close();
  if (videoRenderer_) {
    videoRenderer_->clear();
  }
  state_ = Stopped;
  currentTimeMs_ = 0;
  pendingVideoFrame_ = std::nullopt;
  emit timeChanged(0);
  emit stateChanged(Stopped);
  LOG_MP(info, "stop() state=Stopped");
}

void MediaPlayer::seek(qint64 ms) {
  LOG_MP(info, "seek() request target=" << ms << "ms");

  State oldState = state_;
  if (state_ == Playing) {
    pause();
  }

  currentTimeMs_ = ms;
  seekTargetMs_ = ms;
  pendingVideoFrame_ = std::nullopt;
  previewFrameRendered_ = false;

  if (oldState == Playing) {
    decoder_->requestSeek(ms);
    decoder_->clearAllQueues();
    play();
  } else {
    seekPreviewMode_ = true;
    seekDecoder_->requestSeek(ms);
    decoder_->requestSeek(ms);
    decoder_->clearAllQueues();
  }

  emit timeChanged(currentTimeMs_);
  LOG_MP(info, "seek() complete");
}

void MediaPlayer::previewSeek(qint64 ms) {
  if (!decoder_)
    return;

  currentTimeMs_ = ms;
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

  if (seekDecoder_ && seekDecoder_->isRunning()) {
    seekDecoder_->requestSeek(pendingSeekMs_);
  }
}

void MediaPlayer::onSeekFrameReady(DecodedVideoFrame frame) {
  if (!isPreviewDragging_ && !seekPreviewMode_)
    return;

  if (videoRenderer_) {
    videoRenderer_->renderFrame(frame);
  }

  if (!isPreviewDragging_) {
    seekPreviewMode_ = false;
  }
}

void MediaPlayer::stopPreviewDragging() {
  if (!isPreviewDragging_)
    return;

  seekCoalesceTimer_->stop();
  hasPendingSeek_ = false;
  isPreviewDragging_ = false;
  seekPreviewMode_ = false;

  decoder_->requestSeek(currentTimeMs_);
  decoder_->clearAudioQueue();

  if (playbackTimerRunning_) {
    playbackTimer_->stop();
    playbackTimerRunning_ = false;
  }
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

  // 2. Calculate playback clock based on elapsed real time
  double audioClock = 0.0;
  if (playbackTimerRunning_) {
    audioClock =
        (playbackStartTimeMs_ + playbackElapsedTimer_.elapsed()) / 1000.0;
  } else {
    audioClock = currentTimeMs_ / 1000.0;
  }

  // 3. Process video frame (sync)
  if (!pendingVideoFrame_) {
    pendingVideoFrame_ = decoder_->dequeueVideoFrame();
  }
  if (!pendingVideoFrame_) {
    // Check for end of stream
    if (decoder_->videoQueueSize() == 0 && decoder_->audioQueueSize() == 0 &&
        decoder_->isFinished()) {
      stop();
      emit playbackFinished();
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
