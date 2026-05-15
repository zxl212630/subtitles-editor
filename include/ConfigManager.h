#pragma once

#include <QSettings>
#include <QString>

class ConfigManager {
public:
  static ConfigManager &instance();

  // Check if all required config is present
  bool isValid() const;
  QString configFilePath() const;

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

  QString getString(const QString &group, const QString &key) const;
  void setValue(const QString &group, const QString &key, const QVariant &value);
  void sync();
  QString theme() const;
  QString language() const;

private:
  ConfigManager();
  ~ConfigManager() = default;

  QString configFilePath_;
  mutable QSettings settings_;
};
