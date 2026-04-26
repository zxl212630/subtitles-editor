#pragma once

#include <QFontDatabase>
#include <QWidget>

class QComboBox;
class QLabel;
class QFrame;
class QPushButton;

class MediaPlayer;
class SoftwareVideoRenderer;
class SubtitleTrack;

class VideoPreviewPanel : public QWidget {
  Q_OBJECT

public:
  explicit VideoPreviewPanel(QWidget *parent = nullptr);

  void setMediaPlayer(MediaPlayer *player);
  void setSubtitleTrack(SubtitleTrack *track);
  void onMediaLoaded(qint64 durationMs, QSize videoSize);
  void seekTo(qint64 ms);
  void updateSubtitleOverlay();

signals:
  void fontChanged(const QString &family);
  void fontSizeChanged(int size);
  void playRequested();
  void pauseRequested();
  void seekRequested(qint64 ms);
  void stepForwardRequested();
  void stepBackwardRequested();

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void setupUi();
  void populateFontCombo();
  void populateSizeCombo();
  void updateHandlePositions();
  void paintSubtitle();
  void onTimeChanged(qint64 ms);

  QComboBox *fontCombo_ = nullptr;
  QComboBox *sizeCombo_ = nullptr;
  QFrame *videoArea_ = nullptr;
  QList<QFrame *> handles_;

  MediaPlayer *mediaPlayer_ = nullptr;
  SoftwareVideoRenderer *videoRenderer_ = nullptr;
  SubtitleTrack *subtitleTrack_ = nullptr;
  QPushButton *playBtn_ = nullptr;
  QPushButton *pauseBtn_ = nullptr;
  QPushButton *stepFwdBtn_ = nullptr;
  QPushButton *stepBwdBtn_ = nullptr;
  QLabel *currentTimeLabel_ = nullptr;
  bool isPlaying_ = false;
  qint64 totalDurationMs_ = 0;
};
