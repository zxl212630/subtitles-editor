#include "VideoPreviewPanel.h"

#include "AppWindow.h"
#include "MediaPlayer.h"
#include "SoftwareVideoRenderer.h"
#include "SubtitleTrack.h"

#include "SubtitleItem.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyleOption>
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

  void setDuration(qint64 totalMs) {
    totalDurationMs_ = totalMs;
    if (totalMs <= 0) {
      ratio_ = 0.0;
      update();
    }
  }

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
  setObjectName("VolumeSliderWidget");
  setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                 Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_StyledBackground);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 8, 4, 6);
  layout->setSpacing(4);

  slider_ = new QSlider(Qt::Vertical, this);
  slider_->setRange(0, 100);
  slider_->setValue(100);
  slider_->setFixedHeight(80);
  slider_->setFocusPolicy(Qt::NoFocus);
  slider_->setStyleSheet("border: none; background: transparent;");
  layout->addWidget(slider_, 0, Qt::AlignHCenter);

  label_ = new QLabel("100", this);
  label_->setAlignment(Qt::AlignCenter);
  layout->addWidget(label_, 0, Qt::AlignHCenter);

  connect(slider_, &QSlider::valueChanged, this, [this](int val) {
    label_->setText(QString::number(val));
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

  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &VolumeSliderWidget::updateTheme);
  updateTheme();
}

void VolumeSliderWidget::updateTheme() {
  QColor bg = ThemeManager::instance().getBgPanelColor();
  QColor border = ThemeManager::instance().getBorderColor();
  QColor primary = ThemeManager::instance().getPrimaryColor();
  QColor textNormal = ThemeManager::instance().getTextNormalColor();

  QString bgHex = bg.name();
  QString borderHex = border.name();
  QString primaryHex = primary.name();
  QString textHex = textNormal.name();

  setStyleSheet(QString("#VolumeSliderWidget {"
                        "  background-color: %1;"
                        "  border: 1px solid %2;"
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
                        "  background: %3;"
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
                        "}")
                    .arg(bgHex)
                    .arg(borderHex)
                    .arg(primaryHex));

  if (label_) {
    label_->setStyleSheet(
        QString("color: %1; font-size: 10px; border: none; background: "
                "transparent;")
            .arg(textHex));
  }
}

void VolumeSliderWidget::setVolume(qreal vol, bool muted) {
  int val = muted ? 0 : static_cast<int>(vol * 100.0);
  slider_->blockSignals(true);
  slider_->setValue(val);
  label_->setText(QString::number(val));
  slider_->blockSignals(false);
}

