#include "SubtitleTrack.h"
#include "ConfigManager.h"
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>

// 状态快照撤销命令
class SubtitleTrackCommand : public QUndoCommand {
public:
  SubtitleTrackCommand(SubtitleTrack *track, const QString &text,
                       std::function<void()> doAction)
      : track_(track), doAction_(doAction) {
    setText(text);
    oldItems_ = track->items();
    oldSelectedId_ = track->selectedId();
    oldSpeakers_ = track->speakersMap();
  }

  void redo() override {
    if (isFirst_) {
      isFirst_ = false;

      // 临时锁定撤销记录以 O(N) 极高效率执行批量数据修改，避免 O(N^2)
      // 重复深拷贝
      track_->isPerformingUndoRedo_ = true;
      doAction_();
      track_->isPerformingUndoRedo_ = false;

      newItems_ = track_->items();
      newSelectedId_ = track_->selectedId();
      newSpeakers_ = track_->speakersMap();
    } else {
      track_->setTrackState(newItems_, newSelectedId_, newSpeakers_);
    }
  }

  void undo() override {
    track_->setTrackState(oldItems_, oldSelectedId_, oldSpeakers_);
  }

private:
  SubtitleTrack *track_;
  std::function<void()> doAction_;
  bool isFirst_ = true;
  QList<SubtitleItem> oldItems_;
  QString oldSelectedId_;
  QMap<int, SpeakerInfo> oldSpeakers_;
  QList<SubtitleItem> newItems_;
  QString newSelectedId_;
  QMap<int, SpeakerInfo> newSpeakers_;
};

SubtitleTrack::SubtitleTrack(QObject *parent) : QObject(parent) {
  loadGlobalSettings();
}

void SubtitleTrack::setUndoStack(QUndoStack *stack) { undoStack_ = stack; }

void SubtitleTrack::executeBatchAction(const QString &commandName,
                                       std::function<void()> action) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(this, commandName, action));
  } else {
    bool orig = isPerformingUndoRedo_;
    isPerformingUndoRedo_ = true;
    action();
    isPerformingUndoRedo_ = orig;
  }
}

void SubtitleTrack::setTrackState(const QList<SubtitleItem> &items,
                                  const QString &selectedId,
                                  const QMap<int, SpeakerInfo> &speakers) {
  isPerformingUndoRedo_ = true;
  items_ = items;
  selectedId_ = selectedId;
  speakers_ = speakers;

  emit dataChanged();
  emit speakersChanged();
  emit itemSelected(selectedId_);
  isPerformingUndoRedo_ = false;
}

void SubtitleTrack::clear() {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(this, tr("清空字幕"),
                                              [this]() { clearDirect(); }));
  } else {
    clearDirect();
  }
}

void SubtitleTrack::clearDirect() {
  items_.clear();
  selectedId_.clear();
  emit dataChanged();
}

// --- 公共修改接口与命令分发 ---

void SubtitleTrack::addItem(const SubtitleItem &item) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(
        this, tr("添加字幕"), [this, item]() { addItemDirect(item); }));
  } else {
    addItemDirect(item);
  }
}

void SubtitleTrack::removeItem(const QString &id) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(
        this, tr("删除字幕"), [this, id]() { removeItemDirect(id); }));
  } else {
    removeItemDirect(id);
  }
}

void SubtitleTrack::updateItem(const QString &id, const SubtitleItem &newItem) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(
        new SubtitleTrackCommand(this, tr("修改字幕"), [this, id, newItem]() {
          updateItemDirect(id, newItem);
        }));
  } else {
    updateItemDirect(id, newItem);
  }
}

void SubtitleTrack::updateItems(const QList<SubtitleItem> &newItems) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(
        new SubtitleTrackCommand(this, tr("调整字幕时间"), [this, newItems]() {
          updateItemsDirect(newItems);
        }));
  } else {
    updateItemsDirect(newItems);
  }
}

