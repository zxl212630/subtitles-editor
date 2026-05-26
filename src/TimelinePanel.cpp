#include "TimelinePanel.h"
#include "AsrProgressDialog.h"
#include "AudioTranscoder.h"
#include "ConfigManager.h"
#include "CosUploader.h"
#include "OssUploader.h"
#include "QUuid"
#include "SubtitleItem.h"
#include "SubtitleTrack.h"
#include "TencentAsrService.h"
#include "ThemeManager.h"
#include <QFile>
#include <QSvgRenderer>
#include <QUndoStack>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScrollBar>
#include <QWheelEvent>

static QCursor createSvgCursor(const QString &path, int hotX, int hotY) {
  QIcon icon(path);
  // QIcon::pixmap handles devicePixelRatio automatically on supported platforms
  QPixmap pixmap = icon.pixmap(32, 32);
  return QCursor(pixmap, hotX, hotY);
}

class TimelineCanvas : public QWidget {
public:
  explicit TimelineCanvas(TimelinePanel *panel, QWidget *parent = nullptr)
      : QWidget(parent), panel_(panel) {
    setAttribute(Qt::WA_StyledBackground);
    setMouseTracking(true);
  }

protected:
  void paintEvent(QPaintEvent * /*event*/) override {
    QPainter painter(this);
    panel_->drawOnCanvas(painter);
  }

  void mousePressEvent(QMouseEvent *event) override {
    panel_->mousePressEvent(event);
  }
  void mouseMoveEvent(QMouseEvent *event) override {
    panel_->mouseMoveEvent(event);
  }
  void mouseReleaseEvent(QMouseEvent *event) override {
    panel_->mouseReleaseEvent(event);
  }

private:
  TimelinePanel *panel_;
};

TimelinePanel::TimelinePanel(QWidget *parent) : QWidget(parent) {
  setObjectName("TimelinePanel");
  setAttribute(Qt::WA_StyledBackground);
  setAcceptDrops(true);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  canvas_ = new TimelineCanvas(this, this);

  hScrollBar_ = new QScrollBar(Qt::Horizontal, this);
  hScrollBar_->setObjectName("TimelineScrollBar");
  hScrollBar_->setFixedHeight(12);
  connect(hScrollBar_, &QScrollBar::valueChanged, this, [this](int value) {
    scrollOffsetX_ = value;
    canvas_->update();
  });

  updateIcons();
  connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this]() {
            updateIcons();
            canvas_->update();
          });
}

int TimelinePanel::timeToX(qint64 ms) const {
  return TRACK_HEAD_WIDTH + static_cast<int>(ms * pixelsPerSecond_ / 1000.0) -
         scrollOffsetX_;
}

qint64 TimelinePanel::xToTime(int x) const {
  if (x < TRACK_HEAD_WIDTH)
    return 0;
  int relX = x - TRACK_HEAD_WIDTH + scrollOffsetX_;
  return static_cast<qint64>(relX * 1000.0 / pixelsPerSecond_);
}

void TimelinePanel::clampScrollOffset() {
  if (totalDurationMs_ == 0) {
    scrollOffsetX_ = 0;
    return;
  }
  int canvasWidth = canvas_->width();
  int videoPixelWidth =
      static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
  int tailMargin = qMax(0, (canvasWidth - TRACK_HEAD_WIDTH) / 3);
  int contentWidth = videoPixelWidth + tailMargin;
  int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);
  scrollOffsetX_ = qBound(0, scrollOffsetX_, maxOffset);
}

void TimelinePanel::updateScrollBar() {
  clampScrollOffset();

  if (totalDurationMs_ == 0) {
    hScrollBar_->setRange(0, 0);
    hScrollBar_->setValue(0);
    return;
  }

  int canvasWidth = canvas_->width();
  int videoPixelWidth =
      static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
  int tailMargin = qMax(0, (canvasWidth - TRACK_HEAD_WIDTH) / 3);
  int contentWidth = videoPixelWidth + tailMargin;
  int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);

  hScrollBar_->setRange(0, maxOffset);
  hScrollBar_->setPageStep(canvasWidth);
  hScrollBar_->setSingleStep(qMax(1, static_cast<int>(pixelsPerSecond_)));
  hScrollBar_->setValue(scrollOffsetX_);
}

void TimelinePanel::setTrack(SubtitleTrack *track) {
  if (track_) {
    disconnect(track_, &SubtitleTrack::dataChanged, canvas_,
               QOverload<>::of(&TimelineCanvas::update));
    disconnect(track_, &SubtitleTrack::itemSelected, canvas_,
               QOverload<>::of(&TimelineCanvas::update));
  }
  track_ = track;
  if (track_) {
    connect(track_, &SubtitleTrack::dataChanged, canvas_,
            QOverload<>::of(&TimelineCanvas::update));
    connect(track_, &SubtitleTrack::itemSelected, canvas_,
            QOverload<>::of(&TimelineCanvas::update));
  }
  canvas_->update();
}

void TimelinePanel::setCurrentTime(qint64 ms) {
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;
  currentTimeMs_ = ms;

  // Reset scroll to start when time returns to zero
  if (currentTimeMs_ == 0 && scrollOffsetX_ != 0) {
    scrollOffsetX_ = 0;
    updateScrollBar();
  }

  // Auto-scroll during playback to keep playhead at configured anchor
  // position within the viewport.
  if (isPlaying_) {
    int playheadX = timeToX(currentTimeMs_);
    int viewportWidth = canvas_->width() - TRACK_HEAD_WIDTH;
    int targetOffset = 0;
    switch (playheadAnchor_) {
    case PlayheadAnchor::LeftThird:
      targetOffset = viewportWidth / 3;
      break;
    case PlayheadAnchor::Center:
      targetOffset = viewportWidth / 2;
      break;
    case PlayheadAnchor::RightThird:
      targetOffset = viewportWidth * 2 / 3;
      break;
    }
    int targetX = TRACK_HEAD_WIDTH + targetOffset;

    if (playheadX > targetX) {
      // Playhead moved past target position, scroll left to keep it there
      scrollOffsetX_ += (playheadX - targetX);
      clampScrollOffset();
      updateScrollBar();
    } else if (playheadX < TRACK_HEAD_WIDTH) {
      // Playhead is outside viewport on the left, scroll right to bring it to
      // target
      int contentX =
          static_cast<int>(currentTimeMs_ * pixelsPerSecond_ / 1000.0);
      scrollOffsetX_ = contentX - targetOffset;
      clampScrollOffset();
      updateScrollBar();
    }
  } else {
    // Manual seek (e.g. from subtitle list) - ensure playhead is visible
    int playheadX = timeToX(currentTimeMs_);
    int viewportWidth = canvas_->width() - TRACK_HEAD_WIDTH;
    int leftLimit = TRACK_HEAD_WIDTH;
    int rightLimit = canvas_->width();

    if (playheadX < leftLimit || playheadX > rightLimit) {
      // Move scroll offset so playhead appears in the center of the viewport
      int contentX =
          static_cast<int>(currentTimeMs_ * pixelsPerSecond_ / 1000.0);
      scrollOffsetX_ = contentX - (viewportWidth / 2);
      clampScrollOffset();
      updateScrollBar();
    }
  }

  canvas_->update();
}