void VolumeSliderWidget::paintEvent(QPaintEvent *event) {
  QStyleOption opt;
  opt.initFrom(this);
  QPainter p(this);
  style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
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

  // 绑定视频渲染器包围框拖拽和缩放的坐标变更信号，实时写入字幕项
  connect(videoRenderer_, &SoftwareVideoRenderer::subtitleRectChanged, this,
          [this](const QRectF &rect) {
            updateCurrentItemStyle([rect](SubtitleItem &item) {
              item.rectX = rect.x();
              item.rectY = rect.y();
              item.rectW = rect.width();
              item.rectH = rect.height();
            });
          });

  // 绑定视频渲染器旋转变更信号，实时写入字幕项
  connect(videoRenderer_, &SoftwareVideoRenderer::subtitleRotationChanged, this,
          [this](double rotation) {
            updateCurrentItemStyle(
                [rotation](SubtitleItem &item) { item.rotation = rotation; });
          });

  // 绑定视频渲染器包围框拖拽和缩放的字号变更信号，写入字幕项并更新工具栏显示
  connect(videoRenderer_, &SoftwareVideoRenderer::subtitleFontSizeChanged, this,
          [this](int size) {
            updateCurrentItemStyle(
                [size](SubtitleItem &item) { item.fontSize = size; });
            sizeCombo_->blockSignals(true);
            sizeCombo_->setCurrentText(QString::number(size));
            sizeCombo_->blockSignals(false);
            updateSubtitleOverlay();
          });

  // 绑定视频渲染器点击选中信号，在当前播放时间查找并选中对应的字幕项
  connect(videoRenderer_, &SoftwareVideoRenderer::subtitleClicked, this,
          [this]() {
            if (subtitleTrack_ && mediaPlayer_) {
              qint64 currentTimeMs = mediaPlayer_->currentTimeMs();
              const auto &items = subtitleTrack_->items();
              for (int i = 0; i < items.size(); ++i) {
                const auto &item = items[i];
                bool isLast = (i == items.size() - 1);
                if (currentTimeMs >= item.startMs) {
                  if (currentTimeMs < item.endMs ||
                      (isLast && currentTimeMs == item.endMs)) {
                    subtitleTrack_->selectItem(item.id);
                    break;
                  }
                }
              }
            }
          });

  // 绑定视频渲染器字幕双击编辑完成信号，更新当前字幕文本内容
  connect(videoRenderer_, &SoftwareVideoRenderer::subtitleTextEdited, this,
          [this](const QString &text) {
            updateCurrentItemStyle(
                [text](SubtitleItem &item) { item.text = text; });
            updateSubtitleOverlay();
          });

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
            QString family = fontCombo_->currentText();
            emit fontChanged(family);
            updateCurrentItemStyle(
                [family](SubtitleItem &item) { item.fontFamily = family; });
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
      updateCurrentItemStyle(
          [size](SubtitleItem &item) { item.fontSize = size; });
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

  bBtn_ = createIconBtn(btnGroup, ":/icons/bold.svg", 28, 28);
  bBtn_->setObjectName("PreviewIconBtn");
  bBtn_->setCheckable(true);
  bBtn_->setToolTip(tr("加粗"));
  btnGroupLayout->addWidget(bBtn_);

  iBtn_ = createIconBtn(btnGroup, ":/icons/italic.svg", 28, 28);
  iBtn_->setObjectName("PreviewIconBtn");
  iBtn_->setCheckable(true);
  iBtn_->setToolTip(tr("斜体"));
  btnGroupLayout->addWidget(iBtn_);

  uBtn_ = createIconBtn(btnGroup, ":/icons/underline.svg", 28, 28);
  uBtn_->setObjectName("PreviewIconBtn");
  uBtn_->setCheckable(true);
  uBtn_->setToolTip(tr("下划线"));
  btnGroupLayout->addWidget(uBtn_);

  // 对齐按钮：改用对应的 SVG 图标并分组互斥
  alBtn_ = createIconBtn(btnGroup, ":/icons/align-left.svg", 28, 28);
  alBtn_->setObjectName("PreviewIconBtn");
  alBtn_->setCheckable(true);
  alBtn_->setToolTip(tr("左对齐"));
  btnGroupLayout->addWidget(alBtn_);

  acBtn_ = createIconBtn(btnGroup, ":/icons/align-center.svg", 28, 28);
  acBtn_->setObjectName("PreviewIconBtn");
  acBtn_->setCheckable(true);
  acBtn_->setToolTip(tr("居中对齐"));
  btnGroupLayout->addWidget(acBtn_);

  arBtn_ = createIconBtn(btnGroup, ":/icons/align-right.svg", 28, 28);
  arBtn_->setObjectName("PreviewIconBtn");
  arBtn_->setCheckable(true);
  arBtn_->setToolTip(tr("右对齐"));
  btnGroupLayout->addWidget(arBtn_);

  ajBtn_ = createIconBtn(btnGroup, ":/icons/align-justify.svg", 28, 28);
  ajBtn_->setObjectName("PreviewIconBtn");
  ajBtn_->setCheckable(true);
  ajBtn_->setToolTip(tr("分散对齐"));
  btnGroupLayout->addWidget(ajBtn_);

  // 对齐按钮互斥组
  auto *alignGroup = new QButtonGroup(this);
  alignGroup->setExclusive(true);
  alignGroup->addButton(alBtn_, Qt::AlignLeft);
  alignGroup->addButton(acBtn_, Qt::AlignHCenter);
  alignGroup->addButton(arBtn_, Qt::AlignRight);
  alignGroup->addButton(ajBtn_, Qt::AlignJustify);

  tbLayout->addWidget(btnGroup);

  // 连接样式按钮的槽函数
  connect(bBtn_, &QPushButton::clicked, this, [this](bool checked) {
    updateCurrentItemStyle(
        [checked](SubtitleItem &item) { item.bold = checked; });
    updateSubtitleOverlay();
  });
  connect(iBtn_, &QPushButton::clicked, this, [this](bool checked) {
    updateCurrentItemStyle(
        [checked](SubtitleItem &item) { item.italic = checked; });
    updateSubtitleOverlay();
  });
  connect(uBtn_, &QPushButton::clicked, this, [this](bool checked) {
    updateCurrentItemStyle(
        [checked](SubtitleItem &item) { item.underline = checked; });
    updateSubtitleOverlay();
  });
  connect(alignGroup, &QButtonGroup::idClicked, this, [this](int id) {
    updateCurrentItemStyle([id](SubtitleItem &item) { item.alignment = id; });
    updateSubtitleOverlay();
  });

  layout->addWidget(toolbar);

  // --- Video display area ---
  videoArea_ = new QFrame(this);
  videoArea_->setObjectName("PreviewVideoArea");
  videoArea_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto *vaLayout = new QVBoxLayout(videoArea_);
  vaLayout->setContentsMargins(0, 0, 0, 0);

  videoRenderer_ = new SoftwareVideoRenderer(videoArea_);
  vaLayout->addWidget(videoRenderer_);

  layout->addWidget(videoArea_, 1);

  // --- Playback control bar ---
  auto *controlBar = new QFrame(this);
  controlBar->setObjectName("PreviewControlBar");
  controlBar->setFixedHeight(36);
  auto *cbLayout = new QHBoxLayout(controlBar);
  cbLayout->setContentsMargins(8, 0, 6, 0);
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

  cbLayout->addSpacing(4);

  currentTimeLabel_ = new QLabel("00:00:00.000 / 00:00:00.000", controlBar);
  currentTimeLabel_->setObjectName("PreviewCurrentTimeLabel");
  currentTimeLabel_->setFixedWidth(190);
  currentTimeLabel_->setAlignment(Qt::AlignCenter);
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

void VideoPreviewPanel::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslateUi();
  }
  QWidget::changeEvent(event);
}

