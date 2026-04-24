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
  qDebug() << "=== OssUploader::upload() ===";
  qDebug() << "Local file:" << localFilePath;

  // Validate credentials
  if (ossAccessKeyId_.isEmpty() || ossAccessKeySecret_.isEmpty() ||
      ossBucket_.isEmpty() || ossRegion_.isEmpty()) {
    QString error = "OSS credentials not configured. Please check config.ini at: " +
                    ConfigManager::instance().configFilePath();
    qDebug() << "ERROR:" << error;
    emit uploadFailed(error);
    return;
  }

  qDebug() << "AccessKeyId:" << ossAccessKeyId_;
  qDebug() << "Bucket:" << ossBucket_;
  qDebug() << "Region:" << ossRegion_;

  QFile *file = new QFile(localFilePath, this);
  if (!file->open(QIODevice::ReadOnly)) {
    QString error = "Cannot open file: " + localFilePath;
    qDebug() << "ERROR:" << error;
    emit uploadFailed(error);
    return;
  }

  QByteArray fileData = file->readAll();
  qDebug() << "File size:" << fileData.size() << "bytes";
  file->close();

  QString ossPath = generateOssPath(localFilePath);
  qDebug() << "OSS path:" << ossPath;

  QString endpoint =
      "https://" + ossBucket_ + ".oss-" + ossRegion_ + ".aliyuncs.com";
  QString urlStr = endpoint + "/" + ossPath;
  qDebug() << "URL:" << urlStr;

  QNetworkRequest request((QUrl(urlStr)));

  // Build string to sign - Alibaba Cloud OSS format
  QString contentType = "audio/wav";
  QString date =
      QDateTime::currentDateTimeUtc().toString("ddd, dd MMM yyyy HH:mm:ss") +
      " GMT";

  // StringToSign format:
  // PUT\n
  // \n (empty Content-MD5)
  // {contentType}\n
  // {date}\n
  // /{bucket}/{object}
  QString stringToSign = QString("PUT\n\n%1\n%2\n/%3/%4")
                             .arg(contentType)
                             .arg(date)
                             .arg(ossBucket_)
                             .arg(ossPath);

  qDebug() << "=== Signature Calculation ===";
  qDebug() << "StringToSign (raw):" << stringToSign;
  qDebug() << "StringToSign (escaped):" << stringToSign.toUtf8().toHex();

  // Calculate HMAC-SHA1
  QByteArray key = ossAccessKeySecret_.toUtf8();
  QByteArray data = stringToSign.toUtf8();
  qDebug() << "Key length:" << key.length();
  qDebug() << "Data length:" << data.length();

  QByteArray hash = hmacSha1(key, data);
  QString signature = hash.toBase64();
  qDebug() << "HMAC-SHA1 (base64):" << signature;

  QString authHeader = QString("OSS %1:%2").arg(ossAccessKeyId_).arg(signature);
  qDebug() << "Authorization header:" << authHeader;

  request.setRawHeader("Authorization", authHeader.toUtf8());
  request.setRawHeader("Date", date.toUtf8());
  request.setRawHeader("Content-Type", "audio/wav");
  request.setRawHeader("Content-Length", QByteArray::number(fileData.size()));

  qDebug() << "=== Sending Request ===";

  QNetworkAccessManager *manager = new QNetworkAccessManager(this);
  QNetworkReply *reply = manager->put(request, fileData);

  connect(reply, &QNetworkReply::uploadProgress, this,
          [this](qint64 bytesSent, qint64 bytesTotal) {
            qDebug() << "Progress:" << bytesSent << "/" << bytesTotal;
            if (bytesTotal > 0) {
              int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
              emit uploadProgress(percent);
            }
          });

  connect(reply, &QNetworkReply::finished, this, [this, reply, urlStr]() {
    qDebug() << "=== Request Finished ===";
    qDebug() << "Error:" << reply->error() << reply->errorString();
    QByteArray response = reply->readAll();
    qDebug() << "Response body:" << response;

    if (reply->error() == QNetworkReply::NoError) {
      qDebug() << "Upload SUCCESS! URL:" << urlStr;
      emit uploadFinished(urlStr);
    } else {
      QString errorMsg = QString("Upload failed: %1").arg(reply->errorString());
      if (!response.isEmpty()) {
        errorMsg += QString(" | Response: %1").arg(QString::fromUtf8(response));
      }
      qDebug() << "Upload FAILED:" << errorMsg;
      emit uploadFailed(errorMsg);
    }
    reply->deleteLater();
  });

  emit uploadStarted();
}
