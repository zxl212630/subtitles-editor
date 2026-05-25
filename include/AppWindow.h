#pragma once

#include <QMainWindow>
#include <memory>

class VideoPreviewPanel;
class SubtitleListPanel;
class TimelinePanel;
class SubtitleTrack;
class MediaPlayer;
class QSplitter;
class QMenuBar;
class QMenu;
class ProjectManager;

class AppWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit AppWindow(QWidget *parent = nullptr);
  ~AppWindow() override;

public slots:
  void loadFile(const QString &path);

signals:
  void windowClicked(QPoint globalPos);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void setupUi();
  void setupTitleBar();
  void setupSplitterLayout();
  void setupDummyData();
  void checkConfig();
  void retranslateUi();
  void setupMenuBar();

  void onSubtitleFileDropped(const QString &path);
  void onVideoAsrRequested();
  void onVideoPropertyRequested();
  void onOpenFileLocationRequested();
  void onSettingsRequested();
  void onExportRequested();

private slots:
  void onNewProject();
  void onOpenProject();
  void onSaveProject();
  void onSaveProjectAs();
  void onOpenRecentFile(const QString &filePath);
  void onClearRecentFiles();
  void onExportSrt();
  void onExportTxt();
  void onSelectAll();
  void onDeleteSelected();
  void onConfigApplied();
  void onAbout();

private:
  struct Private;
  std::unique_ptr<Private> d;
};
