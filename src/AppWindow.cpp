#include "AppWindow.h"
#include "ConfigManager.h"
#include "MediaPlayer.h"
#include "SubtitleItem.h"
#include "SubtitleListPanel.h"
#include "SubtitleTrack.h"
#include "TimelinePanel.h"
#include "VideoPreviewPanel.h"

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

struct AppWindow::Private {
  QWK::WidgetWindowAgent *windowAgent = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;

  QSplitter *verticalSplitter = nullptr;
  QSplitter *topSplitter = nullptr;
  VideoPreviewPanel *videoPreviewPanel = nullptr;
  SubtitleListPanel *subtitleListPanel = nullptr;
  TimelinePanel *timelinePanel = nullptr;

  SubtitleTrack *subtitleTrack = nullptr;
  MediaPlayer *mediaPlayer = nullptr;
};

AppWindow::AppWindow(QWidget *parent)
    : QMainWindow(parent), d(std::make_unique<Private>()) {
  setupUi();
  checkConfig();
}

void AppWindow::checkConfig() {
  if (!ConfigManager::instance().isValid()) {
    QString configPath = ConfigManager::instance().configFilePath();
    QMessageBox::warning(
        this, "配置缺失",
        QString("未检测到有效配置文件，部分功能（如语音识别）将无法使用。\n\n"
                "请在以下路径创建或编辑配置文件：\n%1\n\n"
                "确保包含 ffmpeg、腾讯云 ASR 和阿里云 OSS 的必要配置项。")
            .arg(configPath));
  }
}

AppWindow::~AppWindow() = default;

void AppWindow::setupUi() {
  setWindowTitle("字幕编辑");
  resize(1440, 900);
  setMinimumSize(960, 600);

  // Window background (#151515) shows around edges and between panels
  setStyleSheet("QMainWindow { background-color: #151515; }");

  d->windowAgent = new QWK::WidgetWindowAgent(this);
  d->windowAgent->setup(this);

  setupTitleBar();
  setupSplitterLayout();

  setMenuWidget(d->titleBar);
  d->windowAgent->setTitleBar(d->titleBar);
}

void AppWindow::setupTitleBar() {
  d->titleBar = new QFrame(this);
  d->titleBar->setFixedHeight(36);
  d->titleBar->setObjectName("TitleBar");
  d->titleBar->setStyleSheet(R"(
        QFrame#TitleBar {
            background-color: #262626;
            border: none;
        }
    )");

  auto *layout = new QHBoxLayout(d->titleBar);
  layout->setContentsMargins(12, 0, 12, 0);
  layout->setSpacing(8);
  layout->setAlignment(Qt::AlignVCenter);

  auto *leftSpacer = new QWidget(d->titleBar);
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

  auto *rightSpacer = new QWidget(d->titleBar);
  rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(rightSpacer);
}

