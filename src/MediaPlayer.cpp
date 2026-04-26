#include "MediaPlayer.h"
#include "FFmpegDecoder.h"
#include "QtAudioOutput.h"
#include "SoftwareVideoRenderer.h"

#include <QDebug>
#include <QThread>

#define LOG_MP_info(msg) qInfo() << "[MediaPlayer]" << msg
#define LOG_MP_warning(msg) qWarning() << "[MediaPlayer]" << msg
#define LOG_MP_critical(msg) qCritical() << "[MediaPlayer]" << msg
#define LOG_MP_debug(msg) qDebug() << "[MediaPlayer]" << msg
#define LOG_MP(level, msg) LOG_MP_##level(msg)

MediaPlayer::MediaPlayer(QObject *parent) : QObject(parent) {
  decoder_ = new FFmpegDecoder(this);
  audioOutput_ = new QtAudioOutput(this);
  videoRenderer_ = new SoftwareVideoRenderer();
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
  delete videoRenderer_;
  delete audioOutput_;
  delete decoder_;
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
  LOG_MP(info, "load() success state=Ready duration="
                  << decoder_->durationMs());

  return true;
}

void MediaPlayer::play() {
  if (state_ == Ready || state_ == Paused) {
    state_ = Playing;
    emit stateChanged(Playing);
    decoder_->setPlaying(true);
    playbackTimer_->start(16);
    LOG_MP(info, "play() state=Playing");
  }
}

void MediaPlayer::pause() {
  if (state_ == Playing) {
    state_ = Paused;
    emit stateChanged(Paused);
    decoder_->setPlaying(false);
    playbackTimer_->stop();
    LOG_MP(info, "pause() state=Paused");
  }
}

void MediaPlayer::stop() {
  playbackTimer_->stop();
  decoder_->stop();
  decoder_->wait(5000);
  audioOutput_->close();
  videoRenderer_->clear();
  state_ = Stopped;
  currentTimeMs_ = 0;
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
  audioOutput_->flush();
  currentTimeMs_ = ms;

  if (oldState == Playing) {
    play();
  }

  LOG_MP(info, "seek() complete");
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
  // 1. Process audio frames
  while (auto aframe = decoder_->dequeueAudioFrame()) {
    audioOutput_->write(aframe->pcmData.constData(), aframe->pcmData.size());
    LOG_MP(debug, "audio write() bytes=" << aframe->pcmData.size());
  }

  // 2. Calculate audio clock
  double audioClock = 0.0;
  if (decoder_->hasAudio() && audioOutput_->isOpen()) {
    audioClock =
        audioOutput_->samplesPlayed() / double(decoder_->audioSampleRate());
  } else {
    audioClock = currentTimeMs_ / 1000.0;
  }

  // 3. Process video frame (sync)
  auto vframe = decoder_->dequeueVideoFrame();
  if (!vframe) {
    return;
  }

  double videoPts = vframe->ptsMs / 1000.0;
  double delayMs = (videoPts - audioClock) * 1000.0;

  bool shouldRender = true;
  if (decoder_->hasAudio()) {
    if (delayMs > 5.0) {
      QThread::msleep(static_cast<int>(delayMs));
    } else if (delayMs < -40.0) {
      droppedFrames_++;
      shouldRender = false;
      LOG_MP(debug, "sync videoPts=" << videoPts << " audioClock=" << audioClock
                                     << " delay=" << delayMs << " action=drop");
    }
  }

  // 4. Render frame
  if (shouldRender) {
    videoRenderer_->renderFrame(*vframe);
    currentTimeMs_ = vframe->ptsMs;
    emit timeChanged(currentTimeMs_);
    renderedFrames_++;
    LOG_MP(debug, "sync videoPts=" << videoPts << " audioClock=" << audioClock
                                   << " delay=" << delayMs
                                   << " action=render");
  }

  // 5. Check for end of stream
  if (decoder_->videoQueueSize() == 0 && decoder_->audioQueueSize() == 0 &&
      decoder_->isFinished()) {
    stop();
    emit playbackFinished();
  }
}

void MediaPlayer::onDecoderError(const QString &error) {
  LOG_MP(critical, "Decoder error: " << error);
  emit playbackError(error);
  stop();
}

void MediaPlayer::onEndOfStream() {
  LOG_MP(info, "EOS");
  emit playbackFinished();
  stop();
}
