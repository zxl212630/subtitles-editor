#include "OssUploader.h"
#include "ConfigManager.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

OssUploader::OssUploader(QObject *parent)
    : QObject(parent), networkManager_(new QNetworkAccessManager(this)) {
  ossAccessKeyId_ = ConfigManager::instance().ossAccessKeyId();
  ossAccessKeySecret_ = ConfigManager::instance().ossAccessKeySecret();
  ossBucket_ = ConfigManager::instance().ossBucket();
  ossRegion_ = ConfigManager::instance().ossRegion();
}

void OssUploader::abort() {
  qDebug() << "[OssUploader] abort(), reply_=" << reply_
           << "isRunning=" << (reply_ ? reply_->isRunning() : false);
  if (reply_ && reply_->isRunning()) {
    aborting_ = true;
    reply_->abort();
    emit uploadFailed(tr("用户已取消上传"));
  }
}

QString OssUploader::generateOssPath(const QString &localPath) {
  QFileInfo info(localPath);
  QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
  // Use timestamp-based name to avoid Chinese characters in OSS path
  return "asr/" + timestamp + "_audio.wav";
}

QByteArray OssUploader::hmacSha1(const QByteArray &key,
                                 const QByteArray &data) {
  const int blockSize = 64;
  QByteArray keyData = key;

  // If key > blockSize, hash it
  if (keyData.length() > blockSize) {
    keyData = QCryptographicHash::hash(keyData, QCryptographicHash::Sha1);
  }

  // Pad key with zeros to blockSize
  while (keyData.length() < blockSize) {
    keyData.append('\0');
  }

  QByteArray ipad(blockSize, 0);
  QByteArray opad(blockSize, 0);

  for (int i = 0; i < blockSize; i++) {
    ipad[i] = keyData[i] ^ 0x36;
    opad[i] = keyData[i] ^ 0x5c;
  }

  QByteArray innerData = ipad + data;
  QByteArray innerHash =
      QCryptographicHash::hash(innerData, QCryptographicHash::Sha1);
  QByteArray outerData = opad + innerHash;
  return QCryptographicHash::hash(outerData, QCryptographicHash::Sha1);
}

QString OssUploader::computeSignature(const QString &stringToSign) {
  QByteArray key = ossAccessKeySecret_.toUtf8();
  QByteArray data = stringToSign.toUtf8();
  QByteArray hash = hmacSha1(key, data);
  return hash.toBase64();
}

QString OssUploader::generatePresignedUrl(const QString &ossPath) {
  // Generate presigned URL for public access
  // Format:
  // https://bucket.region.aliyuncs.com/path?Expires=timestamp&OSSAccessKeyId=key&Signature=signature

  QString endpoint =
      "https://" + ossBucket_ + ".oss-" + ossRegion_ + ".aliyuncs.com";

  // Expires in 1 hour (3600 seconds)
  qint64 expireTimestamp = QDateTime::currentSecsSinceEpoch() + 3600;

  // StringToSign format for GET: GET\n\n\n{expires}\n/{bucket}/{object}
  // Note: ossPath should be URL-encoded in the string to sign
  QString stringToSign = QString("GET\n\n\n%1\n/%2/%3")
                             .arg(expireTimestamp)
                             .arg(ossBucket_)
                             .arg(ossPath);

  // Compute signature using HMAC-SHA1
  QByteArray key = ossAccessKeySecret_.toUtf8();
  QByteArray data = stringToSign.toUtf8();
  QByteArray signatureBytes = hmacSha1(key, data);
  QString signature = QString::fromLatin1(signatureBytes.toBase64());

  // Build URL manually with proper query parameters
  QString urlStr =
      QString("%1/%2?Expires=%3&OSSAccessKeyId=%4&Signature=%5")
          .arg(endpoint)
          .arg(ossPath)
          .arg(expireTimestamp)
          .arg(ossAccessKeyId_)
          .arg(QString::fromLatin1(QUrl::toPercentEncoding(signature)));

  return urlStr;
}