void TimelinePanel::setPlayheadAnchor(PlayheadAnchor anchor) {
  playheadAnchor_ = anchor;
}

void TimelinePanel::setPlaying(bool playing) { isPlaying_ = playing; }

void TimelinePanel::setVideoFps(double fps) {
  if (fps > 0.0) {
    videoFps_ = fps;
  }
}

void TimelinePanel::setMediaFilePath(const QString &path) {
  mediaFilePath_ = path;
  mediaFileName_ = QFileInfo(path).fileName();
  if (mediaFileName_.isEmpty())
    mediaFileName_ = path;
  canvas_->update();
}

void TimelinePanel::setTotalDuration(qint64 ms) {
  totalDurationMs_ = ms;
  clampScrollOffset();
  updateScrollBar();
  canvas_->update();
}

void TimelinePanel::setVideoDuration(qint64 ms) {
  videoDurationMs_ = ms;
  canvas_->update();
}

void TimelinePanel::clear() {
  mediaFileName_.clear();
  mediaFilePath_.clear();
  totalDurationMs_ = 0;
  videoDurationMs_ = 0;
  currentTimeMs_ = 0;
  scrollOffsetX_ = 0;
  isPlaying_ = false;
  if (hScrollBar_) {
    hScrollBar_->blockSignals(true);
    hScrollBar_->setValue(0);
    hScrollBar_->setRange(0, 0);
    hScrollBar_->blockSignals(false);
  }
  canvas_->update();
}

TimelinePanel::ClipInteractionMode
TimelinePanel::hitTestClip(int mouseX, int mouseY, QString *outId) const {
  int trackY = RULER_HEIGHT;
  if (mouseY < trackY || mouseY >= trackY + SUBTITLE_TRACK_HEIGHT)
    return ClipInteractionMode::Idle;
  if (mouseX < TRACK_HEAD_WIDTH)
    return ClipInteractionMode::Idle;
  if (!track_)
    return ClipInteractionMode::Idle;

  for (const auto &item : track_->items()) {
    int clipX = timeToX(item.startMs);
    int clipEndX = timeToX(item.endMs);

    if (mouseX < clipX || mouseX > clipEndX)
      continue;

    *outId = item.id;

    if (mouseX - clipX <= DRAG_EDGE_THRESHOLD_PX) {
      return ClipInteractionMode::ClipResizeLeft;
    }
    // Check if near right edge
    if (clipEndX - mouseX <= DRAG_EDGE_THRESHOLD_PX) {
      return ClipInteractionMode::ClipResizeRight;
    }
    return ClipInteractionMode::ClipMove;
  }

  return ClipInteractionMode::Idle;
}

void TimelinePanel::updateClipCursor(int mouseX, int mouseY) {
  QString id;
  ClipInteractionMode mode = hitTestClip(mouseX, mouseY, &id);
  switch (mode) {
  case ClipInteractionMode::ClipResizeLeft: {
    setCursor(createSvgCursor(":/icons/resize-left.svg", 5, 16));
    break;
  }
  case ClipInteractionMode::ClipResizeRight: {
    setCursor(createSvgCursor(":/icons/resize-right.svg", 27, 16));
    break;
  }
  default:
    unsetCursor();
    break;
  }
}

void TimelinePanel::drawOnCanvas(QPainter &painter) {
  painter.setRenderHint(QPainter::Antialiasing);

  // 校验并动态更新图标 DPR，确保在 Retina 等高分屏下始终清晰且尺寸正确
  qreal currentDpr = this->devicePixelRatioF();
  if (subIconPixmap_.isNull() ||
      subIconPixmap_.devicePixelRatio() != currentDpr) {
    updateIcons();
  }

  // Clip to rounded rect so the panel corners stay rounded
  QPainterPath clipPath;
  clipPath.addRoundedRect(canvas_->rect().adjusted(1, 1, -1, -1), 10, 10);
  painter.setClipPath(clipPath);

  // Background
  QColor bgPanel = ThemeManager::instance().getBgPanelColor();
  painter.fillRect(canvas_->rect(), bgPanel);

  drawRuler(painter);

  QColor bgLighter = ThemeManager::instance().getBgLighterColor();
  QColor textMuted = ThemeManager::instance().getTextMutedColor();
  QColor borderDark = ThemeManager::instance().getBorderDarkColor();
  QColor bgBase = ThemeManager::instance().getBgBaseColor();

  if (totalDurationMs_ == 0) {
    int subY = RULER_HEIGHT;
    int vidY = RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT;

    // Track heads only (no content background or separators)
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, subY, TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT,
                     bgLighter);
    painter.fillRect(0, vidY, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT, bgLighter);

    painter.setPen(textMuted);
    QFont font = painter.font();
    font.setPointSize(11);
    painter.setFont(font);

    // 绘制字幕轨道图标与文字
    int subIconY = subY + (SUBTITLE_TRACK_HEIGHT - 14) / 2;
    if (!subIconPixmap_.isNull()) {
      painter.drawPixmap(12, subIconY, subIconPixmap_);
    }
    int subTextX = 12 + (subIconPixmap_.isNull() ? 0 : 14 + 6);
    int subTextW =
        TRACK_HEAD_WIDTH - 12 - 12 - (subIconPixmap_.isNull() ? 0 : 14 + 6);
    painter.drawText(subTextX, subY, subTextW, SUBTITLE_TRACK_HEIGHT,
                     Qt::AlignVCenter | Qt::AlignLeft, tr("字幕"));

    // 绘制视频轨道图标与文字
    int vidIconY = vidY + (VIDEO_TRACK_HEIGHT - 14) / 2;
    if (!videoIconPixmap_.isNull()) {
      painter.drawPixmap(12, vidIconY, videoIconPixmap_);
    }
    int vidTextX = 12 + (videoIconPixmap_.isNull() ? 0 : 14 + 6);
    int vidTextW =
        TRACK_HEAD_WIDTH - 12 - 12 - (videoIconPixmap_.isNull() ? 0 : 14 + 6);
    painter.drawText(vidTextX, vidY, vidTextW, VIDEO_TRACK_HEIGHT,
                     Qt::AlignVCenter | Qt::AlignLeft, tr("视频"));

    // Separator between track heads
    painter.setPen(borderDark);
    painter.drawLine(0, vidY, TRACK_HEAD_WIDTH, vidY);

    // Unified right-side background for empty state
    painter.setPen(Qt::NoPen);
    painter.fillRect(TRACK_HEAD_WIDTH, subY,
                     canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN,
                     SUBTITLE_TRACK_HEIGHT + VIDEO_TRACK_HEIGHT, bgBase);

    drawEmptyState(painter);
    drawPlayhead(painter);
  } else {
    drawSubtitleTrack(painter, RULER_HEIGHT);
    drawVideoTrack(painter, RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT);
    drawPlayhead(painter);

    // Draw rubber band selection box if in RubberBandSelect mode and dragging
    if (clipMode_ == ClipInteractionMode::RubberBandSelect && isDragging_) {
      painter.save();
      QRect selectionRect =
          QRect(rubberBandStart_, rubberBandEnd_).normalized();
      int subTrackY = RULER_HEIGHT;
      int subTrackH = SUBTITLE_TRACK_HEIGHT;
      int top = qMax(subTrackY, selectionRect.top());
      int bottom = qMin(subTrackY + subTrackH, selectionRect.bottom());
      QRect drawRect(selectionRect.left(), top, selectionRect.width(),
                     qMax(0, bottom - top));
      if (drawRect.isValid() && drawRect.width() > 0 && drawRect.height() > 0) {
        QColor primaryColor = ThemeManager::instance().getPrimaryColor();
        QColor fillColor = QColor(primaryColor.red(), primaryColor.green(),
                                  primaryColor.blue(), 40);
        QColor strokeColor = QColor(primaryColor.red(), primaryColor.green(),
                                    primaryColor.blue(), 180);
        painter.setBrush(fillColor);
        painter.setPen(QPen(strokeColor, 1.5, Qt::DashLine));
        painter.drawRect(drawRect);
      }
      painter.restore();
    }
  }
}

