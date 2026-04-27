#include "QtAudioOutput.h"
#include <QDebug>

#define LOG_AUDIO_info(msg) qInfo() << "[AudioOutput]" << msg
#define LOG_AUDIO_warning(msg) qWarning() << "[AudioOutput]" << msg
#define LOG_AUDIO_critical(msg) qCritical() << "[AudioOutput]" << msg
#define LOG_AUDIO_debug(msg) qDebug() << "[AudioOutput]" << msg
#define LOG_AUDIO(level, msg) LOG_AUDIO_##level(msg)

QtAudioOutput::QtAudioOutput(QObject *parent) : QObject(parent) {}

QtAudioOutput::~QtAudioOutput() { close(); }

bool QtAudioOutput::open(int sampleRate, int channels) {
  LOG_AUDIO(info,
            "open() sampleRate=" << sampleRate << " channels=" << channels);

  close();

  format_.setSampleRate(sampleRate);
  format_.setSampleFormat(QAudioFormat::Int16);
  format_.setChannelConfig(
      QAudioFormat::defaultChannelConfigForChannelCount(channels));

  QAudioDevice device = QMediaDevices::defaultAudioOutput();
  if (!device.isFormatSupported(format_)) {
    LOG_AUDIO(warning,
              "Requested format not supported, using nearest format."
              " requested sampleRate="
                  << sampleRate << " channels=" << channels << " config="
                  << format_.channelConfig() << " preferred sampleRate="
                  << device.preferredFormat().sampleRate()
                  << " channels=" << device.preferredFormat().channelCount());
    format_ = device.preferredFormat();
  }

  audioSink_ = new QAudioSink(device, format_, this);
  // Buffer size for ~2 seconds of audio (use sink's actual format)
  int bufferSize = audioSink_->format().sampleRate() *
                   audioSink_->format().channelCount() *
                   static_cast<int>(sizeof(int16_t)) * 2;
  audioSink_->setBufferSize(bufferSize);
  ioDevice_ = audioSink_->start();

  sampleRate_ = audioSink_->format().sampleRate();
  channels_ = audioSink_->format().channelCount();
  totalBytesWritten_ = 0;

  return ioDevice_ != nullptr;
}

void QtAudioOutput::close() {
  if (audioSink_) {
    audioSink_->stop();
    delete audioSink_;
    audioSink_ = nullptr;
  }
  ioDevice_ = nullptr;
  LOG_AUDIO(info, "close()");
}

void QtAudioOutput::write(const void *data, size_t size) {
  if (!ioDevice_)
    return;

  qint64 remaining = static_cast<qint64>(size);
  const char *ptr = static_cast<const char *>(data);
  qint64 totalWritten = 0;

  while (remaining > 0) {
    qint64 written = ioDevice_->write(ptr, remaining);
    if (written <= 0) {
      break;
    }
    totalWritten += written;
    ptr += written;
    remaining -= written;
  }

  totalBytesWritten_ += totalWritten;
  LOG_AUDIO(debug, "write() bytes=" << totalWritten << "/" << size
                                    << " totalPlayed=" << totalBytesWritten_);
}

void QtAudioOutput::flush() {
  if (audioSink_) {
    audioSink_->reset();
  }
  totalBytesWritten_ = 0;
  LOG_AUDIO(debug, "flush() cleared");
}

qint64 QtAudioOutput::samplesPlayed() const {
  if (channels_ == 0)
    return 0;
  return totalBytesWritten_ / (channels_ * static_cast<int>(sizeof(int16_t)));
}

qint64 QtAudioOutput::playedUSecs() const {
  if (!audioSink_)
    return 0;
  return audioSink_->processedUSecs();
}

qint64 QtAudioOutput::bytesFree() const {
  if (!audioSink_)
    return 0;
  return audioSink_->bytesFree();
}

void QtAudioOutput::setVolume(qreal volume) {
  if (audioSink_) {
    audioSink_->setVolume(volume);
  }
}

bool QtAudioOutput::isOpen() const { return audioSink_ != nullptr; }