void SubtitleTrack::splitItem(const QString &id, int cursorPosition,
                              const QString &currentText) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(
        this, tr("分割字幕"), [this, id, cursorPosition, currentText]() {
          splitItemDirect(id, cursorPosition, currentText);
        }));
  } else {
    splitItemDirect(id, cursorPosition, currentText);
  }
}

void SubtitleTrack::splitItemAtTime(const QString &id, qint64 splitMs) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(
        this, tr("分割字幕"), [this, id, splitMs]() {
          splitItemAtTimeDirect(id, splitMs);
        }));
  } else {
    splitItemAtTimeDirect(id, splitMs);
  }
}

void SubtitleTrack::mergeItems(const QString &id1, const QString &id2) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(
        new SubtitleTrackCommand(this, tr("合并字幕"), [this, id1, id2]() {
          mergeItemsDirect(id1, id2);
        }));
  } else {
    mergeItemsDirect(id1, id2);
  }
}

void SubtitleTrack::addGapItem(qint64 startMs, qint64 endMs) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(
        this, tr("插入空白字幕"),
        [this, startMs, endMs]() { addGapItemDirect(startMs, endMs); }));
  } else {
    addGapItemDirect(startMs, endMs);
  }
}

void SubtitleTrack::setSpeakerInfo(int id, const SpeakerInfo &info) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(
        new SubtitleTrackCommand(this, tr("修改说话人"), [this, id, info]() {
          setSpeakerInfoDirect(id, info);
        }));
  } else {
    setSpeakerInfoDirect(id, info);
  }
}

void SubtitleTrack::clearSpeakers() {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(
        this, tr("清空说话人"), [this]() { clearSpeakersDirect(); }));
  } else {
    clearSpeakersDirect();
  }
}

void SubtitleTrack::applyStyleToAll(const QString &sourceId) {
  if (undoStack_ && !isPerformingUndoRedo_) {
    undoStack_->push(new SubtitleTrackCommand(
        this, tr("应用样式到全部"),
        [this, sourceId]() { applyStyleToAllDirect(sourceId); }));
  } else {
    applyStyleToAllDirect(sourceId);
  }
}

// --- 底层 Direct 动作实现 ---

void SubtitleTrack::addItemDirect(const SubtitleItem &item) {
  items_.append(item);
  emit itemAdded(item);
  emit dataChanged();
}

void SubtitleTrack::removeItemDirect(const QString &id) {
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].id == id) {
      items_.removeAt(i);
      if (selectedId_ == id) {
        selectedId_.clear();
      }
      emit itemRemoved(id);
      emit dataChanged();
      return;
    }
  }
}

void SubtitleTrack::updateItemDirect(const QString &id,
                                     const SubtitleItem &newItem) {
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].id == id) {
      items_[i] = newItem;
      items_[i].id = id; // preserve id
      emit itemUpdated(id);
      emit dataChanged();
      return;
    }
  }
}

void SubtitleTrack::updateItemsDirect(const QList<SubtitleItem> &newItems) {
  bool changed = false;
  for (const auto &newItem : newItems) {
    for (int i = 0; i < items_.size(); ++i) {
      if (items_[i].id == newItem.id) {
        items_[i] = newItem;
        emit itemUpdated(newItem.id);
        changed = true;
        break;
      }
    }
  }
  if (changed) {
    emit dataChanged();
  }
}

void SubtitleTrack::splitItemDirect(const QString &id, int cursorPosition,
                                    const QString &currentText) {
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].id == id) {
      SubtitleItem original = items_[i];
      if (!currentText.isNull()) {
        original.text = currentText;
      }
      int textLen = original.text.length();

      qint64 splitMs;
      QString text1, text2;

      if (cursorPosition > 0 && cursorPosition < textLen) {
        text1 = original.text.left(cursorPosition);
        text2 = original.text.mid(cursorPosition);
        double ratio = static_cast<double>(cursorPosition) / textLen;
        splitMs =
            original.startMs +
            static_cast<qint64>((original.endMs - original.startMs) * ratio);
      } else {
        int mid = textLen / 2;
        text1 = original.text.left(mid);
        text2 = original.text.mid(mid);
        splitMs = original.startMs + (original.endMs - original.startMs) / 2;
      }

      items_[i].text = text1;
      items_[i].endMs = splitMs;

      SubtitleItem newItem = original;
      newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      newItem.text = text2;
      newItem.startMs = splitMs;
      newItem.endMs = original.endMs;
      newItem.selected = false;

      items_.insert(i + 1, newItem);

      emit itemUpdated(original.id);
      emit itemAdded(newItem);
      emit dataChanged();
      return;
    }
  }
}

