#include "ConfigManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

ConfigManager &ConfigManager::instance() {
  static ConfigManager inst;
  return inst;
}

ConfigManager::ConfigManager()
    : configFilePath_([] {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QDir().mkpath(configDir);
        QString path = configDir + "/config.ini";

        // If config doesn't exist, copy from template
        if (!QFile::exists(path)) {
          QString templatePath = QCoreApplication::applicationDirPath() + "/config.ini.template";
          if (QFile::exists(templatePath)) {
            QFile::copy(templatePath, path);
          }
        }
        return path;
      }()),
      settings_(configFilePath_, QSettings::IniFormat) {}

bool ConfigManager::isValid() const {
  // Check all required keys
  return !getString("ffmpeg", "path").isEmpty() &&
         !getString("tencent_asr", "secret_id").isEmpty() &&
         !getString("tencent_asr", "secret_key").isEmpty() &&
         !getString("tencent_asr", "app_id").isEmpty() &&
         !getString("aliyun_oss", "access_key_id").isEmpty() &&
         !getString("aliyun_oss", "access_key_secret").isEmpty() &&
         !getString("aliyun_oss", "bucket").isEmpty() &&
         !getString("aliyun_oss", "region").isEmpty();
}

QString ConfigManager::configFilePath() const {
  return configFilePath_;
}

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