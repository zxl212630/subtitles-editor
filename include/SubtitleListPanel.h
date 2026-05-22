#pragma once

#include <QWidget>

class SubtitleTrack;
class SubtitleListModel;
class SubtitleListDelegate;
class QListView;
class QLineEdit;
class QPushButton;
class QLabel;
class SubtitleActionOverlay;

class SubtitleListPanel : public QWidget {
  Q_OBJECT

public:
  explicit SubtitleListPanel(QWidget *parent = nullptr);

  void setTrack(SubtitleTrack *track);
  void setVideoFps(double fps);
  void setTotalDuration(qint64 ms);

signals:
  void itemSelected(const QString &id);
  void itemDeleteRequested(const QString &id);
  void itemSeekRequested(const QString &id, qint64 startMs);
  void itemDoubleClicked(const QString &id, qint64 startMs);

private:
  void setupUi();
  void retranslateUi();
  void onItemClicked(const QModelIndex &index);
  void onItemDoubleClicked(const QModelIndex &index);
  bool eventFilter(QObject *watched, QEvent *event) override;
  void leaveEvent(QEvent *event) override;

  SubtitleTrack *track_ = nullptr;
  SubtitleListModel *model_ = nullptr;
  SubtitleListDelegate *delegate_ = nullptr;
  QListView *listView_ = nullptr;
  QLineEdit *searchEdit_ = nullptr;
  QPushButton *searchClearBtn_ = nullptr;
  SubtitleActionOverlay *actionOverlay_ = nullptr;

  QPushButton *tabSubtitle_ = nullptr;
  QPushButton *tabPreset_ = nullptr;
  QPushButton *tabCustom_ = nullptr;
  QPushButton *tabAnimation_ = nullptr;
  QLabel *headerTime_ = nullptr;
  QLabel *headerText_ = nullptr;
  QLabel *headerAction_ = nullptr;

  double videoFps_ = 25.0;
  qint64 totalDurationMs_ = 0;
};
