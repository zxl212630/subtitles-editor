#include "TimelinePanel.h"
#include "AudioTranscoder.h"
#include "OssUploader.h"
#include "QUuid"
#include "SubtitleItem.h"
#include "SubtitleTrack.h"
#include "TencentAsrService.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QWheelEvent>

class TimelineCanvas : public QWidget {
public:
  explicit TimelineCanvas(TimelinePanel *panel, QWidget *parent = nullptr)
      : QWidget(parent), panel_(panel) {
    setAttribute(Qt::WA_StyledBackground);
  }

protected:
  void paintEvent(QPaintEvent * /*event*/) override {
    QPainter painter(this);
    panel_->drawOnCanvas(painter);
  }

private:
  TimelinePanel *panel_;
};

TimelinePanel::TimelinePanel(QWidget *parent) : QWidget(parent) {
  setObjectName("TimelinePanel");
  setAttribute(Qt::WA_StyledBackground);
  setAcceptDrops(true);
  setMouseTracking(true);

  canvas_ = new TimelineCanvas(this, this);

  hScrollBar_ = new QScrollBar(Qt::Horizontal, this);
  hScrollBar_->setFixedHeight(12);
  hScrollBar_->setStyleSheet(R"(
      QScrollBar:horizontal {
          background: transparent;
          height: 14px;
          border: none;
      }
      QScrollBar::groove:horizontal {
          background: transparent;
          height: 14px;
          border: none;
      }
      QScrollBar::handle:horizontal {
          background: #4a4a4a;
          border-radius: 4px;
          min-width: 20px;
          margin: 2px 0px;
      }
      QScrollBar::handle:horizontal:hover {
          background: #5a5a5a;
      }
      QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
          width: 0px;
          border: none;
      }
      QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
          background: transparent;
          border: none;
      }
  )");

  connect(hScrollBar_, &QScrollBar::valueChanged, this, [this](int value) {
    scrollOffsetX_ = value;
    canvas_->update();
  });

  setStyleSheet(R"(
      QWidget#TimelinePanel {
          background-color: #1e1e1e;
          border-radius: 10px;
      }
  )");
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
  // position within the viewport. Do NOT auto-scroll on manual seek.
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

void TimelinePanel::drawOnCanvas(QPainter &painter) {
  painter.setRenderHint(QPainter::Antialiasing);

  // Clip to rounded rect so the panel corners stay rounded
  QPainterPath clipPath;
  clipPath.addRoundedRect(canvas_->rect().adjusted(1, 1, -1, -1), 10, 10);
  painter.setClipPath(clipPath);

  // Background
  painter.fillRect(canvas_->rect(), QColor("#1e1e1e"));

  drawRuler(painter);

  if (totalDurationMs_ == 0) {
    int subY = RULER_HEIGHT;
    int vidY = RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT;

    // Track heads only (no content background or separators)
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, subY, TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT,
                     QColor("#262626"));
    painter.fillRect(0, vidY, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT,
                     QColor("#262626"));

    painter.setPen(QColor("#9ca3af"));
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(12, subY + 18, "T  字幕1");
    painter.drawText(12, vidY + 18, "F  视频1");

    // Separator between track heads
    painter.setPen(QColor("#333333"));
    painter.drawLine(0, vidY, TRACK_HEAD_WIDTH, vidY);

    // Unified right-side background for empty state
    painter.setPen(Qt::NoPen);
    painter.fillRect(TRACK_HEAD_WIDTH, subY,
                     canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN,
                     SUBTITLE_TRACK_HEIGHT + VIDEO_TRACK_HEIGHT,
                     QColor("#2a2a2a"));

    drawEmptyState(painter);
  } else {
    drawSubtitleTrack(painter, RULER_HEIGHT);
    drawVideoTrack(painter, RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT);
    drawPlayhead(painter);
  }
}

void TimelinePanel::drawRuler(QPainter &painter) {
  painter.save();
  int contentWidth = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;
  painter.setClipRect(TRACK_HEAD_WIDTH, 0, contentWidth, RULER_HEIGHT);

  painter.setPen(QColor("#6b7280"));
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

    painter.setPen(QColor("#404040"));
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
    painter.setPen(QColor("#6b7280"));
    painter.drawText(x - 30, 2, 60, 14, Qt::AlignCenter, label);
  }

  painter.restore();
}

