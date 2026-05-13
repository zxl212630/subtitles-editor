#include "VideoPreviewPanel.h"

#include "MediaPlayer.h"
#include "SoftwareVideoRenderer.h"
#include "SubtitleTrack.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>
#include <QValidator>

#define LOG_SUB_debug(msg) qDebug() << "[SubtitleOverlay]" << msg
#define LOG_SUB(level, msg) LOG_SUB_##level(msg)

static QString formatTime(qint64 ms) {
  return QTime::fromMSecsSinceStartOfDay(static_cast<int>(ms))
      .toString("hh:mm:ss.zzz");
}

static QPushButton *createTextBtn(QWidget *parent, const QString &text, int w,
                                  int h, const QString &bg = "#333333",
                                  const QString &color = "#d1d5db") {
  auto *btn = new QPushButton(text, parent);
  btn->setFixedSize(w, h);
  btn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 4px;
            font-family: Inter, sans-serif;
            font-size: 12px;
            font-weight: bold;
        }
    )")
                         .arg(bg, color));
  return btn;
}

static QPushButton *createIconBtn(QWidget *parent, const QString &iconPath,
                                  int w, int h,
                                  const QString &hoverBg = "#333333") {
  auto *btn = new QPushButton(parent);
  btn->setFixedSize(w, h);
  btn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: transparent;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background-color: %1;
        }
        QPushButton::icon {
            width: 16px;
            height: 16px;
        }
    )")
                         .arg(hoverBg));
  if (!iconPath.isEmpty()) {
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(16, 16));
  }
  return btn;
}

VideoPreviewPanel::VideoPreviewPanel(QWidget *parent) : QWidget(parent) {
  setupUi();
}

