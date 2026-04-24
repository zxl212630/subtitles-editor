#pragma once

#include <QSettings>
#include <QString>

class ConfigManager {
public:
  static ConfigManager &instance();

  // FFmpeg
  QString ffmpegPath() const;

  // Tencent ASR
  QString tencentSecretId() const;
  QString tencentSecretKey() const;
  QString tencentAppId() const;

  // Aliyun OSS
  QString ossAccessKeyId() const;
  QString ossAccessKeySecret() const;
  QString ossBucket() const;
  QString ossRegion() const;

private:
  ConfigManager();
  ~ConfigManager() = default;

  QString getString(const QString &group, const QString &key) const;

  mutable QSettings settings_;
};