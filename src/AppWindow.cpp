#include "AppWindow.h"
#include "AppMessageBox.h"
#include "AsrConfigDialog.h"
#include "ConfigDialog.h"
#include "ConfigManager.h"
#include "MediaPlayer.h"
#include "ProjectManager.h"
#include "SubtitleExporter.h"
#include "SubtitleItem.h"
#include "SubtitleListPanel.h"
#include "SubtitleProject.h"
#include "SubtitleTrack.h"
#include "TimelinePanel.h"
#include "TranslationManager.h"
#include "VideoPreviewPanel.h"
#include "VideoPropertyDialog.h"
#include "srtparser.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTime>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWindowKit/QWKWidgets/widgetwindowagent.h>

struct AppWindow::Private {
  QWK::WidgetWindowAgent *windowAgent = nullptr;
  QFrame *titleBar = nullptr;
  QLabel *titleLabel = nullptr;
  QPushButton *exportBtn = nullptr;
  QPushButton *settingsBtn = nullptr;

  QSplitter *verticalSplitter = nullptr;
  QSplitter *topSplitter = nullptr;
  VideoPreviewPanel *videoPreviewPanel = nullptr;
  SubtitleListPanel *subtitleListPanel = nullptr;
  TimelinePanel *timelinePanel = nullptr;

  SubtitleTrack *subtitleTrack = nullptr;
  MediaPlayer *mediaPlayer = nullptr;

  QDateTime videoImportTime_;

  // 菜单栏
  QMenuBar *menuBar = nullptr;
  QMenu *fileMenu = nullptr;
  QMenu *editMenu = nullptr;
  QMenu *settingsMenu = nullptr;
  QMenu *helpMenu = nullptr;
  QMenu *recentFilesMenu = nullptr;

  // ProjectManager
  ProjectManager *projectManager = nullptr;
};

AppWindow::AppWindow(QWidget *parent)
    : QMainWindow(parent), d(std::make_unique<Private>()) {
  setupUi();
  checkConfig();

  connect(&TranslationManager::instance(), &TranslationManager::languageChanged,
          this, [this]() { retranslateUi(); });
}

void AppWindow::checkConfig() {
  if (!ConfigManager::instance().isValid()) {
    QString configPath = ConfigManager::instance().configFilePath();
    AppMessageBox::warning(
        this, tr("配置缺失"),
        QString(
            tr("未检测到有效配置文件，部分功能（如语音识别）将无法使用。\n\n"
               "请在以下路径创建或编辑配置文件：\n%1\n\n"
               "确保包含 ffmpeg、腾讯云 ASR 和阿里云 OSS 的必要配置项。"))
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
  setWindowTitle(tr("字幕编辑"));
  resize(1440, 900);
  setMinimumSize(960, 600);

  d->windowAgent = new QWK::WidgetWindowAgent(this);
  d->windowAgent->setup(this);

  setupTitleBar();
  setupSplitterLayout();

  setMenuWidget(d->titleBar);
  d->windowAgent->setTitleBar(d->titleBar);
  qApp->installEventFilter(this);
}

bool AppWindow::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    auto *me = static_cast<QMouseEvent *>(event);
    emit windowClicked(me->globalPosition().toPoint());
  }
  if (event->type() == QEvent::ContextMenu) {
    if (qobject_cast<QScrollBar *>(obj)) {
      return true; // Block scroll bar's context menu
    }
  }
  return QMainWindow::eventFilter(obj, event);
}