void VideoPreviewPanel::setupUi() {
  setObjectName("VideoPreviewPanel");
  setAttribute(Qt::WA_StyledBackground);
  setStyleSheet(R"(
        QWidget#VideoPreviewPanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // --- Toolbar ---
  auto *toolbar = new QFrame(this);
  toolbar->setFixedHeight(40);
  toolbar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-top-left-radius: 10px;
            border-top-right-radius: 10px;
            border: none;
        }
    )");
  auto *tbLayout = new QHBoxLayout(toolbar);
  tbLayout->setContentsMargins(12, 0, 16, 0);
  tbLayout->setSpacing(12);
  tbLayout->setAlignment(Qt::AlignVCenter);

  // Font combo
  fontCombo_ = new QComboBox(toolbar);
  fontCombo_->setFixedSize(140, 28);
  fontCombo_->setStyleSheet(R"(
        QComboBox {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 4px;
            padding-left: 8px;
            padding-right: 20px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
            subcontrol-position: center right;
        }
        QComboBox::down-arrow {
            image: url(:/icons/down-arrow.svg);
            width: 14px;
            height: 14px;
        }
        QComboBox QAbstractItemView {
            background-color: #141414;
            color: #d1d5db;
            selection-background-color: #333333;
        }
    )");
  populateFontCombo();
  tbLayout->addWidget(fontCombo_);

  connect(fontCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() { emit fontChanged(fontCombo_->currentText()); });

  // Size combo
  sizeCombo_ = new QComboBox(toolbar);
  sizeCombo_->setFixedSize(70, 28);
  sizeCombo_->setEditable(true);
  sizeCombo_->setStyleSheet(R"(
        QComboBox {
            background-color: #141414;
            color: #d1d5db;
            border: none;
            border-radius: 4px;
            padding-left: 8px;
            padding-right: 20px;
            font-family: Inter, sans-serif;
            font-size: 12px;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
            subcontrol-position: center right;
        }
        QComboBox::down-arrow {
            image: url(:/icons/down-arrow.svg);
            width: 14px;
            height: 14px;
        }
    )");
  populateSizeCombo();
  tbLayout->addWidget(sizeCombo_);

  connect(
      sizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
      [this]() { emit fontSizeChanged(sizeCombo_->currentText().toInt()); });

  // Size input validation
  auto *validator = new QIntValidator(1, 999, sizeCombo_);
  sizeCombo_->setValidator(validator);

  // Elastic spacer
  auto *tbSpacer = new QWidget(toolbar);
  tbSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  tbSpacer->setStyleSheet("background: transparent;");
  tbLayout->addWidget(tbSpacer);

  // Format buttons group (tighter internal spacing)
  auto *btnGroup = new QWidget(toolbar);
  btnGroup->setStyleSheet("background: transparent;");
  auto *btnGroupLayout = new QHBoxLayout(btnGroup);
  btnGroupLayout->setContentsMargins(0, 0, 0, 0);
  btnGroupLayout->setSpacing(6);
  btnGroupLayout->addWidget(createTextBtn(btnGroup, "B", 28, 28));
  btnGroupLayout->addWidget(createTextBtn(btnGroup, "I", 28, 28));
  btnGroupLayout->addWidget(createTextBtn(btnGroup, "U", 28, 28));
  btnGroupLayout->addWidget(
      createTextBtn(btnGroup, QString(QChar(0x2261)), 28, 28));
  btnGroupLayout->addWidget(
      createTextBtn(btnGroup, QString(QChar(0x2261)), 28, 28));
  btnGroupLayout->addWidget(
      createTextBtn(btnGroup, QString(QChar(0x2261)), 28, 28));
  tbLayout->addWidget(btnGroup);

  layout->addWidget(toolbar);

  // --- Video display area ---
  videoArea_ = new QFrame(this);
  videoArea_->setStyleSheet("background-color: transparent; border: none;");
  videoArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *vaLayout = new QVBoxLayout(videoArea_);
  vaLayout->setContentsMargins(0, 0, 0, 0);

  videoRenderer_ = new SoftwareVideoRenderer(videoArea_);
  vaLayout->addWidget(videoRenderer_);

  // Drag handles removed / hidden for now

  layout->addWidget(videoArea_, 1);

  // --- Playback control bar ---
  auto *controlBar = new QFrame(this);
  controlBar->setFixedHeight(36);
  controlBar->setStyleSheet(R"(
        QFrame {
            background-color: #262626;
            border-bottom-left-radius: 10px;
            border-bottom-right-radius: 10px;
            border: none;
        }
    )");
  auto *cbLayout = new QHBoxLayout(controlBar);
  cbLayout->setContentsMargins(8, 0, 12, 0);
  cbLayout->setSpacing(8);
  cbLayout->setAlignment(Qt::AlignVCenter);

  stepBwdBtn_ = createIconBtn(controlBar, ":/icons/step-backward.svg", 28, 28);
  playPauseBtn_ = createIconBtn(controlBar, ":/icons/play.svg", 28, 28);
  stopBtn_ = createIconBtn(controlBar, ":/icons/stop.svg", 28, 28);
  stepFwdBtn_ = createIconBtn(controlBar, ":/icons/step-forward.svg", 28, 28);

  cbLayout->addWidget(stepBwdBtn_);
  cbLayout->addWidget(playPauseBtn_);
  cbLayout->addWidget(stopBtn_);
  cbLayout->addWidget(stepFwdBtn_);

  connect(playPauseBtn_, &QPushButton::clicked, this, [this]() {
    if (isPlaying_) {
      emit pauseRequested();
    } else {
      emit playRequested();
    }
  });
  connect(stopBtn_, &QPushButton::clicked, this,
          [this]() { emit stopRequested(); });
  connect(stepFwdBtn_, &QPushButton::clicked, this,
          [this]() { emit stepForwardRequested(); });
  connect(stepBwdBtn_, &QPushButton::clicked, this,
          [this]() { emit stepBackwardRequested(); });

  // Progress bar container
  progressContainer_ = new QFrame(controlBar);
  progressContainer_->setFixedHeight(4);
  progressContainer_->setMinimumWidth(200);
  progressContainer_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  progressContainer_->setStyleSheet(
      "background-color: #333333; border-radius: 2px;");
  progressFill_ = new QFrame(progressContainer_);
  progressFill_->setFixedSize(0, 4);
  progressFill_->setStyleSheet(
      "background-color: #38bdf8; border-radius: 2px;");
  progressFill_->move(0, 0);

  // Progress handle (draggable dot)
  progressHandle_ = new QFrame(progressContainer_);
  progressHandle_->setFixedSize(8, 8);
  progressHandle_->setStyleSheet(
      "background-color: #9ca3af; border-radius: 4px;");
  progressHandle_->move(0, -2);
  progressHandle_->raise();

  cbLayout->addWidget(progressContainer_);

  currentTimeLabel_ = new QLabel("00:00:00.000 / 00:00:00.000", controlBar);
  currentTimeLabel_->setFixedWidth(170);
  currentTimeLabel_->setStyleSheet(
      "color: #d1d5db; font-family: Inter, sans-serif; "
      "font-size: 11px; background: transparent;");
  cbLayout->addWidget(currentTimeLabel_);

  auto *volLabel = new QLabel("Vol", controlBar);
  volLabel->setFixedSize(24, 16);
  volLabel->setAlignment(Qt::AlignCenter);
  volLabel->setStyleSheet(
      "color: #d1d5db; font-family: Inter; font-size: 12px; "
      "background: transparent;");
  cbLayout->addWidget(volLabel);

  auto *fsLabel = new QLabel("FS", controlBar);
  fsLabel->setFixedSize(20, 16);
  fsLabel->setAlignment(Qt::AlignCenter);
  fsLabel->setStyleSheet("color: #d1d5db; font-family: Inter; font-size: 12px; "
                         "background: transparent;");
  cbLayout->addWidget(fsLabel);

  layout->addWidget(controlBar);
}