void AppWindow::setupSplitterLayout() {
  // Subtitle track (shared data model)
  d->subtitleTrack = new SubtitleTrack(this);

  // Create panels
  d->videoPreviewPanel = new VideoPreviewPanel(this);
  d->subtitleListPanel = new SubtitleListPanel(this);
  d->timelinePanel = new TimelinePanel(this);

  // Connect panels to data
  d->subtitleListPanel->setTrack(d->subtitleTrack);
  d->timelinePanel->setTrack(d->subtitleTrack);

  // Create media player
  d->mediaPlayer = new MediaPlayer(this);
  d->mediaPlayer->setVideoRenderer(d->videoPreviewPanel->videoRenderer());
  d->videoPreviewPanel->setMediaPlayer(d->mediaPlayer);
  d->videoPreviewPanel->setSubtitleTrack(d->subtitleTrack);

  // Connect cross-panel signals
  // 1. Timeline -> MediaPlayer: file drop
  connect(d->timelinePanel, &TimelinePanel::mediaFileDropped, d->mediaPlayer,
          &MediaPlayer::load);

  // 2. MediaPlayer -> Timeline: duration
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->timelinePanel,
          &TimelinePanel::setTotalDuration);

  // 3. MediaPlayer -> VideoPreview: seek display
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->videoPreviewPanel,
          &VideoPreviewPanel::onMediaLoaded);

  // 4. MediaPlayer -> Timeline: time sync
  connect(d->mediaPlayer, &MediaPlayer::timeChanged, d->timelinePanel,
          &TimelinePanel::setCurrentTime);

  // 5. Timeline click -> MediaPlayer seek
  connect(d->timelinePanel, &TimelinePanel::timeClicked, d->mediaPlayer,
          &MediaPlayer::seek);

  // 6. SubtitleList -> MediaPlayer seek
  connect(d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested,
          d->mediaPlayer,
          [this](const QString &, qint64 ms) { d->mediaPlayer->seek(ms); });

  // 7. SubtitleList -> Timeline sync
  connect(d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested,
          d->timelinePanel, [this](const QString &, qint64 ms) {
            d->timelinePanel->setCurrentTime(ms);
          });

  // 8. VideoPreview play -> MediaPlayer
  connect(d->videoPreviewPanel, &VideoPreviewPanel::playRequested,
          d->mediaPlayer, &MediaPlayer::play);

  // 9. VideoPreview pause -> MediaPlayer
  connect(d->videoPreviewPanel, &VideoPreviewPanel::pauseRequested,
          d->mediaPlayer, &MediaPlayer::pause);

  // 10. VideoPreview step -> MediaPlayer
  connect(d->videoPreviewPanel, &VideoPreviewPanel::stepForwardRequested,
          d->mediaPlayer, &MediaPlayer::stepForward);
  connect(d->videoPreviewPanel, &VideoPreviewPanel::stepBackwardRequested,
          d->mediaPlayer, &MediaPlayer::stepBackward);

  // 11. SubtitleTrack data change -> VideoPreview subtitle refresh
  connect(d->subtitleTrack, &SubtitleTrack::dataChanged, d->videoPreviewPanel,
          &VideoPreviewPanel::updateSubtitleOverlay);

  // Existing connections
  connect(d->subtitleListPanel, &SubtitleListPanel::itemSelected,
          d->timelinePanel, [this](const QString &id) {
            Q_UNUSED(id)
            d->timelinePanel->update();
          });

  connect(d->timelinePanel, &TimelinePanel::asrFailed, this,
          [](const QString &error) {
            QMessageBox::critical(nullptr, "语音识别失败", error);
          });

  connect(d->timelinePanel, &TimelinePanel::asrSucceeded, this, []() {
    QMessageBox::information(nullptr, "语音识别完成", "字幕已成功生成！");
  });

  // Top horizontal splitter
  d->topSplitter = new QSplitter(Qt::Horizontal, this);
  d->topSplitter->addWidget(d->videoPreviewPanel);
  d->topSplitter->addWidget(d->subtitleListPanel);
  d->topSplitter->setStretchFactor(0, 1);
  d->topSplitter->setStretchFactor(1, 0);
  d->topSplitter->setHandleWidth(10);
  d->topSplitter->setStyleSheet(
      "QSplitter::handle { background-color: #0a0a0a; }");
  d->topSplitter->setCollapsible(0, false);
  d->topSplitter->setCollapsible(1, false);
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
  d->verticalSplitter->setStyleSheet(
      "QSplitter::handle { background-color: #0a0a0a; }");
  d->verticalSplitter->setCollapsible(0, false);
  d->verticalSplitter->setCollapsible(1, false);
  d->timelinePanel->setMinimumHeight(150);
  d->timelinePanel->setMaximumHeight(400);
  d->verticalSplitter->setSizes({680, 220});

  // Set central widget
  auto *central = new QWidget(this);
  central->setStyleSheet("background-color: #0a0a0a;");
  auto *centralLayout = new QVBoxLayout(central);
  centralLayout->setContentsMargins(10, 10, 10, 10);
  centralLayout->setSpacing(0);
  centralLayout->addWidget(d->verticalSplitter);
  setCentralWidget(central);

  setupDummyData();
}

void AppWindow::loadFile(const QString &path) {
  if (d->mediaPlayer) {
    d->mediaPlayer->load(path);
    d->mediaPlayer->play();
  }
}

void AppWindow::setupDummyData() {
  auto addItem = [&](const QString &text, qint64 start, qint64 end) {
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
