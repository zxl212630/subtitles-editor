#include "TimelinePanel.h"
#include "AudioTranscoder.h"
#include "OssUploader.h"
#include "QUuid"
#include "SubtitleItem.h"
#include "SubtitleTrack.h"
#include "TencentAsrService.h"

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
  int contentWidth = static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
  int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);
  scrollOffsetX_ = qBound(0, scrollOffsetX_, maxOffset);
}

void TimelinePanel::updateScrollBar() {
  clampScrollOffset();

  int canvasWidth = canvas_->width();
  int contentWidth = static_cast<int>(totalDurationMs_ * pixelsPerSecond_ / 1000.0);
  int maxOffset = qMax(0, contentWidth - canvasWidth + TRACK_HEAD_WIDTH);

  hScrollBar_->setRange(0, maxOffset);
  hScrollBar_->setPageStep(canvasWidth);
  hScrollBar_->setSingleStep(static_cast<int>(pixelsPerSecond_));
  hScrollBar_->setValue(scrollOffsetX_);
}

void TimelinePanel::setTrack(SubtitleTrack *track) {
  if (track_) {
    disconnect(track_, &SubtitleTrack::dataChanged, this,
               QOverload<>::of(&TimelinePanel::update));
    disconnect(track_, &SubtitleTrack::itemSelected, this,
               QOverload<>::of(&TimelinePanel::update));
  }
  track_ = track;
  if (track_) {
    connect(track_, &SubtitleTrack::dataChanged, this,
            QOverload<>::of(&TimelinePanel::update));
    connect(track_, &SubtitleTrack::itemSelected, this,
            QOverload<>::of(&TimelinePanel::update));
  }
  update();
}

void TimelinePanel::setCurrentTime(qint64 ms) {
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;
  currentTimeMs_ = ms;
  update();
}

void TimelinePanel::setTotalDuration(qint64 ms) {
  totalDurationMs_ = ms;
  update();
}

void TimelinePanel::drawOnCanvas(QPainter &painter) {
  painter.setRenderHint(QPainter::Antialiasing);

  // Clip to rounded rect so border-radius works with drawOnCanvas
  QPainterPath clipPath;
  clipPath.addRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);
  painter.setClipPath(clipPath);

  // Background
  painter.fillRect(rect(), QColor("#1e1e1e"));

  drawRuler(painter);
  drawSubtitleTrack(painter, RULER_HEIGHT);
  drawVideoTrack(painter, RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT);
  drawPlayhead(painter);
}

void TimelinePanel::drawRuler(QPainter &painter) {
  painter.setPen(QColor("#6b7280"));
  QFont font = painter.font();
  font.setPointSize(8);
  painter.setFont(font);

  int contentWidth = width() - TRACK_HEAD_WIDTH;
  int seconds = totalDurationMs_ / 1000;
  for (int s = 0; s <= seconds; ++s) {
    int x = TRACK_HEAD_WIDTH + s * pixelsPerSecond_;
    if (x > width())
      break;

    QString label = QString("00:00:%1:00").arg(s, 2, 10, QChar('0'));
    painter.drawText(x - 20, 8, 60, 14, Qt::AlignCenter, label);

    painter.setPen(QColor("#333333"));
    painter.drawLine(x, 24, x, 34);
    painter.setPen(QColor("#6b7280"));
  }

  // Minor ticks
  painter.setPen(QColor("#404040"));
  for (int s = 0; s < seconds; ++s) {
    int midX = TRACK_HEAD_WIDTH + s * pixelsPerSecond_ + pixelsPerSecond_ / 2;
    painter.drawLine(midX, 28, midX, 31);
  }
}