void VideoPreviewPanel::populateFontCombo() {
  QFontDatabase db;
  QStringList families = db.families();
  families.sort();

  for (const QString &family : families) {
    if (family.startsWith('.') || family.isEmpty())
      continue;
    fontCombo_->addItem(family);
  }

  int idx = fontCombo_->findText("Arial");
  if (idx >= 0) {
    fontCombo_->setCurrentIndex(idx);
  } else if (fontCombo_->count() > 0) {
    fontCombo_->setCurrentIndex(0);
  }
}

void VideoPreviewPanel::populateSizeCombo() {
  const QList<int> sizes = {8,  9,  10, 11, 12, 14, 16, 18, 20, 22,
                            24, 28, 32, 36, 40, 48, 56, 64, 72};
  for (int s : sizes) {
    sizeCombo_->addItem(QString::number(s));
  }
  sizeCombo_->setCurrentText("24");
}

void VideoPreviewPanel::updateHandlePositions() {
  if (!videoArea_ || handles_.size() != 8)
    return;

  int w = videoArea_->width();
  int h = videoArea_->height();
  int left = 48; // 40 padding + 8 offset
  int right = w - 48;
  int midX = w / 2 - 3;
  int top = 40;
  int midY = h / 2 - 3;
  int bottom = h - 40;

  handles_[0]->move(left, top);     // TL
  handles_[1]->move(midX, top);     // TM
  handles_[2]->move(right, top);    // TR
  handles_[3]->move(left, midY);    // ML
  handles_[4]->move(right, midY);   // MR
  handles_[5]->move(left, bottom);  // BL
  handles_[6]->move(midX, bottom);  // BM
  handles_[7]->move(right, bottom); // BR
}

void VideoPreviewPanel::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  updateHandlePositions();
}

void VideoPreviewPanel::setMediaPlayer(MediaPlayer *player) {
  if (mediaPlayer_) {
    disconnect(mediaPlayer_, nullptr, this, nullptr);
  }
  mediaPlayer_ = player;
  if (mediaPlayer_) {
    connect(mediaPlayer_, &MediaPlayer::timeChanged, this,
            &VideoPreviewPanel::onTimeChanged, Qt::QueuedConnection);
  }
}

void VideoPreviewPanel::setSubtitleTrack(SubtitleTrack *track) {
  subtitleTrack_ = track;
}

void VideoPreviewPanel::onMediaLoaded(qint64 durationMs, QSize videoSize) {
  Q_UNUSED(videoSize)
  totalDurationMs_ = durationMs;
  if (videoRenderer_) {
    videoRenderer_->clear();
  }
  onTimeChanged(0);
  seekTo(0);
}

void VideoPreviewPanel::seekTo(qint64 ms) {
  if (mediaPlayer_) {
    mediaPlayer_->seek(ms);
  }
}

void VideoPreviewPanel::onTimeChanged(qint64 ms) {
  if (currentTimeLabel_) {
    currentTimeLabel_->setText(QString("%1 / %2")
                                   .arg(formatTime(ms))
                                   .arg(formatTime(totalDurationMs_)));
  }
  if (progressFill_ && totalDurationMs_ > 0) {
    double ratio = static_cast<double>(ms) / totalDurationMs_;
    ratio = qBound(0.0, ratio, 1.0);
    int parentWidth = progressFill_->parentWidget()->width();
    progressFill_->setFixedWidth(static_cast<int>(parentWidth * ratio));
  }
  if (progressHandle_ && totalDurationMs_ > 0) {
    double ratio = static_cast<double>(ms) / totalDurationMs_;
    ratio = qBound(0.0, ratio, 1.0);
    int parentWidth = progressHandle_->parentWidget()->width();
    int handleOffset = progressHover_ ? 2 : 0;
    int handleY = progressHover_ ? -4 : -2;
    progressHandle_->move(static_cast<int>(parentWidth * ratio) - handleOffset,
                          handleY);
  }
  updateSubtitleOverlay();
}