void SubtitleTrack::splitItemAtTimeDirect(const QString &id, qint64 splitMs) {
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].id == id) {
      SubtitleItem original = items_[i];
      if (splitMs <= original.startMs || splitMs >= original.endMs)
        return; // Invalid split point

      double ratio = static_cast<double>(splitMs - original.startMs) / (original.endMs - original.startMs);
      int textLen = original.text.length();
      int cursorPosition = qRound(textLen * ratio);

      QString text1 = original.text.left(cursorPosition);
      QString text2 = original.text.mid(cursorPosition);

      items_[i].text = text1;
      items_[i].endMs = splitMs;

      SubtitleItem newItem = original;
      newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      newItem.text = text2;
      newItem.startMs = splitMs;
      newItem.endMs = original.endMs;
      newItem.selected = false;

      items_.insert(i + 1, newItem);

      emit itemUpdated(original.id);
      emit itemAdded(newItem);
      emit dataChanged();
      return;
    }
  }
}

void SubtitleTrack::mergeItemsDirect(const QString &id1, const QString &id2) {
  int idx1 = -1, idx2 = -1;
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].id == id1)
      idx1 = i;
    if (items_[i].id == id2)
      idx2 = i;
  }

  if (idx1 != -1 && idx2 != -1) {
    items_[idx1].endMs = items_[idx2].endMs;
    items_[idx1].text += items_[idx2].text;

    QString removedId = items_[idx2].id;
    items_.removeAt(idx2);

    if (selectedId_ == removedId) {
      selectedId_ = items_[idx1].id;
      emit itemSelected(selectedId_);
    }

    emit itemUpdated(items_[idx1].id);
    emit itemRemoved(removedId);
    emit dataChanged();
  }
}

void SubtitleTrack::addGapItemDirect(qint64 startMs, qint64 endMs) {
  SubtitleItem newItem = defaultStyleItem();
  newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  newItem.text = "";
  newItem.startMs = startMs;
  newItem.endMs = endMs;
  newItem.selected = false;
  newItem.speakerId = -1;

  int insertIdx = items_.size();
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].startMs > startMs) {
      insertIdx = i;
      break;
    }
  }
  items_.insert(insertIdx, newItem);

  emit itemAdded(newItem);
  emit dataChanged();
}

void SubtitleTrack::setSpeakerInfoDirect(int id, const SpeakerInfo &info) {
  speakers_[id] = info;
  speakers_[id].id = id;
  emit speakersChanged();
}

void SubtitleTrack::clearSpeakersDirect() {
  speakers_.clear();
  emit speakersChanged();
}

