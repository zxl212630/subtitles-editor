#pragma once

#include "SubtitleItem.h"
#include <QList>
#include <QObject>
#include <QString>

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

signals:
  void itemAdded(const SubtitleItem &item);
  void itemRemoved(const QString &id);
  void itemUpdated(const QString &id);
  void itemSelected(const QString &id);
  void dataChanged();

private:
  QList<SubtitleItem> items_;
  QString selectedId_;
};