void AppWindow::setupTitleBar() {
  d->titleBar = new QFrame(this);
  d->titleBar->setFixedHeight(36);
  d->titleBar->setObjectName("TitleBar");

  auto *layout = new QHBoxLayout(d->titleBar);
  layout->setContentsMargins(12, 0, 12, 0);
  layout->setSpacing(8);
  layout->setAlignment(Qt::AlignVCenter);

  auto *leftSpacer = new QWidget(d->titleBar);
  leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(leftSpacer);

  d->titleLabel = new QLabel(tr("字幕编辑"), d->titleBar);
  d->titleLabel->setObjectName("AppTitleLabel");
  d->titleLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(d->titleLabel);

  auto *rightSpacer = new QWidget(d->titleBar);
  rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(rightSpacer);

  d->settingsBtn = new QPushButton(d->titleBar);
  d->settingsBtn->setObjectName("TitleBarSettingsBtn");
  d->settingsBtn->setIcon(QIcon(":/icons/settings.svg"));
  d->settingsBtn->setIconSize(QSize(16, 16));
  d->settingsBtn->setFixedSize(26, 26);
  d->settingsBtn->setToolTip(tr("设置"));
  d->settingsBtn->setCursor(Qt::PointingHandCursor);
  layout->addWidget(d->settingsBtn);
  connect(d->settingsBtn, &QPushButton::clicked, this,
          &AppWindow::onSettingsRequested);

  d->exportBtn = new QPushButton(d->titleBar);
  d->exportBtn->setObjectName("TitleBarExportBtn");
  d->exportBtn->setIcon(QIcon(":/icons/export.svg"));
  d->exportBtn->setIconSize(QSize(16, 16));
  d->exportBtn->setFixedSize(26, 26);
  d->exportBtn->setToolTip(tr("导出字幕"));
  d->exportBtn->setCursor(Qt::PointingHandCursor);
  layout->addWidget(d->exportBtn);
  connect(d->exportBtn, &QPushButton::clicked, this,
          &AppWindow::onExportRequested);

  d->windowAgent->setHitTestVisible(d->exportBtn, true);
  d->windowAgent->setHitTestVisible(d->settingsBtn, true);
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
            d->videoImportTime_ = QDateTime::currentDateTime();
            d->mediaPlayer->load(path);
          });

  // 1a. Timeline empty-state import button -> file dialog
  connect(d->timelinePanel, &TimelinePanel::importMediaRequested, this,
          [this]() {
            QString path = QFileDialog::getOpenFileName(
                this, tr("导入视频"), QString(),
                tr("媒体文件 (*.mp4 *.mkv *.avi *.mov *.srt);;视频文件 (*.mp4 "
                   "*.mkv *.avi *.mov);;字幕文件 (*.srt);;所有文件 (*)"));
            if (!path.isEmpty()) {
              QString ext = QFileInfo(path).suffix().toLower();
              if (ext == "srt") {
                onSubtitleFileDropped(path);
              } else if (d->mediaPlayer) {
                d->timelinePanel->setMediaFilePath(path);
                d->videoImportTime_ = QDateTime::currentDateTime();
                d->mediaPlayer->load(path);
              }
            }
          });

  // 2. MediaPlayer -> Timeline: duration
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->timelinePanel,
          &TimelinePanel::setTotalDuration);
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, d->subtitleListPanel,
          &SubtitleListPanel::setTotalDuration);

  // 2a. MediaPlayer -> Timeline & VideoPreview: video fps for drag throttle
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, this,
          [this](qint64, QSize) {
            double fps = d->mediaPlayer->decoderFps();
            d->timelinePanel->setVideoFps(fps);
            d->videoPreviewPanel->setVideoFps(fps);
            d->subtitleListPanel->setVideoFps(fps);
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
  connect(
      d->subtitleListPanel, &SubtitleListPanel::itemSeekRequested,
      d->mediaPlayer, [this](const QString &id, qint64 ms) {
        if (d->subtitleTrack && d->mediaPlayer) {
          const SubtitleItem *item = d->subtitleTrack->findItem(id);
          if (item) {
            qint64 current = d->mediaPlayer->currentTimeMs();
            if (current >= item->startMs && current < item->endMs) {
              return; // Playhead already within the clip range, no need to seek
            }
          }
        }
        d->mediaPlayer->seek(ms);
      });

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

  // 12. TimelinePanel subtitle drop -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::subtitleFileDropped, this,
          &AppWindow::onSubtitleFileDropped);

  // 13. TimelinePanel video ASR -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::videoAsrRequested, this,
          &AppWindow::onVideoAsrRequested);

  // 14. TimelinePanel video property -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::videoPropertyRequested, this,
          &AppWindow::onVideoPropertyRequested);

  // 15. TimelinePanel open file location -> AppWindow
  connect(d->timelinePanel, &TimelinePanel::openFileLocationRequested, this,
          &AppWindow::onOpenFileLocationRequested);

  // Top horizontal splitter
  d->topSplitter = new QSplitter(Qt::Horizontal, this);
  d->topSplitter->addWidget(d->videoPreviewPanel);
  d->topSplitter->addWidget(d->subtitleListPanel);
  d->topSplitter->setStretchFactor(0, 1);
  d->topSplitter->setStretchFactor(1, 0);
  d->topSplitter->setHandleWidth(10);
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
  d->verticalSplitter->setCollapsible(0, false);
  d->verticalSplitter->setCollapsible(1, false);
  d->timelinePanel->setMinimumHeight(180);
  d->timelinePanel->setMaximumHeight(400);
  d->verticalSplitter->setSizes({720, 180});

  // Set central widget
  auto *central = new QWidget(this);
  central->setObjectName("CentralWidget");
  auto *centralLayout = new QVBoxLayout(central);
  centralLayout->setContentsMargins(10, 10, 10, 10);
  centralLayout->setSpacing(0);
  centralLayout->addWidget(d->verticalSplitter);
  setCentralWidget(central);

  // 创建菜单栏
  setupMenuBar();

  // 创建 ProjectManager
  d->projectManager = new ProjectManager(d->subtitleTrack, this);

  // 启用自动保存
  d->projectManager->enableAutoSave(true);
  d->projectManager->setAutoSaveInterval(60); // 1 分钟

  // 连接字幕修改信号到 ProjectManager
  connect(d->subtitleTrack, &SubtitleTrack::dataChanged, this, [this]() {
    if (d->projectManager) {
      d->projectManager->setDirty(true);
    }
  });

  // 连接自动保存信号
  connect(d->projectManager, &ProjectManager::autoSaveTriggered, this,
          [this]() { statusBar()->showMessage(tr("工程已自动保存"), 2000); });
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
    int ret = AppMessageBox::question(
        this, tr("确认覆盖"),
        tr("字幕轨道已有内容，继续导入将清空现有字幕，是否继续？"));
    if (ret != AppMessageBox::Yes)
      return;
    d->subtitleTrack->clear();
  }

  // Parse SRT
  try {
    SrtParser::SubtitleParserFactory parserFactory(path.toStdString());
    SrtParser::SubtitleParser *parser = parserFactory.getParser();
    auto subtitles = parser->getSubtitles();

    qint64 maxEndMs = 0;
    qint64 previousEndMs = 0;
    for (auto *sub : subtitles) {
      if (!sub)
        continue;
      SubtitleItem item;
      item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      item.text = QString::fromStdString(sub->getText());
      item.startMs = static_cast<qint64>(sub->getStartTime());
      item.endMs = static_cast<qint64>(sub->getEndTime());

      // Overlap check
      if (item.startMs < previousEndMs) {
        qWarning() << "Illegal overlapping subtitle ignored: " << item.text
                   << " start=" << item.startMs << " prevEnd=" << previousEndMs;
        continue;
      }

      d->subtitleTrack->addItem(item);
      previousEndMs = item.endMs;
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
    AppMessageBox::critical(this, tr("字幕文件格式错误"),
                            tr("无法解析字幕文件，请检查文件格式。"));
  }
}

