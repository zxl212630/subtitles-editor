#pragma once

#include <QMainWindow>
#include <memory>

class VideoPreviewPanel;
class SubtitleListPanel;
class TimelinePanel;
class SubtitleTrack;
class QSplitter;

class AppWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AppWindow(QWidget* parent = nullptr);
    ~AppWindow() override;

private:
    void setupUi();
    void setupTitleBar();
    void setupSplitterLayout();
    void setupDummyData();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
