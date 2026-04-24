#pragma once

#include "AsrServiceBase.h"
#include <QNetworkReply>

class QNetworkAccessManager;

class TencentAsrService : public AsrServiceBase {
  Q_OBJECT

public:
  explicit TencentAsrService(QObject *parent = nullptr);
  ~TencentAsrService();

  void transcribe(const QString &audioUrl) override;

private slots:
  void onTaskCreated(QNetworkReply *reply);
  void onResultQueried(QNetworkReply *reply);

private:
  void createRecTask(const QString &audioUrl);
  void queryTaskStatus(const QString &taskId);
  void parseResultText(const QString &resultStr,
                       QList<TranscriptSegment> &segments);

  QString secretId_;
  QString secretKey_;
  QString appId_;
  QString currentTaskId_;
  QNetworkAccessManager *networkManager_;
  int pollingAttempts_ = 0;
  static constexpr int kMaxPollingAttempts = 60;
};
