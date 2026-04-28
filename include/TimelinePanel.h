#pragma once

#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QWidget>

#include "AudioTranscoder.h"
#include "OssUploader.h"
#include "SubtitleItem.h"
#include "TencentAsrService.h"

class SubtitleTrack;
class TimelineCanvas;

class TimelinePanel : public QWidget {
  Q_OBJECT

public:
  explicit TimelinePanel(QWidget *parent = nullptr);

  void setTrack(SubtitleTrack *track);
  void setCurrentTime(qint64 ms);
  void setTotalDuration(qint64 ms);

signals:
  void timeClicked(qint64 ms);
  void itemSelected(const QString &id);
  void asrFailed(const QString &error);
  void asrSucceeded();
  void mediaFileDropped(const QString &path);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void drawRuler(QPainter &painter);
  void drawSubtitleTrack(QPainter &painter, int y);
  void drawVideoTrack(QPainter &painter, int y);
  void drawPlayhead(QPainter &painter);
  void startAsrPipeline(const QString &localPath);

  int timeToX(qint64 ms) const;
  qint64 xToTime(int x) const;
  void updateScrollBar();
  void clampScrollOffset();
  void drawOnCanvas(QPainter &painter);

  SubtitleTrack *track_ = nullptr;
  qint64 totalDurationMs_ = 0;
  qint64 currentTimeMs_ = 0;
  static constexpr int RULER_HEIGHT = 36;
  static constexpr int SUBTITLE_TRACK_HEIGHT = 48;
  static constexpr int VIDEO_TRACK_HEIGHT = 96;
  static constexpr int TRACK_HEAD_WIDTH = 120;

  double pixelsPerSecond_ = 100.0;
  int scrollOffsetX_ = 0;
  QScrollBar *hScrollBar_ = nullptr;
  TimelineCanvas *canvas_ = nullptr;
};
