#pragma once

#include "AsrServiceBase.h"
#include <QMutex>
#include <QPointer>
#include <QThread>

struct whisper_context;

class WhisperAsrService : public AsrServiceBase {
  Q_OBJECT

public:
  explicit WhisperAsrService(QObject *parent = nullptr);
  ~WhisperAsrService() override;

  void transcribe(const QString &audioFilePath) override;
  void abort();

  bool isAborted() const;

private:
  void runTranscribe(const QString &audioFilePath);

  struct whisper_context *ctx_ = nullptr;
  mutable QMutex mutex_;
  bool isAborted_ = false;
  QPointer<QThread> activeThread_;
};
