#pragma once

#include <QMouseEvent>
#include <QStyledItemDelegate>

class SubtitleListDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  explicit SubtitleListDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
  bool editorEvent(QEvent *event, QAbstractItemModel *model,
                   const QStyleOptionViewItem &option,
                   const QModelIndex &index) override;

  void setHoveredIndex(const QModelIndex &index, int button = 0);
  QRect splitButtonRect(const QStyleOptionViewItem &option) const;
  QRect deleteButtonRect(const QStyleOptionViewItem &option) const;

signals:
  void deleteClicked(const QString &id);

private:
  static QString formatTime(qint64 ms);
  static QString getIdAtIndex(const QModelIndex &index);

  QModelIndex hoveredIndex_;
  int hoveredButton_ = 0; // 0=none, 1=split, 2=delete
};
