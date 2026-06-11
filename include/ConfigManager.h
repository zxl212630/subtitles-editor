#pragma once

#include <QKeySequence>
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
  bool speakerDiarization() const;
  void setSpeakerDiarization(bool enabled);
  int sentenceMaxLength() const;
  void setSentenceMaxLength(int length);
  QString engineModelType() const;
  void setEngineModelType(const QString &model);

  // Aliyun OSS
  QString ossAccessKeyId() const;
  QString ossAccessKeySecret() const;
  QString ossBucket() const;
  QString ossRegion() const;

  // Storage Provider
  QString storageProvider() const;

  // Tencent COS
  QString cosSecretId() const;
  QString cosSecretKey() const;
  QString cosBucket() const;
  QString cosRegion() const;

  QString getString(const QString &group, const QString &key) const;
  QString getString(const QString &group, const QString &key,
                    const QString &defaultValue) const;
  int getInt(const QString &group, const QString &key, int defaultValue) const;
  bool getBool(const QString &group, const QString &key,
               bool defaultValue) const;
  double getDouble(const QString &group, const QString &key,
                   double defaultValue) const;
  void setValue(const QString &group, const QString &key,
                const QVariant &value);
  void sync();
  QString theme() const;
  QString primaryColor() const;
  QString language() const;

  qreal volume() const;
  void setVolume(qreal volume);
  bool muted() const;
  void setMuted(bool muted);

  bool snapEnabled() const;
  void setSnapEnabled(bool enabled);

  // Shortcuts
  QKeySequence
  getShortcut(const QString &name,
              const QKeySequence &defaultKey = QKeySequence()) const;
  void setShortcut(const QString &name, const QKeySequence &key);

private:
  ConfigManager();
  ~ConfigManager() = default;

  QString configFilePath_;
  mutable QSettings settings_;
};
