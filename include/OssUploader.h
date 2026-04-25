#pragma once

#include <QObject>
#include <QString>

class OssUploader : public QObject {
  Q_OBJECT

public:
  explicit OssUploader(QObject *parent = nullptr);

signals:
  void uploadStarted();
  void uploadProgress(int percent);
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
};
