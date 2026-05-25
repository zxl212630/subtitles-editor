#pragma once

#include "SubtitleItem.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMargins>
#include <QObject>
#include <QRectF>
#include <QSet>
#include <QString>

struct SpeakerInfo {
  int id = -1;
  QString name;
  QString bgImageFile; // 文件名 (如 "alice.png")，配合 globalBgFolder_ 使用
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
  void updateItems(const QList<SubtitleItem> &newItems);
  void selectItem(const QString &id);
  void setSelectedItems(const QSet<QString> &selectedIds);
  void clearSelection();

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

  // --- 全局默认样式设置 ---
  QString defaultFontFamily() const { return defaultFontFamily_; }
  void setDefaultFontFamily(const QString &family) {
    defaultFontFamily_ = family;
  }

  int defaultFontSize() const { return defaultFontSize_; }
  void setDefaultFontSize(int size) { defaultFontSize_ = size; }

  bool defaultBold() const { return defaultBold_; }
  void setDefaultBold(bool bold) { defaultBold_ = bold; }

  bool defaultItalic() const { return defaultItalic_; }
  void setDefaultItalic(bool italic) { defaultItalic_ = italic; }

  bool defaultUnderline() const { return defaultUnderline_; }
  void setDefaultUnderline(bool underline) { defaultUnderline_ = underline; }

  int defaultAlignment() const { return defaultAlignment_; }
  void setDefaultAlignment(int alignment) { defaultAlignment_ = alignment; }

  QRectF defaultSubtitleRect() const { return defaultSubtitleRect_; }
  void setDefaultSubtitleRect(const QRectF &rect) {
    defaultSubtitleRect_ = rect;
  }

  double defaultRotation() const { return defaultRotation_; }
  void setDefaultRotation(double rotation) { defaultRotation_ = rotation; }

  // 一键应用样式到全部字幕
  void applyStyleToAll(const QString &sourceId);

  // --- 持久化（仅保存全局统一设置到 config.ini）---
  void loadGlobalSettings();
  void saveGlobalSettings();

  // --- 序列化 ---
  QJsonObject toJsonObject() const;
  void fromJsonObject(const QJsonObject &obj);

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

  // 全局默认样式私有成员
  QString defaultFontFamily_ = "Arial";
  int defaultFontSize_ = 24;
  bool defaultBold_ = false;
  bool defaultItalic_ = false;
  bool defaultUnderline_ = false;
  int defaultAlignment_ = 0x84;
  QRectF defaultSubtitleRect_{0.1, 0.75, 0.8, 0.2};
  double defaultRotation_ = 0.0;
};
