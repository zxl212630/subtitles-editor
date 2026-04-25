#pragma once

#include <QWidget>

#include "AudioTranscoder.h"
#include "OssUploader.h"
#include "TencentAsrService.h"
#include "SubtitleItem.h"

class SubtitleTrack;

class TimelinePanel : public QWidget
{
    Q_OBJECT

public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    void setTrack(SubtitleTrack* track);

signals:
    void timeClicked(qint64 ms);
    void itemSelected(const QString& id);
    void asrFailed(const QString& error);
    void asrSucceeded();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void drawRuler(QPainter& painter);
    void drawSubtitleTrack(QPainter& painter, int y);
    void drawVideoTrack(QPainter& painter, int y);
    void drawPlayhead(QPainter& painter);
    void startAsrPipeline(const QString& localPath);

    qint64 pixelsToMs(int px) const;
    int msToPixels(qint64 ms) const;

    SubtitleTrack* track_ = nullptr;
    qint64 totalDurationMs_ = 11000; // placeholder 11 seconds
    qint64 currentTimeMs_ = 6040;    // placeholder ~6 seconds
    static constexpr int RULER_HEIGHT = 36;
    static constexpr int SUBTITLE_TRACK_HEIGHT = 48;
    static constexpr int VIDEO_TRACK_HEIGHT = 96;
    static constexpr int TRACK_HEAD_WIDTH = 120;
    static constexpr int PIXELS_PER_SECOND = 100;
};
