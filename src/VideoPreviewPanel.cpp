#include "VideoPreviewPanel.h"

#include "AppWindow.h"
#include "MediaPlayer.h"
#include "SoftwareVideoRenderer.h"
#include "SubtitleTrack.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTime>
#include <QVBoxLayout>
#include <QValidator>

#include "ThemeManager.h"

#define LOG_SUB_debug(msg) qDebug() << "[SubtitleOverlay]" << msg
#define LOG_SUB(level, msg) LOG_SUB_##level(msg)

// ------------------------------------------------------------------
// Internal progress bar widget: track + fill + draggable handle
// ------------------------------------------------------------------
class ProgressBarWidget : public QWidget {
public:
  explicit ProgressBarWidget(VideoPreviewPanel *panel,
                             QWidget *parent = nullptr)
      : QWidget(parent), panel_(panel) {
    setFixedHeight(20);
    setMinimumWidth(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this]() { update(); });
  }
  void setRatio(double ratio) {
    ratio_ = qBound(0.0, ratio, 1.0);
    update();
  }

  void setDuration(qint64 totalMs) { totalDurationMs_ = totalMs; }

  void setVideoFps(double fps) {
    if (fps > 0.0)
      videoFps_ = fps;
  }

  VideoPreviewPanel *panel_ = nullptr;

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int trackH = 4;
    int trackY = (height() - trackH) / 2;

    // Use colors from ThemeManager
    auto &tm = ThemeManager::instance();
    QColor trackBg = tm.getBorderDarkColor();
    QColor fillBg = tm.getPrimaryColor();
    QColor textMuted = tm.getTextMutedColor();
    QColor textNormal = tm.getTextNormalColor();

    // Track background
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackBg);
    painter.drawRoundedRect(0, trackY, width(), trackH, 2, 2);

    // Progress fill
    int fillW = static_cast<int>(width() * ratio_);
    if (fillW > 0) {
      painter.setBrush(fillBg);
      painter.drawRoundedRect(0, trackY, fillW, trackH, 2, 2);
    }

    // Handle
    int handleSize = hover_ ? 14 : 12;
    int handleX = static_cast<int>(width() * ratio_) - handleSize / 2;
    handleX = qBound(0, handleX, width() - handleSize);
    int handleY = (height() - handleSize) / 2;

    if (hover_ && totalDurationMs_ > 0) {
      painter.setBrush(textNormal);
      painter.drawEllipse(handleX, handleY, handleSize, handleSize);
    } else {
      painter.setBrush(textMuted);
      painter.drawEllipse(handleX, handleY, handleSize, handleSize);
    }
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (totalDurationMs_ <= 0)
      return;
    if (event->button() == Qt::LeftButton) {
      dragging_ = true;
      lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
      if (panel_)
        emit panel_->previewSeekRequested(timeFromPos(event->pos().x()));
    }
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (totalDurationMs_ <= 0)
      return;
    bool inside = rect().contains(event->pos());
    if (inside != hover_) {
      hover_ = inside;
      update();
    }
    if (dragging_ && panel_) {
      qint64 now = QDateTime::currentMSecsSinceEpoch();
      qint64 intervalMs = static_cast<qint64>(1000.0 / videoFps_);
      if (now - lastPreviewSystemTime_ >= intervalMs) {
        lastPreviewSystemTime_ = now;
        emit panel_->previewSeekRequested(timeFromPos(event->pos().x()));
      }
    }
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (totalDurationMs_ <= 0)
      return;
    if (event->button() == Qt::LeftButton && dragging_) {
      dragging_ = false;
      if (panel_)
        emit panel_->previewSeekFinished();
    }
  }

  void enterEvent(QEnterEvent *) override {
    if (totalDurationMs_ <= 0)
      return;
    if (!hover_) {
      hover_ = true;
      update();
    }
  }

  void leaveEvent(QEvent *) override {
    if (totalDurationMs_ <= 0)
      return;
    if (hover_) {
      hover_ = false;
      update();
    }
  }

private:
  qint64 timeFromPos(int x) const {
    if (width() <= 0 || totalDurationMs_ <= 0)
      return 0;
    double r = static_cast<double>(x) / width();
    r = qBound(0.0, r, 1.0);
    return static_cast<qint64>(r * totalDurationMs_);
  }

  double ratio_ = 0.0;
  qint64 totalDurationMs_ = 0;
  bool dragging_ = false;
  bool hover_ = false;
  qint64 lastPreviewSystemTime_ = 0;
  double videoFps_ = 25.0;
};

