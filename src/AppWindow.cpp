#include "AppWindow.h"
#include "ConfigManager.h"
#include "MediaPlayer.h"
#include "SubtitleItem.h"
#include "SubtitleListPanel.h"
#include "SubtitleTrack.h"
#include "TimelinePanel.h"
#include "VideoPreviewPanel.h"
#include "VideoPropertyDialog.h"
#include "srtparser.h"

#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSplitter>
#include <QTime>
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

AppWindow::~AppWindow() {
  // MediaPlayer is destroyed first (last child). Its destructor calls
  // stop() which emits stateChanged, triggering slots on still-alive
  // children (timelinePanel, videoPreviewPanel). Disconnect all
  // outbound signals so no slot fires during MediaPlayer destruction.
  disconnect(d->mediaPlayer, nullptr, nullptr, nullptr);
}

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
  connect(d->timelinePanel, &TimelinePanel::mediaFileDropped, this,
          [this](const QString &path) {
            d->timelinePanel->setMediaFilePath(path);
            d->mediaPlayer->load(path);
          });

  // 1a. Timeline empty-state import button -> file dialog
  connect(d->timelinePanel, &TimelinePanel::importMediaRequested, this,
          [this]() {
            QString path = QFileDialog::getOpenFileName(
                this, "导入视频", QString(),
                "媒体文件 (*.mp4 *.mkv *.avi *.mov *.srt);;视频文件 (*.mp4 "
                "*.mkv *.avi *.mov);;字幕文件 (*.srt);;所有文件 (*)");
            if (!path.isEmpty()) {
              QString ext = QFileInfo(path).suffix().toLower();
              if (ext == "srt") {
                onSubtitleFileDropped(path);
              } else if (d->mediaPlayer) {
                d->timelinePanel->setMediaFilePath(path);
                d->mediaPlayer->load(path);
              }
            }
          });

  // 2. MediaPlayer -> Timeline: duration
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->timelinePanel,
          &TimelinePanel::setTotalDuration);

  // 2a. MediaPlayer -> Timeline & VideoPreview: video fps for drag throttle
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, this,
          [this](qint64, QSize) {
            d->timelinePanel->setVideoFps(d->mediaPlayer->decoderFps());
            d->videoPreviewPanel->setVideoFps(d->mediaPlayer->decoderFps());
          });

  // 3. MediaPlayer -> VideoPreview: seek display
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->videoPreviewPanel,
          &VideoPreviewPanel::onMediaLoaded);

  // 4. MediaPlayer -> Timeline: time sync
  connect(d->mediaPlayer, &MediaPlayer::timeChanged, d->timelinePanel,
          &TimelinePanel::setCurrentTime);

  // 4a. MediaPlayer -> Timeline: playback state for auto-scroll control
  connect(d->mediaPlayer, &MediaPlayer::stateChanged, d->timelinePanel,
          [this](MediaPlayer::State state) {
            d->timelinePanel->setPlaying(state == MediaPlayer::Playing);
          });

  // 5. Timeline click -> MediaPlayer seek
  connect(d->timelinePanel, &TimelinePanel::timeClicked, d->mediaPlayer,
          &MediaPlayer::seek);

  // 5a. Timeline drag -> MediaPlayer preview seek
  connect(d->timelinePanel, &TimelinePanel::previewSeekRequested,
          d->mediaPlayer, &MediaPlayer::previewSeek);

  // 5b. Timeline drag end -> MediaPlayer commit final position
  connect(d->timelinePanel, &TimelinePanel::dragSeekFinished, d->mediaPlayer,
          &MediaPlayer::stopPreviewDragging);

  // 6. SubtitleList -> MediaPlayer seek
  //    Timeline time is updated via MediaPlayer::timeChanged signal, no
  //    direct connection needed.
  connect(d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested,
          d->mediaPlayer,
          [this](const QString &, qint64 ms) { d->mediaPlayer->seek(ms); });

  // 8. VideoPreview play/pause/stop -> MediaPlayer
  connect(d->videoPreviewPanel, &VideoPreviewPanel::playRequested,
          d->mediaPlayer, &MediaPlayer::play);
  connect(d->videoPreviewPanel, &VideoPreviewPanel::pauseRequested,
          d->mediaPlayer, &MediaPlayer::pause);
  connect(d->videoPreviewPanel, &VideoPreviewPanel::stopRequested,
          d->mediaPlayer, &MediaPlayer::stop);

  // 9. MediaPlayer state -> VideoPreview button sync
  connect(d->mediaPlayer, &MediaPlayer::stateChanged, d->videoPreviewPanel,
          &VideoPreviewPanel::onPlaybackStateChanged);

  // 10. VideoPreview step -> MediaPlayer
  connect(d->videoPreviewPanel, &VideoPreviewPanel::stepForwardRequested,
          d->mediaPlayer, &MediaPlayer::stepForward);
  connect(d->videoPreviewPanel, &VideoPreviewPanel::stepBackwardRequested,
          d->mediaPlayer, &MediaPlayer::stepBackward);

  // 10a. VideoPreview progress bar drag -> MediaPlayer (same as Timeline)
  connect(d->videoPreviewPanel, &VideoPreviewPanel::previewSeekRequested,
          d->mediaPlayer, &MediaPlayer::previewSeek);
  connect(d->videoPreviewPanel, &VideoPreviewPanel::previewSeekFinished,
          d->mediaPlayer, &MediaPlayer::stopPreviewDragging);

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

  // 12. TimelinePanel subtitle drop -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::subtitleFileDropped, this,
          &AppWindow::onSubtitleFileDropped);

  // 13. TimelinePanel video ASR -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::videoAsrRequested, this,
          &AppWindow::onVideoAsrRequested);

  // 14. TimelinePanel video property -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::videoPropertyRequested, this,
          &AppWindow::onVideoPropertyRequested);

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
}