void OssUploader::upload(const QString &localFilePath) {
  aborting_ = false;

  // Validate credentials
  if (ossAccessKeyId_.isEmpty() || ossAccessKeySecret_.isEmpty() ||
      ossBucket_.isEmpty() || ossRegion_.isEmpty()) {
    QString error =
        "OSS credentials not configured. Please check config.ini at: " +
        ConfigManager::instance().configFilePath();
    qDebug() << "ERROR:" << error;
    emit uploadFailed(error);
    return;
  }

  QFile file(localFilePath);
  if (!file.open(QIODevice::ReadOnly)) {
    QString error = "Cannot open file: " + localFilePath;
    qDebug() << "ERROR:" << error;
    emit uploadFailed(error);
    return;
  }

  QByteArray fileData = file.readAll();
  file.close();

  QString ossPath = generateOssPath(localFilePath);

  QString endpoint =
      "https://" + ossBucket_ + ".oss-" + ossRegion_ + ".aliyuncs.com";
  QString urlStr = endpoint + "/" + ossPath;

  QNetworkRequest request((QUrl(urlStr)));

  // Build string to sign - Alibaba Cloud OSS format
  QString contentType = "audio/wav";
  QString date =
      QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy HH:mm:ss") +
      " GMT";

  // Calculate Content-MD5 first (needed for signature)
  QByteArray contentMd5 =
      QCryptographicHash::hash(fileData, QCryptographicHash::Md5).toBase64();

  // StringToSign format:
  // PUT\n
  // {Content-MD5}\n
  // {contentType}\n
  // {date}\n
  // /{bucket}/{object}
  //
  // Note: Object key in URL should be URL-encoded when signing
  QString stringToSign = "PUT\n";
  stringToSign += QString::fromLatin1(contentMd5) + "\n";
  stringToSign += contentType + "\n";
  stringToSign += date + "\n";
  stringToSign += "/" + ossBucket_ + "/" + ossPath;

  // Calculate HMAC-SHA1
  QByteArray key = ossAccessKeySecret_.toUtf8();
  QByteArray data = stringToSign.toUtf8();

  QByteArray hash = hmacSha1(key, data);
  QString signature = hash.toBase64();

  QString authHeader = QString("OSS %1:%2").arg(ossAccessKeyId_).arg(signature);

  request.setRawHeader("Authorization", authHeader.toUtf8());
  request.setRawHeader("Content-Type", contentType.toUtf8());
  request.setRawHeader("Content-MD5", contentMd5);
  request.setRawHeader("Date", date.toUtf8());

  reply_ = networkManager_->put(request, fileData);

  connect(reply_, &QNetworkReply::uploadProgress, this,
          [this](qint64 bytesSent, qint64 bytesTotal) {
            emit uploadProgressBytes(bytesSent, bytesTotal);
            if (bytesTotal > 0) {
              int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
              emit uploadProgress(percent);
            }
          });

  connect(reply_, &QNetworkReply::finished, this, [this, urlStr, ossPath]() {
    if (!reply_)
      return;
    QByteArray response = reply_->readAll();

    if (aborting_) {
      aborting_ = false;
      reply_->deleteLater();
      reply_ = nullptr;
      return;
    }

    if (reply_->error() == QNetworkReply::NoError) {
      QString presignedUrl = generatePresignedUrl(ossPath);
      emit uploadFinished(urlStr, presignedUrl);
    } else {
      QString errorMsg =
          tr("Upload failed: %1").arg(reply_->errorString());
      if (!response.isEmpty()) {
        errorMsg += QString(" | Response: %1").arg(QString::fromUtf8(response));
      }
      emit uploadFailed(errorMsg);
    }
    reply_->deleteLater();
    reply_ = nullptr;
  });

  emit uploadStarted();
}
