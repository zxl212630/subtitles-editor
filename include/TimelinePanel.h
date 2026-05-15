#pragma once

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

  enum class ClipInteractionMode {
    Idle,
    ClipMove,
    ClipResizeLeft,
    ClipResizeRight
  };

  explicit TimelinePanel(QWidget *parent = nullptr);

  void setTrack(SubtitleTrack *track);
  void setCurrentTime(qint64 ms);
  void setTotalDuration(qint64 ms);
  void setPlayheadAnchor(PlayheadAnchor anchor);
  void setPlaying(bool playing);
  void setVideoFps(double fps);
  void setMediaFilePath(const QString &path);

  qint64 totalDuration() const { return totalDurationMs_; }
  QString mediaFilePath() const { return mediaFilePath_; }

  void startAsrPipeline(const QString &localPath);

signals:
  void timeClicked(qint64 ms);
  void previewSeekRequested(qint64 ms);
  void dragSeekFinished(qint64 ms);
  void itemSelected(const QString &id);
  void asrFailed(const QString &error);
  void asrSucceeded();
  void mediaFileDropped(const QString &path);
  void importMediaRequested();
  void subtitleFileDropped(const QString &path);
  void videoAsrRequested();
  void videoPropertyRequested();
  void openFileLocationRequested();

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void drawRuler(QPainter &painter);
  void drawSubtitleTrack(QPainter &painter, int y);
  void drawVideoTrack(QPainter &painter, int y);
  void drawEmptyState(QPainter &painter);
  void drawPlayhead(QPainter &painter);

  int timeToX(qint64 ms) const;
  qint64 xToTime(int x) const;
  void updateScrollBar();
  void clampScrollOffset();

  ClipInteractionMode hitTestClip(int mouseX, int mouseY, QString *outId) const;
  void updateClipCursor(int mouseX, int mouseY);

protected:
  friend class TimelineCanvas;
  void drawOnCanvas(QPainter &painter);

private:
  SubtitleTrack *track_ = nullptr;
  qint64 totalDurationMs_ = 0;
  qint64 currentTimeMs_ = 0;
  static constexpr int RULER_HEIGHT = 36;
  static constexpr int SUBTITLE_TRACK_HEIGHT = 48;
  static constexpr int VIDEO_TRACK_HEIGHT = 80;
  static constexpr int TRACK_HEAD_WIDTH = 120;
  static constexpr int PANEL_RIGHT_MARGIN = 8;

  double pixelsPerSecond_ = 100.0;
  int scrollOffsetX_ = 0;
  QScrollBar *hScrollBar_ = nullptr;
  TimelineCanvas *canvas_ = nullptr;
  PlayheadAnchor playheadAnchor_ = PlayheadAnchor::Center;
  bool isPlaying_ = false;

  // Empty state import button rect
  QRect emptyStateRect_;

  // Drag-to-seek state
  bool isDragging_ = false;
  bool mousePressed_ = false;
  int dragStartX_ = 0;
  qint64 lastPreviewSystemTime_ = 0;
  double videoFps_ = 25.0;
  static constexpr int DRAG_THRESHOLD_PX = 3;

  QString mediaFileName_;
  QString mediaFilePath_; // 视频完整路径

  // Clip drag/resize state
  ClipInteractionMode clipMode_ = ClipInteractionMode::Idle;
  QString dragTargetId_;       // id of the clip being dragged
  qint64 dragOrigStartMs_ = 0; // original startMs before drag
  qint64 dragOrigEndMs_ = 0;   // original endMs before drag
  qint64 dragTempStartMs_ = 0; // current temp startMs during drag
  qint64 dragTempEndMs_ = 0;   // current temp endMs during drag
  static constexpr int DRAG_EDGE_THRESHOLD_PX = 6;
};
