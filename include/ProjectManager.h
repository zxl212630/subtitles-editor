#pragma once

#include "SubtitleProject.h"
#include "SubtitleTrack.h"
#include <QObject>
#include <QTimer>

class QUndoStack;

class ProjectManager : public QObject {
  Q_OBJECT

public:
  explicit ProjectManager(SubtitleTrack *track, QObject *parent = nullptr);
  ~ProjectManager();

  // 获取撤销栈
  QUndoStack *undoStack() const { return undoStack_; }

  // 文件操作
  bool newProject();
  bool openProject(const QString &filePath);
  bool saveProject();
  bool saveProjectAs(const QString &filePath);

  // 状态查询
  QString currentFilePath() const;
  QString currentProjectName() const;
  QString videoPath() const;
  bool hasProject() const;
  bool isDirty() const;

  // 视频路径设置
  void setVideoPath(const QString &path);
  void setVideoPathDirect(const QString &path);

  // 脏状态管理
  void setDirty(bool dirty);

  // 自动保存
  void enableAutoSave(bool enable);
  void setAutoSaveInterval(int seconds);

signals:
  void projectChanged(const QString &filePath);
  void dirtyStateChanged(bool dirty);
  void autoSaveTriggered();
  void videoPathChanged(const QString &path);

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

  QUndoStack *undoStack_ = nullptr;
  bool isPerformingUndoRedo_ = false;

  friend class SetVideoPathCommand;
};