void AppWindow::onVideoAsrRequested() {
  if (!d->subtitleTrack || !d->timelinePanel)
    return;

  QString videoPath = d->timelinePanel->mediaFilePath();
  if (videoPath.isEmpty())
    return;

  if (!d->subtitleTrack->items().isEmpty()) {
    int ret = AppMessageBox::question(
        this, tr("确认覆盖"),
        tr("字幕轨道已有内容，语音识别将清空现有字幕，是否继续？"));
    if (ret != AppMessageBox::Yes)
      return;
  }

  AsrConfigDialog configDlg(this);
  if (configDlg.exec() != QDialog::Accepted) {
    return;
  }

  QString model = configDlg.engineModelType();
  int maxLen = configDlg.sentenceMaxLength();
  bool enableSpeaker = configDlg.speakerDiarization();

  d->timelinePanel->startAsrPipeline(videoPath, model, maxLen, enableSpeaker);
}

void AppWindow::onVideoPropertyRequested() {
  if (!d->mediaPlayer || !d->timelinePanel)
    return;

  QString path = d->timelinePanel->mediaFilePath();
  if (path.isEmpty())
    return;

  QFileInfo fi(path);
  QList<VideoPropertyDialog::Section> sections;

  // ---- Basic Info ----
  QMap<QString, QString> basicInfo;
  basicInfo.insert(tr("名称"), fi.completeBaseName());
  basicInfo.insert(tr("位置"), fi.path());
  basicInfo.insert(
      tr("文件大小"),
      QString("%1 MB").arg(fi.size() / (1024.0 * 1024.0), 0, 'f', 2));

  double fps = d->mediaPlayer->decoderFps();
  qint64 durationMs = d->mediaPlayer->durationMs();
  if (durationMs > 0) {
    int totalSeconds = static_cast<int>(durationMs / 1000);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    int frames = 0;
    if (fps > 0.0) {
      frames = static_cast<int>((durationMs % 1000) * fps / 1000.0);
    }
    basicInfo.insert(tr("时长"), QString("%1:%2:%3:%4")
                                     .arg(hours, 2, 10, QLatin1Char('0'))
                                     .arg(minutes, 2, 10, QLatin1Char('0'))
                                     .arg(seconds, 2, 10, QLatin1Char('0'))
                                     .arg(frames, 2, 10, QLatin1Char('0')));
  }

  if (fi.birthTime().isValid())
    basicInfo.insert(tr("创建日期"),
                     fi.birthTime().toString("yyyy/MM/dd hh:mm:ss"));
  if (d->videoImportTime_.isValid())
    basicInfo.insert(tr("导入时间"),
                     d->videoImportTime_.toString("yyyy/MM/dd hh:mm:ss"));

  QString mediaCreation = d->mediaPlayer->mediaCreationTime();
  if (!mediaCreation.isEmpty()) {
    QDateTime dt = QDateTime::fromString(mediaCreation, Qt::ISODate);
    if (dt.isValid())
      basicInfo.insert(tr("创建媒体时间"),
                       dt.toLocalTime().toString("yyyy/MM/dd hh:mm:ss"));
  }

  sections.append(qMakePair(tr("基本信息"), basicInfo));

  // ---- Video Info ----
  QMap<QString, QString> videoInfo;
  QString vCodec = d->mediaPlayer->videoCodecName();
  if (!vCodec.isEmpty())
    videoInfo.insert(tr("编译码器"), vCodec.toUpper());

  QSize size = d->mediaPlayer->videoSize();
  if (size.isValid())
    videoInfo.insert(tr("分辨率"),
                     QString("%1x%2").arg(size.width()).arg(size.height()));

  if (fps > 0.0)
    videoInfo.insert(tr("帧率"), QString("%1 fps").arg(fps, 0, 'f', 0));

  qint64 vBitrate = d->mediaPlayer->videoBitRate();
  if (vBitrate > 0)
    videoInfo.insert(tr("码率"), QString("%1 kbps").arg(vBitrate / 1000));

  videoInfo.insert(tr("传输特性"), "-");

  sections.append(qMakePair(tr("视频信息"), videoInfo));

  // ---- Audio Info ----
  QMap<QString, QString> audioInfo;
  QString aCodec = d->mediaPlayer->audioCodecName();
  if (!aCodec.isEmpty())
    audioInfo.insert(tr("编译码器"), aCodec.toUpper());

  int channels = d->mediaPlayer->audioChannels();
  if (channels > 0)
    audioInfo.insert(tr("声道"), QString("%1").arg(channels));

  int sampleRate = d->mediaPlayer->audioSampleRate();
  int bitDepth = d->mediaPlayer->audioBitDepth();
  if (sampleRate > 0) {
    if (bitDepth > 0)
      audioInfo.insert(tr("采样率"),
                       QString("%1 Hz,%2 Bits").arg(sampleRate).arg(bitDepth));
    else
      audioInfo.insert(tr("采样率"), QString("%1 Hz").arg(sampleRate));
  }

  qint64 aBitrate = d->mediaPlayer->audioBitRate();
  if (aBitrate > 0)
    audioInfo.insert(tr("码率"), QString("%1 kbps").arg(aBitrate / 1000));

  sections.append(qMakePair(tr("音频信息"), audioInfo));

  VideoPropertyDialog dialog(sections, this);
  dialog.exec();
}

