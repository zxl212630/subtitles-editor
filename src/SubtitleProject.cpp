#include "SubtitleProject.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

SubtitleProject::SubtitleProject()
    : created_(QDateTime::currentDateTime()),
      modified_(QDateTime::currentDateTime()) {}

bool SubtitleProject::load(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  QByteArray data = file.readAll();
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    return false;
  }

  QJsonObject root = doc.object();

  // 元数据
  created_ = QDateTime::fromString(root["created"].toString(), Qt::ISODate);
  modified_ = QDateTime::fromString(root["modified"].toString(), Qt::ISODate);

  // 视频信息
  QJsonObject videoObj = root["video"].toObject();
  videoPath_ = videoObj["path"].toString();
  videoFps_ = videoObj["fps"].toDouble(25.0);

  // 设置
  settings_ = root["settings"].toObject();

  filePath_ = filePath;
  dirty_ = false;
  rootObject_ = root;

  return true;
}

bool SubtitleProject::save(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  QJsonObject root;

  // 元数据
  root["version"] = "1.0";
  root["created"] = created_.toString(Qt::ISODate);
  root["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  // 视频信息
  QJsonObject videoObj;
  videoObj["path"] = videoPath_;
  videoObj["fps"] = videoFps_;
  root["video"] = videoObj;

  // 设置
  root["settings"] = settings_;

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));

  filePath_ = filePath;
  modified_ = QDateTime::currentDateTime();
  dirty_ = false;

  return true;
}

bool SubtitleProject::save(const QString &filePath,
                           const SubtitleTrack &track) {
  QJsonObject root;

  // 元数据
  root["version"] = "1.0";
  root["created"] = created_.toString(Qt::ISODate);
  root["modified"] = QDateTime::currentDateTime().toString(Qt::ISODate);

  // 视频信息
  QJsonObject videoObj;
  videoObj["path"] = videoPath_;
  videoObj["fps"] = videoFps_;
  root["video"] = videoObj;

  // 字幕和说话人数据
  QJsonObject trackData = track.toJsonObject();
  root["subtitles"] = trackData["subtitles"];
  root["speakers"] = trackData["speakers"];

  // 设置
  root["settings"] = settings_;

  // 写入文件
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));

  filePath_ = filePath;
  modified_ = QDateTime::currentDateTime();
  dirty_ = false;

  return true;
}

QString SubtitleProject::videoPath() const { return videoPath_; }

double SubtitleProject::videoFps() const { return videoFps_; }

QJsonObject SubtitleProject::settings() const { return settings_; }

QDateTime SubtitleProject::created() const { return created_; }

QDateTime SubtitleProject::modified() const { return modified_; }

void SubtitleProject::setVideoPath(const QString &path) {
  videoPath_ = path;
  dirty_ = true;
}

void SubtitleProject::setVideoFps(double fps) {
  videoFps_ = fps;
  dirty_ = true;
}

void SubtitleProject::setSettings(const QJsonObject &settings) {
  settings_ = settings;
  dirty_ = true;
}

QString SubtitleProject::resolveVideoPath(const QString &projectDir) const {
  if (QFileInfo::exists(videoPath_)) {
    return videoPath_;
  }

  // 尝试相对路径
  QString relativePath = QDir(projectDir).filePath(videoPath_);
  if (QFileInfo::exists(relativePath)) {
    return relativePath;
  }

  return videoPath_;
}

bool SubtitleProject::isDirty() const { return dirty_; }

void SubtitleProject::setDirty(bool dirty) { dirty_ = dirty; }

QJsonObject SubtitleProject::rootObject() const { return rootObject_; }

QStringList SubtitleProject::recentFiles() {
  QSettings settings;
  return settings.value("recentFiles").toStringList();
}

void SubtitleProject::addRecentFile(const QString &filePath) {
  QSettings settings;
  QStringList files = settings.value("recentFiles").toStringList();
  files.removeAll(filePath);
  files.prepend(filePath);

  while (files.size() > kMaxRecentFiles) {
    files.removeLast();
  }

  settings.setValue("recentFiles", files);
}

void SubtitleProject::clearRecentFiles() {
  QSettings settings;
  settings.remove("recentFiles");
}
