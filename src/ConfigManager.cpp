#include "ConfigManager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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

  bool storageValid = false;
  QString provider = storageProvider();
  if (provider == "tencent_cos") {
    storageValid = check("tencent_cos", "secret_id") &&
                   check("tencent_cos", "secret_key") &&
                   check("tencent_cos", "bucket") &&
                   check("tencent_cos", "region");
  } else {
    // Default is aliyun_oss
    storageValid = check("aliyun_oss", "access_key_id") &&
                   check("aliyun_oss", "access_key_secret") &&
                   check("aliyun_oss", "bucket") &&
                   check("aliyun_oss", "region");
  }

  bool valid = check("tencent_asr", "secret_id") &&
               check("tencent_asr", "secret_key") &&
               check("tencent_asr", "app_id") && storageValid;

  qDebug() << "[ConfigManager] Configuration is valid:" << valid;
  return valid;
}

QString ConfigManager::configFilePath() const { return configFilePath_; }

QString ConfigManager::ffmpegPath() const {
  // First, try bundled executable in app bundle
  QString appDir = QCoreApplication::applicationDirPath();
  QString bundledPath = appDir + "/ffmpeg";
#ifdef Q_OS_WIN
  bundledPath += ".exe";
#endif
  if (QFileInfo::exists(bundledPath)) {
    return bundledPath;
  }

  // Fall back to config value
  QString configPath = getString("ffmpeg", "path");
  if (!configPath.isEmpty() && !configPath.contains("FFMPEG_PATH")) {
    return configPath;
  }

  // Finally, try system path
  return "ffmpeg";
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

bool ConfigManager::speakerDiarization() const {
  QString val = getString("tencent_asr", "speaker_diarization");
  return val == "true";
}

void ConfigManager::setSpeakerDiarization(bool enabled) {
  setValue("tencent_asr", "speaker_diarization", enabled ? "true" : "false");
}

int ConfigManager::sentenceMaxLength() const {
  QString val = getString("tencent_asr", "sentence_max_length");
  if (val.isEmpty()) {
    return 16;
  }
  bool ok;
  int len = val.toInt(&ok);
  if (!ok || len < 6 || len > 40) {
    return 16;
  }
  return len;
}

void ConfigManager::setSentenceMaxLength(int length) {
  setValue("tencent_asr", "sentence_max_length", QString::number(length));
}

QString ConfigManager::engineModelType() const {
  QString val = getString("tencent_asr", "engine_model_type");
  if (val.isEmpty()) {
    return "16k_zh";
  }
  return val;
}

void ConfigManager::setEngineModelType(const QString &model) {
  setValue("tencent_asr", "engine_model_type", model);
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

QString ConfigManager::storageProvider() const {
  return getString("storage", "provider");
}

QString ConfigManager::cosSecretId() const {
  return getString("tencent_cos", "secret_id");
}

QString ConfigManager::cosSecretKey() const {
  return getString("tencent_cos", "secret_key");
}

QString ConfigManager::cosBucket() const {
  return getString("tencent_cos", "bucket");
}

QString ConfigManager::cosRegion() const {
  return getString("tencent_cos", "region");
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

QString ConfigManager::getString(const QString &group, const QString &key,
                                 const QString &defaultValue) const {
  QString val = getString(group, key);
  return val.isEmpty() ? defaultValue : val;
}

int ConfigManager::getInt(const QString &group, const QString &key,
                          int defaultValue) const {
  QString val = getString(group, key);
  if (val.isEmpty())
    return defaultValue;
  bool ok;
  int result = val.toInt(&ok);
  return ok ? result : defaultValue;
}

bool ConfigManager::getBool(const QString &group, const QString &key,
                            bool defaultValue) const {
  QString val = getString(group, key);
  if (val.isEmpty())
    return defaultValue;
  return val == "true" || val == "1";
}

double ConfigManager::getDouble(const QString &group, const QString &key,
                                double defaultValue) const {
  QString val = getString(group, key);
  if (val.isEmpty())
    return defaultValue;
  bool ok;
  double result = val.toDouble(&ok);
  return ok ? result : defaultValue;
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

qreal ConfigManager::volume() const {
  QString val = getString("audio", "volume");
  if (val.isEmpty()) {
    return 1.0;
  }
  bool ok;
  qreal v = val.toDouble(&ok);
  if (!ok || v < 0.0 || v > 1.0) {
    return 1.0;
  }
  return v;
}

void ConfigManager::setVolume(qreal volume) {
  setValue("audio", "volume", QString::number(volume, 'f', 2));
  sync();
}

bool ConfigManager::muted() const {
  QString val = getString("audio", "muted");
  return val == "true";
}

void ConfigManager::setMuted(bool muted) {
  setValue("audio", "muted", muted ? "true" : "false");
  sync();
}