void AppWindow::onOpenFileLocationRequested() {
  if (!d->timelinePanel)
    return;

  QString path = d->timelinePanel->mediaFilePath();
  if (path.isEmpty())
    return;

#ifdef Q_OS_MAC
  // Use 'open -R' to reveal and select the file in Finder
  QProcess::startDetached("open", QStringList() << "-R" << path);
#else
  QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).path()));
#endif
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

void AppWindow::onExportRequested() {
  if (!d->subtitleTrack || d->subtitleTrack->items().isEmpty()) {
    AppMessageBox::warning(this, tr("导出字幕"),
                           tr("当前没有字幕内容，无法导出。"));
    return;
  }

  QString filter = tr("SRT 字幕 (*.srt);;纯文本 (*.txt);;Premiere XML [实验] "
                      "(*.xml);;Final Cut Pro XML [实验] (*.fcpxml)");
  QString selectedFilter;
  QString filePath = QFileDialog::getSaveFileName(
      this, tr("导出字幕"), QString(), filter, &selectedFilter);

  if (filePath.isEmpty())
    return;

  // Ensure correct extension based on selected filter
  QString ext = QFileInfo(filePath).suffix().toLower();
  if (ext.isEmpty()) {
    if (selectedFilter.contains("*.srt", Qt::CaseInsensitive)) {
      filePath += ".srt";
      ext = "srt";
    } else if (selectedFilter.contains("*.txt", Qt::CaseInsensitive)) {
      filePath += ".txt";
      ext = "txt";
    } else if (selectedFilter.contains("*.xml", Qt::CaseInsensitive)) {
      filePath += ".xml";
      ext = "xml";
    } else if (selectedFilter.contains("*.fcpxml", Qt::CaseInsensitive)) {
      filePath += ".fcpxml";
      ext = "fcpxml";
    }
  }

  // 提取视频的属性
  QSize videoSize;
  double fps = 25.0;
  if (d->mediaPlayer) {
    videoSize = d->mediaPlayer->videoSize();
    double mediaFps = d->mediaPlayer->decoderFps();
    if (mediaFps > 0.0) {
      fps = mediaFps;
    }
  }

  bool success = false;
  if (ext == "srt") {
    success = SubtitleExporter::exportToSRT(*d->subtitleTrack, filePath);
  } else if (ext == "txt") {
    success = SubtitleExporter::exportToTXT(*d->subtitleTrack, filePath);
  } else if (ext == "xml") {
    success = SubtitleExporter::exportToPremiereXML(*d->subtitleTrack, filePath,
                                                    fps, videoSize);
  } else if (ext == "fcpxml") {
    success = SubtitleExporter::exportToFCPXML(*d->subtitleTrack, filePath, fps,
                                               videoSize);
  }

  if (!success) {
    AppMessageBox::critical(this, tr("导出失败"),
                            tr("导出字幕失败，请检查文件路径和权限。"));
  }
}

