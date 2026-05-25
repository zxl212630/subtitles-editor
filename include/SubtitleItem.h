#pragma once

#include <QString>

struct SubtitleItem {
  QString id;            // UUID
  QString text;          // 字幕文本
  qint64 startMs = 0;    // 开始时间（毫秒）
  qint64 endMs = 0;      // 结束时间（毫秒）
  bool selected = false; // 选中状态
  int speakerId = -1;    // 说话人 ID (-1 表示未分配)

  // === 个体样式与位置属性 ===
  QString fontFamily = "Arial";
  int fontSize = 24;
  bool bold = false;
  bool italic = false;
  bool underline = false;
  int alignment = 0x84; // Qt::AlignHCenter | Qt::AlignVCenter = 132

  // 归一化排版坐标，默认底部区域 (x: 10%, y: 75%, width: 80%, height: 20%)
  double rectX = 0.1;
  double rectY = 0.75;
  double rectW = 0.8;
  double rectH = 0.2;
  double rotation = 0.0; // Rotation angle in degrees
};
