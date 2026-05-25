#include "SubtitleTrack.h"
#include <QDebug>
#include <QSettings>
#include <QUuid>

SubtitleTrack::SubtitleTrack(QObject *parent) : QObject(parent) {
  loadGlobalSettings();
}

void SubtitleTrack::clear() {
  items_.clear();
  selectedId_.clear();
  emit dataChanged();
}

void SubtitleTrack::addItem(const SubtitleItem &item) {
  items_.append(item);
  emit itemAdded(item);
  emit dataChanged();
}

void SubtitleTrack::removeItem(const QString &id) {
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

void SubtitleTrack::updateItem(const QString &id, const SubtitleItem &newItem) {
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

void SubtitleTrack::updateItems(const QList<SubtitleItem> &newItems) {
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

void SubtitleTrack::splitItem(const QString &id, int cursorPosition,
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
        // Case A: Split at specified position (only if not at boundaries)
        text1 = original.text.left(cursorPosition);
        text2 = original.text.mid(cursorPosition);
        double ratio = static_cast<double>(cursorPosition) / textLen;
        splitMs =
            original.startMs +
            static_cast<qint64>((original.endMs - original.startMs) * ratio);
      } else {
        // Case B: 50/50 split (automatic fallback for boundary or -1)
        int mid = textLen / 2;
        text1 = original.text.left(mid);
        text2 = original.text.mid(mid);
        splitMs = original.startMs + (original.endMs - original.startMs) / 2;
      }

      // Update original
      items_[i].text = text1;
      items_[i].endMs = splitMs;

      // Create new
      SubtitleItem newItem;
      newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      newItem.text = text2;
      newItem.startMs = splitMs;
      newItem.endMs = original.endMs;
      newItem.speakerId = original.speakerId;

      // 继承原字幕样式
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

      items_.insert(i + 1, newItem);

      emit itemUpdated(original.id);
      emit itemAdded(newItem);
      emit dataChanged();
      return;
    }
  }
}

void SubtitleTrack::mergeItems(const QString &id1, const QString &id2) {
  int idx1 = -1, idx2 = -1;
  for (int i = 0; i < items_.size(); ++i) {
    if (items_[i].id == id1)
      idx1 = i;
    if (items_[i].id == id2)
      idx2 = i;
  }

  if (idx1 != -1 && idx2 != -1) {
    // Combine into the first one
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

void SubtitleTrack::addGapItem(qint64 startMs, qint64 endMs) {
  SubtitleItem newItem;
  newItem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  newItem.text = "";
  newItem.startMs = startMs;
  newItem.endMs = endMs;

  // 使用全局默认的模板样式
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

  // Insert maintaining time order
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

// ---- Speaker management ----

void SubtitleTrack::setSpeakerInfo(int id, const SpeakerInfo &info) {
  speakers_[id] = info;
  speakers_[id].id = id;
  emit speakersChanged();
}

SpeakerInfo SubtitleTrack::speakerInfo(int id) const {
  return speakers_.value(id);
}

QList<SpeakerInfo> SubtitleTrack::allSpeakers() const {
  return speakers_.values();
}

void SubtitleTrack::clearSpeakers() {
  speakers_.clear();
  emit speakersChanged();
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

// ---- Global settings ----

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

void SubtitleTrack::applyStyleToAll(const QString &sourceId) {
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
      emit itemUpdated(items_[i].id);
      changed = true;
    }
  }

  // 同步更新全局默认样式配置，使之后创建的字幕项也使用此样式
  defaultFontFamily_ = source->fontFamily;
  defaultFontSize_ = source->fontSize;
  defaultBold_ = source->bold;
  defaultItalic_ = source->italic;
  defaultUnderline_ = source->underline;
  defaultAlignment_ = source->alignment;
  defaultSubtitleRect_ =
      QRectF(source->rectX, source->rectY, source->rectW, source->rectH);

  saveGlobalSettings();

  if (changed) {
    emit dataChanged();
  }
}

void SubtitleTrack::loadGlobalSettings() {
  QSettings settings;
  globalBgFolder_ = settings.value("speaker/bgFolder").toString();
  int left = settings.value("speaker/marginLeft", 15).toInt();
  int top = settings.value("speaker/marginTop", 15).toInt();
  int right = settings.value("speaker/marginRight", 15).toInt();
  int bottom = settings.value("speaker/marginBottom", 15).toInt();
  unifiedBorderMargins_ = QMargins(left, top, right, bottom);

  // 加载全局默认模板样式
  defaultFontFamily_ =
      settings.value("defaultStyle/fontFamily", "Arial").toString();
  defaultFontSize_ = settings.value("defaultStyle/fontSize", 24).toInt();
  defaultBold_ = settings.value("defaultStyle/bold", false).toBool();
  defaultItalic_ = settings.value("defaultStyle/italic", false).toBool();
  defaultUnderline_ = settings.value("defaultStyle/underline", false).toBool();
  defaultAlignment_ = settings.value("defaultStyle/alignment", 0x84).toInt();

  double rx = settings.value("defaultStyle/rectX", 0.1).toDouble();
  double ry = settings.value("defaultStyle/rectY", 0.75).toDouble();
  double rw = settings.value("defaultStyle/rectW", 0.8).toDouble();
  double rh = settings.value("defaultStyle/rectH", 0.2).toDouble();
  defaultSubtitleRect_ = QRectF(rx, ry, rw, rh);
}

void SubtitleTrack::saveGlobalSettings() {
  QSettings settings;
  settings.setValue("speaker/bgFolder", globalBgFolder_);
  settings.setValue("speaker/marginLeft", unifiedBorderMargins_.left());
  settings.setValue("speaker/marginTop", unifiedBorderMargins_.top());
  settings.setValue("speaker/marginRight", unifiedBorderMargins_.right());
  settings.setValue("speaker/marginBottom", unifiedBorderMargins_.bottom());

  // 保存全局默认模板样式
  settings.setValue("defaultStyle/fontFamily", defaultFontFamily_);
  settings.setValue("defaultStyle/fontSize", defaultFontSize_);
  settings.setValue("defaultStyle/bold", defaultBold_);
  settings.setValue("defaultStyle/italic", defaultItalic_);
  settings.setValue("defaultStyle/underline", defaultUnderline_);
  settings.setValue("defaultStyle/alignment", defaultAlignment_);

  settings.setValue("defaultStyle/rectX", defaultSubtitleRect_.x());
  settings.setValue("defaultStyle/rectY", defaultSubtitleRect_.y());
  settings.setValue("defaultStyle/rectW", defaultSubtitleRect_.width());
  settings.setValue("defaultStyle/rectH", defaultSubtitleRect_.height());
}