void AppWindow::onSettingsRequested() {
  ConfigDialog dialog(this);
  connect(&dialog, &ConfigDialog::configApplied, this, [this]() {
    if (d->subtitleListPanel) {
      d->subtitleListPanel->updateSpeakerColumnVisibility();
    }
  });
  dialog.exec();
}

void AppWindow::retranslateUi() {
  setWindowTitle(tr("字幕编辑"));
  d->titleLabel->setText(tr("字幕编辑"));
  if (d->exportBtn)
    d->exportBtn->setToolTip(tr("导出字幕"));
  if (d->settingsBtn)
    d->settingsBtn->setToolTip(tr("设置"));
}

void AppWindow::setupMenuBar() {
  d->menuBar = new QMenuBar(this);
  setMenuBar(d->menuBar);

  // 文件菜单
  d->fileMenu = d->menuBar->addMenu(tr("文件"));

  QAction *newAction = d->fileMenu->addAction(tr("新建工程"));
  newAction->setShortcut(QKeySequence::New);
  connect(newAction, &QAction::triggered, this, &AppWindow::onNewProject);

  QAction *openAction = d->fileMenu->addAction(tr("打开工程..."));
  openAction->setShortcut(QKeySequence::Open);
  connect(openAction, &QAction::triggered, this, &AppWindow::onOpenProject);

  QAction *saveAction = d->fileMenu->addAction(tr("保存工程"));
  saveAction->setShortcut(QKeySequence::Save);
  connect(saveAction, &QAction::triggered, this, &AppWindow::onSaveProject);

  QAction *saveAsAction = d->fileMenu->addAction(tr("另存为..."));
  saveAsAction->setShortcut(QKeySequence::SaveAs);
  connect(saveAsAction, &QAction::triggered, this, &AppWindow::onSaveProjectAs);

  d->fileMenu->addSeparator();

  // 最近打开
  d->recentFilesMenu = d->fileMenu->addMenu(tr("最近打开"));
  connect(d->recentFilesMenu, &QMenu::aboutToShow, this, [this]() {
    d->recentFilesMenu->clear();
    QStringList recentFiles = SubtitleProject::recentFiles();
    if (recentFiles.isEmpty()) {
      d->recentFilesMenu->addAction(tr("无最近文件"))->setEnabled(false);
    } else {
      for (const auto &file : recentFiles) {
        QAction *action =
            d->recentFilesMenu->addAction(QFileInfo(file).fileName());
        action->setData(file);
        connect(action, &QAction::triggered, this, [this, action]() {
          onOpenRecentFile(action->data().toString());
        });
      }
      d->recentFilesMenu->addSeparator();
      QAction *clearAction = d->recentFilesMenu->addAction(tr("清除最近"));
      connect(clearAction, &QAction::triggered, this,
              &AppWindow::onClearRecentFiles);
    }
  });

  d->fileMenu->addSeparator();

  // 导出字幕
  QMenu *exportMenu = d->fileMenu->addMenu(tr("导出字幕"));
  QAction *srtAction = exportMenu->addAction("SRT");
  connect(srtAction, &QAction::triggered, this, &AppWindow::onExportSrt);
  QAction *txtAction = exportMenu->addAction("TXT");
  connect(txtAction, &QAction::triggered, this, &AppWindow::onExportTxt);

  d->fileMenu->addSeparator();

  QAction *exitAction = d->fileMenu->addAction(tr("退出"));
  connect(exitAction, &QAction::triggered, this, &QWidget::close);

  // 编辑菜单
  d->editMenu = d->menuBar->addMenu(tr("编辑"));

  QAction *undoAction = d->editMenu->addAction(tr("撤销"));
  undoAction->setShortcut(QKeySequence::Undo);
  undoAction->setEnabled(false); // 预留

  QAction *redoAction = d->editMenu->addAction(tr("重做"));
  redoAction->setShortcut(QKeySequence::Redo);
  redoAction->setEnabled(false); // 预留

  d->editMenu->addSeparator();

  QAction *cutAction = d->editMenu->addAction(tr("剪切"));
  cutAction->setShortcut(QKeySequence::Cut);
  cutAction->setEnabled(false); // 预留

  QAction *copyAction = d->editMenu->addAction(tr("复制"));
  copyAction->setShortcut(QKeySequence::Copy);
  copyAction->setEnabled(false); // 预留

  QAction *pasteAction = d->editMenu->addAction(tr("粘贴"));
  pasteAction->setShortcut(QKeySequence::Paste);
  pasteAction->setEnabled(false); // 预留

  d->editMenu->addSeparator();

  QAction *selectAllAction = d->editMenu->addAction(tr("全选"));
  selectAllAction->setShortcut(QKeySequence::SelectAll);
  connect(selectAllAction, &QAction::triggered, this, &AppWindow::onSelectAll);

  QAction *deleteAction = d->editMenu->addAction(tr("删除选中"));
  deleteAction->setShortcut(QKeySequence::Delete);
  connect(deleteAction, &QAction::triggered, this,
          &AppWindow::onDeleteSelected);

  // 设置菜单
  d->settingsMenu = d->menuBar->addMenu(tr("设置"));

  QAction *configAction = d->settingsMenu->addAction(tr("配置..."));
  connect(configAction, &QAction::triggered, this, [this]() {
    ConfigDialog dlg(this);
    connect(&dlg, &ConfigDialog::configApplied, this,
            &AppWindow::onConfigApplied);
    dlg.exec();
  });

  QMenu *langMenu = d->settingsMenu->addMenu(tr("语言"));
  QAction *zhAction = langMenu->addAction(tr("中文"));
  connect(zhAction, &QAction::triggered, this,
          []() { TranslationManager::instance().loadLanguage("zh_CN"); });
  QAction *enAction = langMenu->addAction("English");
  connect(enAction, &QAction::triggered, this,
          []() { TranslationManager::instance().loadLanguage("en_US"); });

  // 帮助菜单
  d->helpMenu = d->menuBar->addMenu(tr("帮助"));

  QAction *aboutAction = d->helpMenu->addAction(tr("关于"));
  connect(aboutAction, &QAction::triggered, this, &AppWindow::onAbout);
}