void TimelinePanel::drawSubtitleTrack(QPainter &painter, int y) {
  int contentWidth = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;
  // Track background
  painter.fillRect(TRACK_HEAD_WIDTH, y, contentWidth, SUBTITLE_TRACK_HEIGHT,
                   QColor("#2a2a2a"));

  // Track head
  painter.setPen(Qt::NoPen);
  painter.fillRect(0, y, TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT,
                   QColor("#262626"));
  painter.setPen(QColor("#9ca3af"));
  QFont font = painter.font();
  font.setPointSize(10);
  painter.setFont(font);
  painter.drawText(12, y + 18, "T  字幕1");

  // Separator (full width including track head)
  painter.setPen(QColor("#333333"));
  painter.drawLine(0, y + SUBTITLE_TRACK_HEIGHT - 1,
                   TRACK_HEAD_WIDTH + contentWidth,
                   y + SUBTITLE_TRACK_HEIGHT - 1);

  if (!track_)
    return;

  painter.save();
  painter.setClipRect(TRACK_HEAD_WIDTH, y, contentWidth, SUBTITLE_TRACK_HEIGHT);

  // Subtitle bars
  for (const auto &item : track_->items()) {
    int x = timeToX(item.startMs);
    int w = timeToX(item.endMs) - x;
    if (w < 4)
      w = 4;
    if (x + w < TRACK_HEAD_WIDTH)
      continue;
    if (x > canvas_->width() - PANEL_RIGHT_MARGIN)
      continue;

    QColor barColor = item.selected ? QColor("#0ea5e9") : QColor("#38bdf8");
    painter.setPen(Qt::NoPen);
    painter.setBrush(barColor);
    painter.drawRoundedRect(x, y + 2, w, SUBTITLE_TRACK_HEIGHT - 4, 4, 4);

    painter.setPen(QColor("#e5e5e5"));
    QFont barFont = painter.font();
    barFont.setPointSize(9);
    painter.setFont(barFont);

    int textX = qMax(x + 8, TRACK_HEAD_WIDTH + 4);
    int textMaxWidth = qMax(0, x + w - 4 - textX);
    if (textMaxWidth > 0) {
      QFontMetrics fm(barFont);
      QString elided = fm.elidedText(item.text, Qt::ElideRight, textMaxWidth);
      painter.drawText(textX, y + 18, elided);
    }
  }

  painter.restore();
}

void TimelinePanel::drawVideoTrack(QPainter &painter, int y) {
  int contentWidth = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;
  painter.fillRect(TRACK_HEAD_WIDTH, y, contentWidth, VIDEO_TRACK_HEIGHT,
                   QColor("#2a2a2a"));

  painter.setPen(Qt::NoPen);
  painter.fillRect(0, y, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT,
                   QColor("#262626"));
  painter.setPen(QColor("#9ca3af"));
  QFont font = painter.font();
  font.setPointSize(10);
  painter.setFont(font);
  painter.drawText(12, y + 18, "F  视频1");

  painter.save();
  painter.setClipRect(TRACK_HEAD_WIDTH, y, contentWidth, VIDEO_TRACK_HEIGHT);

  // Video bar (duration-based) - only draw if a video file is loaded
  if (!mediaFilePath_.isEmpty()) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#0284c7"));
    int videoX = timeToX(0);
    int videoEndX = timeToX(totalDurationMs_);
    int videoWidth = videoEndX - videoX;
    if (videoWidth < 4)
      videoWidth = 4;
    painter.drawRoundedRect(videoX, y + 2, videoWidth, VIDEO_TRACK_HEIGHT - 4,
                            4, 4);
    painter.setPen(QColor("#e5e5e5"));
    painter.drawText(TRACK_HEAD_WIDTH + 16, y + 50, mediaFileName_);
  }

  painter.restore();
}

void TimelinePanel::drawEmptyState(QPainter &painter) {
  int contentX = TRACK_HEAD_WIDTH;
  int contentW = canvas_->width() - TRACK_HEAD_WIDTH - PANEL_RIGHT_MARGIN;
  int centerX = contentX + contentW / 2;
  int centerY = RULER_HEIGHT + (SUBTITLE_TRACK_HEIGHT + VIDEO_TRACK_HEIGHT) / 2;

  int boxW = 360;
  int boxH = 100;
  QRect boxRect(centerX - boxW / 2, centerY - boxH / 2 - 10, boxW, boxH);
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
                   "将视频和资源拖拽到此处，开始创作");
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

  mousePressed_ = true;
  isDragging_ = false;
  dragStartX_ = event->x();

  qint64 ms = xToTime(event->x());
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;

  // Update playhead position immediately (visual feedback)
  currentTimeMs_ = ms;
  lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
  canvas_->update();
}

void TimelinePanel::mouseMoveEvent(QMouseEvent *event) {
  if (!mousePressed_)
    return;
  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  // Check if we've crossed the drag threshold
  if (!isDragging_) {
    if (qAbs(event->x() - dragStartX_) < DRAG_THRESHOLD_PX)
      return;
    isDragging_ = true;
    lastPreviewSystemTime_ = QDateTime::currentMSecsSinceEpoch();
  }

  qint64 ms = xToTime(event->x());
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;

  // Always update playhead position (instant, no delay)
  currentTimeMs_ = ms;
  canvas_->update();

  // Throttle video preview based on frame rate
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  qint64 intervalMs = static_cast<qint64>(1000.0 / videoFps_);
  if (now - lastPreviewSystemTime_ >= intervalMs) {
    lastPreviewSystemTime_ = now;
    emit previewSeekRequested(ms);
  }
}