static QString formatTime(qint64 ms) {
  return QTime::fromMSecsSinceStartOfDay(static_cast<int>(ms))
      .toString("hh:mm:ss.zzz");
}

// ==================================================================
// VolumeButton Implementation
// ==================================================================
VolumeButton::VolumeButton(QWidget *parent) : QPushButton(parent) {
  setMouseTracking(true);
}

void VolumeButton::enterEvent(QEnterEvent *event) {
  emit hovered();
  QPushButton::enterEvent(event);
}

void VolumeButton::leaveEvent(QEvent *event) {
  emit unhovered();
  QPushButton::leaveEvent(event);
}

// ==================================================================
// VolumeSliderWidget Implementation
// ==================================================================
VolumeSliderWidget::VolumeSliderWidget(QWidget *parent) : QFrame(parent) {
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                 Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);

  setStyleSheet("VolumeSliderWidget {"
                "  background-color: rgba(30, 30, 30, 230);"
                "  border: 1px solid #444444;"
                "  border-radius: 6px;"
                "}"
                "QSlider::groove:vertical {"
                "  background: #333333;"
                "  width: 4px;"
                "  border-radius: 2px;"
                "}"
                "QSlider::sub-page:vertical {"
                "  background: #333333;"
                "  border-radius: 2px;"
                "}"
                "QSlider::add-page:vertical {"
                "  background: #5288c1;"
                "  border-radius: 2px;"
                "}"
                "QSlider::handle:vertical {"
                "  background: #d1d5db;"
                "  border: none;"
                "  height: 12px;"
                "  width: 12px;"
                "  margin: 0 -4px;"
                "  border-radius: 6px;"
                "}"
                "QSlider::handle:vertical:hover {"
                "  background: #ffffff;"
                "}");

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 10, 6, 10);
  layout->setSpacing(6);

  label_ = new QLabel("100%", this);
  label_->setAlignment(Qt::AlignCenter);
  label_->setStyleSheet("color: #d1d5db; font-size: 10px; border: none; "
                        "background: transparent;");
  layout->addWidget(label_, 0, Qt::AlignHCenter);

  slider_ = new QSlider(Qt::Vertical, this);
  slider_->setRange(0, 100);
  slider_->setValue(100);
  slider_->setFixedHeight(100);
  slider_->setFocusPolicy(Qt::NoFocus);
  slider_->setStyleSheet("border: none; background: transparent;");
  layout->addWidget(slider_, 0, Qt::AlignHCenter);

  connect(slider_, &QSlider::valueChanged, this, [this](int val) {
    label_->setText(QString("%1%").arg(val));
    emit volumeChanged(val / 100.0);
  });

  connect(slider_, &QSlider::sliderPressed, this, [this]() {
    isDragging_ = true;
    stopHideTimer();
  });
  connect(slider_, &QSlider::sliderReleased, this, [this]() {
    isDragging_ = false;
    if (!underMouse()) {
      startHideTimer();
    }
  });

  hideTimer_ = new QTimer(this);
  hideTimer_->setSingleShot(true);
  connect(hideTimer_, &QTimer::timeout, this, &QWidget::hide);
}

void VolumeSliderWidget::setVolume(qreal vol, bool muted) {
  int val = muted ? 0 : static_cast<int>(vol * 100.0);
  slider_->blockSignals(true);
  slider_->setValue(val);
  label_->setText(QString("%1%").arg(val));
  slider_->blockSignals(false);
}

void VolumeSliderWidget::startHideTimer() { hideTimer_->start(300); }

void VolumeSliderWidget::stopHideTimer() { hideTimer_->stop(); }

void VolumeSliderWidget::enterEvent(QEnterEvent *event) {
  stopHideTimer();
  QFrame::enterEvent(event);
}

void VolumeSliderWidget::leaveEvent(QEvent *event) {
  if (!isDragging_) {
    startHideTimer();
  }
  QFrame::leaveEvent(event);
}

static QPushButton *createTextBtn(QWidget *parent, const QString &text, int w,
                                  int h, const QString &bg = "#333333",
                                  const QString &color = "#d1d5db") {
  auto *btn = new QPushButton(text, parent);
  btn->setFixedSize(w, h);
  return btn;
}