void AppWindow::onNewProject() {
  if (d->projectManager && d->projectManager->isDirty()) {
    int ret = AppMessageBox::question(
        this, tr("确认新建"), tr("当前工程有未保存的更改，是否继续新建？"));
    if (ret != AppMessageBox::Yes)
      return;
  }
  if (d->projectManager) {
    d->projectManager->newProject();
  }
  setWindowTitle(tr("字幕编辑"));
}

void AppWindow::onOpenProject() {
  QString filePath =
      QFileDialog::getOpenFileName(this, tr("打开工程"), QString(),
                                   tr("字幕编辑工程 (*.sedit);;所有文件 (*)"));

  if (filePath.isEmpty())
    return;

  if (d->projectManager && d->projectManager->openProject(filePath)) {
    setWindowTitle(
        tr("字幕编辑 - %1").arg(d->projectManager->currentProjectName()));
  } else {
    AppMessageBox::critical(this, tr("打开失败"),
                            tr("无法打开工程文件，请检查文件格式。"));
  }
}

void AppWindow::onSaveProject() {
  if (d->projectManager && d->projectManager->currentFilePath().isEmpty()) {
    onSaveProjectAs();
    return;
  }
  if (d->projectManager) {
    d->projectManager->saveProject();
  }
}

void AppWindow::onSaveProjectAs() {
  QString filePath = QFileDialog::getSaveFileName(this, tr("另存为"), QString(),
                                                  tr("字幕编辑工程 (*.sedit)"));

  if (filePath.isEmpty())
    return;

  if (!filePath.endsWith(".sedit")) {
    filePath += ".sedit";
  }

  if (d->projectManager && d->projectManager->saveProjectAs(filePath)) {
    setWindowTitle(
        tr("字幕编辑 - %1").arg(d->projectManager->currentProjectName()));
  } else {
    AppMessageBox::critical(this, tr("保存失败"),
                            tr("无法保存工程文件，请检查磁盘空间。"));
  }
}

