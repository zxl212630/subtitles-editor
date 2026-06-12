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
#include <functional>

class QUndoStack;

struct SpeakerInfo {
  int id = -1;
  QString name;
  QString bgImageFile; // 文件名 (如 "alice.png")，配合 globalBgFolder_ 使用
  bool is9Patch = true;
};

class SubtitleTrack : public QObject {
  Q_OBJECT
  friend class SubtitleListPanel;

public:
  explicit SubtitleTrack(QObject *parent = nullptr);

  // --- 撤销/恢复栈集成接口 ---
  void setUndoStack(QUndoStack *stack);
  QUndoStack *undoStack() const { return undoStack_; }
  QString selectedId() const { return selectedId_; }
  QMap<int, SpeakerInfo> speakersMap() const { return speakers_; }

  void clear();
  void addItem(const SubtitleItem &item);
  void removeItem(const QString &id);
  void updateItem(const QString &id, const SubtitleItem &newItem);
  void updateItems(const QList<SubtitleItem> &newItems);
  void selectItem(const QString &id);
  void setSelectedItems(const QSet<QString> &selectedIds);
  void clearSelection();

  // 批量操作包装接口（支持撤销）
  void executeBatchAction(const QString &commandName,
                          std::function<void()> action);

  const QList<SubtitleItem> &items() const;
  const SubtitleItem *selectedItem() const;
  const SubtitleItem *findItem(const QString &id) const;

  void splitItem(const QString &id, int cursorPosition = -1,
                 const QString &currentText = QString());
  void splitItemAtTime(const QString &id, qint64 splitMs);
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
  int refHeight() const { return 1080; }
  void setRefHeight(int h) { Q_UNUSED(h); }

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

  SubtitleItem defaultStyleItem() const;
  void setDefaultStyleItem(const SubtitleItem &item);

  // 一键应用样式到全部字幕
  void applyStyleToAll(const QString &sourceId);

  // 刷新和默认样式应用方法
  void reloadGlobalSettings();
  void applyDefaultStyle(SubtitleItem &item) const;

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
  // --- 撤销快照状态恢复与 Direct 操作（仅由撤销系统调用） ---
  void setTrackState(const QList<SubtitleItem> &items,
                     const QString &selectedId,
                     const QMap<int, SpeakerInfo> &speakers);

  void clearDirect();
  void addItemDirect(const SubtitleItem &item);
  void removeItemDirect(const QString &id);
  void updateItemDirect(const QString &id, const SubtitleItem &newItem);
  void updateItemsDirect(const QList<SubtitleItem> &newItems);
  void splitItemDirect(const QString &id, int cursorPosition,
                       const QString &currentText);
  void splitItemAtTimeDirect(const QString &id, qint64 splitMs);
  void mergeItemsDirect(const QString &id1, const QString &id2);
  void addGapItemDirect(qint64 startMs, qint64 endMs);
  void setSpeakerInfoDirect(int id, const SpeakerInfo &info);
  void clearSpeakersDirect();
  void applyStyleToAllDirect(const QString &sourceId);
  void sortItems();

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

  // 高级样式默认私有成员
  int defaultFillType_ = 0;
  QString defaultFillColor_ = "#FFFFFF";
  QString defaultFillColor2_ = "#FFFFFF";
  int defaultFillAngle_ = 90;
  QString defaultFillTexturePath_ = "";
  bool defaultFillTextureTile_ = true;
  double defaultTextOpacity_ = 1.0;
  bool defaultStrokeEnabled_ = false;
  int defaultStrokeWidth_ = 1;
  QString defaultStrokeColor_ = "#000000";
  double defaultStrokeOpacity_ = 1.0;
  bool defaultShadowEnabled_ = false;
  int defaultShadowOffsetX_ = 0;
  int defaultShadowOffsetY_ = 0;
  int defaultShadowBlur_ = 0;
  QString defaultShadowColor_ = "#000000";
  double defaultShadowOpacity_ = 1.0;
  int defaultBgType_ = 0;
  QString defaultBgColor_ = "#000000";
  double defaultBgOpacity_ = 1.0;
  int defaultBgRoundness_ = 10;
  int defaultBgPaddingX_ = 0;
  int defaultBgPaddingY_ = 0;
  QString defaultBgImagePath_ = "";
  bool defaultBgImage9Patch_ = true;
  int defaultBgOffsetX_ = 0;
  int defaultBgOffsetY_ = 0;
  bool defaultBubbleEnabled_ = false;
  QString defaultBubbleImagePath_ = "";
  int defaultBubblePaddingLeft_ = 10;
  int defaultBubblePaddingRight_ = 10;
  int defaultBubblePaddingTop_ = 10;
  int defaultBubblePaddingBottom_ = 10;

  int refHeight_ = 1080;

  QUndoStack *undoStack_ = nullptr;
  bool isPerformingUndoRedo_ = false;

  friend class SubtitleTrackCommand;
};
