#pragma once

#include <QObject>
#include <QString>

class QNetworkReply;

class OssUploader : public QObject {
  Q_OBJECT

public:
  explicit OssUploader(QObject *parent = nullptr);
  void abort();

signals:
  void uploadStarted();
  void uploadProgress(int percent);
  void uploadProgressBytes(qint64 bytesSent, qint64 bytesTotal);
  void uploadFinished(const QString &ossUrl, const QString &presignedUrl);
  void uploadFailed(const QString &errorMessage);

public slots:
  void upload(const QString &localFilePath);

private:
  QString generateOssPath(const QString &localPath);
  QString generatePresignedUrl(const QString &ossPath);
  QByteArray hmacSha1(const QByteArray &key, const QByteArray &data);
  QString computeSignature(const QString &stringToSign);

  QString ossBucket_;
  QString ossRegion_;
  QString ossAccessKeyId_;
  QString ossAccessKeySecret_;
  QNetworkReply *reply_ = nullptr;
  bool aborting_ = false;
};
