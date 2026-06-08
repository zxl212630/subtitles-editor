#include "SubtitleRenderer.h"
#include "SubtitleItem.h"
#include "SubtitleTrack.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QMap>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

static QMutex s_bgCacheMutex;
static QMap<QString, QImage> s_bgCache;

void SubtitleRenderer::render(const SubtitleTrack &track, QPainter &painter,
                              qint64 currentPtsMs, const QSize &videoSize,
                              const QPoint &offset) {
  if (videoSize.isEmpty())
    return;

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(offset);

  // 裁剪在 (0, 0) - videoSize 范围内，防止字幕画到视频区域外面
  painter.setClipRect(QRect(QPoint(0, 0), videoSize), Qt::IntersectClip);

  for (const auto &item : track.items()) {
    if (item.startMs <= currentPtsMs && currentPtsMs < item.endMs) {
      renderItem(painter, item, track, videoSize);
    }
  }

  painter.restore();
}

void SubtitleRenderer::render(const SubtitleTrack &track, QImage &image,
                              qint64 currentPtsMs, const QSize &videoSize) {
  QPainter painter(&image);
  render(track, painter, currentPtsMs, videoSize, QPoint(0, 0));
}

void SubtitleRenderer::drawNinePatch(QPainter &painter, const QImage &src,
                                     const QRect &target, const QMargins &m) {
  int sw = src.width();
  int sh = src.height();
  int tw = target.width();
  int th = target.height();

  int ml = m.left(), mr = m.right(), mt = m.top(), mb = m.bottom();

  // Clamp margins to source size
  ml = qMin(ml, sw / 2);
  mr = qMin(mr, sw / 2);
  mt = qMin(mt, sh / 2);
  mb = qMin(mb, sh / 2);

  // Source rects (9 regions)
  QRect sTL(0, 0, ml, mt);
  QRect sTC(ml, 0, sw - ml - mr, mt);
  QRect sTR(sw - mr, 0, mr, mt);
  QRect sML(0, mt, ml, sh - mt - mb);
  QRect sMC(ml, mt, sw - ml - mr, sh - mt - mb);
  QRect sMR(sw - mr, mt, mr, sh - mt - mb);
  QRect sBL(0, sh - mb, ml, mb);
  QRect sBC(ml, sh - mb, sw - ml - mr, mb);
  QRect sBR(sw - mr, sh - mb, mr, mb);

  int tx = target.x(), ty = target.y();

  // Target rects
  QRect dTL(tx, ty, ml, mt);
  QRect dTC(tx + ml, ty, tw - ml - mr, mt);
  QRect dTR(tx + tw - mr, ty, mr, mt);
  QRect dML(tx, ty + mt, ml, th - mt - mb);
  QRect dMC(tx + ml, ty + mt, tw - ml - mr, th - mt - mb);
  QRect dMR(tx + tw - mr, ty + mt, mr, th - mt - mb);
  QRect dBL(tx, ty + th - mb, ml, mb);
  QRect dBC(tx + ml, ty + th - mb, tw - ml - mr, mb);
  QRect dBR(tx + tw - mr, ty + th - mb, mr, mb);

  // Draw 9 patches
  painter.drawImage(dTL, src, sTL);
  painter.drawImage(dTC, src, sTC);
  painter.drawImage(dTR, src, sTR);
  painter.drawImage(dML, src, sML);
  painter.drawImage(dMC, src, sMC);
  painter.drawImage(dMR, src, sMR);
  painter.drawImage(dBL, src, sBL);
  painter.drawImage(dBC, src, sBC);
  painter.drawImage(dBR, src, sBR);
}

QRect SubtitleRenderer::calculateItemRect(const SubtitleItem &item,
                                          const QSize &videoSize) {
  int rx = qRound(videoSize.width() * item.rectX);
  int ry = qRound(videoSize.height() * item.rectY);
  int rw = qRound(videoSize.width() * item.rectW);
  int rh = qRound(videoSize.height() * item.rectH);
  return QRect(rx, ry, rw, rh);
}

