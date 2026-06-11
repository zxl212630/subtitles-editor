#pragma once

#include <QFont>
#include <QImage>
#include <QMargins>
#include <QPoint>
#include <QRect>
#include <QSize>

class SubtitleTrack;
struct SubtitleItem;
class QPainter;

class SubtitleRenderer {
public:
  // 将 track 轨道中当前 PTS 时刻对应的字幕绘制在给定的 QPainter 上，支持 offset
  // 偏移量
  static void render(const SubtitleTrack &track, QPainter &painter,
                     qint64 currentPtsMs, const QSize &videoSize,
                     const QPoint &offset = QPoint(0, 0));

  // 辅助包装：直接绘制到给定的 QImage 图像上
  static void render(const SubtitleTrack &track, QImage &image,
                     qint64 currentPtsMs, const QSize &videoSize);

  // 绘制九宫格背景图片的通用静态方法
  static void drawNinePatch(QPainter &painter, const QImage &src,
                            const QRect &target, const QMargins &margins);

  // 根据归一化坐标和当前渲染视频区域大小，计算出未旋转状态下的字幕像素排版框
  static QRect calculateItemRect(const SubtitleItem &item,
                                 const QSize &videoSize);

  // 根据当前渲染视频区域高度缩放并构建出最终的 QFont
  static QFont buildFont(const SubtitleItem &item, const QSize &videoSize,
                         double refHeight = 1080.0);

  // 底层通用绘制方法，解耦具体数据模型，可供预览渲染器和导出渲染器底层共同调用
  static void renderSubtitle(QPainter &painter, const QString &text,
                             const QFont &font, const SubtitleItem &style,
                             const QRect &textRect, double rotation,
                             const QString &bgPath, bool bgIs9Patch,
                             const QMargins &bgMargins);

private:
  // 渲染单个字幕项
  static void renderItem(QPainter &painter, const SubtitleItem &item,
                         const SubtitleTrack &track, const QSize &videoSize);
};