void TimelinePanel::drawRuler(QPainter &painter) {
  painter.save();
  int contentWidth = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;

  QColor bgLighter = ThemeManager::instance().getBgLighterColor();
  QColor textMuted = ThemeManager::instance().getTextMutedColor();
  QColor borderDark = ThemeManager::instance().getBorderDarkColor();

  // Background for ruler
  painter.fillRect(canvas_->rect().left(), 0, canvas_->rect().width(),
                   RULER_HEIGHT, bgLighter);

  painter.setClipRect(TRACK_HEAD_WIDTH, 0, contentWidth, RULER_HEIGHT);

  painter.setPen(textMuted);
  QFont font = painter.font();
  font.setPointSize(8);
  painter.setFont(font);

  // Dynamic major interval: ensure labels don't overlap.
  // Estimate max label width (~50 px) and choose the smallest nice
  // interval that gives at least that much screen space.
  double minSpacingPx = 50.0;
  double rawInterval = minSpacingPx / pixelsPerSecond_;
  static const double NICE_INTERVALS[] = {1,  2,  5,   10,  15,
                                          30, 60, 120, 300, 600};
  double majorIntervalSec = 600.0;
  for (double v : NICE_INTERVALS) {
    if (v >= rawInterval) {
      majorIntervalSec = v;
      break;
    }
  }

  bool showHours = totalDurationMs_ >= 3600000;

  int canvasWidth = canvas_->width() - PANEL_RIGHT_MARGIN;
  int startSec = static_cast<int>(xToTime(TRACK_HEAD_WIDTH) / 1000.0);
  int endSec = static_cast<int>(xToTime(canvasWidth) / 1000.0) + 1;
  if (startSec < 0)
    startSec = 0;

  // Always 10 subdivisions between major ticks
  double minorIntervalSec = majorIntervalSec / 10.0;
  int majorMs = static_cast<int>(majorIntervalSec * 1000);
  int minorMs = static_cast<int>(minorIntervalSec * 1000);
  int halfMs = majorMs / 2;

  // ---- Draw all tick marks ----
  // Align start to the nearest minor grid to avoid missing ticks
  int firstMinorMs =
      static_cast<int>(std::ceil(startSec * 1000.0 / minorMs) * minorMs);
  for (int ms = firstMinorMs; ms <= endSec * 1000; ms += minorMs) {
    int x = timeToX(static_cast<qint64>(ms));
    if (x > canvasWidth)
      break;
    if (x < TRACK_HEAD_WIDTH)
      continue;

    int tickLen;
    int modMajor = ms % majorMs;
    int modHalf = ms % halfMs;
    if (modMajor == 0) {
      tickLen = 12; // Major tick (longest)
    } else if (modHalf == 0) {
      tickLen = 8; // Half tick (medium)
    } else {
      tickLen = 5; // Minor tick (shortest)
    }

    painter.setPen(borderDark);
    painter.drawLine(x, RULER_HEIGHT - tickLen, x, RULER_HEIGHT);
  }

  // ---- Draw labels independently so they are never skipped ----
  int firstMajorSec = static_cast<int>(std::ceil(startSec / majorIntervalSec) *
                                       majorIntervalSec);
  for (int s = firstMajorSec; s <= endSec;
       s += static_cast<int>(majorIntervalSec)) {
    int x = timeToX(static_cast<qint64>(s) * 1000);
    if (x > canvasWidth)
      break;
    if (x < TRACK_HEAD_WIDTH)
      continue;

    int sec = s % 60;
    int min = (s / 60) % 60;
    int hr = s / 3600;
    QString label;
    if (showHours) {
      label = QString("%1:%2:%3")
                  .arg(hr, 2, 10, QChar('0'))
                  .arg(min, 2, 10, QChar('0'))
                  .arg(sec, 2, 10, QChar('0'));
    } else {
      label = QString("%1:%2")
                  .arg(min, 2, 10, QChar('0'))
                  .arg(sec, 2, 10, QChar('0'));
    }
    painter.setPen(textMuted);
    painter.drawText(x - 30, 2, 60, 14, Qt::AlignCenter, label);
  }

  painter.restore();
}

