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

void OssUploader::upload(const QString &localFilePath) {
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