void VideoPreviewPanel::retranslateUi() {
  bBtn_->setToolTip(tr("加粗"));
  iBtn_->setToolTip(tr("斜体"));
  uBtn_->setToolTip(tr("下划线"));
  alBtn_->setToolTip(tr("左对齐"));
  acBtn_->setToolTip(tr("居中对齐"));
  arBtn_->setToolTip(tr("右对齐"));
  ajBtn_->setToolTip(tr("分散对齐"));
  volBtn_->setToolTip(tr("音量 / 静音"));
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
  if (subtitleTrack_) {
    disconnect(subtitleTrack_, nullptr, this, nullptr);
  }
  subtitleTrack_ = track;
  if (subtitleTrack_) {
    // 当切换到另一条字幕时，阻塞工具栏控件信号以更新界面状态
    connect(subtitleTrack_, &SubtitleTrack::itemSelected, this,
            [this](const QString &id) {
              Q_UNUSED(id)

              fontCombo_->blockSignals(true);
              sizeCombo_->blockSignals(true);
              bBtn_->blockSignals(true);
              iBtn_->blockSignals(true);
              uBtn_->blockSignals(true);
              alBtn_->blockSignals(true);
              acBtn_->blockSignals(true);
              arBtn_->blockSignals(true);
              ajBtn_->blockSignals(true);

              const SubtitleItem *sel = subtitleTrack_->selectedItem();
              if (sel) {
                int fontIdx = fontCombo_->findText(sel->fontFamily);
                if (fontIdx >= 0)
                  fontCombo_->setCurrentIndex(fontIdx);
                sizeCombo_->setCurrentText(QString::number(sel->fontSize));
                bBtn_->setChecked(sel->bold);
                iBtn_->setChecked(sel->italic);
                uBtn_->setChecked(sel->underline);

                if (sel->alignment & Qt::AlignLeft)
                  alBtn_->setChecked(true);
                else if (sel->alignment & Qt::AlignRight)
                  arBtn_->setChecked(true);
                else if (sel->alignment & Qt::AlignJustify)
                  ajBtn_->setChecked(true);
                else
                  acBtn_->setChecked(true);

              } else {
                // 使用全局默认模板样式
                int fontIdx =
                    fontCombo_->findText(subtitleTrack_->defaultFontFamily());
                if (fontIdx >= 0)
                  fontCombo_->setCurrentIndex(fontIdx);
                sizeCombo_->setCurrentText(
                    QString::number(subtitleTrack_->defaultFontSize()));
                bBtn_->setChecked(subtitleTrack_->defaultBold());
                iBtn_->setChecked(subtitleTrack_->defaultItalic());
                uBtn_->setChecked(subtitleTrack_->defaultUnderline());

                int align = subtitleTrack_->defaultAlignment();
                if (align & Qt::AlignLeft)
                  alBtn_->setChecked(true);
                else if (align & Qt::AlignRight)
                  arBtn_->setChecked(true);
                else if (align & Qt::AlignJustify)
                  ajBtn_->setChecked(true);
                else
                  acBtn_->setChecked(true);
              }

              fontCombo_->blockSignals(false);
              sizeCombo_->blockSignals(false);
              bBtn_->blockSignals(false);
              iBtn_->blockSignals(false);
              uBtn_->blockSignals(false);
              alBtn_->blockSignals(false);
              acBtn_->blockSignals(false);
              arBtn_->blockSignals(false);
              ajBtn_->blockSignals(false);

              updateSubtitleOverlay();
            });

    // 如果当前有选中项，手动发射一次选择信号以更新 UI 状态
    if (subtitleTrack_->selectedItem()) {
      emit subtitleTrack_->itemSelected(subtitleTrack_->selectedItem()->id);
    } else {
    }
  }
}

