#include "TencentAsrService.h"
#include "ConfigManager.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
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

void TencentAsrService::abort() {
  qDebug() << "[TencentAsrService] abort(), isAborted_=" << isAborted_
           << "activeReply_=" << activeReply_ << "isRunning="
           << (activeReply_ ? activeReply_->isRunning() : false);
  if (isAborted_)
    return;
  isAborted_ = true;
  if (activeReply_ && activeReply_->isRunning()) {
    activeReply_->abort();
  }
  TranscriptResult result;
  result.success = false;
  result.errorMessage = "用户已取消识别";
  qDebug() << "[TencentAsrService] abort() emitting transcribeFinished";
  emit transcribeFinished(result);
}

void TencentAsrService::transcribe(const QString &audioUrl) {
  isAborted_ = false;
  pollingAttempts_ = 0;
  createRecTask(audioUrl);
}

QByteArray TencentAsrService::signTC3(const QByteArray &key,
                                      const QByteArray &data) {
  int blockSize = 64;

  // If key > blockSize, hash it
  QByteArray keyData = key;
  if (keyData.length() > blockSize) {
    keyData = QCryptographicHash::hash(keyData, QCryptographicHash::Sha256);
  }

  // Pad key with zeros to blockSize
  while (keyData.length() < blockSize) {
    keyData.append('\0');
  }

  QByteArray ipad(blockSize, 0x36);
  QByteArray opad(blockSize, 0x5c);

  for (int i = 0; i < blockSize; i++) {
    ipad[i] = keyData[i] ^ 0x36;
    opad[i] = keyData[i] ^ 0x5c;
  }

  QByteArray innerData = ipad + data;
  QByteArray innerHash =
      QCryptographicHash::hash(innerData, QCryptographicHash::Sha256);
  QByteArray outerData = opad + innerHash;
  return QCryptographicHash::hash(outerData, QCryptographicHash::Sha256);
}

void TencentAsrService::createRecTask(const QString &audioUrl) {
  qDebug() << "=== Creating ASR Task ===";
  qDebug() << "SecretId:" << secretId_;
  qDebug() << "AppId:" << appId_;
  qDebug() << "Audio URL:" << audioUrl;

  QString host = "asr.tencentcloudapi.com";
  QString path = "/";
  QString timestamp = QString::number(QDateTime::currentSecsSinceEpoch());
  QString date = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd");
  QString service = host.split(".").first();

  // Build canonical request
  // All headers sorted alphabetically, lowercase, each ending with newline
  QString httpRequestMethod = "POST";
  QString canonicalUri = "/";
  QString canonicalQueryString = "";
  QString canonicalHeaders = "content-type:application/json\n"
                             "host:" +
                             host +
                             "\n"
                             "x-tc-action:createrectask\n\n";
  QString signedHeaders = "content-type;host;x-tc-action";
  QByteArray payloadBytes =
      QJsonDocument(payload(audioUrl)).toJson(QJsonDocument::Compact);
  QString hashedRequestPayload =
      QString(QCryptographicHash::hash(payloadBytes, QCryptographicHash::Sha256)
                  .toHex());

  QString canonicalRequest = httpRequestMethod + "\n" + canonicalUri + "\n" +
                             canonicalQueryString + "\n" + canonicalHeaders +
                             signedHeaders + "\n" + hashedRequestPayload;

  qDebug() << "=== Canonical Request ===";
  qDebug() << canonicalRequest;

  // Build string to sign
  QString algorithm = "TC3-HMAC-SHA256";
  QString credentialScope = date + "/" + service + "/tc3_request";
  QString hashedCanonicalRequest =
      QString(QCryptographicHash::hash(canonicalRequest.toUtf8(),
                                       QCryptographicHash::Sha256)
                  .toHex());

  QString stringToSign = algorithm + "\n" + timestamp + "\n" + credentialScope +
                         "\n" + hashedCanonicalRequest;

  qDebug() << "=== String to Sign ===";
  qDebug() << stringToSign;

  // TC3-HMAC-SHA256 signature: 4-step key derivation
  // Step 1: HMAC("TC3" + secretKey, date)
  QByteArray secretDate = signTC3(("TC3" + secretKey_).toUtf8(), date.toUtf8());
  // Step 2: HMAC(secretDate, service)
  QByteArray secretService = signTC3(secretDate, service.toUtf8());
  // Step 3: HMAC(secretService, "tc3_request")
  QByteArray secretSigning = signTC3(secretService, QByteArray("tc3_request"));
  // Step 4: HMAC(secretSigning, stringToSign)
  QByteArray signature = signTC3(secretSigning, stringToSign.toUtf8());
  QString signatureStr = QString(signature.toHex());

  // Build authorization header
  QString authorization =
      QString(
          "TC3-HMAC-SHA256 Credential=%1/%2, SignedHeaders=%3, Signature=%4")
          .arg(secretId_)
          .arg(credentialScope)
          .arg(signedHeaders)
          .arg(signatureStr);

  qDebug() << "=== Authorization ===";
  qDebug() << authorization;

  QUrl url("https://" + host);
  QNetworkRequest request(url);
  request.setRawHeader("Content-Type", "application/json");
  request.setRawHeader("Host", host.toUtf8());
  request.setRawHeader("X-TC-Action", "CreateRecTask");
  request.setRawHeader("X-TC-Timestamp", timestamp.toUtf8());
  request.setRawHeader("X-TC-Version", "2019-06-14");
  request.setRawHeader("X-TC-Region", "ap-guangzhou");
  request.setRawHeader("Authorization", authorization.toUtf8());

  qDebug() << "Request payload:" << QString::fromUtf8(payloadBytes);

  QNetworkReply *reply = networkManager_->post(request, payloadBytes);
  activeReply_ = reply;
  connect(reply, &QNetworkReply::finished, this,
          [this, reply]() { onTaskCreated(reply); });
}

