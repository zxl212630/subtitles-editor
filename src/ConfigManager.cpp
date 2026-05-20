#include "ConfigManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

ConfigManager &ConfigManager::instance() {
  static ConfigManager inst;
  return inst;
}

ConfigManager::ConfigManager()
    : configFilePath_([] {
        QString configDir =
            QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        QString standardPath = configDir + "/config.ini";
        QString localPath =
            QCoreApplication::applicationDirPath() + "/config.ini";

        QString finalPath = standardPath;

        if (!QFile::exists(standardPath)) {
          if (QFile::exists(localPath)) {
            finalPath = localPath;
          } else {
            QDir().mkpath(configDir);
            QString templatePath =
                QCoreApplication::applicationDirPath() + "/config.ini.template";
            if (QFile::exists(templatePath)) {
              QFile::copy(templatePath, standardPath);
            }
          }
        }
        qDebug() << "[ConfigManager] Selected config path:" << finalPath;
        return finalPath;
      }()),
      settings_(configFilePath_, QSettings::IniFormat) {}

bool ConfigManager::isValid() const {
  auto check = [this](const QString &group, const QString &key) {
    QString val = getString(group, key);
    return !val.isEmpty() && !val.contains("YOUR_") &&
           !val.contains("FFMPEG_PATH");
  };

  bool valid = check("ffmpeg", "path") && check("tencent_asr", "secret_id") &&
               check("tencent_asr", "secret_key") &&
               check("tencent_asr", "app_id") &&
               check("aliyun_oss", "access_key_id") &&
               check("aliyun_oss", "access_key_secret") &&
               check("aliyun_oss", "bucket") && check("aliyun_oss", "region");

  qDebug() << "[ConfigManager] Configuration is valid:" << valid;
  return valid;
}

QString ConfigManager::configFilePath() const { return configFilePath_; }

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
  QString fullKey = group + "/" + key;
  QString value = settings_.value(fullKey).toString().trimmed();

  if (value.startsWith('\"') && value.endsWith('\"')) {
    value = value.mid(1, value.length() - 2);
  }
  return value;
}

void ConfigManager::setValue(const QString &group, const QString &key,
                             const QVariant &value) {
  QString fullKey = group + "/" + key;
  settings_.setValue(fullKey, value);
}

void ConfigManager::sync() { settings_.sync(); }

QString ConfigManager::theme() const { return getString("", "theme"); }

QString ConfigManager::primaryColor() const {
  return getString("", "primary_color");
}

QString ConfigManager::language() const { return getString("", "language"); }
