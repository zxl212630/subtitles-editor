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

  // === 高级填充与样式属性 ===
  int fillType = 0; // 0 = 纯色 (Solid), 1 = 线性渐变 (Linear Gradient), 2 =
                    // 贴图图片 (Texture Image)
  QString fillColor = "#FFFFFF";  // 填充主色/渐变色1 (Hex)
  QString fillColor2 = "#FFFFFF"; // 填充渐变色2 (Hex)
  int fillAngle = 90;             // 渐变角度
  QString fillTexturePath;        // 填充贴图文件路径
  bool fillTextureTile = true;    // 贴图是否平铺
  double textOpacity = 1.0;       // 不透明度 (0.0 - 1.0)

  // 描边属性
  bool strokeEnabled = false;
  int strokeWidth = 1;
  QString strokeColor = "#000000";
  double strokeOpacity = 1.0;

  // 阴影属性
  bool shadowEnabled = false;
  int shadowOffsetX = 0;
  int shadowOffsetY = 0;
  int shadowBlur = 0;
  QString shadowColor = "#000000";
  double shadowOpacity = 1.0;

  // 背景属性
  int bgType = 0; // 0 = 无背景, 1 = 单色底框, 2 = 图片底框
  QString bgColor = "#000000";
  double bgOpacity = 1.0;
  int bgRoundness = 10;
  int bgPaddingX = 0;
  int bgPaddingY = 0;
  QString bgImagePath;
  bool bgImage9Patch = true;
  int bgOffsetX = 0;
  int bgOffsetY = 0;

  // 气泡属性
  bool bubbleEnabled = false;
  QString bubbleImagePath;
  int bubblePaddingLeft = 10;
  int bubblePaddingRight = 10;
  int bubblePaddingTop = 10;
  int bubblePaddingBottom = 10;
  int bubbleSliceLeft = 10;
  int bubbleSliceRight = 10;
  int bubbleSliceTop = 10;
  int bubbleSliceBottom = 10;

  // 归一化排版坐标，默认底部区域 (x: 10%, y: 75%, width: 80%, height: 20%)
  double rectX = 0.1;
  double rectY = 0.75;
  double rectW = 0.8;
  double rectH = 0.2;
  double rotation = 0.0; // Rotation angle in degrees
};
