#include "MediaPlayer.h"
#include "FFmpegDecoder.h"
#include "QtAudioOutput.h"
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
  audioOutput_ = new QtAudioOutput(this);
  playbackTimer_ = new QTimer(this);

  connect(decoder_, &FFmpegDecoder::decodeError, this,
          &MediaPlayer::onDecoderError);
  connect(decoder_, &FFmpegDecoder::endOfStream, this,
          &MediaPlayer::onEndOfStream);
  connect(playbackTimer_, &QTimer::timeout, this,
          &MediaPlayer::onPlaybackTimer);
}

MediaPlayer::~MediaPlayer() {
  stop();
  delete audioOutput_;
  delete decoder_;
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

  decoder_->requestSeek(ms);
  currentTimeMs_ = ms;
  seekTargetMs_ = ms;
  pendingVideoFrame_ = std::nullopt;
  previewFrameRendered_ = false;

  if (oldState == Playing) {
    play();
  } else {
    decoder_->setPlaying(true);
    seekPreviewMode_ = true;
    seekPreviewTimer_.start();
    playbackTimer_->start(16);
    LOG_MP(info, "seek() preview mode started");
  }

  emit timeChanged(currentTimeMs_);
  LOG_MP(info, "seek() complete");
}

void MediaPlayer::previewSeek(qint64 ms) {
  if (!decoder_ || !decoder_->hasVideo())
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

  lastRenderedPreviewPts_ = -1;
  seekTargetMs_ = ms;
  currentTimeMs_ = ms;
  previewFrameRendered_ = false;
  decoder_->requestSeek(ms);
  decoder_->setPlaying(true);

  if (!playbackTimerRunning_) {
    playbackTimer_->start(16);
    playbackTimerRunning_ = true;
  }

  emit timeChanged(currentTimeMs_);
}

void MediaPlayer::stopPreviewDragging() {
  if (!isPreviewDragging_)
    return;

  isPreviewDragging_ = false;

  if (decoder_) {
    decoder_->clearAudioQueue();
  }

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
    // Discard accumulated audio frames during preview
    while (decoder_->audioQueueSize() > 0) {
      decoder_->dequeueAudioFrame();
    }

    if (isPreviewDragging_) {
      // Drag mode: render frames up to seekTargetMs_ to reach the
      // drag position. Skip old frames (before last rendered) and
      // discard frames past the target to prevent continuous playback
      // when the mouse stops moving.
      auto frame = decoder_->dequeueVideoFrame();
      while (frame) {
        if (frame->ptsMs > lastRenderedPreviewPts_ &&
            frame->ptsMs <= seekTargetMs_ && videoRenderer_) {
          videoRenderer_->renderFrame(*frame);
          lastRenderedPreviewPts_ = frame->ptsMs;
        }
        frame = decoder_->dequeueVideoFrame();
      }
      // Keep decoder running for next seek
      decoder_->setPlaying(true);

      if (seekPreviewTimer_.elapsed() > 5000) {
        decoder_->setPlaying(false);
        playbackTimer_->stop();
        playbackTimerRunning_ = false;
        seekPreviewMode_ = false;
        isPreviewDragging_ = false;
        LOG_MP(warning, "drag preview timed out");
      }
      return;
    }

    // Non-drag seek preview (single click): drain frames until we reach
    // the exact target position, then stop.
    if (!previewFrameRendered_) {
      auto frame = decoder_->dequeueVideoFrame();
      while (frame) {
        if (frame->ptsMs >= seekTargetMs_) {
          if (videoRenderer_)
            videoRenderer_->renderFrame(*frame);
          previewFrameRendered_ = true;
          break;
        }
        frame = decoder_->dequeueVideoFrame();
      }

      if (previewFrameRendered_) {
        decoder_->setPlaying(false);
        playbackTimer_->stop();
        playbackTimerRunning_ = false;
        seekPreviewMode_ = false;
        LOG_MP(info,
               "seek preview frame rendered at " << seekTargetMs_ << "ms");
        return;
      }

      // Target frame not yet available: keep decoder running
      decoder_->setPlaying(true);

      if (seekPreviewTimer_.elapsed() > 3000) {
        auto lastFrame = decoder_->dequeueVideoFrame();
        if (lastFrame && videoRenderer_) {
          videoRenderer_->renderFrame(*lastFrame);
        }
        decoder_->setPlaying(false);
        playbackTimer_->stop();
        playbackTimerRunning_ = false;
        seekPreviewMode_ = false;
        LOG_MP(warning, "seek preview timed out");
      }
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