QFont SubtitleRenderer::buildFont(const SubtitleItem &item,
                                  const QSize &videoSize) {
  QFont font(item.fontFamily);
  double refHeight = 1080.0;
  double scale =
      (videoSize.height() > 0) ? (videoSize.height() / refHeight) : 1.0;
  int scaledSize = qRound(item.fontSize * scale);
  if (scaledSize < 1)
    scaledSize = 1;

  font.setPixelSize(scaledSize);
  font.setBold(item.bold);
  font.setItalic(item.italic);
  font.setUnderline(item.underline);
  font.setStyleStrategy(QFont::PreferAntialias);
  font.setHintingPreference(QFont::PreferFullHinting);
  return font;
}

void SubtitleRenderer::renderSubtitle(QPainter &painter, const QString &text,
                                      const QFont &font,
                                      const SubtitleItem &style,
                                      const QRect &textRect, double rotation,
                                      const QString &bgPath, bool bgIs9Patch,
                                      const QMargins &bgMargins) {
  if (text.isEmpty())
    return;

  painter.save();

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

  painter.setFont(font);

  // Apply rotation transform around the text rect center
  QTransform trans;
  trans.translate(textRect.center().x(), textRect.center().y());
  trans.rotate(rotation);
  trans.translate(-textRect.center().x(), -textRect.center().y());
  painter.setTransform(trans, true);

  int alignment = style.alignment;
  int alignFlags = alignment | Qt::AlignVCenter;

  // 1. Calculate text bounding rect for background drawing
  QFontMetrics fm(font);
  QStringList lines = text.split('\n');
  int maxLineW = 0;
  for (const QString &line : lines) {
    maxLineW = qMax(maxLineW, fm.horizontalAdvance(line));
  }
  int textH = qMax(1, (int)lines.size()) * fm.height();

  QRect textBounding;
  int bx = textRect.x();
  int by = textRect.y();

  if (alignment & Qt::AlignHCenter) {
    bx = textRect.center().x() - maxLineW / 2;
  } else if (alignment & Qt::AlignRight) {
    bx = textRect.right() - maxLineW;
  }

  if (alignFlags & Qt::AlignVCenter) {
    by = textRect.center().y() - textH / 2;
  } else if (alignFlags & Qt::AlignBottom) {
    by = textRect.bottom() - textH;
  }
  textBounding = QRect(bx, by, maxLineW, textH);

  // 2. Draw Background
  bool bgDrawn = false;

  // If a speaker background is active, draw it first
  if (!bgPath.isEmpty()) {
    QImage bgImage;
    {
      QMutexLocker lock(&s_bgCacheMutex);
      auto it = s_bgCache.find(bgPath);
      if (it != s_bgCache.end()) {
        bgImage = *it;
      } else {
        bgImage = QImage(bgPath);
        if (!bgImage.isNull()) {
          s_bgCache.insert(bgPath, bgImage);
        }
      }
    }

    if (!bgImage.isNull()) {
      QRect bgRect =
          textBounding.adjusted(-bgMargins.left(), -bgMargins.top(),
                                bgMargins.right(), bgMargins.bottom());
      if (bgIs9Patch) {
        drawNinePatch(painter, bgImage, bgRect, bgMargins);
      } else {
        int imgX = textBounding.center().x() - bgImage.width() / 2;
        int imgY = textBounding.center().y() - bgImage.height() / 2;
        painter.drawImage(imgX, imgY, bgImage);
      }
      bgDrawn = true;
    }
  }

  // Draw custom background box if general style background is enabled and no
  // speaker bg was drawn
  if (!bgDrawn && style.bgType > 0) {
    QRect bgRect = textBounding.adjusted(-style.bgPaddingX, -style.bgPaddingY,
                                         style.bgPaddingX, style.bgPaddingY);
    if (style.bgType == 1) { // Solid Box
      painter.save();
      painter.setRenderHint(QPainter::Antialiasing, true);
      QColor bgColor = QColor(style.bgColor);
      bgColor.setAlphaF(style.bgOpacity);
      painter.setBrush(bgColor);
      painter.setPen(Qt::NoPen);
      painter.drawRoundedRect(bgRect, style.bgRoundness, style.bgRoundness);
      painter.restore();
    } else if (style.bgType == 2 &&
               !style.bgImagePath.isEmpty()) { // Custom Image
      QImage bgImage;
      {
        QMutexLocker lock(&s_bgCacheMutex);
        auto it = s_bgCache.find(style.bgImagePath);
        if (it != s_bgCache.end()) {
          bgImage = *it;
        } else {
          bgImage = QImage(style.bgImagePath);
          if (!bgImage.isNull()) {
            s_bgCache.insert(style.bgImagePath, bgImage);
          }
        }
      }
      if (!bgImage.isNull()) {
        painter.save();
        painter.setOpacity(style.bgOpacity);
        if (style.bgImage9Patch) {
          QMargins margins(15, 15, 15, 15);
          drawNinePatch(painter, bgImage, bgRect, margins);
        } else {
          painter.drawImage(bgRect, bgImage);
        }
        painter.restore();
      }
    }
  }

  // 3. Build text path to support precise outline, fill, and shadow rendering
  // concurrently
  QPainterPath textPath;
  int lineSpacing = fm.lineSpacing();
  for (int i = 0; i < lines.size(); ++i) {
    QString line = lines[i];
    int lw = fm.horizontalAdvance(line);

    int lx = textRect.x();
    if (alignment & Qt::AlignHCenter) {
      lx = textRect.center().x() - lw / 2;
    } else if (alignment & Qt::AlignRight) {
      lx = textRect.right() - lw;
    }

    int ly = by + i * lineSpacing + fm.ascent();
    textPath.addText(lx, ly, font, line);
  }

  // 4. Draw Shadow
  if (style.shadowEnabled) {
    painter.save();
    QColor shColor = QColor(style.shadowColor);
    shColor.setAlphaF(style.shadowOpacity);
    painter.setBrush(shColor);
    painter.setPen(Qt::NoPen);

    QTransform shadowTrans;
    shadowTrans.translate(style.shadowOffsetX, style.shadowOffsetY);
    QPainterPath shadowPath = shadowTrans.map(textPath);

    if (style.shadowBlur > 0) {
      // Approximate soft shadows by drawing offset paths at reduced opacity
      int passes = qMin(5, style.shadowBlur);
      double stepOpacity = style.shadowOpacity / passes;
      for (int p = 1; p <= passes; ++p) {
        QColor blurColor = QColor(style.shadowColor);
        blurColor.setAlphaF(stepOpacity);
        painter.setBrush(blurColor);

        QTransform blurTrans;
        double radius =
            static_cast<double>(p) * style.shadowBlur / passes / 2.0;
        blurTrans.translate(radius, radius);
        painter.fillPath(blurTrans.map(shadowPath), blurColor);
        blurTrans.reset();
        blurTrans.translate(-radius, -radius);
        painter.fillPath(blurTrans.map(shadowPath), blurColor);
      }
    } else {
      painter.fillPath(shadowPath, shColor);
    }
    painter.restore();
  }

  // 5. Draw Outline (Stroke)
  if (style.strokeEnabled && style.strokeWidth > 0) {
    painter.save();
    QColor strColor = QColor(style.strokeColor);
    strColor.setAlphaF(style.strokeOpacity);
    QPen strokePen(strColor, style.strokeWidth, Qt::SolidLine, Qt::RoundCap,
                   Qt::RoundJoin);
    painter.setPen(strokePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(textPath);
    painter.restore();
  }

  // 6. Fill Text
  painter.save();
  QBrush fillBrush;
  double opacity = style.textOpacity;

  if (style.fillType == 0) { // Solid
    QColor fColor = QColor(style.fillColor);
    fColor.setAlphaF(opacity);
    fillBrush = QBrush(fColor);
  } else if (style.fillType == 1) { // Linear Gradient
    QLinearGradient grad;
    double angleRad = qDegreesToRadians(static_cast<double>(style.fillAngle));
    double dx = qCos(angleRad);
    double dy = -qSin(angleRad); // QPainter Y is inverted

    double cx = textBounding.center().x();
    double cy = textBounding.center().y();
    double halfW = textBounding.width() / 2.0;
    double halfH = textBounding.height() / 2.0;
    double r = qAbs(halfW * dx) + qAbs(halfH * dy);

    grad.setStart(cx - r * dx, cy - r * dy);
    grad.setFinalStop(cx + r * dx, cy + r * dy);

    QColor c1 = QColor(style.fillColor);
    c1.setAlphaF(opacity);
    QColor c2 = QColor(style.fillColor2);
    c2.setAlphaF(opacity);

    grad.setColorAt(0.0, c1);
    grad.setColorAt(1.0, c2);
    fillBrush = QBrush(grad);
  } else if (style.fillType == 2 &&
             !style.fillTexturePath.isEmpty()) { // Texture Image
    QImage textureImage(style.fillTexturePath);
    if (!textureImage.isNull()) {
      QPixmap pm = QPixmap::fromImage(textureImage);
      if (!style.fillTextureTile) {
        pm = pm.scaled(textBounding.size(), Qt::IgnoreAspectRatio,
                       Qt::SmoothTransformation);
      }
      QBrush texBrush(pm);
      QTransform brushTrans;
      brushTrans.translate(textBounding.x(), textBounding.y());
      texBrush.setTransform(brushTrans);
      fillBrush = texBrush;

      if (opacity < 1.0) {
        painter.setOpacity(opacity);
      }
    } else {
      QColor fColor = QColor(style.fillColor);
      fColor.setAlphaF(opacity);
      fillBrush = QBrush(fColor);
    }
  } else {
    QColor fColor = QColor(style.fillColor);
    fColor.setAlphaF(opacity);
    fillBrush = QBrush(fColor);
  }

  painter.setBrush(fillBrush);
  painter.setPen(Qt::NoPen);
  painter.fillPath(textPath, fillBrush);
  painter.restore();

  painter.restore();
}

void SubtitleRenderer::renderItem(QPainter &painter, const SubtitleItem &item,
                                  const SubtitleTrack &track,
                                  const QSize &videoSize) {
  if (item.text.isEmpty())
    return;

  // 1. 构建字体
  QFont font = buildFont(item, videoSize);

  // 2. 计算排版像素位置
  QRect textRect = calculateItemRect(item, videoSize);

  // 3. 如果字幕框太小，等比例缩小字体
  QFontMetrics fm(font);
  QStringList linesTemp = item.text.split('\n');
  int textW = 0;
  for (const QString &line : linesTemp) {
    textW = qMax(textW, fm.horizontalAdvance(line));
  }
  int textH = qMax(1, (int)linesTemp.size()) * fm.height();
  double shrinkScale = 1.0;
  if (textW > textRect.width() && textRect.width() > 0) {
    shrinkScale =
        qMin(shrinkScale, static_cast<double>(textRect.width()) / textW);
  }
  if (textH > textRect.height() && textRect.height() > 0) {
    shrinkScale =
        qMin(shrinkScale, static_cast<double>(textRect.height()) / textH);
  }
  if (shrinkScale < 1.0) {
    int newSize = qMax(1, qRound(font.pixelSize() * shrinkScale));
    font.setPixelSize(newSize);
  }

  // 3. 获取背景图和说话人信息
  QString bgPath;
  bool is9Patch = true;
  QMargins bgMargins = track.unifiedBorderMargins();

  if (item.speakerId != -1) {
    SpeakerInfo sp = track.speakerInfo(item.speakerId);
    if (!sp.bgImageFile.isEmpty()) {
      QString folder = track.globalBgFolder();
      if (!folder.isEmpty()) {
        bgPath = folder + "/" + sp.bgImageFile;
      } else {
        bgPath = sp.bgImageFile;
      }
      is9Patch = sp.is9Patch;
    }
  }

  // 4. 调用通用的无状态底层渲染方法，传入本项作为样式来源
  renderSubtitle(painter, item.text, font, item, textRect, item.rotation,
                 bgPath, is9Patch, bgMargins);
}
