#include "AppWindow.h"
#include "AppMessageBox.h"
#include "AsrConfigDialog.h"
#include "ConfigDialog.h"
#include "ConfigManager.h"
#include "ExportDialog.h"
#include "MediaPlayer.h"
#include "ProjectManager.h"
#include "SubtitleExporter.h"
#include "SubtitleItem.h"
#include "SubtitleListPanel.h"
#include "SubtitleProject.h"
#include "SubtitleTrack.h"
#include "TimelinePanel.h"
#include "TranslationManager.h"
#include "VideoExportDialog.h"
#include "VideoExporter.h"
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
#include <QUndoStack>
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
  QAction *exportAction = nullptr;
  QMenu *langMenu = nullptr;

  // 菜单动作
  QAction *newAction = nullptr;
  QAction *openAction = nullptr;
  QAction *saveAction = nullptr;
  QAction *saveAsAction = nullptr;
  QAction *exitAction = nullptr;
  QAction *undoAction = nullptr;
  QAction *redoAction = nullptr;
  QAction *cutAction = nullptr;
  QAction *copyAction = nullptr;
  QAction *pasteAction = nullptr;
  QAction *selectAllAction = nullptr;
  QAction *deleteAction = nullptr;
  QAction *configAction = nullptr;
  QAction *aboutAction = nullptr;

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

  // 组合标题栏和菜单栏到同一个 widget 中
  auto *menuContainer = new QWidget(this);
  auto *menuContainerLayout = new QVBoxLayout(menuContainer);
  menuContainerLayout->setContentsMargins(0, 0, 0, 0);
  menuContainerLayout->setSpacing(0);
  menuContainerLayout->addWidget(d->titleBar);
  menuContainerLayout->addWidget(d->menuBar);

  setMenuWidget(menuContainer);
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

void AppWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QMainWindow::changeEvent(event);
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
            // load() is triggered via ProjectManager::setVideoPath →
            // videoPathChanged signal
            if (d->projectManager) {
              d->projectManager->setVideoPath(path);
            } else {
              d->mediaPlayer->load(path);
            }
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
                // load() is triggered via ProjectManager::setVideoPath →
                // videoPathChanged signal
                if (d->projectManager) {
                  d->projectManager->setVideoPath(path);
                } else {
                  d->mediaPlayer->load(path);
                }
              }
            }
          });

  // 2. MediaPlayer -> AppWindow: duration update
  connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, this,
          [this](qint64, QSize) { updateTotalDuration(true); });

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

  // 11. SubtitleTrack data change -> VideoPreview subtitle refresh & duration
  // update
  connect(d->subtitleTrack, &SubtitleTrack::dataChanged, d->videoPreviewPanel,
          &VideoPreviewPanel::updateSubtitleOverlay);
  connect(d->subtitleTrack, &SubtitleTrack::dataChanged, this,
          [this]() { updateTotalDuration(false); });

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

  // 暴露撤销栈给字幕数据模型
  d->subtitleTrack->setUndoStack(d->projectManager->undoStack());

  // 连接撤销栈与动作启用状态
  connect(d->projectManager->undoStack(), &QUndoStack::canUndoChanged,
          d->undoAction, &QAction::setEnabled);
  connect(d->projectManager->undoStack(), &QUndoStack::canRedoChanged,
          d->redoAction, &QAction::setEnabled);

  // 动态更新撤销与恢复的菜单动作文本
  connect(d->projectManager->undoStack(), &QUndoStack::undoTextChanged, this,
          [this](const QString &text) {
            d->undoAction->setText(text.isEmpty() ? tr("撤销")
                                                  : tr("撤销 %1").arg(text));
          });
  connect(d->projectManager->undoStack(), &QUndoStack::redoTextChanged, this,
          [this](const QString &text) {
            d->redoAction->setText(text.isEmpty() ? tr("重做")
                                                  : tr("重做 %1").arg(text));
          });

  // 绑定触发动作
  connect(d->undoAction, &QAction::triggered, d->projectManager->undoStack(),
          &QUndoStack::undo);
  connect(d->redoAction, &QAction::triggered, d->projectManager->undoStack(),
          &QUndoStack::redo);

  // 监听撤销/重做导致的视频路径变化
  connect(d->projectManager, &ProjectManager::videoPathChanged, this,
          [this](const QString &path) {
            d->timelinePanel->setMediaFilePath(path);
            d->videoImportTime_ = QDateTime::currentDateTime();
            if (d->mediaPlayer) {
              d->mediaPlayer->load(path);
            }
          });

  // 绑定标题栏更新
  connect(d->projectManager, &ProjectManager::dirtyStateChanged, this,
          &AppWindow::updateWindowTitle);
  connect(d->projectManager, &ProjectManager::projectChanged, this,
          &AppWindow::updateWindowTitle);

  // 启用自动保存
  d->projectManager->enableAutoSave(true);
  d->projectManager->setAutoSaveInterval(60); // 1 分钟

  // 连接自动保存信号
  connect(d->projectManager, &ProjectManager::autoSaveTriggered, this,
          [this]() { statusBar()->showMessage(tr("工程已自动保存"), 2000); });
}

