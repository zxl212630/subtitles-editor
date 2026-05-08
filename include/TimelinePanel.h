#pragma once

#include <QElapsedTimer>
#include <QWidget>

class QResizeEvent;
class QScrollBar;
class QWheelEvent;
class SubtitleTrack;
class TimelineCanvas;

class TimelinePanel : public QWidget {
  Q_OBJECT

public:
  enum class PlayheadAnchor { LeftThird, Center, RightThird };

  explicit TimelinePanel(QWidget *parent = nullptr);

  void setTrack(SubtitleTrack *track);
  void setCurrentTime(qint64 ms);
  void setTotalDuration(qint64 ms);
  void setPlayheadAnchor(PlayheadAnchor anchor);
  void setPlaying(bool playing);

signals:
  void timeClicked(qint64 ms);
  void itemSelected(const QString &id);
  void asrFailed(const QString &error);
  void asrSucceeded();
  void mediaFileDropped(const QString &path);

  // Drag seek signals
  void dragSeekStarted();
  void dragSeekMoved(qint64 ms);
  void dragSeekEnded();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
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

protected:
  friend class TimelineCanvas;
  void drawOnCanvas(QPainter &painter);

private:
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
  PlayheadAnchor playheadAnchor_ = PlayheadAnchor::Center;
  bool isPlaying_ = false;

  // Drag state
  bool isDragging_ = false;
  bool dragMoved_ = false;
  int dragStartX_ = 0;
  QElapsedTimer dragThrottleTimer_;
  static constexpr int DRAG_THROTTLE_MS = 30;
};
