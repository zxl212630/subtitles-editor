#pragma once

#include "SubtitleProject.h"
#include "SubtitleTrack.h"
#include <QObject>
#include <QTimer>

class ProjectManager : public QObject {
  Q_OBJECT

public:
  explicit ProjectManager(SubtitleTrack *track, QObject *parent = nullptr);

  // 文件操作
  bool newProject();
  bool openProject(const QString &filePath);
  bool saveProject();
  bool saveProjectAs(const QString &filePath);

  // 状态查询
  QString currentFilePath() const;
  QString currentProjectName() const;
  bool hasProject() const;
  bool isDirty() const;

  // 自动保存
  void enableAutoSave(bool enable);
  void setAutoSaveInterval(int seconds);

signals:
  void projectChanged(const QString &filePath);
  void dirtyStateChanged(bool dirty);
  void autoSaveTriggered();

private slots:
  void onAutoSave();

private:
  void updateTitle();
  void markDirty();

  SubtitleTrack *track_;
  SubtitleProject project_;
  QString currentFilePath_;
  bool hasProject_ = false;

  QTimer autoSaveTimer_;
  bool autoSaveEnabled_ = false;
};