QJsonObject TencentAsrService::payload(const QString &audioUrl) {
  QJsonObject obj;
  obj["ChannelNum"] = 1;
  obj["EngineModelType"] = ConfigManager::instance().engineModelType();
  obj["ResTextFormat"] = 3;
  obj["Url"] = audioUrl;
  obj["SourceType"] = 0; // 0=URL
  
  bool enableDiarization = ConfigManager::instance().speakerDiarization();
  obj["SpeakerDiarization"] = enableDiarization ? 1 : 0;
  if (enableDiarization) {
    obj["SpeakerNumber"] = 0;
  }
  
  obj["SentenceMaxLength"] = ConfigManager::instance().sentenceMaxLength();
  obj["FilterPunc"] = 1; // 过滤句末标点（去掉末尾标点符号）
  return obj;
}

void TencentAsrService::onTaskCreated(QNetworkReply *reply) {
  if (isAborted_) {
    reply->deleteLater();
    return;
  }
  if (reply->error() != QNetworkReply::NoError) {
    TranscriptResult result;
    result.success = false;
    result.errorMessage = "Network error: " + reply->errorString();
    emit transcribeFinished(result);
    reply->deleteLater();
    return;
  }

  QByteArray data = reply->readAll();
  qDebug() << "=== CreateRecTask Response ===";
  qDebug() << "Raw response:" << QString::fromUtf8(data);
  QJsonDocument doc = QJsonDocument::fromJson(data);
  QJsonObject resp = doc.object();

  if (resp.contains("Response")) {
    QJsonObject response = resp["Response"].toObject();
    if (response.contains("Error")) {
      QString errorMsg = response["Error"].toObject()["Message"].toString();
      TranscriptResult result;
      result.success = false;
      result.errorMessage = "CreateRecTask failed: " + errorMsg;
      emit transcribeFinished(result);
      reply->deleteLater();
      return;
    }
    qDebug() << "Response object:" << response;
    QJsonObject dataObj = response["Data"].toObject();
    currentTaskId_ =
        QString::number(dataObj["TaskId"].toVariant().toLongLong());
    qDebug() << "Task created, TaskId:" << currentTaskId_;
    queryTaskStatus(currentTaskId_);
  } else {
    TranscriptResult result;
    result.success = false;
    result.errorMessage = "CreateRecTask failed: " + QString::fromUtf8(data);
    emit transcribeFinished(result);
  }
  reply->deleteLater();
}

