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

      SubtitleItem newItem;
      newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      newItem.text = text2;
      newItem.startMs = splitMs;
      newItem.endMs = original.endMs;
      newItem.speakerId = original.speakerId;

      newItem.fontFamily = original.fontFamily;
      newItem.fontSize = original.fontSize;
      newItem.bold = original.bold;
      newItem.italic = original.italic;
      newItem.underline = original.underline;
      newItem.alignment = original.alignment;
      newItem.rectX = original.rectX;
      newItem.rectY = original.rectY;
      newItem.rectW = original.rectW;
      newItem.rectH = original.rectH;
      newItem.rotation = original.rotation;

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
  SubtitleItem newItem;
  newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  newItem.text = "";
  newItem.startMs = startMs;
  newItem.endMs = endMs;

  newItem.fontFamily = defaultFontFamily_;
  newItem.fontSize = defaultFontSize_;
  newItem.bold = defaultBold_;
  newItem.italic = defaultItalic_;
  newItem.underline = defaultUnderline_;
  newItem.alignment = defaultAlignment_;
  newItem.rectX = defaultSubtitleRect_.x();
  newItem.rectY = defaultSubtitleRect_.y();
  newItem.rectW = defaultSubtitleRect_.width();
  newItem.rectH = defaultSubtitleRect_.height();
  newItem.rotation = defaultRotation_;

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
      emit itemUpdated(items_[i].id);
      changed = true;
    }
  }

  defaultFontFamily_ = source->fontFamily;
  defaultFontSize_ = source->fontSize;
  defaultBold_ = source->bold;
  defaultItalic_ = source->italic;
  defaultUnderline_ = source->underline;
  defaultAlignment_ = source->alignment;
  defaultSubtitleRect_ =
      QRectF(source->rectX, source->rectY, source->rectW, source->rectH);
  defaultRotation_ = source->rotation;

  saveGlobalSettings();

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
}

void SubtitleTrack::loadGlobalSettings() {
  auto &cfg = ConfigManager::instance();
  globalBgFolder_ = cfg.getString("speaker", "bgFolder");
  int left = cfg.getInt("speaker", "marginLeft", 15);
  int top = cfg.getInt("speaker", "marginTop", 15);
  int right = cfg.getInt("speaker", "marginRight", 15);
  int bottom = cfg.getInt("speaker", "marginBottom", 15);
  unifiedBorderMargins_ = QMargins(left, top, right, bottom);

  defaultFontFamily_ = cfg.getString("subtitle", "fontFamily", "Arial");
  defaultFontSize_ = cfg.getInt("subtitle", "fontSize", 24);
  defaultBold_ = cfg.getBool("subtitle", "bold", false);
  defaultItalic_ = cfg.getBool("subtitle", "italic", false);
  defaultUnderline_ = cfg.getBool("subtitle", "underline", false);
  defaultAlignment_ = cfg.getInt("subtitle", "alignment", 0x84);

  double rx = cfg.getDouble("subtitle", "rectX", 0.1);
  double ry = cfg.getDouble("subtitle", "rectY", 0.75);
  double rw = cfg.getDouble("subtitle", "rectW", 0.8);
  double rh = cfg.getDouble("subtitle", "rectH", 0.2);
  defaultSubtitleRect_ = QRectF(rx, ry, rw, rh);
  defaultRotation_ = cfg.getDouble("subtitle", "rotation", 0.0);
}

void SubtitleTrack::saveGlobalSettings() {
  auto &cfg = ConfigManager::instance();
  cfg.setValue("speaker", "bgFolder", globalBgFolder_);
  cfg.setValue("speaker", "marginLeft", unifiedBorderMargins_.left());
  cfg.setValue("speaker", "marginTop", unifiedBorderMargins_.top());
  cfg.setValue("speaker", "marginRight", unifiedBorderMargins_.right());
  cfg.setValue("speaker", "marginBottom", unifiedBorderMargins_.bottom());

  cfg.setValue("subtitle", "fontFamily", defaultFontFamily_);
  cfg.setValue("subtitle", "fontSize", defaultFontSize_);
  cfg.setValue("subtitle", "bold", defaultBold_);
  cfg.setValue("subtitle", "italic", defaultItalic_);
  cfg.setValue("subtitle", "underline", defaultUnderline_);
  cfg.setValue("subtitle", "alignment", defaultAlignment_);

  cfg.setValue("subtitle", "rectX", defaultSubtitleRect_.x());
  cfg.setValue("subtitle", "rectY", defaultSubtitleRect_.y());
  cfg.setValue("subtitle", "rectW", defaultSubtitleRect_.width());
  cfg.setValue("subtitle", "rectH", defaultSubtitleRect_.height());
  cfg.setValue("subtitle", "rotation", defaultRotation_);
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