void TimelinePanel::drawSubtitleTrack(QPainter &painter, int y) {
  int contentWidth = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;
  QColor bgBase = ThemeManager::instance().getBgBaseColor();
  QColor bgLighter = ThemeManager::instance().getBgLighterColor();
  QColor textMuted = ThemeManager::instance().getTextMutedColor();
  QColor borderDark = ThemeManager::instance().getBorderDarkColor();

  // Track background
  painter.fillRect(TRACK_HEAD_WIDTH, y, contentWidth, SUBTITLE_TRACK_HEIGHT,
                   bgBase);

  // Track head
  painter.setPen(Qt::NoPen);
  painter.fillRect(0, y, TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT, bgLighter);
  painter.setPen(textMuted);
  QFont font = painter.font();
  font.setPointSize(11);
  painter.setFont(font);

  // 绘制字幕轨道图标与文字
  int subIconY = y + (SUBTITLE_TRACK_HEIGHT - 14) / 2;
  if (!subIconPixmap_.isNull()) {
    painter.drawPixmap(12, subIconY, subIconPixmap_);
  }
  int subTextX = 12 + (subIconPixmap_.isNull() ? 0 : 14 + 6);
  int subTextW =
      TRACK_HEAD_WIDTH - 12 - 12 - (subIconPixmap_.isNull() ? 0 : 14 + 6);
  painter.drawText(subTextX, y, subTextW, SUBTITLE_TRACK_HEIGHT,
                   Qt::AlignVCenter | Qt::AlignLeft, tr("字幕"));

  // Separator (full width including track head)
  painter.setPen(borderDark);
  painter.drawLine(0, y + SUBTITLE_TRACK_HEIGHT - 1,
                   TRACK_HEAD_WIDTH + contentWidth,
                   y + SUBTITLE_TRACK_HEIGHT - 1);

  if (!track_)
    return;

  painter.save();
  painter.setClipRect(TRACK_HEAD_WIDTH, y, contentWidth, SUBTITLE_TRACK_HEIGHT);

  QColor primaryColor = ThemeManager::instance().getPrimaryColor();
  QColor primaryHover =
      primaryColor.lighter(110); // Simple derivation for selected state

  // Subtitle bars
  for (const auto &item : track_->items()) {
    qint64 startMs = item.startMs;
    qint64 endMs = item.endMs;
    // Use temp values if this clip is being dragged
    if (isDragging_) {
      for (const auto &dc : dragClips_) {
        if (dc.id == item.id) {
          startMs = dc.tempStartMs;
          endMs = dc.tempEndMs;
          break;
        }
      }
    }

    int x = timeToX(startMs);
    int w = timeToX(endMs) - x;
    if (w < 4)
      w = 4;
    if (x + w < TRACK_HEAD_WIDTH)
      continue;
    if (x > canvas_->width() - PANEL_RIGHT_MARGIN)
      continue;

    QColor barColor = item.selected ? primaryHover : primaryColor;
    if (item.selected) {
      QColor borderColor = ThemeManager::instance().getTextNormalColor();
      painter.setPen(QPen(borderColor, 2.0));
      painter.setBrush(barColor);
      painter.drawRoundedRect(x + 1, y + 3, w - 2, SUBTITLE_TRACK_HEIGHT - 6, 3,
                              3);
    } else {
      QColor borderColor = primaryColor.darker(120);
      painter.setPen(QPen(borderColor, 1.0));
      painter.setBrush(barColor);
      painter.drawRoundedRect(x, y + 2, w, SUBTITLE_TRACK_HEIGHT - 4, 4, 4);
    }

    painter.setPen(Qt::white);
    QFont barFont = painter.font();
    barFont.setPointSize(9);
    painter.setFont(barFont);

    int textX = qMax(x + 8, TRACK_HEAD_WIDTH + 4);
    int textMaxWidth = qMax(0, x + w - 4 - textX);
    if (textMaxWidth > 0) {
      QFontMetrics fm(barFont);
      QString elided = fm.elidedText(item.text, Qt::ElideRight, textMaxWidth);
      painter.drawText(textX, y, textMaxWidth, SUBTITLE_TRACK_HEIGHT,
                       Qt::AlignVCenter | Qt::AlignLeft, elided);
    }
  }

  painter.restore();
}

void TimelinePanel::drawVideoTrack(QPainter &painter, int y) {
  int contentWidth = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;
  QColor bgBase = ThemeManager::instance().getBgBaseColor();
  QColor bgLighter = ThemeManager::instance().getBgLighterColor();
  QColor textMuted = ThemeManager::instance().getTextMutedColor();

  painter.fillRect(TRACK_HEAD_WIDTH, y, contentWidth, VIDEO_TRACK_HEIGHT,
                   bgBase);

  painter.setPen(Qt::NoPen);
  painter.fillRect(0, y, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT, bgLighter);
  painter.setPen(textMuted);
  QFont font = painter.font();
  font.setPointSize(11);
  painter.setFont(font);

  // 绘制视频轨道图标与文字
  int vidIconY = y + (VIDEO_TRACK_HEIGHT - 14) / 2;
  if (!videoIconPixmap_.isNull()) {
    painter.drawPixmap(12, vidIconY, videoIconPixmap_);
  }
  int vidTextX = 12 + (videoIconPixmap_.isNull() ? 0 : 14 + 6);
  int vidTextW =
      TRACK_HEAD_WIDTH - 12 - 12 - (videoIconPixmap_.isNull() ? 0 : 14 + 6);
  painter.drawText(vidTextX, y, vidTextW, VIDEO_TRACK_HEIGHT,
                   Qt::AlignVCenter | Qt::AlignLeft, tr("视频"));

  painter.save();
  painter.setClipRect(TRACK_HEAD_WIDTH, y, contentWidth, VIDEO_TRACK_HEIGHT);

  // Video bar (duration-based) - only draw if a video file is loaded
  if (!mediaFilePath_.isEmpty()) {
    painter.setPen(Qt::NoPen);
    QColor videoBarColor =
        ThemeManager::instance().getPrimaryColor().darker(120);
    painter.setBrush(videoBarColor);
    int videoX = timeToX(0);
    int videoEndX = timeToX(videoDurationMs_);
    int videoWidth = videoEndX - videoX;
    if (videoWidth < 4)
      videoWidth = 4;
    painter.drawRoundedRect(videoX, y + 2, videoWidth, VIDEO_TRACK_HEIGHT - 4,
                            4, 4);
    painter.setPen(Qt::white);
    painter.drawText(TRACK_HEAD_WIDTH + 16, y, contentWidth - 32,
                     VIDEO_TRACK_HEIGHT, Qt::AlignVCenter | Qt::AlignLeft,
                     mediaFileName_);
  }

  painter.restore();
}