void SubtitleTrack::applyStyleToAllDirect(const QString &sourceId) {
  const SubtitleItem *source = findItem(sourceId);
  if (!source)
    return;

  bool changed = false;
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].id != sourceId) {
      items_[i].fontFamily = source->fontFamily;
      items_[i].fontSize = source->fontSize;
      items_[i].bold = source->bold;
      items_[i].italic = source->italic;
      items_[i].underline = source->underline;
      items_[i].alignment = source->alignment;
      items_[i].rectX = source->rectX;
      items_[i].rectY = source->rectY;
      items_[i].rectW = source->rectW;
      items_[i].rectH = source->rectH;
      items_[i].rotation = source->rotation;

      items_[i].fillType = source->fillType;
      items_[i].fillColor = source->fillColor;
      items_[i].fillColor2 = source->fillColor2;
      items_[i].fillAngle = source->fillAngle;
      items_[i].fillTexturePath = source->fillTexturePath;
      items_[i].fillTextureTile = source->fillTextureTile;
      items_[i].textOpacity = source->textOpacity;

      items_[i].strokeEnabled = source->strokeEnabled;
      items_[i].strokeWidth = source->strokeWidth;
      items_[i].strokeColor = source->strokeColor;
      items_[i].strokeOpacity = source->strokeOpacity;

      items_[i].shadowEnabled = source->shadowEnabled;
      items_[i].shadowOffsetX = source->shadowOffsetX;
      items_[i].shadowOffsetY = source->shadowOffsetY;
      items_[i].shadowBlur = source->shadowBlur;
      items_[i].shadowColor = source->shadowColor;
      items_[i].shadowOpacity = source->shadowOpacity;

      items_[i].bgType = source->bgType;
      items_[i].bgColor = source->bgColor;
      items_[i].bgOpacity = source->bgOpacity;
      items_[i].bgRoundness = source->bgRoundness;
      items_[i].bgPaddingX = source->bgPaddingX;
      items_[i].bgPaddingY = source->bgPaddingY;
      items_[i].bgImagePath = source->bgImagePath;
      items_[i].bgImage9Patch = source->bgImage9Patch;
      items_[i].bgOffsetX = source->bgOffsetX;
      items_[i].bgOffsetY = source->bgOffsetY;
      items_[i].bubbleEnabled = source->bubbleEnabled;
      items_[i].bubbleImagePath = source->bubbleImagePath;
      items_[i].bubblePaddingLeft = source->bubblePaddingLeft;
      items_[i].bubblePaddingRight = source->bubblePaddingRight;
      items_[i].bubblePaddingTop = source->bubblePaddingTop;
      items_[i].bubblePaddingBottom = source->bubblePaddingBottom;

      emit itemUpdated(items_[i].id);
      changed = true;
    }
  }

  setDefaultStyleItem(*source);

  if (changed) {
    emit dataChanged();
  }
}

// --- 纯查询与辅助方法保持不变 ---

void SubtitleTrack::selectItem(const QString &id) {
  bool found = false;
  for (const auto &item : items_) {
    if (item.id == id) {
      found = true;
      break;
    }
  }
  if (!found)
    return;

  selectedId_ = id;
  for (int i = 0; i < items_.size(); ++i) {
    items_[i].selected = (items_[i].id == id);
  }
  emit itemSelected(id);
  emit dataChanged();
}

void SubtitleTrack::setSelectedItems(const QSet<QString> &selectedIds) {
  bool changed = false;
  for (int i = 0; i < items_.size(); ++i) {
    bool shouldSelect = selectedIds.contains(items_[i].id);
    if (items_[i].selected != shouldSelect) {
      items_[i].selected = shouldSelect;
      changed = true;
    }
  }
  if (changed) {
    if (selectedIds.isEmpty()) {
      selectedId_.clear();
    } else if (!selectedIds.contains(selectedId_)) {
      selectedId_ = *selectedIds.begin();
    }
    emit dataChanged();
  }
}

void SubtitleTrack::clearSelection() {
  selectedId_.clear();
  bool changed = false;
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].selected) {
      items_[i].selected = false;
      changed = true;
    }
  }
  if (changed) {
    emit dataChanged();
  }
}

const QList<SubtitleItem> &SubtitleTrack::items() const { return items_; }

const SubtitleItem *SubtitleTrack::selectedItem() const {
  for (const auto &item : items_) {
    if (item.id == selectedId_) {
      return &item;
    }
  }
  return nullptr;
}

const SubtitleItem *SubtitleTrack::findItem(const QString &id) const {
  for (const auto &item : items_) {
    if (item.id == id) {
      return &item;
    }
  }
  return nullptr;
}

SpeakerInfo SubtitleTrack::speakerInfo(int id) const {
  return speakers_.value(id);
}

QList<SpeakerInfo> SubtitleTrack::allSpeakers() const {
  return speakers_.values();
}