void AppWindow::onOpenRecentFile(const QString &filePath) {
  if (d->projectManager && d->projectManager->openProject(filePath)) {
    setWindowTitle(
        tr("字幕编辑 - %1").arg(d->projectManager->currentProjectName()));
  }
}

void AppWindow::onClearRecentFiles() { SubtitleProject::clearRecentFiles(); }

void AppWindow::onExportSrt() {
  if (!d->subtitleTrack)
    return;

  QString filePath = QFileDialog::getSaveFileName(
      this, tr("导出 SRT"), QString(), tr("SRT 字幕 (*.srt)"));

  if (filePath.isEmpty())
    return;

  if (SubtitleExporter::exportToSRT(*d->subtitleTrack, filePath)) {
    AppMessageBox::information(this, tr("导出成功"),
                               tr("字幕已导出到：%1").arg(filePath));
  } else {
    AppMessageBox::critical(this, tr("导出失败"), tr("无法导出字幕文件。"));
  }
}

void AppWindow::onExportTxt() {
  if (!d->subtitleTrack)
    return;

  QString filePath = QFileDialog::getSaveFileName(
      this, tr("导出 TXT"), QString(), tr("TXT 文本 (*.txt)"));

  if (filePath.isEmpty())
    return;

  if (SubtitleExporter::exportToTXT(*d->subtitleTrack, filePath)) {
    AppMessageBox::information(this, tr("导出成功"),
                               tr("字幕已导出到：%1").arg(filePath));
  } else {
    AppMessageBox::critical(this, tr("导出失败"), tr("无法导出字幕文件。"));
  }
}

void AppWindow::onSelectAll() {
  if (d->subtitleTrack) {
    QSet<QString> allIds;
    for (const auto &item : d->subtitleTrack->items()) {
      allIds.insert(item.id);
    }
    d->subtitleTrack->setSelectedItems(allIds);
  }
}

void AppWindow::onDeleteSelected() {
  if (!d->subtitleTrack)
    return;

  auto selected = d->subtitleTrack->selectedItem();
  if (selected) {
    d->subtitleTrack->removeItem(selected->id);
  }
}

void AppWindow::onConfigApplied() {
  if (d->subtitleListPanel) {
    d->subtitleListPanel->updateSpeakerColumnVisibility();
  }
}

void AppWindow::onAbout() {
  AppMessageBox::information(
      this, tr("关于"),
      tr("字幕编辑器 v1.0\n\n一个简单易用的视频字幕编辑工具。"));
}