static QPushButton *createIconBtn(QWidget *parent, const QString &iconPath,
                                  int w, int h,
                                  const QString &hoverBg = "#333333") {
  auto *btn = new QPushButton(parent);
  btn->setFixedSize(w, h);
  if (!iconPath.isEmpty()) {
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(16, 16));
  }
  return btn;
}

VideoPreviewPanel::VideoPreviewPanel(QWidget *parent) : QWidget(parent) {
  setupUi();
  if (auto *aw = qobject_cast<AppWindow *>(window())) {
    connect(aw, &AppWindow::windowClicked, this, [this](QPoint globalPos) {
      QWidget *w = QApplication::widgetAt(globalPos);
      while (w) {
        if (qobject_cast<QComboBox *>(w))
          return;
        if (QLatin1String(w->metaObject()->className())
                .contains(QLatin1String("ComboBox")))
          return;
        w = w->parentWidget();
      }
      fontCombo_->hidePopup();
      sizeCombo_->hidePopup();
    });
  }
}

void VideoPreviewPanel::setupUi() {
  setObjectName("VideoPreviewPanel");
  setAttribute(Qt::WA_StyledBackground);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // --- Toolbar ---
  auto *toolbar = new QFrame(this);
  toolbar->setObjectName("PreviewToolbar");
  toolbar->setFixedHeight(40);
  auto *tbLayout = new QHBoxLayout(toolbar);
  tbLayout->setContentsMargins(12, 0, 16, 0);
  tbLayout->setSpacing(12);
  tbLayout->setAlignment(Qt::AlignVCenter);

  // Font combo
  fontCombo_ = new QComboBox(toolbar);
  fontCombo_->setFocusPolicy(Qt::NoFocus);
  fontCombo_->setObjectName("PreviewFontCombo");
  fontCombo_->setFixedSize(140, 28);
  populateFontCombo();
  tbLayout->addWidget(fontCombo_);

  connect(fontCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() {
            emit fontChanged(fontCombo_->currentText());
            updateSubtitleOverlay();
          });

  // Size combo
  sizeCombo_ = new QComboBox(toolbar);
  sizeCombo_->setObjectName("PreviewSizeCombo");
  sizeCombo_->setFixedSize(80, 28);
  sizeCombo_->setMaxVisibleItems(10);
  sizeCombo_->setEditable(true);
  if (sizeCombo_->lineEdit()) {
    sizeCombo_->lineEdit()->setObjectName("PreviewSizeComboLineEdit");
    sizeCombo_->lineEdit()->setStyleSheet(
        "background: transparent; border: none; color: inherit; padding: 0px; "
        "margin: 0px;");
  }
  populateSizeCombo();
  tbLayout->addWidget(sizeCombo_);

  auto onSizeUpdated = [this]() {
    int size = sizeCombo_->currentText().toInt();
    if (size > 0) {
      emit fontSizeChanged(size);
      updateSubtitleOverlay();
    }
  };

  connect(sizeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          onSizeUpdated);

  if (sizeCombo_->lineEdit()) {
    connect(sizeCombo_->lineEdit(), &QLineEdit::editingFinished, this,
            [this, onSizeUpdated]() {
              onSizeUpdated();
              sizeCombo_->clearFocus();
            });
  }

  // Size input validation
  auto *validator = new QIntValidator(1, 999, sizeCombo_);
  sizeCombo_->setValidator(validator);

  // Elastic spacer
  auto *tbSpacer = new QWidget(toolbar);
  tbSpacer->setObjectName("PreviewToolbarSpacer");
  tbSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  tbLayout->addWidget(tbSpacer);

  // Format buttons group (tighter internal spacing)
  auto *btnGroup = new QWidget(toolbar);
  btnGroup->setObjectName("PreviewFormatBtnGroup");
  auto *btnGroupLayout = new QHBoxLayout(btnGroup);
  btnGroupLayout->setContentsMargins(0, 0, 0, 0);
  btnGroupLayout->setSpacing(6);
  auto *bBtn = createTextBtn(btnGroup, "B", 28, 28);
  bBtn->setObjectName("PreviewTextBtn");
  btnGroupLayout->addWidget(bBtn);

  auto *iBtn = createTextBtn(btnGroup, "I", 28, 28);
  iBtn->setObjectName("PreviewTextBtn");
  btnGroupLayout->addWidget(iBtn);

  auto *uBtn = createTextBtn(btnGroup, "U", 28, 28);
  uBtn->setObjectName("PreviewTextBtn");
  btnGroupLayout->addWidget(uBtn);

  auto *alBtn = createTextBtn(btnGroup, QString(QChar(0x2261)), 28, 28);
  alBtn->setObjectName("PreviewTextBtn");
  btnGroupLayout->addWidget(alBtn);

  auto *acBtn = createTextBtn(btnGroup, QString(QChar(0x2261)), 28, 28);
  acBtn->setObjectName("PreviewTextBtn");
  btnGroupLayout->addWidget(acBtn);

  auto *arBtn = createTextBtn(btnGroup, QString(QChar(0x2261)), 28, 28);
  arBtn->setObjectName("PreviewTextBtn");
  btnGroupLayout->addWidget(arBtn);

  tbLayout->addWidget(btnGroup);
  btnGroup->hide();

  layout->addWidget(toolbar);

  // --- Video display area ---
  videoArea_ = new QFrame(this);
  videoArea_->setObjectName("PreviewVideoArea");
  videoArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *vaLayout = new QVBoxLayout(videoArea_);
  vaLayout->setContentsMargins(0, 0, 0, 0);

  videoRenderer_ = new SoftwareVideoRenderer(videoArea_);
  vaLayout->addWidget(videoRenderer_);

  // Drag handles removed / hidden for now

  layout->addWidget(videoArea_, 1);

  // --- Playback control bar ---
  auto *controlBar = new QFrame(this);
  controlBar->setObjectName("PreviewControlBar");
  controlBar->setFixedHeight(36);
  auto *cbLayout = new QHBoxLayout(controlBar);
  cbLayout->setContentsMargins(8, 0, 12, 0);
  cbLayout->setSpacing(8);
  cbLayout->setAlignment(Qt::AlignVCenter);

  stepBwdBtn_ = createIconBtn(controlBar, ":/icons/step-backward.svg", 28, 28);
  stepBwdBtn_->setObjectName("PreviewIconBtn");
  playPauseBtn_ = createIconBtn(controlBar, ":/icons/play.svg", 28, 28);
  playPauseBtn_->setObjectName("PreviewIconBtn");
  stopBtn_ = createIconBtn(controlBar, ":/icons/stop.svg", 28, 28);
  stopBtn_->setObjectName("PreviewIconBtn");
  stepFwdBtn_ = createIconBtn(controlBar, ":/icons/step-forward.svg", 28, 28);
  stepFwdBtn_->setObjectName("PreviewIconBtn");

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

  // Progress bar
  progressBar_ = new ProgressBarWidget(this, controlBar);
  progressBar_->setObjectName("PreviewProgressBar");
  cbLayout->addWidget(progressBar_);

  currentTimeLabel_ = new QLabel("00:00:00.000 / 00:00:00.000", controlBar);
  currentTimeLabel_->setObjectName("PreviewCurrentTimeLabel");
  currentTimeLabel_->setFixedWidth(170);
  cbLayout->addWidget(currentTimeLabel_);

  volBtn_ = new VolumeButton(controlBar);
  volBtn_->setObjectName("PreviewIconBtn");
  volBtn_->setFixedSize(24, 24);
  volBtn_->setIcon(QIcon(":/icons/volume.svg"));
  volBtn_->setIconSize(QSize(16, 16));
  volBtn_->setToolTip(tr("音量 / 静音"));
  cbLayout->addWidget(volBtn_);

  sliderWidget_ = new VolumeSliderWidget(this);
  sliderWidget_->hide();

  connect(volBtn_, &VolumeButton::hovered, this,
          &VideoPreviewPanel::showVolumeSlider);
  connect(volBtn_, &VolumeButton::unhovered, this,
          &VideoPreviewPanel::hideVolumeSliderDeferred);
  connect(volBtn_, &QPushButton::clicked, this, &VideoPreviewPanel::toggleMute);
  connect(sliderWidget_, &VolumeSliderWidget::volumeChanged, this,
          [this](qreal vol) {
            if (mediaPlayer_) {
              mediaPlayer_->setVolume(vol);
            }
          });

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

  if (auto *view = fontCombo_->view()) {
    view->setMinimumWidth(220);
    if (QWidget *w = view->window()) {
      w->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                        Qt::NoDropShadowWindowHint);
      w->setAttribute(Qt::WA_TranslucentBackground);
    }
  }
}