void AppWindow::loadFile(const QString &path) {
  if (d->mediaPlayer) {
    // Auto-play once loading completes
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(d->mediaPlayer, &MediaPlayer::mediaLoaded, this,
                    [this, conn]() {
                      disconnect(*conn);
                      d->mediaPlayer->play();
                    });
    d->mediaPlayer->load(path);
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
  }

  // 先在外部解析 SRT，避免格式错误将垃圾状态记入撤销栈
  QList<SubtitleItem> itemsToImport;
  qint64 maxEndMs = 0;
  try {
    SrtParser::SubtitleParserFactory parserFactory(path.toStdString());
    SrtParser::SubtitleParser *parser = parserFactory.getParser();
    auto subtitles = parser->getSubtitles();

    qint64 previousEndMs = 0;
    for (auto *sub : subtitles) {
      if (!sub)
        continue;
      SubtitleItem item;
      item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
      item.text = QString::fromStdString(sub->getText());
      item.startMs = static_cast<qint64>(sub->getStartTime());
      item.endMs = static_cast<qint64>(sub->getEndTime());

      d->subtitleTrack->applyDefaultStyle(item);

      // Overlap check
      if (item.startMs < previousEndMs) {
        qWarning() << "Illegal overlapping subtitle ignored: " << item.text
                   << " start=" << item.startMs << " prevEnd=" << previousEndMs;
        continue;
      }

      itemsToImport.append(item);
      previousEndMs = item.endMs;
      if (item.endMs > maxEndMs)
        maxEndMs = item.endMs;
    }
    delete parser;

  } catch (...) {
    AppMessageBox::critical(this, tr("字幕文件格式错误"),
                            tr("无法解析字幕文件，请检查文件格式。"));
    return;
  }

  // 使用 executeBatchAction 将数据写入折叠为单一撤销历史，并保证 O(N) 性能
  d->subtitleTrack->executeBatchAction(
      tr("导入字幕文件"), [this, itemsToImport]() {
        d->subtitleTrack->clear();
        for (const auto &item : itemsToImport) {
          d->subtitleTrack->addItem(item);
        }
      });

  updateTotalDuration(true);

  // Seek to 0
  if (d->mediaPlayer)
    d->mediaPlayer->seek(0);
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
  if (!d->subtitleTrack)
    return;

  // 1. 打开统一导出对话框
  ExportDialog dlg(this);
  dlg.setSubtitleTrack(d->subtitleTrack);

  // 从 mediaPlayer 获取当前源视频的详细属性并注入
  QString videoPath =
      d->timelinePanel ? d->timelinePanel->mediaFilePath() : QString();
  QSize videoSize;
  double fps = 0.0;
  bool hasAudio = false;
  int audioSampleRate = 0;
  int audioBitrate = 0;

  if (d->mediaPlayer && !videoPath.isEmpty()) {
    videoSize = d->mediaPlayer->videoSize();
    fps = d->mediaPlayer->decoderFps();
    hasAudio = (d->mediaPlayer->audioChannels() > 0);
    audioSampleRate = d->mediaPlayer->audioSampleRate();
    audioBitrate = d->mediaPlayer->audioBitRate();
  }
  dlg.setSourceVideo(videoPath, videoSize, fps, hasAudio, audioSampleRate,
                     audioBitrate);

  if (dlg.exec() != QDialog::Accepted) {
    return;
  }

  QString mainOutputPath = dlg.outputPath();
  bool exportVideo = dlg.isVideoSelected();
  bool exportSubtitle = dlg.isSubtitleSelected();

  // 2. 如果勾选了字幕，首先在主线程快速导出字幕文件
  if (exportSubtitle) {
    QString format = dlg.subtitleFormat();
    QString subtitlePath = mainOutputPath;

    // 如果同时导出了视频，字幕文件自动命名为和视频相同、后缀改变的文件
    if (exportVideo) {
      QFileInfo videoInfo(mainOutputPath);
      subtitlePath = videoInfo.absolutePath() + "/" +
                     videoInfo.completeBaseName() + "." + format;
    }

    // 默认使用视频原本的 fps 和大小作为 XML 排版的依据，防崩溃处理
    double useFps = fps > 0.0 ? fps : 25.0;
    QSize useSize = videoSize.isValid() ? videoSize : QSize(1920, 1080);

    bool subSuccess = false;
    if (format == "srt") {
      subSuccess =
          SubtitleExporter::exportToSRT(*d->subtitleTrack, subtitlePath);
    } else if (format == "txt") {
      subSuccess =
          SubtitleExporter::exportToTXT(*d->subtitleTrack, subtitlePath);
    } else if (format == "xml") {
      subSuccess = SubtitleExporter::exportToPremiereXML(
          *d->subtitleTrack, subtitlePath, useFps, useSize);
    } else if (format == "fcpxml") {
      subSuccess = SubtitleExporter::exportToFCPXML(
          *d->subtitleTrack, subtitlePath, useFps, useSize);
    }

    if (!subSuccess) {
      AppMessageBox::critical(this, tr("导出失败"),
                              tr("字幕文件导出失败，请检查保存路径和权限。"));
      return;
    }

    // 如果仅仅导出字幕，导出成功后直接提示并退出
    if (!exportVideo) {
      AppMessageBox::information(
          this, tr("导出成功"),
          tr("字幕文件已成功导出到：\n%1").arg(subtitlePath));
      return;
    }
  }

  // 3. 如果勾选了视频，启动多线程编码烧录管线并展示进度条
  if (exportVideo) {
    VideoExporter *exporter = new VideoExporter(this);
    exporter->setConfig(dlg.videoConfig());
    exporter->setSubtitleTrack(d->subtitleTrack);

    VideoExportDialog progressDlg(exporter, this);
    exporter->start();

    int result = progressDlg.exec();

    exporter->wait();
    delete exporter;

    if (result == QDialog::Accepted) {
      QString successMsg = tr("视频已成功导出到：\n%1").arg(mainOutputPath);
      if (exportSubtitle) {
        successMsg += tr("\n\n关联字幕文件已一并输出。");
      }
      AppMessageBox::information(this, tr("导出成功"), successMsg);
    }
  }
}

