#pragma once

#include "SubtitleItem.h"
#include <QList>
#include <QMap>
#include <QMargins>
#include <QObject>
#include <QString>

struct SpeakerInfo {
  int id = -1;
  QString name;
  QString bgImageFile;   // 文件名 (如 "alice.png")，配合 globalBgFolder_ 使用
  bool is9Patch = true;
};

class SubtitleTrack : public QObject {
  Q_OBJECT

public:
  explicit SubtitleTrack(QObject *parent = nullptr);

  void clear();
  void addItem(const SubtitleItem &item);
  void removeItem(const QString &id);
  void updateItem(const QString &id, const SubtitleItem &newItem);
  void selectItem(const QString &id);

  const QList<SubtitleItem> &items() const;
  const SubtitleItem *selectedItem() const;
  const SubtitleItem *findItem(const QString &id) const;

  void splitItem(const QString &id, int cursorPosition = -1,
                 const QString &currentText = QString());
  void mergeItems(const QString &id1, const QString &id2);
  void addGapItem(qint64 startMs, qint64 endMs);

  // --- 说话人管理接口 ---
  void setSpeakerInfo(int id, const SpeakerInfo &info);
  SpeakerInfo speakerInfo(int id) const;
  QList<SpeakerInfo> allSpeakers() const;
  void clearSpeakers();
  void autoRegisterSpeaker(int speakerId);

  // --- 全局统一设置 ---
  QString globalBgFolder() const;
  void setGlobalBgFolder(const QString &path);
  QMargins unifiedBorderMargins() const;
  void setUnifiedBorderMargins(const QMargins &margins);

  // --- 持久化（仅保存全局统一设置到 config.ini）---
  void loadGlobalSettings();
  void saveGlobalSettings();

signals:
  void itemAdded(const SubtitleItem &item);
  void itemRemoved(const QString &id);
  void itemUpdated(const QString &id);
  void itemSelected(const QString &id);
  void dataChanged();
  void speakersChanged();

private:
  QList<SubtitleItem> items_;
  QString selectedId_;
  QMap<int, SpeakerInfo> speakers_;
  QString globalBgFolder_;
  QMargins unifiedBorderMargins_{15, 15, 15, 15};
};
