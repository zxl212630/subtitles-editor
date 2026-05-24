#include "CosUploader.h"
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

CosUploader::CosUploader(QObject *parent)
    : QObject(parent), networkManager_(new QNetworkAccessManager(this)) {
  cosSecretId_ = ConfigManager::instance().cosSecretId();
  cosSecretKey_ = ConfigManager::instance().cosSecretKey();
  cosBucket_ = ConfigManager::instance().cosBucket();
  cosRegion_ = ConfigManager::instance().cosRegion();
}

void CosUploader::abort() {
  qDebug() << "[CosUploader] abort(), reply_=" << reply_
           << "isRunning=" << (reply_ ? reply_->isRunning() : false);
  if (reply_ && reply_->isRunning()) {
    aborting_ = true;
    reply_->abort();
    emit uploadFailed(tr("用户已取消上传"));
  }
}

QString CosUploader::generateCosPath(const QString &localPath) {
  Q_UNUSED(localPath);
  QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
  return "asr/" + timestamp + "_audio.wav";
}

QByteArray CosUploader::hmacSha1(const QByteArray &key, const QByteArray &data) const {
  const int blockSize = 64;
  QByteArray keyData = key;
  if (keyData.length() > blockSize) {
    keyData = QCryptographicHash::hash(keyData, QCryptographicHash::Sha1);
  }
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
  QByteArray innerHash = QCryptographicHash::hash(innerData, QCryptographicHash::Sha1);
  QByteArray outerData = opad + innerHash;
  return QCryptographicHash::hash(outerData, QCryptographicHash::Sha1);
}

QString CosUploader::calculateSignature(const QString &method,
                                        const QString &cosPath,
                                        const QString &keyTime,
                                        const QString &host) const {
  QString lowerMethod = method.toLower();
  QString httpHeaders = QString("host=%1\n").arg(host);
  QString formatString = QString("%1\n/%2\n\n%3")
                             .arg(lowerMethod)
                             .arg(cosPath)
                             .arg(httpHeaders);

  QString formatStringSha1 = QString(
      QCryptographicHash::hash(formatString.toUtf8(), QCryptographicHash::Sha1)
          .toHex());

  // StringToSign 末尾有换行符
  QString stringToSign =
      QString("sha1\n%1\n%2\n").arg(keyTime).arg(formatStringSha1);

  // 第一步：SignKey 是以 SecretKey 为密钥，KeyTime 为数据进行 HMAC-SHA1 哈希，所得结果的十六进制小写字符串
  QByteArray signKey = hmacSha1(cosSecretKey_.toUtf8(), keyTime.toUtf8()).toHex();

  // 第二步：使用十六进制的 signKey 作为密钥，对 stringToSign 进行 HMAC-SHA1 哈希
  QByteArray signatureBytes = hmacSha1(signKey, stringToSign.toUtf8());
  return QString(signatureBytes.toHex());
}

QString CosUploader::generatePresignedUrl(const QString &cosPath) {
  QString host =
      QString("%1.cos.%2.myqcloud.com").arg(cosBucket_).arg(cosRegion_);
  QString endpoint = "https://" + host;

  qint64 start = QDateTime::currentSecsSinceEpoch() - 60;
  qint64 end = start + 3600;
  QString keyTime = QString("%1;%2").arg(start).arg(end);

  QString signature = calculateSignature("get", cosPath, keyTime, host);

  return QString("%1/%2?q-sign-algorithm=sha1"
                 "&q-ak=%3"
                 "&q-sign-time=%4"
                 "&q-key-time=%5"
                 "&q-header-list=host"
                 "&q-url-param-list="
                 "&q-signature=%6")
      .arg(endpoint)
      .arg(cosPath)
      .arg(cosSecretId_)
      .arg(keyTime)
      .arg(keyTime)
      .arg(signature);
}

void CosUploader::upload(const QString &localFilePath) {
  aborting_ = false;

  if (cosSecretId_.isEmpty() || cosSecretKey_.isEmpty() ||
      cosBucket_.isEmpty() || cosRegion_.isEmpty()) {
    QString error =
        tr("未配置腾讯云 COS 密钥。请检查 config.ini 配置文件：") +
        ConfigManager::instance().configFilePath();
    qDebug() << "ERROR:" << error;
    emit uploadFailed(error);
    return;
  }

  QFile file(localFilePath);
  if (!file.open(QIODevice::ReadOnly)) {
    QString error = tr("无法打开文件：") + localFilePath;
    qDebug() << "ERROR:" << error;
    emit uploadFailed(error);
    return;
  }

  QByteArray fileData = file.readAll();
  file.close();

  QString cosPath = generateCosPath(localFilePath);
  QString host =
      QString("%1.cos.%2.myqcloud.com").arg(cosBucket_).arg(cosRegion_);
  QString urlStr = "https://" + host + "/" + cosPath;

  QNetworkRequest request((QUrl(urlStr)));

  qint64 start = QDateTime::currentSecsSinceEpoch() - 60;
  qint64 end = start + 3600;
  QString keyTime = QString("%1;%2").arg(start).arg(end);

  QString signature = calculateSignature("put", cosPath, keyTime, host);

  // Authorization Header
  QString authorization = QString("q-sign-algorithm=sha1"
                                  "&q-ak=%1"
                                  "&q-sign-time=%2"
                                  "&q-key-time=%3"
                                  "&q-header-list=host"
                                  "&q-url-param-list="
                                  "&q-signature=%4")
                              .arg(cosSecretId_)
                              .arg(keyTime)
                              .arg(keyTime)
                              .arg(signature);

  request.setRawHeader("Authorization", authorization.toUtf8());
  request.setRawHeader("Content-Type", "audio/wav");
  request.setRawHeader("Host", host.toUtf8());

  reply_ = networkManager_->put(request, fileData);

  connect(reply_, &QNetworkReply::uploadProgress, this,
          [this](qint64 bytesSent, qint64 bytesTotal) {
            emit uploadProgressBytes(bytesSent, bytesTotal);
            if (bytesTotal > 0) {
              int percent = static_cast<int>((bytesSent * 100) / bytesTotal);
              emit uploadProgress(percent);
            }
          });

  connect(reply_, &QNetworkReply::finished, this, [this, urlStr, cosPath]() {
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
      QString presignedUrl = generatePresignedUrl(cosPath);
      emit uploadFinished(urlStr, presignedUrl);
    } else {
      QString errorMsg =
          QString(tr("上传失败：%1")).arg(reply_->errorString());
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
