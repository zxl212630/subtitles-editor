#include "TimelinePanel.h"
#include "AudioTranscoder.h"
#include "OssUploader.h"
#include "QUuid"
#include "SubtitleItem.h"
#include "SubtitleTrack.h"
#include "TencentAsrService.h"

#include <QApplication>
#include <QFontDatabase>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QVBoxLayout>
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

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  canvas_ = new TimelineCanvas(this, this);
  canvas_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  layout->addWidget(canvas_);

  hScrollBar_ = new QScrollBar(Qt::Horizontal, this);
  hScrollBar_->setFixedHeight(14);
  hScrollBar_->setStyleSheet(R"(
      QScrollBar:horizontal {
          background: #1e1e1e;
          height: 14px;
          border-radius: 4px;
      }
      QScrollBar::handle:horizontal {
          background: #4a4a4a;
          border-radius: 4px;
          min-width: 20px;
      }
      QScrollBar::handle:horizontal:hover {
          background: #5a5a5a;
      }
      QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
          width: 0px;
          border: none;
      }
  )");
  layout->addWidget(hScrollBar_);

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
  int canvasWidth = canvas_->width();
  int contentWidth =
      static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
  int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);
  scrollOffsetX_ = qBound(0, scrollOffsetX_, maxOffset);
}

void TimelinePanel::updateScrollBar() {
  clampScrollOffset();

  int canvasWidth = canvas_->width();
  int contentWidth =
      static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
  int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);

  hScrollBar_->setRange(0, maxOffset);
  hScrollBar_->setPageStep(canvasWidth);
  hScrollBar_->setSingleStep(static_cast<int>(pixelsPerSecond_));
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

void TimelinePanel::setTotalDuration(qint64 ms) {
  totalDurationMs_ = ms;
  clampScrollOffset();
  updateScrollBar();
  canvas_->update();
}

void TimelinePanel::drawOnCanvas(QPainter &painter) {
  painter.setRenderHint(QPainter::Antialiasing);

  // Clip to rounded rect so border-radius works
  QPainterPath clipPath;
  clipPath.addRoundedRect(canvas_->rect().adjusted(1, 1, -1, -1), 10, 10);
  painter.setClipPath(clipPath);

  // Background
  painter.fillRect(canvas_->rect(), QColor("#1e1e1e"));

  drawRuler(painter);
  drawSubtitleTrack(painter, RULER_HEIGHT);
  drawVideoTrack(painter, RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT);
  drawPlayhead(painter);
}

void TimelinePanel::drawRuler(QPainter &painter) {
  painter.save();
  painter.setClipRect(TRACK_HEAD_WIDTH, 0, canvas_->width() - TRACK_HEAD_WIDTH,
                      RULER_HEIGHT);

  painter.setPen(QColor("#6b7280"));
  QFont font = painter.font();
  font.setPointSize(8);
  painter.setFont(font);

  // Determine tick interval based on zoom
  double majorIntervalSec = 1.0;
  double minorIntervalSec = 0.5;
  if (pixelsPerSecond_ < 20.0) {
    majorIntervalSec = 10.0;
    minorIntervalSec = 5.0;
  } else if (pixelsPerSecond_ < 50.0) {
    majorIntervalSec = 5.0;
    minorIntervalSec = 1.0;
  } else if (pixelsPerSecond_ < 100.0) {
    majorIntervalSec = 2.0;
    minorIntervalSec = 1.0;
  }

  bool showHours = totalDurationMs_ >= 3600000;

  int canvasWidth = canvas_->width();
  int startSec = static_cast<int>(xToTime(TRACK_HEAD_WIDTH) / 1000.0);
  int endSec = static_cast<int>(xToTime(canvasWidth) / 1000.0) + 1;
  if (startSec < 0)
    startSec = 0;

  for (double s = startSec; s <= endSec; s += majorIntervalSec) {
    int x = timeToX(static_cast<qint64>(s * 1000));
    if (x > canvasWidth)
      break;
    if (x < TRACK_HEAD_WIDTH)
      continue;

    int sec = static_cast<int>(s) % 60;
    int min = (static_cast<int>(s) / 60) % 60;
    int hr = static_cast<int>(s) / 3600;
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
    painter.drawText(x - 30, 8, 60, 14, Qt::AlignCenter, label);

    painter.setPen(QColor("#333333"));
    painter.drawLine(x, 24, x, 34);
    painter.setPen(QColor("#6b7280"));
  }

  // Minor ticks
  painter.setPen(QColor("#404040"));
  for (double s = startSec; s <= endSec; s += minorIntervalSec) {
    int x = timeToX(static_cast<qint64>(s * 1000));
    if (x < TRACK_HEAD_WIDTH || x > canvasWidth)
      continue;
    // Skip if too close to a major tick
    int majorTickSec =
        (static_cast<int>(s) / static_cast<int>(majorIntervalSec)) *
        static_cast<int>(majorIntervalSec);
    int majorX = timeToX(static_cast<qint64>(majorTickSec * 1000));
    if (qAbs(x - majorX) < 3)
      continue;
    painter.drawLine(x, 28, x, 31);
  }

  painter.restore();
}