void TimelinePanel::drawSubtitleTrack(QPainter &painter, int y) {
  // Track background
  painter.fillRect(TRACK_HEAD_WIDTH, y, width() - TRACK_HEAD_WIDTH,
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
  painter.drawLine(TRACK_HEAD_WIDTH, y + SUBTITLE_TRACK_HEIGHT - 1, width(),
                   y + SUBTITLE_TRACK_HEIGHT - 1);

  if (!track_)
    return;

  // Subtitle bars
  for (const auto &item : track_->items()) {
    int x = TRACK_HEAD_WIDTH + msToPixels(item.startMs);
    int w = msToPixels(item.endMs - item.startMs);
    if (w < 4)
      w = 4;

    QColor barColor = item.selected ? QColor("#0ea5e9") : QColor("#38bdf8");
    painter.setPen(Qt::NoPen);
    painter.setBrush(barColor);
    painter.drawRoundedRect(x, y + 2, w, SUBTITLE_TRACK_HEIGHT - 4, 4, 4);

    painter.setPen(QColor("#e5e5e5"));
    QFont barFont = painter.font();
    barFont.setPointSize(9);
    painter.setFont(barFont);
    painter.drawText(x + 8, y + 18, item.text);
  }
}

void TimelinePanel::drawVideoTrack(QPainter &painter, int y) {
  painter.fillRect(TRACK_HEAD_WIDTH, y, width() - TRACK_HEAD_WIDTH,
                   VIDEO_TRACK_HEIGHT, QColor("#2a2a2a"));

  painter.setPen(Qt::NoPen);
  painter.fillRect(0, y, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT,
                   QColor("#262626"));
  painter.setPen(QColor("#9ca3af"));
  QFont font = painter.font();
  font.setPointSize(10);
  painter.setFont(font);
  painter.drawText(12, y + 18, "F  视频1");

  // Video bar (duration-based or placeholder)
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#0284c7"));
  if (totalDurationMs_ > 0) {
    int videoWidth = msToPixels(totalDurationMs_);
    painter.drawRoundedRect(TRACK_HEAD_WIDTH + 4, y + 2, videoWidth,
                            VIDEO_TRACK_HEIGHT - 4, 4, 4);
  } else {
    painter.drawRoundedRect(TRACK_HEAD_WIDTH + 4, y + 2, 400,
                            VIDEO_TRACK_HEIGHT - 4, 4, 4);
  }
  painter.setPen(QColor("#e5e5e5"));
  painter.drawText(TRACK_HEAD_WIDTH + 16, y + 50, "video.mp4");
}

void TimelinePanel::drawPlayhead(QPainter &painter) {
  int x = TRACK_HEAD_WIDTH + msToPixels(currentTimeMs_);
  const int triangleTop = 19;
  const int triangleTip = 31;

  // Triangle pointer below time labels
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#f59e0b"));
  QPointF triangle[3] = {QPointF(x - 7, triangleTop),
                         QPointF(x + 7, triangleTop), QPointF(x, triangleTip)};
  painter.drawPolygon(triangle, 3);

  // Vertical line starts from triangle tip
  painter.setPen(QColor("#f59e0b"));
  painter.drawLine(x, triangleTip, x, height());
}

void TimelinePanel::mousePressEvent(QMouseEvent *event) {
  if (event->x() < TRACK_HEAD_WIDTH)
    return;

  qint64 ms = pixelsToMs(event->x() - TRACK_HEAD_WIDTH);
  if (ms < 0)
    ms = 0;
  if (ms > totalDurationMs_)
    ms = totalDurationMs_;

  currentTimeMs_ = ms;
  emit timeClicked(ms);
  update();
}

qint64 TimelinePanel::pixelsToMs(int px) const {
  return static_cast<qint64>(px) * 1000 / pixelsPerSecond_;
}

int TimelinePanel::msToPixels(qint64 ms) const {
  return static_cast<int>(ms * pixelsPerSecond_ / 1000);
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

  startAsrPipeline(localPath);
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

void TimelinePanel::wheelEvent(QWheelEvent * /*event*/) {
  // TODO: Implement zoom/scroll wheel handling in Task 5
}

void TimelinePanel::resizeEvent(QResizeEvent * /*event*/) { updateScrollBar(); }