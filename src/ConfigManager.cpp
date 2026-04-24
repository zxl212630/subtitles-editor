#include "ConfigManager.h"
#include <QCoreApplication>

ConfigManager &ConfigManager::instance() {
  static ConfigManager inst;
  return inst;
}

ConfigManager::ConfigManager()
    : settings_("config.ini", QSettings::IniFormat) {}

QString ConfigManager::ffmpegPath() const {
  return getString("ffmpeg", "path");
}

QString ConfigManager::tencentSecretId() const {
  return getString("tencent_asr", "secret_id");
}

QString ConfigManager::tencentSecretKey() const {
  return getString("tencent_asr", "secret_key");
}

QString ConfigManager::tencentAppId() const {
  return getString("tencent_asr", "app_id");
}

QString ConfigManager::ossAccessKeyId() const {
  return getString("aliyun_oss", "access_key_id");
}

QString ConfigManager::ossAccessKeySecret() const {
  return getString("aliyun_oss", "access_key_secret");
}

QString ConfigManager::ossBucket() const {
  return getString("aliyun_oss", "bucket");
}

QString ConfigManager::ossRegion() const {
  return getString("aliyun_oss", "region");
}

QString ConfigManager::getString(const QString &group,
                                 const QString &key) const {
  settings_.beginGroup(group);
  QString value = settings_.value(key).toString();
  settings_.endGroup();
  return value;
}