void VideoPreviewPanel::populateSizeCombo() {
  const QList<int> sizes = {8,  9,  10, 11, 12, 14, 16, 18, 20, 22,
                            24, 28, 32, 36, 40, 48, 56, 64, 72};
  for (int s : sizes) {
    sizeCombo_->addItem(QString::number(s));
  }
  sizeCombo_->setCurrentText("24");

  if (auto *view = sizeCombo_->view()) {
    if (QWidget *w = view->window()) {
      w->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                        Qt::NoDropShadowWindowHint);
      w->setAttribute(Qt::WA_TranslucentBackground);
    }
  }
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

    connect(mediaPlayer_, &MediaPlayer::volumeChanged, this,
            [this](qreal vol, bool muted) {
              if (volBtn_) {
                volBtn_->setIcon(QIcon(muted ? ":/icons/volume-mute.svg"
                                             : ":/icons/volume.svg"));
              }
              if (sliderWidget_) {
                sliderWidget_->setVolume(vol, muted);
              }
            });

    if (volBtn_) {
      volBtn_->setIcon(QIcon(mediaPlayer_->isMuted() ? ":/icons/volume-mute.svg"
                                                     : ":/icons/volume.svg"));
    }
    if (sliderWidget_) {
      sliderWidget_->setVolume(mediaPlayer_->volume(), mediaPlayer_->isMuted());
    }
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
  if (progressBar_) {
    progressBar_->setDuration(totalDurationMs_);
  }
  onTimeChanged(0);
  seekTo(0);
}