void TimelinePanel::drawEmptyState(QPainter &painter) {
  int contentX = TRACK_HEAD_WIDTH;
  int contentW = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;
  int centerX = contentX + contentW / 2;
  int centerY =
      RULER_HEIGHT + (SUBTITLE_TRACK_HEIGHT + VIDEO_TRACK_HEIGHT) / 2 + 5;

  int boxW = 360;
  int boxH = 100;
  QRect boxRect(centerX - boxW / 2, centerY - boxH / 2, boxW, boxH);
  emptyStateRect_ = boxRect;

  // Dashed rounded rectangle
  QPen dashPen(QColor("#4a4a4a"), 1, Qt::DashLine);
  painter.setPen(dashPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(boxRect, 8, 8);

  // Image icon (simple landscape with plus)
  int iconW = 64;
  int iconH = 48;
  int iconX = centerX - iconW / 2;
  int iconY = centerY - iconH / 2 - 14;

  // Icon background rounded rect
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#3a3a3a"));
  painter.drawRoundedRect(iconX, iconY, iconW, iconH, 6, 6);

  // Landscape line
  painter.setPen(QPen(QColor("#5a5a5a"), 2));
  painter.setBrush(Qt::NoBrush);
  painter.drawLine(iconX + 8, iconY + iconH - 10, iconX + 24, iconY + 22);
  painter.drawLine(iconX + 24, iconY + 22, iconX + 40, iconY + iconH - 16);
  painter.drawLine(iconX + 40, iconY + iconH - 16, iconX + iconW - 8,
                   iconY + 10);

  // Sun circle
  painter.setBrush(QColor("#5a5a5a"));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(iconX + 14, iconY + 8, 8, 8);

  // Plus circle in top-right corner
  int plusR = 10;
  int plusCx = iconX + iconW - 2;
  int plusCy = iconY + 2;
  painter.setBrush(QColor("#4a4a4a"));
  painter.drawEllipse(plusCx - plusR, plusCy - plusR, plusR * 2, plusR * 2);
  painter.setPen(QPen(QColor("#9ca3af"), 1));
  painter.drawLine(plusCx - 5, plusCy, plusCx + 5, plusCy);
  painter.drawLine(plusCx, plusCy - 5, plusCx, plusCy + 5);

  // Hint text inside the dashed box
  painter.setPen(QColor("#6b7280"));
  QFont hintFont = painter.font();
  hintFont.setPointSize(10);
  painter.setFont(hintFont);
  int textY = boxRect.bottom() - 28;
  painter.drawText(centerX - boxW / 2, textY, boxW, 24, Qt::AlignCenter,
                   tr("将视频和资源拖拽到此处，开始创作"));
}

void TimelinePanel::drawPlayhead(QPainter &painter) {
  int x = timeToX(currentTimeMs_);
  const int triangleTop = 19;
  const int triangleTip = 31;

  // Only draw if within visible area
  if (x < TRACK_HEAD_WIDTH || x > canvas_->width())
    return;

  // Triangle pointer below time labels
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#f59e0b"));
  QPointF triangle[3] = {QPointF(x - 7, triangleTop),
                         QPointF(x + 7, triangleTop), QPointF(x, triangleTip)};
  painter.drawPolygon(triangle, 3);

  // Vertical line stops above the scrollbar area
  painter.setPen(QColor("#f59e0b"));
  int lineBottom = canvas_->height() - 24;
  if (lineBottom > triangleTip)
    painter.drawLine(x, triangleTip, x, lineBottom);
}

void TimelinePanel::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  // Empty state import button click
  if (totalDurationMs_ == 0 && !emptyStateRect_.isEmpty() &&
      emptyStateRect_.contains(event->pos())) {
    emit importMediaRequested();
    return;
  }

  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  // Reset clip mode; will be set below if clicking on a clip
  clipMode_ = ClipInteractionMode::Idle;
  dragClips_.clear();

  // Determine interaction mode based on click position
  QString hitId;
  ClipInteractionMode mode = hitTestClip(event->x(), event->y(), &hitId);

  bool hasModifier = (event->modifiers() & Qt::ShiftModifier) ||
                     (event->modifiers() & Qt::ControlModifier) ||
                     (event->modifiers() & Qt::MetaModifier);

  if (mode == ClipInteractionMode::ClipMove ||
      mode == ClipInteractionMode::ClipResizeLeft ||
      mode == ClipInteractionMode::ClipResizeRight) {
    setFocus();
    // Start clip drag/resize
    clipMode_ = mode;
    dragTargetId_ = hitId;
    const SubtitleItem *item = track_->findItem(hitId);
    if (!item) {
      clipMode_ = ClipInteractionMode::Idle;
      return;
    }

    if (clipMode_ == ClipInteractionMode::ClipMove) {
      if (!item->selected) {
        if (hasModifier) {
          // Add to current selection
          QSet<QString> selected;
          for (const auto &it : track_->items()) {
            if (it.selected) {
              selected.insert(it.id);
            }
          }
          selected.insert(hitId);
          track_->setSelectedItems(selected);
        } else {
          // Select only this clip
          track_->selectItem(hitId);
        }
      }

      // Jump playhead pointer to the start of the clicked subtitle clip if not
      // already inside the clip range
      if (currentTimeMs_ < item->startMs || currentTimeMs_ >= item->endMs) {
        currentTimeMs_ = item->startMs;
        emit timeClicked(item->startMs);
        canvas_->update();
      }

      // Populate dragClips_ with all selected clips
      for (const auto &it : track_->items()) {
        if (it.selected) {
          DraggedClipInfo info;
          info.id = it.id;
          info.origStartMs = it.startMs;
          info.origEndMs = it.endMs;
          info.tempStartMs = it.startMs;
          info.tempEndMs = it.endMs;
          dragClips_.append(info);
        }
      }
    } else {
      // Resize modes are single-clip
      DraggedClipInfo info;
      info.id = item->id;
      info.origStartMs = item->startMs;
      info.origEndMs = item->endMs;
      info.tempStartMs = item->startMs;
      info.tempEndMs = item->endMs;
      dragClips_.append(info);
    }

    dragOrigStartMs_ = item->startMs;
    dragOrigEndMs_ = item->endMs;
    dragTempStartMs_ = item->startMs;
    dragTempEndMs_ = item->endMs;

    mousePressed_ = true;
    isDragging_ = false;
    dragStartX_ = event->x();
  } else if (event->y() < RULER_HEIGHT) {
    // Click on ruler: seek behavior (existing)
    mousePressed_ = true;
    isDragging_ = false;
    dragStartX_ = event->x();

    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;
    currentTimeMs_ = ms;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
    canvas_->update();
  } else if (event->y() >= RULER_HEIGHT) {
    // Click on empty track area: clear selection if no modifiers
    if (!hasModifier && track_) {
      track_->clearSelection();
    }
    setFocus();

    if (event->y() < RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT) {
      // Subtitle track empty space: start rubber band selection
      clipMode_ = ClipInteractionMode::RubberBandSelect;
      rubberBandStart_ = event->pos();
      rubberBandEnd_ = event->pos();

      prevSelectedIds_.clear();
      if (track_) {
        for (const auto &it : track_->items()) {
          if (it.selected) {
            prevSelectedIds_.insert(it.id);
          }
        }
      }

      mousePressed_ = true;
      isDragging_ = false;
      dragStartX_ = event->x();
    } else {
      // Video track empty area: just clear selection (done above) and redraw
      canvas_->update();
    }
  }
}

