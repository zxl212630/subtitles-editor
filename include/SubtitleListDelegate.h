#pragma once

#include <QStyledItemDelegate>
#include <QMouseEvent>

class SubtitleListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit SubtitleListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    void setHoveredIndex(const QModelIndex& index);

signals:
    void deleteClicked(const QString& id);

private:
    static QString formatTime(qint64 ms);
    static QString getIdAtIndex(const QModelIndex& index);
    QRect deleteButtonRect(const QStyleOptionViewItem& option) const;

    QModelIndex hoveredIndex_;
};
