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
  format_.setChannelCount(channels);
  format_.setSampleFormat(QAudioFormat::Int16);

  QAudioDevice device = QMediaDevices::defaultAudioOutput();
  if (!device.isFormatSupported(format_)) {
    LOG_AUDIO(warning, "Requested format not supported, using nearest format");
    format_ = device.preferredFormat();
  }

  audioSink_ = new QAudioSink(device, format_, this);
  ioDevice_ = audioSink_->start();

  sampleRate_ = sampleRate;
  channels_ = channels;
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

  qint64 written = ioDevice_->write(static_cast<const char *>(data),
                                    static_cast<qint64>(size));
  totalBytesWritten_ += written;
  LOG_AUDIO(debug, "write() bytes=" << written
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

void QtAudioOutput::setVolume(qreal volume) {
  if (audioSink_) {
    audioSink_->setVolume(volume);
  }
}

bool QtAudioOutput::isOpen() const { return audioSink_ != nullptr; }