void AppWindow::loadFile(const QString &path) {
  if (d->mediaPlayer) {
    d->mediaPlayer->load(path);
    d->mediaPlayer->play();
  }
}

void AppWindow::onSubtitleFileDropped(const QString &path) {
  if (!d->subtitleTrack)
    return;

  // Confirm overwrite if track not empty
  if (!d->subtitleTrack->items().isEmpty()) {
    int ret = QMessageBox::question(
        this, "确认覆盖",
        "字幕轨道已有内容，继续导入将清空现有字幕，是否继续？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
      return;
    d->subtitleTrack->clear();
  }

  // Parse SRT
  try {
    SrtParser::SubtitleParserFactory parserFactory(path.toStdString());
    SrtParser::SubtitleParser *parser = parserFactory.getParser();
    auto subtitles = parser->getSubtitles();

    qint64 maxEndMs = 0;
    for (auto *sub : subtitles) {
      if (!sub)
        continue;
      SubtitleItem item;
      item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      item.text = QString::fromStdString(sub->getText());
      item.startMs = static_cast<qint64>(sub->getStartTime());
      item.endMs = static_cast<qint64>(sub->getEndTime());
      d->subtitleTrack->addItem(item);
      if (item.endMs > maxEndMs)
        maxEndMs = item.endMs;
    }

    delete parser;

    // Extend timeline duration if subtitles exceed video
    if (maxEndMs > 0) {
      if (d->timelinePanel) {
        d->timelinePanel->setTotalDuration(
            qMax(d->timelinePanel->totalDuration(), maxEndMs));
      }
      if (d->videoPreviewPanel) {
        d->videoPreviewPanel->onMediaLoaded(maxEndMs, QSize());
      }
    }

    // Seek to 0
    if (d->mediaPlayer)
      d->mediaPlayer->seek(0);

  } catch (...) {
    QMessageBox::critical(this, "字幕文件格式错误",
                          "无法解析字幕文件，请检查文件格式。");
  }
}

void AppWindow::onVideoAsrRequested() {
  if (!d->subtitleTrack || !d->timelinePanel)
    return;

  QString videoPath = d->timelinePanel->mediaFilePath();
  if (videoPath.isEmpty())
    return;

  if (!d->subtitleTrack->items().isEmpty()) {
    int ret = QMessageBox::question(
        this, "确认覆盖",
        "字幕轨道已有内容，语音识别将清空现有字幕，是否继续？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
      return;
  }

  d->timelinePanel->startAsrPipeline(videoPath);
}

void AppWindow::onVideoPropertyRequested() {
  if (!d->mediaPlayer || !d->timelinePanel)
    return;

  QString path = d->timelinePanel->mediaFilePath();
  if (path.isEmpty())
    return;

  QFileInfo fi(path);
  QMap<QString, QString> props;
  props.insert("文件路径", path);
  props.insert("文件大小",
               QString("%1 MB").arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 2));

  QSize size = d->mediaPlayer->videoSize();
  if (size.isValid())
    props.insert("分辨率",
                 QString("%1x%2").arg(size.width()).arg(size.height()));

  double fps = d->mediaPlayer->decoderFps();
  if (fps > 0.0)
    props.insert("帧率", QString("%1 fps").arg(fps, 0, 'f', 2));

  qint64 duration = d->mediaPlayer->durationMs();
  if (duration > 0)
    props.insert("时长",
                 QTime::fromMSecsSinceStartOfDay(static_cast<int>(duration))
                     .toString("hh:mm:ss.zzz"));

  QString codec = d->mediaPlayer->videoCodecName();
  if (!codec.isEmpty())
    props.insert("视频编码", codec);

  int sampleRate = d->mediaPlayer->audioSampleRate();
  if (sampleRate > 0)
    props.insert("音频采样率", QString("%1 Hz").arg(sampleRate));

  int channels = d->mediaPlayer->audioChannels();
  if (channels > 0)
    props.insert("音频通道", QString("%1").arg(channels));

  VideoPropertyDialog dialog(props, this);
  dialog.exec();
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