void VideoPreviewPanel::seekTo(qint64 ms) {
  if (mediaPlayer_) {
    mediaPlayer_->seek(ms);
  }
}

void VideoPreviewPanel::setVideoFps(double fps) {
  if (progressBar_)
    progressBar_->setVideoFps(fps);
}

void VideoPreviewPanel::onTimeChanged(qint64 ms) {
  if (currentTimeLabel_) {
    currentTimeLabel_->setText(QString("%1 / %2")
                                   .arg(formatTime(ms))
                                   .arg(formatTime(totalDurationMs_)));
  }
  if (progressBar_ && totalDurationMs_ > 0) {
    double ratio = static_cast<double>(ms) / totalDurationMs_;
    progressBar_->setRatio(ratio);
  }
  updateSubtitleOverlay();
}

void VideoPreviewPanel::updateSubtitleOverlay() {
  if (!subtitleTrack_ || !mediaPlayer_ || !videoRenderer_)
    return;

  qint64 currentTimeMs = mediaPlayer_->currentTimeMs();
  const SubtitleItem *activeItem = nullptr;
  const auto &items = subtitleTrack_->items();

  for (int i = 0; i < items.size(); ++i) {
    const auto &item = items[i];
    bool isLast = (i == items.size() - 1);

    // Left-closed, right-open: [start, end)
    // Exception: last item is visible at exactly endMs
    if (currentTimeMs >= item.startMs) {
      if (currentTimeMs < item.endMs ||
          (isLast && currentTimeMs == item.endMs)) {
        activeItem = &item;
        break;
      }
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

void VideoPreviewPanel::showVolumeSlider() {
  if (!volBtn_ || !sliderWidget_)
    return;
  sliderWidget_->stopHideTimer();

  QPoint globalPos = volBtn_->mapToGlobal(QPoint(0, 0));
  int w = 40;
  int h = 150;
  sliderWidget_->setFixedSize(w, h);
  int x = globalPos.x() + (volBtn_->width() - w) / 2;
  int y = globalPos.y() - h - 4;
  sliderWidget_->move(x, y);

  sliderWidget_->show();
}

void VideoPreviewPanel::hideVolumeSliderDeferred() {
  if (sliderWidget_) {
    sliderWidget_->startHideTimer();
  }
}

void VideoPreviewPanel::toggleMute() {
  if (mediaPlayer_) {
    mediaPlayer_->setMuted(!mediaPlayer_->isMuted());
  }
}

// End of VideoPreviewPanel implementation