void AppWindow::onSettingsRequested() {
  ConfigDialog dialog(this);
  connect(&dialog, &ConfigDialog::configApplied, this, [this]() {
    if (d->subtitleListPanel) {
      d->subtitleListPanel->updateSpeakerColumnVisibility();
    }
    if (d->subtitleTrack) {
      d->subtitleTrack->reloadGlobalSettings();
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
  if (d->videoPreviewPanel)
    d->videoPreviewPanel->retranslateUi();

  // 菜单栏
  if (d->fileMenu)
    d->fileMenu->setTitle(tr("文件"));
  if (d->editMenu)
    d->editMenu->setTitle(tr("编辑"));
  if (d->settingsMenu)
    d->settingsMenu->setTitle(tr("设置"));
  if (d->helpMenu)
    d->helpMenu->setTitle(tr("帮助"));
  if (d->recentFilesMenu)
    d->recentFilesMenu->setTitle(tr("最近打开"));
  if (d->exportAction)
    d->exportAction->setText(tr("导出..."));
  if (d->langMenu)
    d->langMenu->setTitle(tr("语言"));

  // 文件菜单动作
  if (d->newAction)
    d->newAction->setText(tr("新建工程"));
  if (d->openAction)
    d->openAction->setText(tr("打开工程..."));
  if (d->saveAction)
    d->saveAction->setText(tr("保存工程"));
  if (d->saveAsAction)
    d->saveAsAction->setText(tr("另存为..."));
  if (d->exitAction)
    d->exitAction->setText(tr("退出"));

  // 编辑菜单动作
  if (d->undoAction)
    d->undoAction->setText(tr("撤销"));
  if (d->redoAction)
    d->redoAction->setText(tr("重做"));
  if (d->cutAction)
    d->cutAction->setText(tr("剪切"));
  if (d->copyAction)
    d->copyAction->setText(tr("复制"));
  if (d->pasteAction)
    d->pasteAction->setText(tr("粘贴"));
  if (d->selectAllAction)
    d->selectAllAction->setText(tr("全选"));
  if (d->deleteAction)
    d->deleteAction->setText(tr("删除选中"));

  // 设置菜单动作
  if (d->configAction)
    d->configAction->setText(tr("配置..."));

  // 帮助菜单动作
  if (d->aboutAction)
    d->aboutAction->setText(tr("关于"));
}

void AppWindow::setupMenuBar() {
  d->menuBar = new QMenuBar(this);

  // 文件菜单
  d->fileMenu = d->menuBar->addMenu(tr("文件"));

  d->newAction = d->fileMenu->addAction(tr("新建工程"));
  d->newAction->setShortcut(QKeySequence::New);
  connect(d->newAction, &QAction::triggered, this, &AppWindow::onNewProject);

  d->openAction = d->fileMenu->addAction(tr("打开工程..."));
  d->openAction->setShortcut(QKeySequence::Open);
  connect(d->openAction, &QAction::triggered, this, &AppWindow::onOpenProject);

  d->saveAction = d->fileMenu->addAction(tr("保存工程"));
  d->saveAction->setShortcut(QKeySequence::Save);
  connect(d->saveAction, &QAction::triggered, this, &AppWindow::onSaveProject);

  d->saveAsAction = d->fileMenu->addAction(tr("另存为..."));
  d->saveAsAction->setShortcut(QKeySequence::SaveAs);
  connect(d->saveAsAction, &QAction::triggered, this,
          &AppWindow::onSaveProjectAs);

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

  // 导出...
  d->exportAction = d->fileMenu->addAction(tr("导出..."));
  d->exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
  connect(d->exportAction, &QAction::triggered, this,
          &AppWindow::onExportRequested);

  d->fileMenu->addSeparator();

  d->exitAction = d->fileMenu->addAction(tr("退出"));
  connect(d->exitAction, &QAction::triggered, this, &QWidget::close);

  // 编辑菜单
  d->editMenu = d->menuBar->addMenu(tr("编辑"));

  d->undoAction = d->editMenu->addAction(tr("撤销"));
  d->undoAction->setShortcut(QKeySequence::Undo);
  d->undoAction->setEnabled(false); // 预留

  d->redoAction = d->editMenu->addAction(tr("重做"));
  d->redoAction->setShortcut(QKeySequence::Redo);
  d->redoAction->setEnabled(false); // 预留

  d->editMenu->addSeparator();

  d->cutAction = d->editMenu->addAction(tr("剪切"));
  d->cutAction->setShortcut(QKeySequence::Cut);
  d->cutAction->setEnabled(false); // 预留

  d->copyAction = d->editMenu->addAction(tr("复制"));
  d->copyAction->setShortcut(QKeySequence::Copy);
  d->copyAction->setEnabled(false); // 预留

  d->pasteAction = d->editMenu->addAction(tr("粘贴"));
  d->pasteAction->setShortcut(QKeySequence::Paste);
  d->pasteAction->setEnabled(false); // 预留

  d->editMenu->addSeparator();

  d->selectAllAction = d->editMenu->addAction(tr("全选"));
  d->selectAllAction->setShortcut(QKeySequence::SelectAll);
  connect(d->selectAllAction, &QAction::triggered, this,
          &AppWindow::onSelectAll);

  d->deleteAction = d->editMenu->addAction(tr("删除选中"));
  d->deleteAction->setShortcut(QKeySequence::Delete);
  connect(d->deleteAction, &QAction::triggered, this,
          &AppWindow::onDeleteSelected);

  // 设置菜单
  d->settingsMenu = d->menuBar->addMenu(tr("设置"));

  d->configAction = d->settingsMenu->addAction(tr("配置..."));
  d->configAction->setMenuRole(QAction::NoRole);
  connect(d->configAction, &QAction::triggered, this, [this]() {
    ConfigDialog dlg(this);
    connect(&dlg, &ConfigDialog::configApplied, this,
            &AppWindow::onConfigApplied);
    dlg.exec();
  });

  d->langMenu = d->settingsMenu->addMenu(tr("语言"));
  QAction *zhAction = d->langMenu->addAction(tr("中文"));
  connect(zhAction, &QAction::triggered, this,
          []() { TranslationManager::instance().loadLanguage("zh_CN"); });
  QAction *enAction = d->langMenu->addAction("English");
  connect(enAction, &QAction::triggered, this,
          []() { TranslationManager::instance().loadLanguage("en_US"); });

  // 帮助菜单
  d->helpMenu = d->menuBar->addMenu(tr("帮助"));

  d->aboutAction = d->helpMenu->addAction(tr("关于"));
  d->aboutAction->setMenuRole(QAction::NoRole);
  connect(d->aboutAction, &QAction::triggered, this, &AppWindow::onAbout);
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
  if (d->mediaPlayer) {
    d->mediaPlayer->clear();
  }
  if (d->timelinePanel) {
    d->timelinePanel->clear();
  }
  updateTotalDuration(true);
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

    // 加载工程中关联的视频文件
    QString videoPath = d->projectManager->videoPath();
    if (!videoPath.isEmpty() && QFileInfo::exists(videoPath)) {
      if (d->mediaPlayer) {
        d->mediaPlayer->load(videoPath);
      }
      if (d->timelinePanel) {
        d->timelinePanel->setMediaFilePath(videoPath);
      }
    } else {
      // 没有视频文件时，根据字幕最大结束时间设置时间线总时长
      qint64 maxEndMs = 0;
      for (const auto &item : d->subtitleTrack->items()) {
        if (item.endMs > maxEndMs)
          maxEndMs = item.endMs;
      }
      if (maxEndMs > 0 && d->timelinePanel) {
        d->timelinePanel->setTotalDuration(maxEndMs);
      }
    }
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

  QList<QString> selectedIds;
  for (const auto &item : d->subtitleTrack->items()) {
    if (item.selected) {
      selectedIds.append(item.id);
    }
  }

  if (selectedIds.isEmpty())
    return;

  d->subtitleTrack->executeBatchAction(tr("删除选中字幕"),
                                       [this, selectedIds]() {
                                         for (const auto &id : selectedIds) {
                                           d->subtitleTrack->removeItem(id);
                                         }
                                       });
}

void AppWindow::onConfigApplied() {
  if (d->subtitleListPanel) {
    d->subtitleListPanel->updateSpeakerColumnVisibility();
  }
}

void AppWindow::onAbout() {
  AppMessageBox::information(this, tr("关于"),
                             tr("字幕编辑器 v1.0.0\n\n"
                                "一个简单易用的视频字幕编辑工具。\n\n"
                                "三方库:\n"
                                "• Qt 6 - 跨平台UI框架\n"
                                "• FFmpeg - 音视频处理\n"
                                "• QWindowKit - 自定义窗口"));
}

void AppWindow::updateWindowTitle() {
  if (!d->projectManager)
    return;
  QString name = d->projectManager->currentProjectName();
  bool dirty = d->projectManager->isDirty();
  if (name.isEmpty()) {
    setWindowTitle(tr("字幕编辑") + (dirty ? " *" : ""));
  } else {
    setWindowTitle(tr("字幕编辑 - %1").arg(name) + (dirty ? " *" : ""));
  }
}

void AppWindow::updateTotalDuration(bool resetPlayback) {
  qint64 videoDuration = (d->mediaPlayer && d->mediaPlayer->durationMs() > 0)
                             ? d->mediaPlayer->durationMs()
                             : 0;
  qint64 subtitleDuration = 0;
  if (d->subtitleTrack) {
    for (const auto &item : d->subtitleTrack->items()) {
      if (item.endMs > subtitleDuration) {
        subtitleDuration = item.endMs;
      }
    }
  }
  qint64 totalDuration = qMax(videoDuration, subtitleDuration);

  if (d->timelinePanel) {
    d->timelinePanel->setVideoDuration(videoDuration);
    d->timelinePanel->setTotalDuration(totalDuration);
  }
  if (d->subtitleListPanel) {
    d->subtitleListPanel->setTotalDuration(totalDuration);
  }

  if (d->videoPreviewPanel) {
    if (resetPlayback) {
      QSize videoSize = (d->mediaPlayer && d->mediaPlayer->durationMs() > 0)
                            ? d->mediaPlayer->videoSize()
                            : QSize();
      d->videoPreviewPanel->onMediaLoaded(totalDuration, videoSize);
    } else {
      d->videoPreviewPanel->setTotalDuration(totalDuration);
    }
  }

  if (d->mediaPlayer) {
    d->mediaPlayer->setTotalDurationLimit(totalDuration);
  }

  // 更新 Fps
  if (videoDuration <= 0) {
    if (d->timelinePanel)
      d->timelinePanel->setVideoFps(25.0);
    if (d->videoPreviewPanel)
      d->videoPreviewPanel->setVideoFps(25.0);
    if (d->subtitleListPanel)
      d->subtitleListPanel->setVideoFps(25.0);
  } else {
    double fps = d->mediaPlayer->decoderFps();
    if (d->timelinePanel)
      d->timelinePanel->setVideoFps(fps);
    if (d->videoPreviewPanel)
      d->videoPreviewPanel->setVideoFps(fps);
    if (d->subtitleListPanel)
      d->subtitleListPanel->setVideoFps(fps);
  }
}
