#include "SubtitleTrack.h"

SubtitleTrack::SubtitleTrack(QObject* parent)
    : QObject(parent)
{
}

void SubtitleTrack::clear()
{
    items_.clear();
    selectedId_.clear();
    emit dataChanged();
}

void SubtitleTrack::addItem(const SubtitleItem& item)
{
    items_.append(item);
    emit itemAdded(item);
    emit dataChanged();
}

void SubtitleTrack::removeItem(const QString& id)
{
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

void SubtitleTrack::updateItem(const QString& id, const SubtitleItem& newItem)
{
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

void SubtitleTrack::selectItem(const QString& id)
{
    bool found = false;
    for (const auto& item : items_) {
        if (item.id == id) {
            found = true;
            break;
        }
    }
    if (!found) return;

    selectedId_ = id;
    emit itemSelected(id);
}

const QList<SubtitleItem>& SubtitleTrack::items() const
{
    return items_;
}

const SubtitleItem* SubtitleTrack::selectedItem() const
{
    for (const auto& item : items_) {
        if (item.id == selectedId_) {
            return &item;
        }
    }
    return nullptr;
}

const SubtitleItem* SubtitleTrack::findItem(const QString& id) const
{
    for (const auto& item : items_) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}
