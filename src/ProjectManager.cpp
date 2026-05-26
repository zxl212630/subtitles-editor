#include "ProjectManager.h"
#include <QDir>
#include <QFileInfo>

ProjectManager::ProjectManager(SubtitleTrack *track, QObject *parent)
    : QObject(parent), track_(track) {
  connect(&autoSaveTimer_, &QTimer::timeout, this, &ProjectManager::onAutoSave);
}

bool ProjectManager::newProject() {
  if (isDirty()) {
    // TODO: 提示用户保存
  }

  track_->clear();
  project_ = SubtitleProject();
  currentFilePath_.clear();
  hasProject_ = false;

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

  SubtitleProject::addRecentFile(filePath);
  emit projectChanged(filePath);

  return true;
}

bool ProjectManager::saveProject() {
  if (currentFilePath_.isEmpty()) {
    return false;
  }
  return project_.save(currentFilePath_, *track_);
}

bool ProjectManager::saveProjectAs(const QString &filePath) {
  if (!project_.save(filePath, *track_)) {
    return false;
  }

  currentFilePath_ = filePath;
  hasProject_ = true;

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
  project_.setVideoPath(path);
  emit dirtyStateChanged(true);
}

bool ProjectManager::hasProject() const { return hasProject_; }

bool ProjectManager::isDirty() const { return project_.isDirty(); }

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
  if (hasProject_ && !currentFilePath_.isEmpty() && project_.isDirty()) {
    saveProject();
    emit autoSaveTriggered();
  }
}

void ProjectManager::setDirty(bool dirty) {
  project_.setDirty(dirty);
  emit dirtyStateChanged(dirty);
}

void ProjectManager::markDirty() {
  project_.setDirty(true);
  emit dirtyStateChanged(true);
}
