#include "ProjectManager.h"
#include <QDir>
#include <QFileInfo>
#include <QUndoCommand>
#include <QUndoStack>

// 视频路径修改命令实现
class SetVideoPathCommand : public QUndoCommand {
public:
  SetVideoPathCommand(ProjectManager *pm, const QString &newPath)
      : pm_(pm), newPath_(newPath) {
    oldPath_ = pm_->videoPath();
    setText(QObject::tr("更换视频"));
  }

  void redo() override {
    pm_->isPerformingUndoRedo_ = true;
    pm_->setVideoPathDirect(newPath_);
    pm_->isPerformingUndoRedo_ = false;
  }

  void undo() override {
    pm_->isPerformingUndoRedo_ = true;
    pm_->setVideoPathDirect(oldPath_);
    pm_->isPerformingUndoRedo_ = false;
  }

private:
  ProjectManager *pm_;
  QString oldPath_;
  QString newPath_;
};

ProjectManager::ProjectManager(SubtitleTrack *track, QObject *parent)
    : QObject(parent), track_(track) {
  connect(&autoSaveTimer_, &QTimer::timeout, this, &ProjectManager::onAutoSave);

  // 初始化撤销栈并设置最大深度限制
  undoStack_ = new QUndoStack(this);
  undoStack_->setUndoLimit(100);

  // 绑定撤销栈状态与脏状态信号
  connect(undoStack_, &QUndoStack::cleanChanged, this,
          [this](bool isClean) { emit dirtyStateChanged(!isClean); });
}

ProjectManager::~ProjectManager() {
  if (undoStack_) {
    undoStack_->clear();
    delete undoStack_;
    undoStack_ = nullptr;
  }
}

bool ProjectManager::newProject() {
  if (isDirty()) {
    // TODO: 提示用户保存
  }

  track_->clear();
  project_ = SubtitleProject();
  currentFilePath_.clear();
  hasProject_ = false;

  if (undoStack_) {
    undoStack_->clear();
  }

  emit projectChanged(QString());
  return true;
}

bool ProjectManager::openProject(const QString &filePath) {
  if (!project_.load(filePath)) {
    return false;
  }

  // 从已解析的项目数据中加载字幕
  QJsonObject root = project_.rootObject();
  if (!root.isEmpty()) {
    track_->fromJsonObject(root);
  }

  currentFilePath_ = filePath;
  hasProject_ = true;

  if (undoStack_) {
    undoStack_->clear();
  }

  SubtitleProject::addRecentFile(filePath);
  emit projectChanged(filePath);

  return true;
}

bool ProjectManager::saveProject() {
  if (currentFilePath_.isEmpty()) {
    return false;
  }
  bool success = project_.save(currentFilePath_, *track_);
  if (success && undoStack_) {
    undoStack_->setClean();
  }
  return success;
}

bool ProjectManager::saveProjectAs(const QString &filePath) {
  bool success = project_.save(filePath, *track_);
  if (!success) {
    return false;
  }

  currentFilePath_ = filePath;
  hasProject_ = true;

  if (undoStack_) {
    undoStack_->setClean();
  }

  SubtitleProject::addRecentFile(filePath);
  emit projectChanged(filePath);

  return true;
}

QString ProjectManager::currentFilePath() const { return currentFilePath_; }

QString ProjectManager::currentProjectName() const {
  if (currentFilePath_.isEmpty()) {
    return QString();
  }
  return QFileInfo(currentFilePath_).completeBaseName();
}

QString ProjectManager::videoPath() const { return project_.videoPath(); }

void ProjectManager::setVideoPath(const QString &path) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SetVideoPathCommand(this, path));
  } else {
    setVideoPathDirect(path);
  }
}

void ProjectManager::setVideoPathDirect(const QString &path) {
  project_.setVideoPath(path);
  emit videoPathChanged(path);
  emit dirtyStateChanged(isDirty());
}

bool ProjectManager::hasProject() const { return hasProject_; }

bool ProjectManager::isDirty() const {
  if (undoStack_) {
    return !undoStack_->isClean();
  }
  return project_.isDirty();
}

void ProjectManager::enableAutoSave(bool enable) {
  autoSaveEnabled_ = enable;
  if (enable) {
    autoSaveTimer_.start();
  } else {
    autoSaveTimer_.stop();
  }
}

void ProjectManager::setAutoSaveInterval(int seconds) {
  autoSaveTimer_.setInterval(seconds * 1000);
}

void ProjectManager::onAutoSave() {
  if (hasProject_ && !currentFilePath_.isEmpty() && isDirty()) {
    saveProject();
    emit autoSaveTriggered();
  }
}

void ProjectManager::setDirty(bool dirty) {
  if (!dirty) {
    if (undoStack_) {
      undoStack_->setClean();
    }
    project_.setDirty(false);
  } else {
    project_.setDirty(true);
  }
  emit dirtyStateChanged(dirty);
}

void ProjectManager::markDirty() { setDirty(true); }
