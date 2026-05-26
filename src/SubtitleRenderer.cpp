#include "SubtitleRenderer.h"
#include "SubtitleItem.h"
#include "SubtitleTrack.h"

#include <QFontMetrics>
#include <QMap>
#include <QMutex>
#include <QPainter>
#include <QPen>

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
  return font;
}

void SubtitleRenderer::renderSubtitle(QPainter &painter, const QString &text,
                                      const QFont &font, int alignment,
                                      const QRect &textRect, double rotation,
                                      const QString &bgPath, bool bgIs9Patch,
                                      const QMargins &bgMargins) {
  if (text.isEmpty())
    return;

  painter.save();
  painter.setFont(font);

  // 应用旋转变换 (以文本框中心为旋转原点)
  QTransform trans;
  trans.translate(textRect.center().x(), textRect.center().y());
  trans.rotate(rotation);
  trans.translate(-textRect.center().x(), -textRect.center().y());
  painter.setTransform(trans, true);

  int alignFlags = alignment | Qt::AlignVCenter;

  // 绘制背景图
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
      QFontMetrics fm(font);
      int layoutFlags = alignFlags;
      if (layoutFlags & Qt::AlignJustify) {
        layoutFlags = (layoutFlags & ~Qt::AlignJustify) | Qt::AlignLeft;
      }
      QRect textBounding =
          fm.boundingRect(textRect, layoutFlags | Qt::TextWordWrap, text);

      // 根据 Margins 扩展背景框
      QRect bgRect =
          textBounding.adjusted(-bgMargins.left(), -bgMargins.top(),
                                bgMargins.right(), bgMargins.bottom());

      if (bgIs9Patch) {
        drawNinePatch(painter, bgImage, bgRect, bgMargins);
      } else {
        // 固定大小居中
        int imgX = textBounding.center().x() - bgImage.width() / 2;
        int imgY = textBounding.center().y() - bgImage.height() / 2;
        painter.drawImage(imgX, imgY, bgImage);
      }
    }
  }

  // 绘制字幕文本 (含描边逻辑)
  auto drawTextStroke = [&](QPainter &p, const QColor &color, int strokeWidth) {
    if (strokeWidth > 0) {
      p.setPen(
          QPen(color, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    } else {
      p.setPen(color);
    }

    if (alignFlags & Qt::AlignJustify) {
      // 英文单词和中文单字的分散对齐自适应逻辑
      QList<QString> tokens;
      QString currentWord;
      for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];
        if (ch.isSpace()) {
          if (!currentWord.isEmpty()) {
            tokens.append(currentWord);
            currentWord.clear();
          }
          continue;
        }
        ushort unicode = ch.unicode();
        bool isEnglish = (unicode >= 0x0020 && unicode <= 0x007E);
        if (isEnglish) {
          currentWord.append(ch);
        } else {
          if (!currentWord.isEmpty()) {
            tokens.append(currentWord);
            currentWord.clear();
          }
          tokens.append(QString(ch));
        }
      }
      if (!currentWord.isEmpty()) {
        tokens.append(currentWord);
      }

      QFontMetrics fm(font);
      int totalW = 0;
      QList<int> tokenWidths;
      for (const QString &t : tokens) {
        int w = fm.horizontalAdvance(t);
        tokenWidths.append(w);
        totalW += w;
      }

      int N = tokens.size();
      if (N <= 1 || totalW >= textRect.width()) {
        p.drawText(textRect,
                   (alignFlags & ~Qt::AlignJustify) | Qt::AlignHCenter, text);
      } else {
        double extra = textRect.width() - totalW;
        double step = extra / (N - 1);
        double currentX = textRect.left();
        for (int i = 0; i < N; ++i) {
          QRectF tokenRect(currentX, textRect.top(), tokenWidths[i],
                           textRect.height());
          p.drawText(tokenRect, Qt::AlignVCenter | Qt::AlignLeft, tokens[i]);
          currentX += tokenWidths[i] + step;
        }
      }
    } else {
      p.drawText(textRect, alignFlags, text);
    }
  };

  // 默认使用 3px 的黑边描边 + 白色填充字
  drawTextStroke(painter, Qt::black, 3);
  drawTextStroke(painter, Qt::white, 0);

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

  // 4. 调用通用的无状态底层渲染方法
  renderSubtitle(painter, item.text, font, item.alignment, textRect,
                 item.rotation, bgPath, is9Patch, bgMargins);
}