void SubtitleTrack::autoRegisterSpeaker(int speakerId) {
  if (speakerId >= 0 && !speakers_.contains(speakerId)) {
    SpeakerInfo info;
    info.id = speakerId;
    info.name = QString("Speaker %1").arg(speakerId);
    speakers_[speakerId] = info;
    emit speakersChanged();
  }
}

QString SubtitleTrack::globalBgFolder() const { return globalBgFolder_; }

void SubtitleTrack::setGlobalBgFolder(const QString &path) {
  globalBgFolder_ = path;
}

QMargins SubtitleTrack::unifiedBorderMargins() const {
  return unifiedBorderMargins_;
}

void SubtitleTrack::setUnifiedBorderMargins(const QMargins &margins) {
  unifiedBorderMargins_ = margins;
}

void SubtitleTrack::reloadGlobalSettings() { loadGlobalSettings(); }

void SubtitleTrack::applyDefaultStyle(SubtitleItem &item) const {
  SubtitleItem def = defaultStyleItem();

  // Preserve properties that should not be overwritten by style templates
  QString origId = item.id;
  QString origText = item.text;
  qint64 origStart = item.startMs;
  qint64 origEnd = item.endMs;
  bool origSel = item.selected;
  int origSpk = item.speakerId;

  item = def;

  item.id = origId;
  item.text = origText;
  item.startMs = origStart;
  item.endMs = origEnd;
  item.selected = origSel;
  item.speakerId = origSpk;
}

SubtitleItem SubtitleTrack::defaultStyleItem() const {
  SubtitleItem item;
  item.fontFamily = defaultFontFamily_;
  item.fontSize = defaultFontSize_;
  item.bold = defaultBold_;
  item.italic = defaultItalic_;
  item.underline = defaultUnderline_;
  item.alignment = defaultAlignment_;
  item.rectX = defaultSubtitleRect_.x();
  item.rectY = defaultSubtitleRect_.y();
  item.rectW = defaultSubtitleRect_.width();
  item.rectH = defaultSubtitleRect_.height();
  item.rotation = defaultRotation_;

  item.fillType = defaultFillType_;
  item.fillColor = defaultFillColor_;
  item.fillColor2 = defaultFillColor2_;
  item.fillAngle = defaultFillAngle_;
  item.fillTexturePath = defaultFillTexturePath_;
  item.fillTextureTile = defaultFillTextureTile_;
  item.textOpacity = defaultTextOpacity_;

  item.strokeEnabled = defaultStrokeEnabled_;
  item.strokeWidth = defaultStrokeWidth_;
  item.strokeColor = defaultStrokeColor_;
  item.strokeOpacity = defaultStrokeOpacity_;

  item.shadowEnabled = defaultShadowEnabled_;
  item.shadowOffsetX = defaultShadowOffsetX_;
  item.shadowOffsetY = defaultShadowOffsetY_;
  item.shadowBlur = defaultShadowBlur_;
  item.shadowColor = defaultShadowColor_;
  item.shadowOpacity = defaultShadowOpacity_;

  item.bgType = defaultBgType_;
  item.bgColor = defaultBgColor_;
  item.bgOpacity = defaultBgOpacity_;
  item.bgRoundness = defaultBgRoundness_;
  item.bgPaddingX = defaultBgPaddingX_;
  item.bgPaddingY = defaultBgPaddingY_;
  item.bgImagePath = defaultBgImagePath_;
  item.bgImage9Patch = defaultBgImage9Patch_;
  item.bgOffsetX = defaultBgOffsetX_;
  item.bgOffsetY = defaultBgOffsetY_;
  item.bubbleEnabled = defaultBubbleEnabled_;
  item.bubbleImagePath = defaultBubbleImagePath_;
  item.bubblePaddingLeft = defaultBubblePaddingLeft_;
  item.bubblePaddingRight = defaultBubblePaddingRight_;
  item.bubblePaddingTop = defaultBubblePaddingTop_;
  item.bubblePaddingBottom = defaultBubblePaddingBottom_;

  return item;
}