void TimelinePanel::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton)
    return;

  if (isDragging_) {
    // Drag ended: emit signal to commit final position
    emit dragSeekFinished(currentTimeMs_);
    isDragging_ = false;
  } else {
    // Click (no drag): emit seek as before
    qint64 ms = xToTime(event->x());
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;
    currentTimeMs_ = ms;
    emit timeClicked(ms);
    canvas_->update();
  }

  mousePressed_ = false;
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

void TimelinePanel::startAsrPipeline(const QString &localPath) {
  qDebug() << "=== Starting ASR Pipeline ===";
  qDebug() << "File:" << localPath;

  // Trigger ASR pipeline
  AudioTranscoder *transcoder = new AudioTranscoder(this);
  OssUploader *uploader = new OssUploader(this);
  TencentAsrService *asrService = new TencentAsrService(this);

  // Connect pipeline
  connect(transcoder, &AudioTranscoder::transcodingFinished, uploader,
          &OssUploader::upload);
  // Use presignedUrl for ASR (public accessible URL with signature)
  connect(uploader, &OssUploader::uploadFinished, asrService,
          [asrService](const QString &, const QString &presignedUrl) {
            qDebug() << "Using presigned URL for ASR:" << presignedUrl;
            asrService->transcribe(presignedUrl);
          });
  connect(asrService, &AsrServiceBase::transcribeFinished, this,
          [this, transcoder, uploader,
           asrService](const AsrServiceBase::TranscriptResult &result) {
            if (!result.success) {
              qWarning() << "ASR failed:" << result.errorMessage;
              emit asrFailed(
                  QString("语音识别失败: %1").arg(result.errorMessage));
            } else {
              qDebug() << "=== ASR Finished ===";
              qDebug() << "Segments count:" << result.segments.size();
              track_->clear(); // 清空旧字幕数据
              for (const auto &seg : result.segments) {
                qDebug() << "Segment:" << seg.startMs << "-" << seg.endMs << ":"
                         << seg.text;
                SubtitleItem item;
                item.id = QUuid::createUuid().toString();
                item.text = seg.text;
                item.startMs = seg.startMs;
                item.endMs = seg.endMs;
                track_->addItem(item);
              }
              emit asrSucceeded();
            }
            transcoder->deleteLater();
            uploader->deleteLater();
            asrService->deleteLater();
          });

  connect(transcoder, &AudioTranscoder::transcodingFailed, this,
          [this, uploader, asrService](const QString &error) {
            qWarning() << "Transcoding failed:" << error;
            emit asrFailed(QString("转码失败: %1").arg(error));
            uploader->deleteLater();
            asrService->deleteLater();
          });

  connect(uploader, &OssUploader::uploadFailed, this,
          [this, transcoder, asrService](const QString &error) {
            qWarning() << "Upload failed:" << error;
            QString displayError = error;
            if (error.contains("AccessDenied")) {
              displayError = " OSS 上传失败：权限不足。请检查 RAM 用户的 "
                             "oss:PutObject 权限是否已授权。";
            } else if (error.contains("Connection closed")) {
              displayError =
                  " OSS 上传失败：连接被拒绝。请检查 OSS Bucket 权限设置。";
            }
            emit asrFailed(displayError);
            transcoder->deleteLater();
            asrService->deleteLater();
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
  if (totalDurationMs_ <= 0 || mediaFilePath_.isEmpty())
    return;

  int y = event->pos().y();
  int videoTrackY = RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT;
  if (y < videoTrackY || y >= videoTrackY + VIDEO_TRACK_HEIGHT)
    return;

  int x = event->pos().x();
  int videoX = timeToX(0);
  int videoEndX = timeToX(totalDurationMs_);
  if (x < videoX || x > videoEndX)
    return;

  QMenu menu(this);
  menu.setStyleSheet(R"(
      QMenu {
          background-color: #1e1e1e;
          border: 1px solid #333333;
          padding: 4px;
      }
      QMenu::item {
          color: #d1d5db;
          padding: 8px 24px;
          font-size: 13px;
      }
      QMenu::item:selected {
          background-color: #2a2a2a;
      }
  )");

  QAction *propAction = menu.addAction("属性");
  QAction *openLocAction = menu.addAction("打开文件所在位置");
  QAction *asrAction = menu.addAction("语音转文字");

  QAction *selected = menu.exec(event->globalPos());
  if (selected == propAction) {
    emit videoPropertyRequested();
  } else if (selected == openLocAction) {
    emit openFileLocationRequested();
  } else if (selected == asrAction) {
    emit videoAsrRequested();
  }
}