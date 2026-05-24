#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class CosUploader : public QObject {
  Q_OBJECT

public:
  explicit CosUploader(QObject *parent = nullptr);
  ~CosUploader() override = default;

  Q_INVOKABLE void abort();

signals:
  void uploadStarted();
  void uploadProgress(int percent);
  void uploadProgressBytes(qint64 bytesSent, qint64 bytesTotal);
  void uploadFinished(const QString &cosUrl, const QString &presignedUrl);
  void uploadFailed(const QString &errorMessage);

public slots:
  void upload(const QString &localFilePath);

private:
  QString generateCosPath(const QString &localPath);
  QString generatePresignedUrl(const QString &cosPath);
  QByteArray hmacSha1(const QByteArray &key, const QByteArray &data) const;
  QString calculateSignature(const QString &method, const QString &cosPath,
                             const QString &keyTime, const QString &host) const;

  QString cosBucket_;
  QString cosRegion_;
  QString cosSecretId_;
  QString cosSecretKey_;
  QNetworkAccessManager *networkManager_ = nullptr;
  QNetworkReply *reply_ = nullptr;
  bool aborting_ = false;
};
