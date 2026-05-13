#pragma once

#include <QMainWindow>
#include <memory>

class VideoPreviewPanel;
class SubtitleListPanel;
class TimelinePanel;
class SubtitleTrack;
class MediaPlayer;
class QSplitter;

class AppWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit AppWindow(QWidget *parent = nullptr);
  ~AppWindow() override;

public slots:
  void loadFile(const QString &path);

private:
  void setupUi();
  void setupTitleBar();
  void setupSplitterLayout();
  void setupDummyData();
  void checkConfig();

  void onSubtitleFileDropped(const QString &path);
  void onVideoAsrRequested();
  void onVideoPropertyRequested();
  void onOpenFileLocationRequested();

private:
  struct Private;
  std::unique_ptr<Private> d;
};
