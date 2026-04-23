#include "TimelinePanel.h"
#include "SubtitleTrack.h"
#include "SubtitleItem.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontDatabase>

TimelinePanel::TimelinePanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("TimelinePanel");
    setStyleSheet(R"(
        QWidget#TimelinePanel {
            background-color: #1e1e1e;
            border-radius: 10px;
        }
    )");
    setMinimumHeight(150);
    setMaximumHeight(400);
}

void TimelinePanel::setTrack(SubtitleTrack* track)
{
    if (track_) {
        disconnect(track_, &SubtitleTrack::dataChanged, this, QOverload<>::of(&TimelinePanel::update));
        disconnect(track_, &SubtitleTrack::itemSelected, this, QOverload<>::of(&TimelinePanel::update));
    }
    track_ = track;
    if (track_) {
        connect(track_, &SubtitleTrack::dataChanged, this, QOverload<>::of(&TimelinePanel::update));
        connect(track_, &SubtitleTrack::itemSelected, this, QOverload<>::of(&TimelinePanel::update));
    }
    update();
}

void TimelinePanel::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor("#1e1e1e"));

    drawRuler(painter);
    drawSubtitleTrack(painter, RULER_HEIGHT);
    drawVideoTrack(painter, RULER_HEIGHT + SUBTITLE_TRACK_HEIGHT);
    drawPlayhead(painter);
}

void TimelinePanel::drawRuler(QPainter& painter)
{
    painter.setPen(QColor("#6b7280"));
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    int contentWidth = width() - TRACK_HEAD_WIDTH;
    int seconds = totalDurationMs_ / 1000;
    for (int s = 0; s <= seconds; ++s) {
        int x = TRACK_HEAD_WIDTH + s * PIXELS_PER_SECOND;
        if (x > width()) break;

        QString label = QString("00:00:%1:00").arg(s, 2, 10, QChar('0'));
        painter.drawText(x - 20, 8, 60, 14, Qt::AlignCenter, label);

        painter.setPen(QColor("#333333"));
        painter.drawLine(x, 24, x, 34);
        painter.setPen(QColor("#6b7280"));
    }

    // Minor ticks
    painter.setPen(QColor("#404040"));
    for (int s = 0; s < seconds; ++s) {
        int midX = TRACK_HEAD_WIDTH + s * PIXELS_PER_SECOND + PIXELS_PER_SECOND / 2;
        painter.drawLine(midX, 28, midX, 31);
    }
}

void TimelinePanel::drawSubtitleTrack(QPainter& painter, int y)
{
    // Track background
    painter.fillRect(TRACK_HEAD_WIDTH, y, width() - TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT, QColor("#2a2a2a"));

    // Track head
    painter.setPen(Qt::NoPen);
    painter.fillRect(0, y, TRACK_HEAD_WIDTH, SUBTITLE_TRACK_HEIGHT, QColor("#262626"));
    painter.setPen(QColor("#9ca3af"));
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(12, y + 18, "T  字幕1");

    // Separator
    painter.setPen(QColor("#333333"));
    painter.drawLine(TRACK_HEAD_WIDTH, y + SUBTITLE_TRACK_HEIGHT - 1, width(), y + SUBTITLE_TRACK_HEIGHT - 1);

    if (!track_) return;

    // Subtitle bars
    for (const auto& item : track_->items()) {
        int x = TRACK_HEAD_WIDTH + msToPixels(item.startMs);
        int w = msToPixels(item.endMs - item.startMs);
        if (w < 4) w = 4;

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

void TimelinePanel::drawVideoTrack(QPainter& painter, int y)
{
    painter.fillRect(TRACK_HEAD_WIDTH, y, width() - TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT, QColor("#2a2a2a"));

    painter.setPen(Qt::NoPen);
    painter.fillRect(0, y, TRACK_HEAD_WIDTH, VIDEO_TRACK_HEIGHT, QColor("#262626"));
    painter.setPen(QColor("#9ca3af"));
    QFont font = painter.font();
    font.setPointSize(10);
    painter.setFont(font);
    painter.drawText(12, y + 18, "F  视频1");

    // Placeholder video bar
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#0284c7"));
    painter.drawRoundedRect(TRACK_HEAD_WIDTH + 4, y + 2, 400, VIDEO_TRACK_HEIGHT - 4, 4, 4);
    painter.setPen(QColor("#e5e5e5"));
    painter.drawText(TRACK_HEAD_WIDTH + 16, y + 50, "video.mp4");
}

void TimelinePanel::drawPlayhead(QPainter& painter)
{
    int x = TRACK_HEAD_WIDTH + msToPixels(currentTimeMs_);
    painter.setPen(QColor("#f59e0b"));
    painter.drawLine(x, 0, x, height());

    // Triangle pointer
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#f59e0b"));
    QPointF triangle[3] = {
        QPointF(x - 5, -5),
        QPointF(x + 5, -5),
        QPointF(x, 5)
    };
    painter.drawPolygon(triangle, 3);
}

void TimelinePanel::mousePressEvent(QMouseEvent* event)
{
    if (event->x() < TRACK_HEAD_WIDTH) return;

    qint64 ms = pixelsToMs(event->x() - TRACK_HEAD_WIDTH);
    if (ms < 0) ms = 0;
    if (ms > totalDurationMs_) ms = totalDurationMs_;

    currentTimeMs_ = ms;
    emit timeClicked(ms);
    update();
}

qint64 TimelinePanel::pixelsToMs(int px) const
{
    return static_cast<qint64>(px) * 1000 / PIXELS_PER_SECOND;
}

int TimelinePanel::msToPixels(qint64 ms) const
{
    return static_cast<int>(ms * PIXELS_PER_SECOND / 1000);
}
