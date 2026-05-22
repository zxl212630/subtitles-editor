#pragma once

#include <QFontDatabase>
#include <QWidget>

#include "MediaPlayer.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>

class QComboBox;
class QLabel;
class QFrame;
class QPushButton;
class ProgressBarWidget;

class VolumeButton : public QPushButton {
  Q_OBJECT
public:
  explicit VolumeButton(QWidget *parent = nullptr);
signals:
  void hovered();
  void unhovered();

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
};

class VolumeSliderWidget : public QFrame {
  Q_OBJECT
public:
  explicit VolumeSliderWidget(QWidget *parent = nullptr);
  void setVolume(qreal vol, bool muted);
  void startHideTimer();
  void stopHideTimer();
signals:
  void volumeChanged(qreal volume);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private slots:
  void updateTheme();

private:
  QSlider *slider_ = nullptr;
  QLabel *label_ = nullptr;
  QTimer *hideTimer_ = nullptr;
  bool isDragging_ = false;
};

class SoftwareVideoRenderer;
class SubtitleTrack;

class VideoPreviewPanel : public QWidget {
  Q_OBJECT

public:
  explicit VideoPreviewPanel(QWidget *parent = nullptr);

  void setMediaPlayer(MediaPlayer *player);
  void setSubtitleTrack(SubtitleTrack *track);

  SoftwareVideoRenderer *videoRenderer() const { return videoRenderer_; }
  void onMediaLoaded(qint64 durationMs, QSize videoSize);
  void seekTo(qint64 ms);
  void updateSubtitleOverlay();
  void setVideoFps(double fps);

signals:
  void fontChanged(const QString &family);
  void fontSizeChanged(int size);
  void playRequested();
  void pauseRequested();
  void stopRequested();
  void seekRequested(qint64 ms);
  void stepForwardRequested();
  void stepBackwardRequested();
  void previewSeekRequested(qint64 ms);
  void previewSeekFinished();

public slots:
  void onPlaybackStateChanged(MediaPlayer::State state);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void setupUi();
  void populateFontCombo();
  void populateSizeCombo();
  void updateHandlePositions();
  void onTimeChanged(qint64 ms);
  void showVolumeSlider();
  void hideVolumeSliderDeferred();
  void toggleMute();

  QComboBox *fontCombo_ = nullptr;
  QComboBox *sizeCombo_ = nullptr;
  QFrame *videoArea_ = nullptr;
  QList<QFrame *> handles_;

  MediaPlayer *mediaPlayer_ = nullptr;
  SoftwareVideoRenderer *videoRenderer_ = nullptr;
  SubtitleTrack *subtitleTrack_ = nullptr;
  QPushButton *playPauseBtn_ = nullptr;
  QPushButton *stopBtn_ = nullptr;
  QPushButton *stepFwdBtn_ = nullptr;
  QPushButton *stepBwdBtn_ = nullptr;
  QLabel *currentTimeLabel_ = nullptr;
  ProgressBarWidget *progressBar_ = nullptr;
  VolumeButton *volBtn_ = nullptr;
  VolumeSliderWidget *sliderWidget_ = nullptr;
  bool isPlaying_ = false;
  qint64 totalDurationMs_ = 0;
};