void TimelinePanel::drawSubtitleTrack(QPainter &painter, int y) {
  // Track background
  painter.fillRect(TRACK_HEAD_WIDTH, y, canvas_->width() - TRACK_HEAD_WIDTH,
                   SUBTITLE_TRACK_HEIGHT, QColor("#2a2a2a"));

  // Track head
  painter.setPen(Qt::NoPen);
  painter.fillRect(0, y, TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT,
                   QColor("#262626"));
  painter.setPen(QColor("#9ca3af"));
  QFont font = painter.font();
  font.setPointSize(10);
  painter.setFont(font);
  painter.drawText(12, y + 18, "T  字幕1");

  // Separator
  painter.setPen(QColor("#333333"));
  painter.drawLine(TRACK_HEAD_WIDTH, y + SUBTITLE_TRACK_HEIGHT - 1,
                   canvas_->width(), y + SUBTITLE_TRACK_HEIGHT - 1);

  if (!track_)
    return;

  painter.save();
  painter.setClipRect(TRACK_HEAD_WIDTH, y, canvas_->width() - TRACK_HEAD_WIDTH,
                      SUBTITLE_TRACK_HEIGHT);

  // Subtitle bars
  for (const auto &item : track_->items()) {
    int x = timeToX(item.startMs);
    int w = timeToX(item.endMs) - x;
    if (w < 4)
      w = 4;
    if (x + w < TRACK_HEAD_WIDTH)
      continue;
    if (x > canvas_->width())
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
  painter.fillRect(TRACK_HEAD_WIDTH, y, canvas_->width() - TRACK_HEAD_WIDTH,
                   VIDEO_TRACK_HEIGHT, QColor("#2a2a2a"));

  painter.setPen(Qt::NoPen);
  painter.fillRect(0, y, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT,
                   QColor("#262626"));
  painter.setPen(QColor("#9ca3af"));
  QFont font = painter.font();
  font.setPointSize(10);
  painter.setFont(font);
  painter.drawText(12, y + 18, "F  视频1");

  painter.save();
  painter.setClipRect(TRACK_HEAD_WIDTH, y, canvas_->width() - TRACK_HEAD_WIDTH,
                      VIDEO_TRACK_HEIGHT);

  // Video bar (duration-based or placeholder)
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#0284c7"));
  if (totalDurationMs_ > 0) {
    int videoX = timeToX(0);
    int videoEndX = timeToX(totalDurationMs_);
    int videoWidth = videoEndX - videoX;
    if (videoWidth < 4)
      videoWidth = 4;
    painter.drawRoundedRect(videoX + 4, y + 2, videoWidth - 8,
                            VIDEO_TRACK_HEIGHT - 4, 4, 4);
  } else {
    painter.drawRoundedRect(TRACK_HEAD_WIDTH + 4, y + 2, 400,
                            VIDEO_TRACK_HEIGHT - 4, 4, 4);
  }
  painter.setPen(QColor("#e5e5e5"));
  painter.drawText(TRACK_HEAD_WIDTH + 16, y + 50, "video.mp4");

  painter.restore();
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

  // Vertical line starts from triangle tip
  painter.setPen(QColor("#f59e0b"));
  painter.drawLine(x, triangleTip, x, canvas_->height());
}

void TimelinePanel::mousePressEvent(QMouseEvent *event) {
  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  isDragging_ = true;
  dragMoved_ = false;
  dragStartX_ = event->x();
  dragThrottleTimer_.start();
  canvas_->update();
}

void TimelinePanel::mouseMoveEvent(QMouseEvent *event) {
  if (!isDragging_)
    return;

  int x = event->x();
  if (x < TRACK_HEAD_WIDTH)
    x = TRACK_HEAD_WIDTH;

  // Detect actual movement (more than 3px from start)
  if (!dragMoved_ && qAbs(x - dragStartX_) > 3) {
    dragMoved_ = true;
    emit dragSeekStarted();
  }

  if (!dragMoved_)
    return;

  // Throttle to avoid overwhelming the decoder
  if (dragThrottleTimer_.elapsed() < DRAG_THROTTLE_MS)
    return;

  qint64 ms = xToTime(x);
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;

  currentTimeMs_ = ms;
  dragThrottleTimer_.start();
  emit dragSeekMoved(ms);
  canvas_->update();
}

void TimelinePanel::mouseReleaseEvent(QMouseEvent *event) {
  Q_UNUSED(event)
  if (!isDragging_)
    return;

  isDragging_ = false;

  if (dragMoved_) {
    emit dragSeekEnded();
  } else {
    qint64 ms = xToTime(dragStartX_);
    if (ms < 0)
      ms = 0;
    if (ms > totalDurationMs_)
      ms = totalDurationMs_;
    currentTimeMs_ = ms;
    emit timeClicked(ms);
  }
  canvas_->update();
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

  emit mediaFileDropped(localPath);

  // TODO: re-enable ASR pipeline
  // startAsrPipeline(localPath);
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
    newPps = qBound(10.0, newPps, 1000.0);

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

void TimelinePanel::resizeEvent(QResizeEvent * /*event*/) { updateScrollBar(); }