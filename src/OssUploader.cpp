#include "OssUploader.h"
#include "ConfigManager.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

OssUploader::OssUploader(QObject *parent) : QObject(parent) {
  ossAccessKeyId_ = ConfigManager::instance().ossAccessKeyId();
  ossAccessKeySecret_ = ConfigManager::instance().ossAccessKeySecret();
  ossBucket_ = ConfigManager::instance().ossBucket();
  ossRegion_ = ConfigManager::instance().ossRegion();
}

QString OssUploader::generateOssPath(const QString &localPath) {
  QFileInfo info(localPath);
  QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
  return "asr/" + timestamp + "_" + info.fileName();
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

void OssUploader::upload(const QString &localFilePath) {
  // Validate credentials
  if (ossAccessKeyId_.isEmpty() || ossAccessKeySecret_.isEmpty() ||
      ossBucket_.isEmpty() || ossRegion_.isEmpty()) {
    emit uploadFailed("OSS credentials not configured. Please check config.ini at: " +
                      ConfigManager::instance().configFilePath());
    return;
  }

  QFile *file = new QFile(localFilePath, this);
  if (!file->open(QIODevice::ReadOnly)) {
    emit uploadFailed("Cannot open file: " + localFilePath);
    return;
  }

  QString ossPath = generateOssPath(localFilePath);
  QString endpoint =
      "https://" + ossBucket_ + ".oss-" + ossRegion_ + ".aliyuncs.com";
  QString urlStr = endpoint + "/" + ossPath;

  QNetworkRequest request((QUrl(urlStr)));

  // Build string to sign for signature
  QString contentType = "audio/wav";
  QString date =
      QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy HH:mm:ss") +
      " GMT";
  QString stringToSign = QString("PUT\n\n%1\n%2\n/%3/%4")
                             .arg(contentType)
                             .arg(date)
                             .arg(ossBucket_)
                             .arg(ossPath);

  QString signature = computeSignature(stringToSign);
  QString authHeader = QString("OSS %1:%2").arg(ossAccessKeyId_).arg(signature);
  request.setRawHeader("Authorization", authHeader.toUtf8());
  request.setRawHeader("Date", date.toUtf8());
  request.setRawHeader("Content-Type", "audio/wav");

  QNetworkAccessManager *manager = new QNetworkAccessManager(this);
  QNetworkReply *reply = manager->put(request, file->readAll());

  connect(reply, &QNetworkReply::uploadProgress, this,
          [this](qint64 bytesSent, qint64 bytesTotal) {
            if (bytesTotal > 0) {
              int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
              emit uploadProgress(percent);
            }
          });

  connect(reply, &QNetworkReply::finished, this, [this, reply, urlStr]() {
    if (reply->error() == QNetworkReply::NoError) {
      emit uploadFinished(urlStr);
    } else {
      emit uploadFailed("Upload failed: " + reply->errorString());
    }
    reply->deleteLater();
  });

  emit uploadStarted();
}
