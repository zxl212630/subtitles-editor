#pragma once

#include <QString>

struct SubtitleItem {
    QString id;              // UUID
    QString text;            // 字幕文本
    qint64 startMs = 0;      // 开始时间（毫秒）
    qint64 endMs = 0;        // 结束时间（毫秒）
    bool selected = false;   // 选中状态
};