void TimelinePanel::mouseMoveEvent(QMouseEvent *event) {
  // Update cursor when not dragging
  if (!mousePressed_) {
    updateClipCursor(event->x(), event->y());
    return;
  }

  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  // Check if we've crossed the drag threshold
  if (!isDragging_) {
    if (qAbs(event->x() - dragStartX_) < DRAG_THRESHOLD_PX)
      return;
    isDragging_ = true;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
  }

  if (clipMode_ == ClipInteractionMode::RubberBandSelect) {
    rubberBandEnd_ = event->pos();
    QRect selectionRect = QRect(rubberBandStart_, rubberBandEnd_).normalized();

    QSet<QString> newlySelected;
    for (const auto &item : track_->items()) {
      int x = timeToX(item.startMs);
      int w = timeToX(item.endMs) - x;
      int y = RULER_HEIGHT + 2;
      int h = SUBTITLE_TRACK_HEIGHT - 4;
      QRect clipRect(x, y, w, h);
      if (clipRect.intersects(selectionRect)) {
        newlySelected.insert(item.id);
      }
    }

    bool hasModifier = (event->modifiers() & Qt::ShiftModifier) ||
                       (event->modifiers() & Qt::ControlModifier) ||
                       (event->modifiers() & Qt::MetaModifier);
    if (hasModifier) {
      newlySelected.unite(prevSelectedIds_);
    }

    track_->setSelectedItems(newlySelected);
    canvas_->update();
  } else if (clipMode_ == ClipInteractionMode::ClipMove ||
             clipMode_ == ClipInteractionMode::ClipResizeLeft ||
             clipMode_ == ClipInteractionMode::ClipResizeRight) {
    // --- Clip drag/resize ---
    qint64 mouseMs = xToTime(event->x());

    if (clipMode_ == ClipInteractionMode::ClipMove) {
      qint64 deltaMs = mouseMs - xToTime(dragStartX_);

      qint64 minDelta = -10000000000LL;
      qint64 maxDelta = 10000000000LL;
      for (const auto &dc : dragClips_) {
        qint64 clipMin = -dc.origStartMs;
        qint64 clipMax = totalDurationMs_ - dc.origEndMs;
        if (clipMin > minDelta)
          minDelta = clipMin;
        if (clipMax < maxDelta)
          maxDelta = clipMax;
      }

      QList<SubtitleItem> unselectedItems;
      for (const auto &item : track_->items()) {
        bool isSelected = false;
        for (const auto &dc : dragClips_) {
          if (dc.id == item.id) {
            isSelected = true;
            break;
          }
        }
        if (!isSelected) {
          unselectedItems.append(item);
        }
      }

      for (const auto &dc : dragClips_) {
        qint64 leftLimit = 0;
        for (const auto &u : unselectedItems) {
          if (u.endMs <= dc.origStartMs) {
            if (u.endMs > leftLimit) {
              leftLimit = u.endMs;
            }
          }
        }
        qint64 clipMin = leftLimit - dc.origStartMs;
        if (clipMin > minDelta)
          minDelta = clipMin;

        qint64 rightLimit = totalDurationMs_;
        for (const auto &u : unselectedItems) {
          if (u.startMs >= dc.origEndMs) {
            if (u.startMs < rightLimit) {
              rightLimit = u.startMs;
            }
          }
        }
        qint64 clipMax = rightLimit - dc.origEndMs;
        if (clipMax < maxDelta)
          maxDelta = clipMax;
      }

      if (deltaMs < minDelta)
        deltaMs = minDelta;
      if (deltaMs > maxDelta)
        deltaMs = maxDelta;

      for (auto &dc : dragClips_) {
        dc.tempStartMs = dc.origStartMs + deltaMs;
        dc.tempEndMs = dc.origEndMs + deltaMs;
      }

      for (const auto &dc : dragClips_) {
        if (dc.id == dragTargetId_) {
          dragTempStartMs_ = dc.tempStartMs;
          dragTempEndMs_ = dc.tempEndMs;
          break;
        }
      }

    } else if (clipMode_ == ClipInteractionMode::ClipResizeLeft) {
      qint64 newStart = xToTime(event->x());

      qint64 prevEnd = -1;
      for (const auto &item : track_->items()) {
        if (item.id == dragTargetId_)
          continue;
        if (item.endMs <= dragOrigStartMs_) {
          if (prevEnd < 0 || item.endMs > prevEnd)
            prevEnd = item.endMs;
        }
      }

      if (prevEnd >= 0 && newStart < prevEnd)
        newStart = prevEnd;

      if (dragOrigEndMs_ - newStart < 100)
        newStart = dragOrigEndMs_ - 100;

      if (newStart < 0)
        newStart = 0;

      for (auto &dc : dragClips_) {
        if (dc.id == dragTargetId_) {
          dc.tempStartMs = newStart;
          dc.tempEndMs = dragOrigEndMs_;
          break;
        }
      }
      dragTempStartMs_ = newStart;
      dragTempEndMs_ = dragOrigEndMs_;

    } else if (clipMode_ == ClipInteractionMode::ClipResizeRight) {
      qint64 newEnd = xToTime(event->x());

      qint64 nextStart = -1;
      for (const auto &item : track_->items()) {
        if (item.id == dragTargetId_)
          continue;
        if (item.startMs >= dragOrigEndMs_) {
          if (nextStart < 0 || item.startMs < nextStart)
            nextStart = item.startMs;
        }
      }

      if (nextStart >= 0 && newEnd > nextStart)
        newEnd = nextStart;

      if (newEnd - dragOrigStartMs_ < 100)
        newEnd = dragOrigStartMs_ + 100;

      if (newEnd > totalDurationMs_)
        newEnd = totalDurationMs_;

      for (auto &dc : dragClips_) {
        if (dc.id == dragTargetId_) {
          dc.tempStartMs = dragOrigStartMs_;
          dc.tempEndMs = newEnd;
          break;
        }
      }
      dragTempStartMs_ = dragOrigStartMs_;
      dragTempEndMs_ = newEnd;
    }

    canvas_->update();

  } else {
    // --- Original seek drag behavior ---
    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;

    currentTimeMs_ = ms;
    canvas_->update();

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 MIN_PREVIEW_INTERVAL_MS = 50;
    if (now - lastPreviewSystemTime_ >= MIN_PREVIEW_INTERVAL_MS) {
      lastPreviewSystemTime_ = now;
      emit previewSeekRequested(ms);
    }
  }
}