void VideoPreviewPanel::updateCurrentItemStyle(
    const std::function<void(SubtitleItem &)> &updateFunc) {
  if (!subtitleTrack_)
    return;

  const SubtitleItem *sel = subtitleTrack_->selectedItem();
  if (sel) {
    SubtitleItem newItem = *sel;
    updateFunc(newItem);
    // 更新选中的特定字幕项样式
    subtitleTrack_->updateItem(sel->id, newItem);
  } else {
    // 未选中时，将新样式记录为全局默认的模板样式，以便新加字幕继承该样式
    SubtitleItem dummy;
    dummy.fontFamily = subtitleTrack_->defaultFontFamily();
    dummy.fontSize = subtitleTrack_->defaultFontSize();
    dummy.bold = subtitleTrack_->defaultBold();
    dummy.italic = subtitleTrack_->defaultItalic();
    dummy.underline = subtitleTrack_->defaultUnderline();
    dummy.alignment = subtitleTrack_->defaultAlignment();
    dummy.rotation = subtitleTrack_->defaultRotation();

    QRectF defRect = subtitleTrack_->defaultSubtitleRect();
    dummy.rectX = defRect.x();
    dummy.rectY = defRect.y();
    dummy.rectW = defRect.width();
    dummy.rectH = defRect.height();

    updateFunc(dummy);

    subtitleTrack_->setDefaultFontFamily(dummy.fontFamily);
    subtitleTrack_->setDefaultFontSize(dummy.fontSize);
    subtitleTrack_->setDefaultBold(dummy.bold);
    subtitleTrack_->setDefaultItalic(dummy.italic);
    subtitleTrack_->setDefaultUnderline(dummy.underline);
    subtitleTrack_->setDefaultAlignment(dummy.alignment);
    subtitleTrack_->setDefaultSubtitleRect(
        QRectF(dummy.rectX, dummy.rectY, dummy.rectW, dummy.rectH));
    subtitleTrack_->setDefaultRotation(dummy.rotation);
    subtitleTrack_->saveGlobalSettings();
  }
}