void VideoPreviewPanel::updateSubtitleOverlay() {
  if (!subtitleTrack_ || !mediaPlayer_ || !videoRenderer_)
    return;

  qint64 currentTimeMs = mediaPlayer_->currentTimeMs();
  const SubtitleItem *activeItem = nullptr;
  for (const auto &item : subtitleTrack_->items()) {
    if (item.startMs <= currentTimeMs && currentTimeMs <= item.endMs) {
      activeItem = &item;
      break;
    }
  }

  int fontSize = qMax(1, sizeCombo_->currentText().toInt());
  QFont font(fontCombo_->currentText(), fontSize);
  videoRenderer_->setSubtitleFont(font);

  if (!activeItem || activeItem->text.isEmpty()) {
    videoRenderer_->setSubtitleText(QString());
    return;
  }

  LOG_SUB(debug,
          "active id=" << activeItem->id << " text=" << activeItem->text);
  videoRenderer_->setSubtitleText(activeItem->text);
}

void VideoPreviewPanel::onPlaybackStateChanged(MediaPlayer::State state) {
  isPlaying_ = (state == MediaPlayer::Playing);
  if (playPauseBtn_) {
    if (isPlaying_) {
      playPauseBtn_->setIcon(QIcon(":/icons/pause.svg"));
    } else {
      playPauseBtn_->setIcon(QIcon(":/icons/play.svg"));
    }
  }
}

void VideoPreviewPanel::enterEvent(QEnterEvent * /*event*/) {
  if (!progressHandle_)
    return;
  progressHover_ = true;
  progressHandle_->setFixedSize(12, 12);
  progressHandle_->setStyleSheet(
      "background-color: #ffffff; border-radius: 6px; "
      "border: 2px solid #38bdf8;");
  int currentX = progressHandle_->x();
  progressHandle_->move(currentX - 2, -4);
}

void VideoPreviewPanel::leaveEvent(QEvent * /*event*/) {
  if (!progressHandle_)
    return;
  progressHover_ = false;
  progressHandle_->setFixedSize(8, 8);
  progressHandle_->setStyleSheet(
      "background-color: #9ca3af; border-radius: 4px;");
  int currentX = progressHandle_->x();
  progressHandle_->move(currentX + 2, -2);
}

static bool isPointInProgressBar(const QFrame *container, const QPoint &pos) {
  if (!container)
    return false;
  QRect rect = container->geometry();
  rect.adjust(0, -6, 0, 6);
  return rect.contains(pos);
}

static qint64 progressPosToTime(const QFrame *container, qint64 totalDurationMs,
                                const QPoint &pos) {
  if (!container || totalDurationMs <= 0)
    return 0;
  QRect rect = container->geometry();
  int relX = pos.x() - rect.left();
  double ratio = static_cast<double>(relX) / rect.width();
  ratio = qBound(0.0, ratio, 1.0);
  return static_cast<qint64>(ratio * totalDurationMs);
}

void VideoPreviewPanel::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  if (!isPointInProgressBar(progressContainer_, event->pos()))
    return;

  progressDragging_ = true;
  qint64 ms =
      progressPosToTime(progressContainer_, totalDurationMs_, event->pos());
  if (mediaPlayer_) {
    mediaPlayer_->previewSeek(ms);
  }
}

void VideoPreviewPanel::mouseMoveEvent(QMouseEvent *event) {
  if (!progressDragging_)
    return;

  qint64 ms =
      progressPosToTime(progressContainer_, totalDurationMs_, event->pos());
  if (mediaPlayer_) {
    mediaPlayer_->previewSeek(ms);
  }
}

void VideoPreviewPanel::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;
  if (!progressDragging_)
    return;

  progressDragging_ = false;
  if (mediaPlayer_) {
    mediaPlayer_->stopPreviewDragging();
  }
}