void TimelinePanel::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  if (isDragging_ && (clipMode_ == ClipInteractionMode::ClipMove ||
                      clipMode_ == ClipInteractionMode::ClipResizeLeft ||
                      clipMode_ == ClipInteractionMode::ClipResizeRight)) {
    // Commit clip positions: apply temp values to the track
    if (track_ && !dragClips_.isEmpty()) {
      QList<SubtitleItem> itemsToUpdate;
      for (const auto &dc : dragClips_) {
        const SubtitleItem *original = track_->findItem(dc.id);
        if (original) {
          SubtitleItem item = *original;
          item.startMs = dc.tempStartMs;
          item.endMs = dc.tempEndMs;
          item.selected = true;
          itemsToUpdate.append(item);
        }
      }
      track_->updateItems(itemsToUpdate);
    }

  } else if (isDragging_ && clipMode_ == ClipInteractionMode::Idle) {
    // Drag seek ended
    emit dragSeekFinished(currentTimeMs_);
  } else if (mousePressed_ && clipMode_ == ClipInteractionMode::Idle) {
    // Click on ruler (no drag): emit seek
    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;
    currentTimeMs_ = ms;
    emit timeClicked(ms);
    canvas_->update();
  }

  // Always reset clip interaction state on release
  clipMode_ = ClipInteractionMode::Idle;
  dragTargetId_.clear();
  dragClips_.clear();
  prevSelectedIds_.clear();

  mousePressed_ = false;
  isDragging_ = false;
  canvas_->update();
}

void TimelinePanel::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    if (track_) {
      track_->clearSelection();
    }
    event->accept();
  } else {
    QWidget::keyPressEvent(event);
  }
}

