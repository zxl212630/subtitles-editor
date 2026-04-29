#pragma once

#include "FFmpegDecoder.h"
#include <QImage>
#include <QMutex>
#include <QWidget>

class SoftwareVideoRenderer : public QWidget {
  Q_OBJECT

public:
  explicit SoftwareVideoRenderer(QWidget *parent = nullptr);

  void renderFrame(const DecodedVideoFrame &frame);
  void clear();
  void setSubtitleText(const QString &text);
  void setSubtitleFont(const QFont &font);

  QSize videoSize() const { return videoSize_; }

protected:
  void paintEvent(QPaintEvent *event) override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;

private:
  QImage currentImage_;
  bool hasFrame_ = false;
  QMutex imageMutex_;
  QSize videoSize_;

  QString subtitleText_;
  QFont subtitleFont_;
  QMutex subtitleMutex_;
};