void TencentAsrService::queryTaskStatus(const QString &taskId) {
  qDebug() << "=== Querying Task Status ===";
  qDebug() << "TaskId:" << taskId;

  QString host = "asr.tencentcloudapi.com";
  QString path = "/";
  QString timestamp = QString::number(QDateTime::currentSecsSinceEpoch());
  QString date = QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd");

  // Build canonical request for query
  // All headers sorted alphabetically, lowercase, each ending with newline
  QString httpRequestMethod = "POST";
  QString canonicalUri = "/";
  QString canonicalQueryString = "";
  QString canonicalHeaders = "content-type:application/json\n"
                             "host:" +
                             host +
                             "\n"
                             "x-tc-action:describetaskstatus\n\n";
  QString signedHeaders = "content-type;host;x-tc-action";

  QJsonObject queryPayload;
  queryPayload["TaskId"] = taskId.toLongLong();
  QByteArray payloadBytes =
      QJsonDocument(queryPayload).toJson(QJsonDocument::Compact);
  QString hashedRequestPayload =
      QString(QCryptographicHash::hash(payloadBytes, QCryptographicHash::Sha256)
                  .toHex());

  QString canonicalRequest = httpRequestMethod + "\n" + canonicalUri + "\n" +
                             canonicalQueryString + "\n" + canonicalHeaders +
                             signedHeaders + "\n" + hashedRequestPayload;

  QString algorithm = "TC3-HMAC-SHA256";
  QString service = host.split(".").first();
  QString credentialScope = date + "/" + service + "/tc3_request";
  QString hashedCanonicalRequest =
      QString(QCryptographicHash::hash(canonicalRequest.toUtf8(),
                                       QCryptographicHash::Sha256)
                  .toHex());

  QString stringToSign = algorithm + "\n" + timestamp + "\n" + credentialScope +
                         "\n" + hashedCanonicalRequest;

  // TC3-HMAC-SHA256 signature: 4-step key derivation
  QByteArray secretDate = signTC3(("TC3" + secretKey_).toUtf8(), date.toUtf8());
  QByteArray secretService = signTC3(secretDate, service.toUtf8());
  QByteArray secretSigning = signTC3(secretService, QByteArray("tc3_request"));
  QByteArray signature = signTC3(secretSigning, stringToSign.toUtf8());
  QString signatureStr = QString(signature.toHex());

  QString authorization =
      QString(
          "TC3-HMAC-SHA256 Credential=%1/%2, SignedHeaders=%3, Signature=%4")
          .arg(secretId_)
          .arg(credentialScope)
          .arg(signedHeaders)
          .arg(signatureStr);

  QUrl url("https://" + host);
  QNetworkRequest request(url);
  request.setRawHeader("Content-Type", "application/json");
  request.setRawHeader("Host", host.toUtf8());
  request.setRawHeader("X-TC-Action", "DescribeTaskStatus");
  request.setRawHeader("X-TC-Timestamp", timestamp.toUtf8());
  request.setRawHeader("X-TC-Version", "2019-06-14");
  request.setRawHeader("X-TC-Region", "ap-guangzhou");
  request.setRawHeader("Authorization", authorization.toUtf8());

  QNetworkReply *reply = networkManager_->post(request, payloadBytes);
  activeReply_ = reply;
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, taskId]() { onResultQueried(reply); });
}

void TencentAsrService::onResultQueried(QNetworkReply *reply) {
  if (isAborted_) {
    reply->deleteLater();
    return;
  }
  if (reply->error() != QNetworkReply::NoError) {
    TranscriptResult result;
    result.success = false;
    result.errorMessage = "Network error: " + reply->errorString();
    emit transcribeFinished(result);
    reply->deleteLater();
    return;
  }

  QByteArray data = reply->readAll();
  qDebug() << "=== QueryTaskStatus Response ===";
  qDebug() << "Raw response:" << QString::fromUtf8(data);
  QJsonDocument doc = QJsonDocument::fromJson(data);
  QJsonObject resp = doc.object();

  TranscriptResult result;

  if (++pollingAttempts_ > kMaxPollingAttempts) {
    result.success = false;
    result.errorMessage = "ASR polling timeout";
    emit transcribeFinished(result);
    pollingAttempts_ = 0;
    reply->deleteLater();
    return;
  }

  if (resp.contains("Response")) {
    QJsonObject response = resp["Response"].toObject();
    if (response.contains("Error")) {
      QString errorMsg = response["Error"].toObject()["Message"].toString();
      result.success = false;
      result.errorMessage = "Query failed: " + errorMsg;
      emit transcribeFinished(result);
      reply->deleteLater();
      return;
    }
    QJsonObject dataObj = response["Data"].toObject();
    int status = dataObj["Status"].toInt();
    qDebug() << "Polling attempt" << pollingAttempts_ << "- Status:" << status;

    if (status == 2) { // 完成
      result.success = true;
      QJsonArray resultDetail = dataObj["ResultDetail"].toArray();
      parseResultDetail(resultDetail, result.segments);
      emit transcribeFinished(result);
    } else if (status == 3 || status == 4) { // 失败
      result.success = false;
      result.errorMessage =
          "ASR task failed, status: " + QString::number(status);
      emit transcribeFinished(result);
    } else {
      // 继续轮询
      QTimer::singleShot(1000, this, [this, taskId = currentTaskId_]() {
        if (!isAborted_)
          queryTaskStatus(taskId);
      });
      emit transcribeProgress(50);
    }
  } else {
    result.success = false;
    result.errorMessage = "Query failed: " + QString::fromUtf8(data);
    emit transcribeFinished(result);
  }
  reply->deleteLater();
}

void TencentAsrService::parseResultDetail(const QJsonArray &resultDetail,
                                          QList<TranscriptSegment> &segments) {
  for (const QJsonValue &val : resultDetail) {
    QJsonObject sentence = val.toObject();
    TranscriptSegment seg;
    seg.text = sentence["FinalSentence"].toString();
    seg.startMs = sentence["StartMs"].toVariant().toLongLong();
    seg.endMs = sentence["EndMs"].toVariant().toLongLong();
    seg.speakerId = sentence["SpeakerId"].toInt(-1); // 解析说话人 ID，默认 -1
    if (!seg.text.isEmpty()) {
      segments.append(seg);
    }
  }
}