void SubtitleTrack::setDefaultStyleItem(const SubtitleItem &item) {
  defaultFontFamily_ = item.fontFamily;
  defaultFontSize_ = item.fontSize;
  defaultBold_ = item.bold;
  defaultItalic_ = item.italic;
  defaultUnderline_ = item.underline;
  defaultAlignment_ = item.alignment;
  defaultSubtitleRect_ = QRectF(item.rectX, item.rectY, item.rectW, item.rectH);
  defaultRotation_ = item.rotation;

  defaultFillType_ = item.fillType;
  defaultFillColor_ = item.fillColor;
  defaultFillColor2_ = item.fillColor2;
  defaultFillAngle_ = item.fillAngle;
  defaultFillTexturePath_ = item.fillTexturePath;
  defaultFillTextureTile_ = item.fillTextureTile;
  defaultTextOpacity_ = item.textOpacity;

  defaultStrokeEnabled_ = item.strokeEnabled;
  defaultStrokeWidth_ = item.strokeWidth;
  defaultStrokeColor_ = item.strokeColor;
  defaultStrokeOpacity_ = item.strokeOpacity;

  defaultShadowEnabled_ = item.shadowEnabled;
  defaultShadowOffsetX_ = item.shadowOffsetX;
  defaultShadowOffsetY_ = item.shadowOffsetY;
  defaultShadowBlur_ = item.shadowBlur;
  defaultShadowColor_ = item.shadowColor;
  defaultShadowOpacity_ = item.shadowOpacity;

  defaultBgType_ = item.bgType;
  defaultBgColor_ = item.bgColor;
  defaultBgOpacity_ = item.bgOpacity;
  defaultBgRoundness_ = item.bgRoundness;
  defaultBgPaddingX_ = item.bgPaddingX;
  defaultBgPaddingY_ = item.bgPaddingY;
  defaultBgImagePath_ = item.bgImagePath;
  defaultBgImage9Patch_ = item.bgImage9Patch;
  defaultBgOffsetX_ = item.bgOffsetX;
  defaultBgOffsetY_ = item.bgOffsetY;
  defaultBubbleEnabled_ = item.bubbleEnabled;
  defaultBubbleImagePath_ = item.bubbleImagePath;
  defaultBubblePaddingLeft_ = item.bubblePaddingLeft;
  defaultBubblePaddingRight_ = item.bubblePaddingRight;
  defaultBubblePaddingTop_ = item.bubblePaddingTop;
  defaultBubblePaddingBottom_ = item.bubblePaddingBottom;

  saveGlobalSettings();
}

void SubtitleTrack::loadGlobalSettings() {
  auto &cfg = ConfigManager::instance();
  globalBgFolder_ = cfg.getString("speaker", "bgFolder");
  int left = cfg.getInt("speaker", "marginLeft", 15);
  int top = cfg.getInt("speaker", "marginTop", 15);
  int right = cfg.getInt("speaker", "marginRight", 15);
  int bottom = cfg.getInt("speaker", "marginBottom", 15);
  unifiedBorderMargins_ = QMargins(left, top, right, bottom);
}

void SubtitleTrack::saveGlobalSettings() {
  auto &cfg = ConfigManager::instance();
  cfg.setValue("speaker", "bgFolder", globalBgFolder_);
  cfg.setValue("speaker", "marginLeft", unifiedBorderMargins_.left());
  cfg.setValue("speaker", "marginTop", unifiedBorderMargins_.top());
  cfg.setValue("speaker", "marginRight", unifiedBorderMargins_.right());
  cfg.setValue("speaker", "marginBottom", unifiedBorderMargins_.bottom());

  cfg.sync();
}

