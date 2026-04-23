#pragma once

#include <QStyledItemDelegate>

class SubtitleListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit SubtitleListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    static QString formatTime(qint64 ms);
};
