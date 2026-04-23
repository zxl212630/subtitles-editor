#include "AppWindow.h"
#include "VideoPreviewPanel.h"
#include "SubtitleListPanel.h"
#include "TimelinePanel.h"
#include "SubtitleTrack.h"
#include "SubtitleItem.h"

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QSplitter>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>
#include <QUuid>

struct AppWindow::Private
{
    QWK::WidgetWindowAgent* windowAgent = nullptr;
    QFrame* titleBar = nullptr;
    QLabel* titleLabel = nullptr;

    QSplitter* verticalSplitter = nullptr;
    QSplitter* topSplitter = nullptr;
    VideoPreviewPanel* videoPreviewPanel = nullptr;
    SubtitleListPanel* subtitleListPanel = nullptr;
    TimelinePanel* timelinePanel = nullptr;

    SubtitleTrack* subtitleTrack = nullptr;
};

AppWindow::AppWindow(QWidget* parent)
    : QMainWindow(parent)
    , d(std::make_unique<Private>())
{
    setupUi();
}

AppWindow::~AppWindow() = default;

void AppWindow::setupUi()
{
    setWindowTitle("字幕编辑");
    resize(1440, 900);
    setMinimumSize(960, 600);

    d->windowAgent = new QWK::WidgetWindowAgent(this);
    d->windowAgent->setup(this);

    setupTitleBar();
    setupSplitterLayout();
    setupDummyData();

    setMenuWidget(d->titleBar);
    d->windowAgent->setTitleBar(d->titleBar);
}

void AppWindow::setupTitleBar()
{
    d->titleBar = new QFrame(this);
    d->titleBar->setFixedHeight(36);
    d->titleBar->setObjectName("TitleBar");
    d->titleBar->setStyleSheet(R"(
        QFrame#TitleBar {
            background-color: #262626;
            border: none;
        }
    )");

    auto* layout = new QHBoxLayout(d->titleBar);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(8);
    layout->setAlignment(Qt::AlignVCenter);

    auto* leftSpacer = new QWidget(d->titleBar);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(leftSpacer);

    d->titleLabel = new QLabel("字幕编辑", d->titleBar);
    d->titleLabel->setAlignment(Qt::AlignCenter);
    d->titleLabel->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-family: Inter, sans-serif;
            font-size: 12px;
            font-weight: normal;
            background: transparent;
        }
    )");
    layout->addWidget(d->titleLabel);

    auto* rightSpacer = new QWidget(d->titleBar);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(rightSpacer);
}

void AppWindow::setupSplitterLayout()
{
    // Subtitle track (shared data model)
    d->subtitleTrack = new SubtitleTrack(this);

    // Create panels
    d->videoPreviewPanel = new VideoPreviewPanel(this);
    d->subtitleListPanel = new SubtitleListPanel(this);
    d->timelinePanel = new TimelinePanel(this);

    // Connect panels to data
    d->subtitleListPanel->setTrack(d->subtitleTrack);
    d->timelinePanel->setTrack(d->subtitleTrack);

    // Connect cross-panel signals
    connect(d->subtitleListPanel, &SubtitleListPanel::itemSelected,
            d->timelinePanel, [this](const QString& id) {
                Q_UNUSED(id)
                d->timelinePanel->update();
            });

    connect(d->timelinePanel, &TimelinePanel::timeClicked,
            this, [this](qint64 ms) {
                Q_UNUSED(ms)
                // TODO: update video preview time display
            });

    // Top horizontal splitter
    d->topSplitter = new QSplitter(Qt::Horizontal, this);
    d->topSplitter->addWidget(d->videoPreviewPanel);
    d->topSplitter->addWidget(d->subtitleListPanel);
    d->topSplitter->setStretchFactor(0, 1);
    d->topSplitter->setStretchFactor(1, 0);
    d->topSplitter->setHandleWidth(10);
    d->topSplitter->setStyleSheet("QSplitter::handle { background-color: #0a0a0a; }");
    d->subtitleListPanel->setMinimumWidth(300);
    d->videoPreviewPanel->setMinimumWidth(400);
    d->topSplitter->setSizes({852, 558});

    // Vertical splitter
    d->verticalSplitter = new QSplitter(Qt::Vertical, this);
    d->verticalSplitter->addWidget(d->topSplitter);
    d->verticalSplitter->addWidget(d->timelinePanel);
    d->verticalSplitter->setStretchFactor(0, 1);
    d->verticalSplitter->setStretchFactor(1, 0);
    d->verticalSplitter->setHandleWidth(10);
    d->verticalSplitter->setStyleSheet("QSplitter::handle { background-color: #0a0a0a; }");
    d->timelinePanel->setMinimumHeight(150);
    d->timelinePanel->setMaximumHeight(400);

    // Set central widget
    auto* central = new QWidget(this);
    central->setStyleSheet("background-color: #0a0a0a;");
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(10, 10, 10, 10);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(d->verticalSplitter);
    setCentralWidget(central);
}

void AppWindow::setupDummyData()
{
    auto addItem = [&](const QString& text, qint64 start, qint64 end) {
        SubtitleItem item;
        item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item.text = text;
        item.startMs = start;
        item.endMs = end;
        d->subtitleTrack->addItem(item);
    };

    addItem("Online tool to convert", 1000, 3170);
    addItem("the subtitle file (SRT) to", 5000, 7170);
    addItem("PremierePro-supported XML format", 8000, 11170);
}