QJsonObject SubtitleTrack::toJsonObject() const {
  QJsonObject root;

  QJsonArray subtitlesArray;
  for (const auto &item : items_) {
    QJsonObject itemObj;
    itemObj["id"] = item.id;
    itemObj["text"] = item.text;
    itemObj["startMs"] = item.startMs;
    itemObj["endMs"] = item.endMs;
    itemObj["speakerId"] = item.speakerId;

    QJsonObject styleObj;
    styleObj["fontFamily"] = item.fontFamily;
    styleObj["fontSize"] = item.fontSize;
    styleObj["bold"] = item.bold;
    styleObj["italic"] = item.italic;
    styleObj["underline"] = item.underline;
    styleObj["alignment"] = item.alignment;

    // Advanced style attributes
    styleObj["fillType"] = item.fillType;
    styleObj["fillColor"] = item.fillColor;
    styleObj["fillColor2"] = item.fillColor2;
    styleObj["fillAngle"] = item.fillAngle;
    styleObj["fillTexturePath"] = item.fillTexturePath;
    styleObj["fillTextureTile"] = item.fillTextureTile;
    styleObj["textOpacity"] = item.textOpacity;

    styleObj["strokeEnabled"] = item.strokeEnabled;
    styleObj["strokeWidth"] = item.strokeWidth;
    styleObj["strokeColor"] = item.strokeColor;
    styleObj["strokeOpacity"] = item.strokeOpacity;

    styleObj["shadowEnabled"] = item.shadowEnabled;
    styleObj["shadowOffsetX"] = item.shadowOffsetX;
    styleObj["shadowOffsetY"] = item.shadowOffsetY;
    styleObj["shadowBlur"] = item.shadowBlur;
    styleObj["shadowColor"] = item.shadowColor;
    styleObj["shadowOpacity"] = item.shadowOpacity;

    styleObj["bgType"] = item.bgType;
    styleObj["bgColor"] = item.bgColor;
    styleObj["bgOpacity"] = item.bgOpacity;
    styleObj["bgRoundness"] = item.bgRoundness;
    styleObj["bgPaddingX"] = item.bgPaddingX;
    styleObj["bgPaddingY"] = item.bgPaddingY;
    styleObj["bgImagePath"] = item.bgImagePath;
    styleObj["bgImage9Patch"] = item.bgImage9Patch;
    styleObj["bgOffsetX"] = item.bgOffsetX;
    styleObj["bgOffsetY"] = item.bgOffsetY;
    styleObj["bubbleEnabled"] = item.bubbleEnabled;
    styleObj["bubbleImagePath"] = item.bubbleImagePath;
    styleObj["bubblePaddingLeft"] = item.bubblePaddingLeft;
    styleObj["bubblePaddingRight"] = item.bubblePaddingRight;
    styleObj["bubblePaddingTop"] = item.bubblePaddingTop;
    styleObj["bubblePaddingBottom"] = item.bubblePaddingBottom;

    itemObj["style"] = styleObj;

    QJsonObject posObj;
    posObj["x"] = item.rectX;
    posObj["y"] = item.rectY;
    posObj["width"] = item.rectW;
    posObj["height"] = item.rectH;
    posObj["rotation"] = item.rotation;
    itemObj["position"] = posObj;

    subtitlesArray.append(itemObj);
  }
  root["subtitles"] = subtitlesArray;

  QJsonArray speakersArray;
  for (auto it = speakers_.constBegin(); it != speakers_.constEnd(); ++it) {
    const auto &speaker = it.value();
    QJsonObject speakerObj;
    speakerObj["id"] = speaker.id;
    speakerObj["name"] = speaker.name;
    speakerObj["bgImageFile"] = speaker.bgImageFile;
    speakerObj["is9Patch"] = speaker.is9Patch;
    speakersArray.append(speakerObj);
  }
  root["speakers"] = speakersArray;

  return root;
}

