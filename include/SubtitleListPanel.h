#pragma once

#include <QWidget>

class SubtitleTrack;
class SubtitleListModel;
class SubtitleListDelegate;
class QListView;
class QLineEdit;
class QPushButton;

class SubtitleListPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SubtitleListPanel(QWidget* parent = nullptr);

    void setTrack(SubtitleTrack* track);

signals:
    void itemSelected(const QString& id);
    void itemDeleteRequested(const QString& id);

private:
    void setupUi();
    void onItemClicked(const QModelIndex& index);

    SubtitleTrack* track_ = nullptr;
    SubtitleListModel* model_ = nullptr;
    SubtitleListDelegate* delegate_ = nullptr;
    QListView* listView_ = nullptr;
    QLineEdit* searchEdit_ = nullptr;
};
