#include "SubtitleListModel.h"
#include "SubtitleItem.h"
#include "SubtitleTrack.h"

SubtitleListModel::SubtitleListModel(QObject *parent)
    : QAbstractListModel(parent) {}

void SubtitleListModel::setTrack(SubtitleTrack *track) {
  if (track_) {
    disconnect(track_, &SubtitleTrack::dataChanged, this,
               &SubtitleListModel::onDataChanged);
  }
  track_ = track;
  if (track_) {
    connect(track_, &SubtitleTrack::dataChanged, this,
            &SubtitleListModel::onDataChanged);
  }
  rebuildFilteredIndices();
}

void SubtitleListModel::setFilterText(const QString &text) {
  filterText_ = text;
  beginResetModel();
  rebuildFilteredIndices();
  endResetModel();
}

int SubtitleListModel::rowCount(const QModelIndex & /*parent*/) const {
  return filteredIndices_.size();
}

QVariant SubtitleListModel::data(const QModelIndex &index, int role) const {
  if (!track_ || !index.isValid() || index.row() >= filteredIndices_.size()) {
    return QVariant();
  }

  const int originalIndex = filteredIndices_[index.row()];
  const auto &items = track_->items();
  if (originalIndex >= items.size()) {
    return QVariant();
  }

  const auto &item = items[originalIndex];

  switch (role) {
  case Qt::DisplayRole:
    return item.text;
  case IdRole:
    return item.id;
  case TextRole:
    return item.text;
  case StartMsRole:
    return QVariant::fromValue(item.startMs);
  case EndMsRole:
    return QVariant::fromValue(item.endMs);
  case SelectedRole:
    return item.selected;
  case StartTimeRole:
    return formatTime(item.startMs);
  case EndTimeRole:
    return formatTime(item.endMs);
  case SpeakerIdRole:
    return item.speakerId;
  default:
    return QVariant();
  }
}

Qt::ItemFlags SubtitleListModel::flags(const QModelIndex &index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;
  return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool SubtitleListModel::setData(const QModelIndex &index, const QVariant &value,
                                int role) {
  if (!track_ || !index.isValid() || index.row() >= filteredIndices_.size())
    return false;

  const int originalIndex = filteredIndices_[index.row()];
  const auto &items = track_->items();
  if (originalIndex >= items.size())
    return false;

  const auto &item = items[originalIndex];

  if (role == SpeakerIdRole) {
    SubtitleItem newItem = item;
    newItem.speakerId = value.toInt();
    disconnect(track_, &SubtitleTrack::dataChanged, this,
               &SubtitleListModel::onDataChanged);
    track_->updateItem(item.id, newItem);
    connect(track_, &SubtitleTrack::dataChanged, this,
            &SubtitleListModel::onDataChanged);
    emit dataChanged(index, index, {SpeakerIdRole});
    return true;
  }

  if (role == Qt::EditRole || role == TextRole) {
    SubtitleItem newItem = item;
    newItem.text = value.toString();

    // Temporarily disconnect to avoid full model reset on text edit
    disconnect(track_, &SubtitleTrack::dataChanged, this,
               &SubtitleListModel::onDataChanged);
    track_->updateItem(item.id, newItem);
    connect(track_, &SubtitleTrack::dataChanged, this,
            &SubtitleListModel::onDataChanged);

    emit dataChanged(index, index, {TextRole, Qt::DisplayRole});
    return true;
  }
  return false;
}

QHash<int, QByteArray> SubtitleListModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[IdRole] = "id";
  roles[TextRole] = "text";
  roles[StartMsRole] = "startMs";
  roles[EndMsRole] = "endMs";
  roles[SelectedRole] = "selected";
  roles[StartTimeRole] = "startTime";
  roles[EndTimeRole] = "endTime";
  roles[SpeakerIdRole] = "speakerId";
  return roles;
}

QModelIndex SubtitleListModel::indexForId(const QString &id) const {
  for (int row = 0; row < filteredIndices_.size(); ++row) {
    int originalIndex = filteredIndices_[row];
    if (track_ && originalIndex < track_->items().size()) {
      if (track_->items()[originalIndex].id == id) {
        return index(row);
      }
    }
  }
  return QModelIndex();
}

void SubtitleListModel::onDataChanged() {
  QList<int> oldIndices = filteredIndices_;

  QList<int> newIndices;
  if (track_) {
    const auto &items = track_->items();
    for (int i = 0; i < items.size(); ++i) {
      if (filterText_.isEmpty() ||
          items[i].text.contains(filterText_, Qt::CaseInsensitive)) {
        newIndices.append(i);
      }
    }
  }

  bool structureChanged = (oldIndices.size() != newIndices.size());
  if (!structureChanged && track_) {
    const auto &items = track_->items();
    for (int i = 0; i < oldIndices.size(); ++i) {
      int oldIdx = oldIndices[i];
      int newIdx = newIndices[i];
      if (oldIdx != newIdx || oldIdx >= items.size() ||
          newIdx >= items.size() || items[oldIdx].id != items[newIdx].id) {
        structureChanged = true;
        break;
      }
    }
  }

  if (structureChanged) {
    beginResetModel();
    filteredIndices_ = newIndices;
    endResetModel();
  } else {
    filteredIndices_ = newIndices;
    if (!filteredIndices_.isEmpty()) {
      emit dataChanged(index(0), index(filteredIndices_.size() - 1));
    }
  }
}

void SubtitleListModel::rebuildFilteredIndices() {
  filteredIndices_.clear();
  if (!track_)
    return;

  const auto &items = track_->items();
  for (int i = 0; i < items.size(); ++i) {
    if (filterText_.isEmpty() ||
        items[i].text.contains(filterText_, Qt::CaseInsensitive)) {
      filteredIndices_.append(i);
    }
  }
}

QString SubtitleListModel::formatTime(qint64 ms) {
  const int hours = ms / 3600000;
  const int minutes = (ms % 3600000) / 60000;
  const int seconds = (ms % 60000) / 1000;
  const int frames = (ms % 1000) / 10; // hundredths of a second
  return QString("%1:%2:%3:%4")
      .arg(hours, 2, 10, QChar('0'))
      .arg(minutes, 2, 10, QChar('0'))
      .arg(seconds, 2, 10, QChar('0'))
      .arg(frames, 2, 10, QChar('0'));
}