void SubtitleTrack::fromJsonObject(const QJsonObject &obj) {
  isPerformingUndoRedo_ = true;

  clear();

  QJsonArray subtitlesArray = obj["subtitles"].toArray();
  for (const auto &itemVal : subtitlesArray) {
    QJsonObject itemObj = itemVal.toObject();
    SubtitleItem item;
    item.id = itemObj["id"].toString();
    item.text = itemObj["text"].toString();
    item.startMs = itemObj["startMs"].toVariant().toLongLong();
    item.endMs = itemObj["endMs"].toVariant().toLongLong();
    item.speakerId = itemObj["speakerId"].toInt(-1);

    QJsonObject styleObj = itemObj["style"].toObject();
    item.fontFamily = styleObj["fontFamily"].toString("Arial");
    item.fontSize = styleObj["fontSize"].toInt(24);
    item.bold = styleObj["bold"].toBool(false);
    item.italic = styleObj["italic"].toBool(false);
    item.underline = styleObj["underline"].toBool(false);
    item.alignment = styleObj["alignment"].toInt(0x84);

    // Advanced style attributes
    item.fillType = styleObj["fillType"].toInt(0);
    item.fillColor = styleObj["fillColor"].toString("#FFFFFF");
    item.fillColor2 = styleObj["fillColor2"].toString("#FFFFFF");
    item.fillAngle = styleObj["fillAngle"].toInt(90);
    item.fillTexturePath = styleObj["fillTexturePath"].toString();
    item.fillTextureTile = styleObj["fillTextureTile"].toBool(true);
    item.textOpacity = styleObj["textOpacity"].toDouble(1.0);

    item.strokeEnabled = styleObj["strokeEnabled"].toBool(false);
    item.strokeWidth = styleObj["strokeWidth"].toInt(2);
    item.strokeColor = styleObj["strokeColor"].toString("#000000");
    item.strokeOpacity = styleObj["strokeOpacity"].toDouble(1.0);

    item.shadowEnabled = styleObj["shadowEnabled"].toBool(false);
    item.shadowOffsetX = styleObj["shadowOffsetX"].toInt(3);
    item.shadowOffsetY = styleObj["shadowOffsetY"].toInt(3);
    item.shadowBlur = styleObj["shadowBlur"].toInt(5);
    item.shadowColor = styleObj["shadowColor"].toString("#000000");
    item.shadowOpacity = styleObj["shadowOpacity"].toDouble(0.5);

    item.bgType = styleObj["bgType"].toInt(0);
    item.bgColor = styleObj["bgColor"].toString("#000000");
    item.bgOpacity = styleObj["bgOpacity"].toDouble(0.6);
    item.bgRoundness = styleObj["bgRoundness"].toInt(4);
    item.bgPaddingX = styleObj["bgPaddingX"].toInt(15);
    item.bgPaddingY = styleObj["bgPaddingY"].toInt(10);
    item.bgImagePath = styleObj["bgImagePath"].toString();
    item.bgImage9Patch = styleObj["bgImage9Patch"].toBool(true);
    item.bgOffsetX = styleObj["bgOffsetX"].toInt(0);
    item.bgOffsetY = styleObj["bgOffsetY"].toInt(0);
    item.bubbleEnabled = styleObj["bubbleEnabled"].toBool(false);
    item.bubbleImagePath = styleObj["bubbleImagePath"].toString();
    item.bubblePaddingLeft = styleObj["bubblePaddingLeft"].toInt(15);
    item.bubblePaddingRight = styleObj["bubblePaddingRight"].toInt(15);
    item.bubblePaddingTop = styleObj["bubblePaddingTop"].toInt(10);
    item.bubblePaddingBottom = styleObj["bubblePaddingBottom"].toInt(10);

    QJsonObject posObj = itemObj["position"].toObject();
    item.rectX = posObj["x"].toDouble(0.1);
    item.rectY = posObj["y"].toDouble(0.75);
    item.rectW = posObj["width"].toDouble(0.8);
    item.rectH = posObj["height"].toDouble(0.2);
    item.rotation = posObj["rotation"].toDouble(0.0);

    addItemDirect(item);
  }

  QJsonArray speakersArray = obj["speakers"].toArray();
  for (const auto &speakerVal : speakersArray) {
    QJsonObject speakerObj = speakerVal.toObject();
    SpeakerInfo speaker;
    speaker.id = speakerObj["id"].toInt(-1);
    speaker.name = speakerObj["name"].toString();
    speaker.bgImageFile = speakerObj["bgImageFile"].toString();
    speaker.is9Patch = speakerObj["is9Patch"].toBool(true);
    setSpeakerInfoDirect(speaker.id, speaker);
  }

  isPerformingUndoRedo_ = false;
}