void VideoPreviewPanel::onMediaLoaded(qint64 durationMs, QSize videoSize) {
  totalDurationMs_ = durationMs;
  if (videoRenderer_) {
    videoRenderer_->clear();
    if (videoSize.isValid()) {
      videoRenderer_->setVideoSize(videoSize);
    }
  }
  if (progressBar_) {
    progressBar_->setDuration(totalDurationMs_);
  }
  onTimeChanged(0);
  seekTo(0);
}

void VideoPreviewPanel::setTotalDuration(qint64 durationMs) {
  totalDurationMs_ = durationMs;
  if (progressBar_) {
    progressBar_->setDuration(totalDurationMs_);
  }
  onTimeChanged(mediaPlayer_ ? mediaPlayer_->currentTimeMs() : 0);
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
  if (progressBar_) {
    double ratio =
        totalDurationMs_ > 0 ? static_cast<double>(ms) / totalDurationMs_ : 0.0;
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

    if (currentTimeMs >= item.startMs) {
      if (currentTimeMs < item.endMs ||
          (isLast && currentTimeMs == item.endMs)) {
        activeItem = &item;
        break;
      }
    }
  }

  // 播放时隐藏字幕包围框虚线和控制手柄，暂停且当前显示的字幕为唯一选中项（单选）时显示
  bool showEdit = false;
  if (!isPlaying_ && subtitleTrack_ && activeItem && activeItem->selected) {
    int selectedCount = 0;
    for (const auto &item : subtitleTrack_->items()) {
      if (item.selected) {
        selectedCount++;
        if (selectedCount > 1) {
          break;
        }
      }
    }
    if (selectedCount == 1) {
      showEdit = true;
    }
  }
  videoRenderer_->setShowEditFrame(showEdit);

  if (!activeItem || activeItem->text.isEmpty()) {
    videoRenderer_->setSubtitleText(QString());
    videoRenderer_->clearSubtitleBg();
    videoRenderer_->setSubtitleRotation(0.0);
    videoRenderer_->setSubtitleStyle(subtitleTrack_->defaultStyleItem());
    return;
  }

  // Pass the full style item to the renderer
  videoRenderer_->setSubtitleStyle(*activeItem);

  // 加载说话人背景
  if (activeItem->speakerId >= 0) {
    SpeakerInfo info = subtitleTrack_->speakerInfo(activeItem->speakerId);
    QString bgFolder = subtitleTrack_->globalBgFolder();
    if (!bgFolder.isEmpty() && !info.bgImageFile.isEmpty()) {
      QString fullPath = QDir(bgFolder).filePath(info.bgImageFile);
      QMargins margins = subtitleTrack_->unifiedBorderMargins();
      videoRenderer_->setSubtitleBg(fullPath, info.is9Patch, margins);
    } else {
      videoRenderer_->clearSubtitleBg();
    }
  } else {
    videoRenderer_->clearSubtitleBg();
  }
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

  // 播放状态变更时，控制编辑虚线框隐藏/显示并更新画面
  if (videoRenderer_) {
    updateSubtitleOverlay();
  }
}

void VideoPreviewPanel::showVolumeSlider() {
  if (!volBtn_ || !sliderWidget_)
    return;
  sliderWidget_->stopHideTimer();

  QPoint globalPos = volBtn_->mapToGlobal(QPoint(0, 0));
  int w = 32;
  int h = 125;
  sliderWidget_->setFixedSize(w, h);
  int x = globalPos.x() + (volBtn_->width() - w) / 2;
  int y = globalPos.y() - h - 2;
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
