#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

class SubtitleTrack;

class SubtitleListModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum Roles {
    IdRole = Qt::UserRole + 1,
    TextRole,
    StartMsRole,
    EndMsRole,
    SelectedRole,
    StartTimeRole,
    EndTimeRole
  };

  explicit SubtitleListModel(QObject *parent = nullptr);

  void setTrack(SubtitleTrack *track);
  void setFilterText(const QString &text);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

private slots:
  void onDataChanged();

private:
  void rebuildFilteredIndices();
  static QString formatTime(qint64 ms);

  SubtitleTrack *track_ = nullptr;
  QString filterText_;
  QList<int> filteredIndices_;
};
