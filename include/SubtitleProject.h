#pragma once

#include "SubtitleTrack.h"
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

class SubtitleProject {
public:
  SubtitleProject();

  // 文件操作
  bool load(const QString &filePath);
  bool save(const QString &filePath);
  bool save(const QString &filePath, const SubtitleTrack &track);

  // 获取数据
  QString videoPath() const;
  double videoFps() const;
  QJsonObject settings() const;
  QDateTime created() const;
  QDateTime modified() const;

  // 设置数据
  void setVideoPath(const QString &path);
  void setVideoFps(double fps);
  void setSettings(const QJsonObject &settings);

  // 工具方法
  QString resolveVideoPath(const QString &projectDir) const;
  bool isDirty() const;
  void setDirty(bool dirty);

  // 最近文件
  static QStringList recentFiles();
  static void addRecentFile(const QString &filePath);
  static void clearRecentFiles();

private:
  QString filePath_;
  QString videoPath_;
  double videoFps_ = 25.0;
  QJsonObject settings_;
  QDateTime created_;
  QDateTime modified_;
  bool dirty_ = false;

  static const int kMaxRecentFiles = 10;
};