void TimelinePanel::dragEnterEvent(QDragEnterEvent *event) {
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void TimelinePanel::dropEvent(QDropEvent *event) {
  const QMimeData *mime = event->mimeData();
  if (!mime->hasUrls())
    return;

  QUrl url = mime->urls().first();
  QString localPath = url.toLocalFile();

  qDebug() << "=== TimelinePanel::dropEvent ===";
  qDebug() << "Dropped file:" << localPath;

  QString ext = QFileInfo(localPath).suffix().toLower();
  if (ext == "srt") {
    emit subtitleFileDropped(localPath);
  } else {
    emit mediaFileDropped(localPath);
  }
}

void TimelinePanel::startAsrPipeline(const QString &localPath,
                                     const QString &engineModelType,
                                     int sentenceMaxLength,
                                     bool speakerDiarization) {
  qDebug() << "=== Starting ASR Pipeline ===";
  asrCancelledByUser_ = false;

  auto *dialog = new AsrProgressDialog(this);
  dialog->setModal(true);
  dialog->setStage(AsrProgressDialog::Stage::Extraction);
  dialog->show();

  AudioTranscoder *transcoder = new AudioTranscoder(this);

  QString provider = ConfigManager::instance().storageProvider();
  QObject *uploader = nullptr;
  if (provider == "tencent_cos") {
    uploader = new CosUploader(this);
  } else {
    uploader = new OssUploader(this);
  }

  TencentAsrService *asrService = new TencentAsrService(this);
  asrService->setEngineModelType(engineModelType);
  asrService->setSentenceMaxLength(sentenceMaxLength);
  asrService->setSpeakerDiarization(speakerDiarization);

  connect(dialog, &AsrProgressDialog::canceled, this,
          [this, dialog, transcoder, uploader, asrService]() {
            qDebug() << "[ASR] canceled signal received";
            asrCancelledByUser_ = true;
            QPointer<AudioTranscoder> t(transcoder);
            QPointer<QObject> u(uploader);
            QPointer<TencentAsrService> a(asrService);
            if (t)
              t->abort();
            if (u)
              QMetaObject::invokeMethod(u, "abort");
            if (a)
              a->abort();
            qDebug() << "[ASR] cancel handler done, cleaning up";
            dialog->deleteLater();
            transcoder->deleteLater();
            if (u)
              u->deleteLater();
            asrService->deleteLater();
          });

  // Connect transcoder and uploader events
  if (provider == "tencent_cos") {
    auto *cos = qobject_cast<CosUploader *>(uploader);
    connect(transcoder, &AudioTranscoder::transcodingFinished, cos,
            &CosUploader::upload);
    connect(cos, &CosUploader::uploadFinished, this,
            [dialog, asrService](const QString &, const QString &presignedUrl) {
              dialog->setStage(AsrProgressDialog::Stage::Recognition);
              asrService->transcribe(presignedUrl);
            });
    connect(cos, &CosUploader::uploadFailed, this,
            [dialog](const QString &error) {
              qDebug() << "[ASR] uploadFailed:" << error;
              dialog->setError(tr("Upload failed: %1").arg(error));
            });
  } else {
    auto *oss = qobject_cast<OssUploader *>(uploader);
    connect(transcoder, &AudioTranscoder::transcodingFinished, oss,
            &OssUploader::upload);
    connect(oss, &OssUploader::uploadFinished, this,
            [dialog, asrService](const QString &, const QString &presignedUrl) {
              dialog->setStage(AsrProgressDialog::Stage::Recognition);
              asrService->transcribe(presignedUrl);
            });
    connect(oss, &OssUploader::uploadFailed, this,
            [dialog](const QString &error) {
              qDebug() << "[ASR] uploadFailed:" << error;
              dialog->setError(tr("Upload failed: %1").arg(error));
            });
  }

  connect(asrService, &AsrServiceBase::transcribeFinished, this,
          [this, transcoder, uploader, asrService,
           dialog](const AsrServiceBase::TranscriptResult &result) {
            qDebug() << "[ASR] transcribeFinished success=" << result.success
                     << "asrCancelledByUser_=" << asrCancelledByUser_;
            if (!result.success) {
              if (!asrCancelledByUser_) {
                qDebug() << "[ASR] real recognition error, showing on dialog";
                dialog->setError(result.errorMessage);
              } else {
                qDebug() << "[ASR] cancelled by user, closing dialog";
                dialog->deleteLater();
                transcoder->deleteLater();
                if (uploader)
                  uploader->deleteLater();
                asrService->deleteLater();
              }
            } else {
              dialog->accept();
              dialog->deleteLater();

              auto loadAction = [this, result]() {
                track_->clear();
                for (const auto &seg : result.segments) {
                  SubtitleItem item;
                  item.id = QUuid::createUuid().toString();
                  item.text = seg.text;
                  item.startMs = seg.startMs;
                  item.endMs = seg.endMs;
                  item.speakerId = seg.speakerId;
                  if (track_) {
                    track_->applyDefaultStyle(item);
                  }
                  track_->addItem(item);
                  track_->autoRegisterSpeaker(seg.speakerId);
                }
              };

              if (track_) {
                track_->executeBatchAction(tr("语音转文字"), loadAction);
              } else {
                loadAction();
              }

              emit asrSucceeded();
              transcoder->deleteLater();
              if (uploader)
                uploader->deleteLater();
              asrService->deleteLater();
            }
          });

  connect(transcoder, &AudioTranscoder::transcodingFailed, this,
          [dialog](const QString &error) {
            qDebug() << "[ASR] transcodingFailed:" << error;
            dialog->setError(tr("Transcoding failed: %1").arg(error));
          });

  transcoder->transcode(localPath);
}

void TimelinePanel::wheelEvent(QWheelEvent *event) {
  // On macOS event->modifiers() may be empty for trackpad gestures,
  // so also query the application-wide keyboard state.
  bool zoomPressed = (event->modifiers() & Qt::MetaModifier) ||
                     (QApplication::keyboardModifiers() & Qt::MetaModifier);

  if (zoomPressed) {
    // Zoom
    QPoint pos = event->position().toPoint();
    qint64 t = xToTime(pos.x());

    int delta = event->angleDelta().y();
    if (delta == 0)
      delta = event->angleDelta().x();
    if (delta == 0)
      delta = event->pixelDelta().y();

    double factor = (delta > 0) ? 1.25 : 0.8;
    double newPps = pixelsPerSecond_ * factor;
    newPps = qBound(1.0, newPps, 1000.0);

    // Adjust scroll so the time under cursor stays at the same screen X
    int cursorRelX = pos.x() - TRACK_HEAD_WIDTH;
    double newScroll = (t * newPps / 1000.0) - cursorRelX;
    scrollOffsetX_ = static_cast<int>(newScroll);

    pixelsPerSecond_ = newPps;
    clampScrollOffset();
    updateScrollBar();
    canvas_->update();
  } else {
    // Horizontal scroll
    int delta = event->angleDelta().y();
    // Support horizontal wheel / trackpad
    if (delta == 0)
      delta = event->angleDelta().x();
    if (delta == 0)
      delta = event->pixelDelta().y();
    scrollOffsetX_ -= delta;
    clampScrollOffset();
    hScrollBar_->setValue(scrollOffsetX_);
    canvas_->update();
  }
  event->accept();
}

void TimelinePanel::resizeEvent(QResizeEvent * /*event*/) {
  int sbHeight = 12;
  // Canvas covers the full panel
  canvas_->setGeometry(0, 0, width(), height());
  // Scrollbar sits on top of the canvas; keep vertical center unchanged
  // old center = height - 14/2 = height - 7
  int sbTop = height() - 7 - sbHeight / 2;
  hScrollBar_->setGeometry(TRACK_HEAD_WIDTH, sbTop,
                           width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN,
                           sbHeight);
  hScrollBar_->raise();
  updateScrollBar();
}

void TimelinePanel::contextMenuEvent(QContextMenuEvent *event) {
  int x = event->pos().x();
  if (x < TRACK_HEAD_WIDTH)
    return;

  // If there is video total duration, prevent clicking beyond the active video
  // timeline
  if (totalDurationMs_ > 0) {
    if (x > timeToX(totalDurationMs_)) {
      return;
    }
  }

  int y = event->pos().y();
  int subtitleTrackY = RULER_HEIGHT;
  int videoTrackY = RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT;

  if (y >= subtitleTrackY && y < videoTrackY) {
    // 右键点击在字幕轨道上
    if (!track_ || track_->items().isEmpty())
      return;

    QMenu menu(this);
    QAction *selectAllAction = menu.addAction(tr("全选"));
    QAction *deselectAllAction = menu.addAction(tr("取消全选"));

    QAction *selected = menu.exec(event->globalPos());
    if (selected == selectAllAction) {
      QSet<QString> allIds;
      for (const auto &item : track_->items()) {
        allIds.insert(item.id);
      }
      track_->setSelectedItems(allIds);
      canvas_->update();
    } else if (selected == deselectAllAction) {
      track_->clearSelection();
      canvas_->update();
    }
    return;
  }

  if (y >= videoTrackY && y < videoTrackY + VIDEO_TRACK_HEIGHT) {
    // 右键点击在视频轨道上
    if (totalDurationMs_ <= 0 || mediaFilePath_.isEmpty())
      return;

    QMenu menu(this);
    QAction *propAction = menu.addAction(tr("属性"));
    QAction *openLocAction = menu.addAction(tr("打开文件所在位置"));
    QAction *asrAction = menu.addAction(tr("语音转文字"));

    QAction *selected = menu.exec(event->globalPos());
    if (selected == propAction) {
      emit videoPropertyRequested();
    } else if (selected == openLocAction) {
      emit openFileLocationRequested();
    } else if (selected == asrAction) {
      emit videoAsrRequested();
    }
    return;
  }
}

void TimelinePanel::changeEvent(QEvent *event) {
  if (event->type() == QEvent::LanguageChange) {
    if (canvas_) {
      canvas_->update();
    }
  }
  QWidget::changeEvent(event);
}

void TimelinePanel::updateIcons() {
  QColor textMuted = ThemeManager::instance().getTextMutedColor();
  qreal dpr = this->devicePixelRatioF();

  auto renderSvgToPixmap = [dpr](const QString &path, const QColor &color,
                                 int size) -> QPixmap {
    QIcon icon(path);
    // 直接传入逻辑尺寸，QIcon 内部会自动处理高分屏 DPR 缩放
    QPixmap pixmap = icon.pixmap(size, size);
    if (pixmap.isNull())
      return QPixmap();

    pixmap.setDevicePixelRatio(dpr);

    QPainter tp(&pixmap);
    tp.setRenderHint(QPainter::Antialiasing);
    tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tp.fillRect(pixmap.rect(), color);
    tp.end();

    // 重新设回 DPR，确保其 devicePixelRatio 属性不被 QPainter::end() 篡改
    pixmap.setDevicePixelRatio(dpr);

    return pixmap;
  };

  subIconPixmap_ =
      renderSvgToPixmap(":/icons/track_subtitle.svg", textMuted, 14);
  videoIconPixmap_ =
      renderSvgToPixmap(":/icons/track_video.svg", textMuted, 14);
}