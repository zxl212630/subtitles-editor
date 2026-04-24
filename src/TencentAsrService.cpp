#include "TencentAsrService.h"
#include "ConfigManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>

TencentAsrService::TencentAsrService(QObject *parent)
    : AsrServiceBase(parent), networkManager_(new QNetworkAccessManager(this)) {
  secretId_ = ConfigManager::instance().tencentSecretId();
  secretKey_ = ConfigManager::instance().tencentSecretKey();
  appId_ = ConfigManager::instance().tencentAppId();
}

TencentAsrService::~TencentAsrService() = default;

void TencentAsrService::transcribe(const QString &audioUrl) {
  createRecTask(audioUrl);
}

void TencentAsrService::createRecTask(const QString &audioUrl) {
  QUrl url("https://asr.tencentcloudapi.com/");

  QJsonObject payload;
  payload["AppId"] = appId_;
  payload["ChannelNum"] = 1;
  payload["EngineType"] = "16k_zh";
  payload["Url"] = audioUrl;
  payload["SourceType"] = 0; // 0=URL

  QNetworkRequest request(url);
  request.setRawHeader("Content-Type", "application/json");
  request.setRawHeader("X-TC-Action", "CreateRecTask");
  request.setRawHeader("X-TC-Timestamp", QString::number(QDateTime::currentSecsSinceEpoch()).toUtf8());
  request.setRawHeader("X-TC-Version", "2019-06-14");
  request.setRawHeader("X-TC-Region", "");

  QNetworkReply *reply =
      networkManager_->post(request, QJsonDocument(payload).toJson());

  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onTaskCreated(reply); });
}

void TencentAsrService::onTaskCreated(QNetworkReply *reply) {
  if (reply->error() != QNetworkReply::NoError) {
    TranscriptResult result;
    result.success = false;
    result.errorMessage = "Network error: " + reply->errorString();
    emit transcribeFinished(result);
    reply->deleteLater();
    return;
  }

  QByteArray data = reply->readAll();
  QJsonDocument doc = QJsonDocument::fromJson(data);
  QJsonObject resp = doc.object();

  if (resp.contains("Response")) {
    QJsonObject response = resp["Response"].toObject();
    currentTaskId_ = QString::number(response["TaskId"].toDouble());
    queryTaskStatus(currentTaskId_);
  } else {
    TranscriptResult result;
    result.success = false;
    result.errorMessage = "CreateRecTask failed";
    emit transcribeFinished(result);
  }
  reply->deleteLater();
}

void TencentAsrService::queryTaskStatus(const QString &taskId) {
  QUrl url("https://asr.tencentcloudapi.com/");

  QJsonObject payload;
  payload["AppId"] = appId_;
  payload["TaskId"] = taskId.toLongLong();

  QNetworkRequest request(url);
  request.setRawHeader("Content-Type", "application/json");
  request.setRawHeader("X-TC-Action", "DescribeTaskStatus");
  request.setRawHeader("X-TC-Timestamp", QString::number(QDateTime::currentSecsSinceEpoch()).toUtf8());
  request.setRawHeader("X-TC-Version", "2019-06-14");
  request.setRawHeader("X-TC-Region", "");

  QNetworkReply *reply =
      networkManager_->post(request, QJsonDocument(payload).toJson());

  connect(reply, &QNetworkReply::finished, this,
          [this, reply, taskId]() { onResultQueried(reply); });
}

void TencentAsrService::onResultQueried(QNetworkReply *reply) {
  if (reply->error() != QNetworkReply::NoError) {
    TranscriptResult result;
    result.success = false;
    result.errorMessage = "Network error: " + reply->errorString();
    emit transcribeFinished(result);
    reply->deleteLater();
    return;
  }

  if (++pollingAttempts_ > kMaxPollingAttempts) {
    TranscriptResult result;
    result.success = false;
    result.errorMessage = "ASR polling timeout";
    emit transcribeFinished(result);
    pollingAttempts_ = 0;
    reply->deleteLater();
    return;
  }

  QByteArray data = reply->readAll();
  QJsonDocument doc = QJsonDocument::fromJson(data);
  QJsonObject resp = doc.object();

  TranscriptResult result;

  if (resp.contains("Response")) {
    QJsonObject response = resp["Response"].toObject();
    int status = response["Status"].toInt();

    if (status == 2) { // 完成
      result.success = true;
      QString resultStr = response["Result"].toString();
      parseResultText(resultStr, result.segments);
      emit transcribeFinished(result);
    } else if (status == 3 || status == 4) { // 失败
      result.success = false;
      result.errorMessage = "ASR task failed";
      emit transcribeFinished(result);
    } else {
      // 继续轮询
      QTimer::singleShot(1000, this, [this, taskId = currentTaskId_]() {
        queryTaskStatus(taskId);
      });
      emit transcribeProgress(50);
    }
  } else {
    result.success = false;
    result.errorMessage = "Query failed";
    emit transcribeFinished(result);
  }
  reply->deleteLater();
}

void TencentAsrService::parseResultText(const QString &resultStr,
                                        QList<TranscriptSegment> &segments) {
  // 腾讯云 Result 格式: [start,end,"text"][start,end,"text"]...
  // Use escaped double quotes inside character class
  QRegularExpression re("\\[(\\d+),(\\d+),\"([^\"]+)\"\\]");
  QRegularExpressionMatchIterator it = re.globalMatch(resultStr);

  while (it.hasNext()) {
    QRegularExpressionMatch match = it.next();
    TranscriptSegment seg;
    seg.startMs = match.capturedView(1).toLongLong();
    seg.endMs = match.capturedView(2).toLongLong();
    seg.text = match.capturedView(3).toString();
    segments.append(seg);
  }
}
