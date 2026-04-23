#include "SubtitleListModel.h"
#include "SubtitleTrack.h"
#include "SubtitleItem.h"

SubtitleListModel::SubtitleListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void SubtitleListModel::setTrack(SubtitleTrack* track)
{
    if (track_) {
        disconnect(track_, &SubtitleTrack::dataChanged, this, &SubtitleListModel::onDataChanged);
    }
    track_ = track;
    if (track_) {
        connect(track_, &SubtitleTrack::dataChanged, this, &SubtitleListModel::onDataChanged);
    }
    rebuildFilteredIndices();
}

void SubtitleListModel::setFilterText(const QString& text)
{
    filterText_ = text;
    rebuildFilteredIndices();
}

int SubtitleListModel::rowCount(const QModelIndex& /*parent*/) const
{
    return filteredIndices_.size();
}

QVariant SubtitleListModel::data(const QModelIndex& index, int role) const
{
    if (!track_ || !index.isValid() || index.row() >= filteredIndices_.size()) {
        return QVariant();
    }

    const int originalIndex = filteredIndices_[index.row()];
    const auto& items = track_->items();
    if (originalIndex >= items.size()) {
        return QVariant();
    }

    const auto& item = items[originalIndex];

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
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> SubtitleListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[TextRole] = "text";
    roles[StartMsRole] = "startMs";
    roles[EndMsRole] = "endMs";
    roles[SelectedRole] = "selected";
    roles[StartTimeRole] = "startTime";
    roles[EndTimeRole] = "endTime";
    return roles;
}

void SubtitleListModel::onDataChanged()
{
    beginResetModel();
    rebuildFilteredIndices();
    endResetModel();
}

void SubtitleListModel::rebuildFilteredIndices()
{
    filteredIndices_.clear();
    if (!track_) return;

    const auto& items = track_->items();
    for (int i = 0; i < items.size(); ++i) {
        if (filterText_.isEmpty() || items[i].text.contains(filterText_, Qt::CaseInsensitive)) {
            filteredIndices_.append(i);
        }
    }
}

QString SubtitleListModel::formatTime(qint64 ms)
{
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
