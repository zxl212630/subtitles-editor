#pragma once

#include "AsrServiceBase.h"
#include <QJsonArray>
#include <QNetworkReply>
#include <QPointer>

class QNetworkAccessManager;

class TencentAsrService : public AsrServiceBase {
  Q_OBJECT

public:
  explicit TencentAsrService(QObject *parent = nullptr);
  ~TencentAsrService();

  void transcribe(const QString &audioUrl) override;
  void abort();

private slots:
  void onTaskCreated(QNetworkReply *reply);
  void onResultQueried(QNetworkReply *reply);

private:
  void createRecTask(const QString &audioUrl);
  void queryTaskStatus(const QString &taskId);
  void parseResultDetail(const QJsonArray &resultDetail,
                         QList<TranscriptSegment> &segments);
  QByteArray signTC3(const QByteArray &key, const QByteArray &data);
  QJsonObject payload(const QString &audioUrl);

  QString secretId_;
  QString secretKey_;
  QString appId_;
  QString currentTaskId_;
  QNetworkAccessManager *networkManager_;
  QPointer<QNetworkReply> activeReply_;
  bool isAborted_ = false;
  int pollingAttempts_ = 0;
  static constexpr int kMaxPollingAttempts = 60;
};
