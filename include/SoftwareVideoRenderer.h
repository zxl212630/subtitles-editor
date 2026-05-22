#pragma once

#include "FFmpegDecoder.h"
#include <QImage>
#include <QMutex>
#include <QMargins>
#include <QHash>
#include <QWidget>

class SoftwareVideoRenderer : public QWidget {
  Q_OBJECT

public:
  explicit SoftwareVideoRenderer(QWidget *parent = nullptr);

  void renderFrame(const DecodedVideoFrame &frame);
  void clear();
  void setSubtitleText(const QString &text);
  void setSubtitleFont(const QFont &font);
  void setSubtitleBg(const QString &imagePath, bool is9Patch,
                     const QMargins &margins);
  void clearSubtitleBg();

  QSize videoSize() const { return videoSize_; }

protected:
  void paintEvent(QPaintEvent *event) override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;

private:
  QByteArray currentRgbaData_;
  int currentWidth_ = 0;
  int currentHeight_ = 0;
  bool hasFrame_ = false;
  QMutex imageMutex_;
  QSize videoSize_;

  QString subtitleText_;
  QFont subtitleFont_;
  QMutex subtitleMutex_;

  QString bgImagePath_;
  bool bgIs9Patch_ = false;
  QMargins bgMargins_;
  QHash<QString, QImage> bgCache_;
  QMutex bgMutex_;

  void drawNinePatch(QPainter &painter, const QImage &src,
                     const QRect &target, const QMargins &margins);
};
