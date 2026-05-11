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

#if PROFILE_TIMING
  previewE2eTimer_.restart();
#endif

  if (!isPreviewDragging_) {
    // First call during drag: initialize preview mode
    if (state_ == Playing) {
      pause();
    }
    isPreviewDragging_ = true;
    seekPreviewMode_ = true;
    seekTargetMs_ = ms;
    currentTimeMs_ = ms;
    lastPreviewSeekMs_ = ms;
    lastRenderedPreviewPts_ = -1;
    currentPreviewStrategy_ = PreviewStrategy::SeekChase;
    decoder_->requestSeek(ms);
    decoder_->setPlaying(true);
    seekPreviewTimer_.start();
    if (!playbackTimerRunning_) {
      playbackTimer_->start(16);
      playbackTimerRunning_ = true;
    }
    LOG_MP(info, "previewSeek() drag started target=" << ms);
  } else {
    // Subsequent calls: choose strategy based on distance and direction
    seekTargetMs_ = ms;
    currentTimeMs_ = ms;

    qint64 distance = ms - lastPreviewSeekMs_;

    if (ms < lastPreviewSeekMs_) {
      // Backward drag: always re-seek and chase
      currentPreviewStrategy_ = PreviewStrategy::SeekChase;
      decoder_->requestSeek(ms);
      lastPreviewSeekMs_ = ms;
      lastRenderedPreviewPts_ = -1;
      LOG_MP(info, "previewSeek() backward reseek target=" << ms);
    } else if (distance < 300) {
      // Small forward (<300ms): keep decoder running, chase exact frame
      currentPreviewStrategy_ = PreviewStrategy::Chase;
      decoder_->setPlaying(true);
      LOG_MP(debug,
             "previewSeek() chase target=" << ms << " distance=" << distance);
    } else if (distance < 3000) {
      // Medium forward (300ms-3s): re-seek then chase
      currentPreviewStrategy_ = PreviewStrategy::SeekChase;
      decoder_->requestSeek(ms);
      lastPreviewSeekMs_ = ms;
      lastRenderedPreviewPts_ = -1;
      LOG_MP(info, "previewSeek() medium reseek target=" << ms << " distance="
                                                          << distance);
    } else {
      // Large forward (>3s): re-seek, render first keyframe only
      currentPreviewStrategy_ = PreviewStrategy::SeekFast;
      decoder_->requestSeek(ms);
      lastPreviewSeekMs_ = ms;
      lastRenderedPreviewPts_ = -1;
      LOG_MP(info, "previewSeek() large reseek target=" << ms << " distance="
                                                         << distance);
    }
  }

  previewFrameRendered_ = false;
  emit timeChanged(currentTimeMs_);
}

void MediaPlayer::stopPreviewDragging() {
  if (!isPreviewDragging_)
    return;

  isPreviewDragging_ = false;

  // Clear any residual frames produced during chasing so that the final
  // seek() starts from a clean state.
  if (decoder_) {
    decoder_->clearVideoQueue();
    decoder_->clearAudioQueue();
  }

  // Use existing seek() for a clean precise seek to final position
  // seek() handles stopping preview mode, re-seeking, etc.
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

    // Only attempt to render once per target change (prevent continuous
    // playback when holding the mouse still).
    if (!previewFrameRendered_) {
      if (currentPreviewStrategy_ == PreviewStrategy::SeekFast) {
        // Large forward: render the first available frame (keyframe) without
        // chasing exact position. This guarantees immediate visual feedback.
        auto frame = decoder_->dequeueVideoFrame();
        if (frame && videoRenderer_) {
          videoRenderer_->renderFrame(*frame);
          lastRenderedPreviewPts_ = frame->ptsMs;
          previewFrameRendered_ = true;
#if PROFILE_TIMING
          qInfo() << "[TIMING:drag_e2e] strategy=SeekFast"
                  << " target=" << seekTargetMs_
                  << " rendered_pts=" << frame->ptsMs
                  << " e2e_us=" << (previewE2eTimer_.nsecsElapsed() / 1000);
#endif
        }
      } else {
        // Chase / SeekChase: loop dequeue until we find a frame at or after
        // the target. Frames earlier than target are dropped.
        int dropped = 0;
        while (decoder_->videoQueueSize() > 0) {
          auto frame = decoder_->dequeueVideoFrame();
          if (!frame)
            break;

          if (frame->ptsMs >= seekTargetMs_) {
            if (videoRenderer_) {
              videoRenderer_->renderFrame(*frame);
            }
            lastRenderedPreviewPts_ = frame->ptsMs;
            previewFrameRendered_ = true;
#if PROFILE_TIMING
            const char *stratName =
                currentPreviewStrategy_ == PreviewStrategy::Chase ? "Chase"
                                                                  : "SeekChase";
            qInfo() << "[TIMING:drag_e2e] strategy=" << stratName
                    << " target=" << seekTargetMs_
                    << " rendered_pts=" << frame->ptsMs
                    << " dropped=" << dropped
                    << " e2e_us=" << (previewE2eTimer_.nsecsElapsed() / 1000);
#endif
            break;
          }
          // Frame is behind target: discard and continue chasing
          dropped++;
        }
      }
    }

    if (previewFrameRendered_) {
      // Rendered one frame for the current target: pause decoder and wait
      // for the next target change.
      decoder_->setPlaying(false);

      if (!isPreviewDragging_) {
        // Normal seek preview: finish
        playbackTimer_->stop();
        playbackTimerRunning_ = false;
        seekPreviewMode_ = false;
        LOG_MP(info,
               "seek preview frame rendered pts=" << lastRenderedPreviewPts_);
      } else {
        // Dragging: wait for next mouse move
        seekPreviewTimer_.start();
        LOG_MP(info, "preview drag frame rendered target="
                         << seekTargetMs_
                         << " pts=" << lastRenderedPreviewPts_);
      }
      return;
    }

    // Target frame not yet available: keep decoder running
    decoder_->setPlaying(true);

    if (seekPreviewTimer_.elapsed() > 2000) {
      // Timeout fallback: render whatever is available and exit preview mode
      auto frame = decoder_->dequeueVideoFrame();
      if (frame && videoRenderer_) {
        videoRenderer_->renderFrame(*frame);
        lastRenderedPreviewPts_ = frame->ptsMs;
      }
      decoder_->setPlaying(false);
      playbackTimer_->stop();
      playbackTimerRunning_ = false;
      seekPreviewMode_ = false;
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
