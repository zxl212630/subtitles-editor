#include "SubtitleExporter.h"
#include "SubtitleTrack.h"
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QTextStream>
#include <QXmlStreamWriter>

static QString getPostScriptName(const QString &fontFamily) {
  static const QHash<QString, QString> fontMap = {
      {"苹方-简", "PingFangSC-Regular"},
      {"苹方", "PingFangSC-Regular"},
      {"PingFang SC", "PingFangSC-Regular"},
      {"冬青黑体简体中文", "HiraginoSansGB-W3"},
      {"Hiragino Sans GB", "HiraginoSansGB-W3"},
      {"黑体-简", "STHeitiSC-Medium"},
      {"Heiti SC", "STHeitiSC-Medium"},
      {"华文黑体", "STHeiti"},
      {"宋体-简", "STSongti-SC-Regular"},
      {"Songti SC", "STSongti-SC-Regular"},
      {"楷体-简", "STKaiti-SC-Regular"},
      {"Kaiti SC", "STKaiti-SC-Regular"},
      {"手札体-简", "STHupo-SC-Regular"},
      {"HanziPen SC", "HanziPenSC-W3"},
      {"HanziPenSC", "HanziPenSC-W3"},
      {"Hanzipen SC", "HanziPenSC-W3"},
      {"Weibei SC", "STXinwei-SC-Regular"},
      {"娃娃体-简", "DFWaWaSC-W5"},
      {"Wawati SC", "DFWaWaSC-W5"},
      {"微软雅黑", "Microsoft YaHei"},
      {"Microsoft YaHei", "Microsoft YaHei"},
      {"黑体", "SimHei"},
      {"SimHei", "SimHei"},
      {"宋体", "SimSun"},
      {"SimSun", "SimSun"},
      {"新宋体", "NSimSun"},
      {"仿宋", "FangSong"},
      {"FangSong", "FangSong"},
      {"楷体", "KaiTi"},
      {"KaiTi", "KaiTi"},
      {"标楷体", "DFKai-SB"}};

  QString cleanName = fontFamily.trimmed();
  if (fontMap.contains(cleanName)) {
    return fontMap.value(cleanName);
  }
  return cleanName;
}

QString SubtitleExporter::formatSrtTime(qint64 ms) {
  if (ms < 0)
    ms = 0;
  qint64 hours = ms / 3600000;
  ms %= 3600000;
  qint64 minutes = ms / 60000;
  ms %= 60000;
  qint64 seconds = ms / 1000;
  qint64 millis = ms % 1000;
  return QString("%1:%2:%3,%4")
      .arg(hours, 2, 10, QLatin1Char('0'))
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(seconds, 2, 10, QLatin1Char('0'))
      .arg(millis, 3, 10, QLatin1Char('0'));
}

bool SubtitleExporter::exportToSRT(const SubtitleTrack &track,
                                   const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing:" << filePath;
    return false;
  }

  QTextStream out(&file);

  const auto &items = track.items();
  for (int i = 0; i < items.size(); ++i) {
    const auto &item = items[i];
    out << (i + 1) << "\n";
    out << formatSrtTime(item.startMs) << " --> " << formatSrtTime(item.endMs)
        << "\n";
    out << item.text << "\n\n";
  }

  file.close();
  return true;
}

bool SubtitleExporter::exportToTXT(const SubtitleTrack &track,
                                   const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing:" << filePath;
    return false;
  }

  QTextStream out(&file);

  const auto &items = track.items();
  for (int i = 0; i < items.size(); ++i) {
    if (i > 0)
      out << "\n";
    out << items[i].text;
  }
  out << "\n";

  file.close();
  return true;
}

static QString formatAssTime(qint64 ms) {
  if (ms < 0)
    ms = 0;
  qint64 hours = ms / 3600000;
  ms %= 3600000;
  qint64 minutes = ms / 60000;
  ms %= 60000;
  qint64 seconds = ms / 1000;
  qint64 centiseconds = (ms % 1000) / 10;
  return QString("%1:%2:%3.%4")
      .arg(hours)
      .arg(minutes, 2, 10, QLatin1Char('0'))
      .arg(seconds, 2, 10, QLatin1Char('0'))
      .arg(centiseconds, 2, 10, QLatin1Char('0'));
}

bool SubtitleExporter::exportToASS(const SubtitleTrack &track,
                                   const QString &filePath,
                                   const QSize &videoSize) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing ASS:" << filePath;
    return false;
  }

  QTextStream out(&file);

  int playResX =
      videoSize.isValid() && !videoSize.isEmpty() ? videoSize.width() : 1920;
  int playResY =
      videoSize.isValid() && !videoSize.isEmpty() ? videoSize.height() : 1080;

  // 1. Script Info
  out << "[Script Info]\n";
  out << "Title: Styled Subtitle Export\n";
  out << "ScriptType: v4.00+\n";
  out << "WrapStyle: 0\n";
  out << "PlayResX: " << playResX << "\n";
  out << "PlayResY: " << playResY << "\n";
  out << "ScaledBorderAndShadow: yes\n\n";

  // 2. Styles
  out << "[V4+ Styles]\n";
  out << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
         "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
         "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
         "Alignment, MarginL, MarginR, MarginV, Encoding\n";

  // Default Style
  QString defFont = track.defaultFontFamily();
  if (defFont.isEmpty())
    defFont = "Arial";
  int defSize = track.defaultFontSize() > 0 ? track.defaultFontSize() : 24;
  int boldVal = track.defaultBold() ? -1 : 0;
  int italicVal = track.defaultItalic() ? -1 : 0;
  int underlineVal = track.defaultUnderline() ? -1 : 0;

  out << QString("Style: "
                 "Default,%1,%2,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,%3,"
                 "%4,%5,0,100,100,0,0,1,2,0,5,10,10,10,1\n")
             .arg(defFont)
             .arg(defSize)
             .arg(boldVal)
             .arg(italicVal)
             .arg(underlineVal);

  // Speakers Styles
  QList<SpeakerInfo> speakers = track.allSpeakers();
  for (const auto &spk : speakers) {
    out << QString("Style: "
                   "Speaker_%1,%2,%3,&H00FFFFFF,&H000000FF,&H00000000,&"
                   "H00000000,%4,%5,%6,0,100,100,0,0,1,2,0,5,10,10,10,1\n")
               .arg(spk.id)
               .arg(defFont)
               .arg(defSize)
               .arg(boldVal)
               .arg(italicVal)
               .arg(underlineVal);
  }
  out << "\n";

  // 3. Events
  out << "[Events]\n";
  out << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, "
         "Effect, Text\n";

  const auto &items = track.items();
  for (const auto &item : items) {
    QString styleName = "Default";
    QString actorName = "";
    if (item.speakerId >= 0) {
      styleName = QString("Speaker_%1").arg(item.speakerId);
      SpeakerInfo spk = track.speakerInfo(item.speakerId);
      if (spk.id >= 0) {
        actorName = spk.name;
      }
    }

    double centerX = item.rectX + item.rectW / 2.0;
    double centerY = item.rectY + item.rectH / 2.0;
    double absX = playResX * centerX;
    double absY = playResY * centerY;

    QString tags = QString("{\\an5\\pos(%1,%2)")
                       .arg(QString::number(absX, 'f', 1))
                       .arg(QString::number(absY, 'f', 1));
    if (qAbs(item.rotation) > 0.001) {
      tags += QString("\\frz%1").arg(QString::number(-item.rotation, 'f', 1));
    }
    tags += QString("\\fn%1\\fs%2").arg(item.fontFamily).arg(item.fontSize);
    tags += item.bold ? "\\b1" : "\\b0";
    tags += item.italic ? "\\i1" : "\\i0";
    tags += item.underline ? "\\u1" : "\\u0";
    tags += "}";

    QString assText = item.text;
    assText.replace("\n", "\\N");

    out << QString("Dialogue: 0,%1,%2,%3,%4,0,0,0,,%5%6\n")
               .arg(formatAssTime(item.startMs))
               .arg(formatAssTime(item.endMs))
               .arg(styleName)
               .arg(actorName)
               .arg(tags)
               .arg(assText);
  }

  file.close();
  return true;
}

bool SubtitleExporter::exportToPremiereXML(const SubtitleTrack &track,
                                           const QString &filePath, double fps,
                                           const QSize &videoSize) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing PR XML:" << filePath;
    return false;
  }

  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeDTD("<!DOCTYPE xmeml SYSTEM \"fcpxml.dtd\">");

  xml.writeStartElement("xmeml");
  xml.writeAttribute("version", "4");

  int width =
      videoSize.isValid() && !videoSize.isEmpty() ? videoSize.width() : 1920;
  int height =
      videoSize.isValid() && !videoSize.isEmpty() ? videoSize.height() : 1080;

  int timebase = qRound(fps);
  if (timebase <= 0)
    timebase = 25;
  QString ntscStr = "FALSE";
  if (qAbs(fps - 29.97) < 0.05 || qAbs(fps - 59.94) < 0.05 ||
      qAbs(fps - 23.976) < 0.05) {
    ntscStr = "TRUE";
  }

  const auto &items = track.items();
  qint64 totalDurationFrames = 0;
  if (!items.isEmpty()) {
    totalDurationFrames = qRound(items.last().endMs * fps / 1000.0) + 25;
  }
  if (totalDurationFrames <= 0)
    totalDurationFrames = timebase * 10;

  xml.writeStartElement("sequence");
  xml.writeAttribute("id", "sequence-1");
  xml.writeTextElement("name", "Subtitle Sequence");
  xml.writeTextElement("duration", QString::number(totalDurationFrames));

  xml.writeStartElement("rate");
  xml.writeTextElement("timebase", QString::number(timebase));
  xml.writeTextElement("ntsc", ntscStr);
  xml.writeEndElement();

  xml.writeStartElement("media");
  xml.writeStartElement("video");

  xml.writeStartElement("format");
  xml.writeStartElement("samplecharacteristics");
  xml.writeTextElement("width", QString::number(width));
  xml.writeTextElement("height", QString::number(height));

  xml.writeStartElement("rate");
  xml.writeTextElement("timebase", QString::number(timebase));
  xml.writeTextElement("ntsc", ntscStr);
  xml.writeEndElement();

  xml.writeEndElement();
  xml.writeEndElement();

  xml.writeStartElement("track");

  for (int i = 0; i < items.size(); ++i) {
    const auto &item = items[i];
    qint64 startFrame = qRound(item.startMs * fps / 1000.0);
    qint64 endFrame = qRound(item.endMs * fps / 1000.0);
    qint64 durationFrames = endFrame - startFrame;
    if (durationFrames <= 0)
      durationFrames = 1;

    QString genId = QString("generator-%1").arg(i + 1);
    xml.writeStartElement("generatoritem");
    xml.writeAttribute("id", genId);

    xml.writeTextElement("name", item.text.left(15));
    xml.writeTextElement("duration", QString::number(durationFrames));

    xml.writeStartElement("rate");
    xml.writeTextElement("timebase", QString::number(timebase));
    xml.writeTextElement("ntsc", ntscStr);
    xml.writeEndElement();

    xml.writeTextElement("start", QString::number(startFrame));
    xml.writeTextElement("end", QString::number(endFrame));
    xml.writeTextElement("in", "0");
    xml.writeTextElement("out", QString::number(durationFrames));

    // effect
    xml.writeStartElement("effect");
    xml.writeTextElement("name", "Text");
    xml.writeTextElement("effectid", "Text");
    xml.writeTextElement("effectcategory", "Text");
    xml.writeTextElement("effecttype", "generator");
    xml.writeTextElement("mediatype", "video");

    // Parameter: str
    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "str");
    xml.writeTextElement("name", "Text");
    xml.writeTextElement("value", item.text);
    xml.writeEndElement();

    // Parameter: fontname
    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "fontname");
    xml.writeTextElement("name", "Font Name");
    xml.writeTextElement("value", getPostScriptName(item.fontFamily));
    xml.writeEndElement();

    // Parameter: size
    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "size");
    xml.writeTextElement("name", "Size");
    xml.writeTextElement("value", QString::number(item.fontSize));
    xml.writeEndElement();

    // Parameter: style & fontstyle (using valuemenu)
    int styleVal = 1; // Plain
    if (item.bold && item.italic)
      styleVal = 4;
    else if (item.bold)
      styleVal = 2;
    else if (item.italic)
      styleVal = 3;

    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "style");
    xml.writeTextElement("name", "Style");
    xml.writeStartElement("valuemenu");
    xml.writeTextElement("value", QString::number(styleVal));
    xml.writeEndElement(); // valuemenu
    xml.writeEndElement(); // parameter

    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "fontstyle");
    xml.writeTextElement("name", "Style");
    xml.writeStartElement("valuemenu");
    xml.writeTextElement("value", QString::number(styleVal));
    xml.writeEndElement(); // valuemenu
    xml.writeEndElement(); // parameter

    // Parameter: angle
    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "angle");
    xml.writeTextElement("name", "Angle");
    xml.writeTextElement("value", QString::number(item.rotation, 'f', 1));
    xml.writeEndElement(); // parameter

    // Parameter: origin
    double centerX = item.rectX + item.rectW / 2.0;
    double centerY = item.rectY + item.rectH / 2.0;
    double horiz = centerX - 0.5;
    double vert = -(centerY - 0.5);

    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "origin");
    xml.writeTextElement("name", "Origin");
    xml.writeStartElement("value");
    xml.writeTextElement("horiz", QString::number(horiz, 'f', 4));
    xml.writeTextElement("vert", QString::number(vert, 'f', 4));
    xml.writeEndElement();
    xml.writeEndElement();

    // Parameter: align (using valuemenu)
    int alignVal = 2; // Center
    if (item.alignment & Qt::AlignLeft)
      alignVal = 1;
    else if (item.alignment & Qt::AlignRight)
      alignVal = 3;

    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "align");
    xml.writeTextElement("name", "Alignment");
    xml.writeStartElement("valuemenu");
    xml.writeTextElement("value", QString::number(alignVal));
    xml.writeEndElement(); // valuemenu
    xml.writeEndElement(); // parameter

    // Parameter: color
    xml.writeStartElement("parameter");
    xml.writeTextElement("parameterid", "color");
    xml.writeTextElement("name", "Color");
    xml.writeStartElement("value");
    xml.writeStartElement("color");
    xml.writeTextElement("red", "255");
    xml.writeTextElement("green", "255");
    xml.writeTextElement("blue", "255");
    xml.writeTextElement("alpha", "255");
    xml.writeEndElement(); // color
    xml.writeEndElement(); // value
    xml.writeEndElement(); // parameter

    xml.writeEndElement(); // effect

    // Standard intrinsic Motion filter for Rotation
    if (qAbs(item.rotation) > 0.001) {
      xml.writeStartElement("filter");
      xml.writeStartElement("effect");
      xml.writeTextElement("name", "Motion");
      xml.writeTextElement("effectid", "motion");
      xml.writeTextElement("effectcategory", "motion");
      xml.writeTextElement("effecttype", "motion");
      xml.writeTextElement("mediatype", "video");

      xml.writeStartElement("parameter");
      xml.writeTextElement("parameterid", "rotation");
      xml.writeTextElement("name", "Rotation");
      xml.writeTextElement("value", QString::number(item.rotation, 'f', 1));
      xml.writeEndElement(); // parameter

      xml.writeEndElement(); // effect
      xml.writeEndElement(); // filter
    }

    xml.writeEndElement(); // generatoritem
  }

  xml.writeEndElement(); // track
  xml.writeEndElement(); // video
  xml.writeEndElement(); // media
  xml.writeEndElement(); // sequence
  xml.writeEndElement(); // xmeml

  xml.writeEndDocument();
  file.close();
  return true;
}

bool SubtitleExporter::exportToFCPXML(const SubtitleTrack &track,
                                      const QString &filePath, double fps,
                                      const QSize &videoSize) {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for writing FCPXML:" << filePath;
    return false;
  }

  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();

  xml.writeDTD("<!DOCTYPE fcpxml>");

  xml.writeStartElement("fcpxml");
  xml.writeAttribute("version", "1.8");

  int width =
      videoSize.isValid() && !videoSize.isEmpty() ? videoSize.width() : 1920;
  int height =
      videoSize.isValid() && !videoSize.isEmpty() ? videoSize.height() : 1080;

  xml.writeStartElement("resources");

  xml.writeStartElement("format");
  xml.writeAttribute("id", "r1");
  xml.writeAttribute(
      "name", QString("FFVideoFormat%1p%2").arg(height).arg(qRound(fps)));

  if (qAbs(fps - 29.97) < 0.05) {
    xml.writeAttribute("frameDuration", "1001/30000s");
  } else if (qAbs(fps - 23.976) < 0.05) {
    xml.writeAttribute("frameDuration", "1001/24000s");
  } else if (qAbs(fps - 59.94) < 0.05) {
    xml.writeAttribute("frameDuration", "1001/60000s");
  } else {
    xml.writeAttribute("frameDuration", QString("1/%1s").arg(qRound(fps)));
  }
  xml.writeAttribute("width", QString::number(width));
  xml.writeAttribute("height", QString::number(height));
  xml.writeEndElement();

  xml.writeStartElement("effect");
  xml.writeAttribute("id", "r2");
  xml.writeAttribute("name", "Basic Title");
  xml.writeAttribute(
      "uid",
      ".../Titles.localized/Bumper:Reveille/Basic Title/Basic Title.moti");
  xml.writeEndElement();

  xml.writeEndElement();

  xml.writeStartElement("library");
  xml.writeStartElement("event");
  xml.writeAttribute("name", "Subtitles");

  xml.writeStartElement("project");
  xml.writeAttribute("name", "Subtitles Project");

  const auto &items = track.items();
  double totalDurationSeconds = 0.0;
  if (!items.isEmpty()) {
    totalDurationSeconds = items.last().endMs / 1000.0 + 1.0;
  }
  if (totalDurationSeconds <= 0.0)
    totalDurationSeconds = 10.0;

  xml.writeStartElement("sequence");
  xml.writeAttribute("duration",
                     QString("%1s").arg(totalDurationSeconds, 0, 'f', 3));
  xml.writeAttribute("format", "r1");
  xml.writeAttribute("tcStart", "0s");

  xml.writeStartElement("spine");

  xml.writeStartElement("gap");
  xml.writeAttribute("name", "Gap");
  xml.writeAttribute("offset", "0s");
  xml.writeAttribute("duration",
                     QString("%1s").arg(totalDurationSeconds, 0, 'f', 3));
  xml.writeAttribute("start", "0s");

  for (int i = 0; i < items.size(); ++i) {
    const auto &item = items[i];
    double startSec = item.startMs / 1000.0;
    double durationSec = (item.endMs - item.startMs) / 1000.0;
    if (durationSec <= 0.001)
      durationSec = 0.1;

    xml.writeStartElement("title");
    xml.writeAttribute("ref", "r2");
    xml.writeAttribute("lane", "1");
    xml.writeAttribute("offset", QString("%1s").arg(startSec, 0, 'f', 3));
    xml.writeAttribute("duration", QString("%1s").arg(durationSec, 0, 'f', 3));
    xml.writeAttribute("start", "0s");

    double centerX = item.rectX + item.rectW / 2.0;
    double centerY = item.rectY + item.rectH / 2.0;
    double absX = (centerX - 0.5) * width;
    double absY = -(centerY - 0.5) * height;

    xml.writeStartElement("param");
    xml.writeAttribute("name", "Position");
    xml.writeAttribute("key", "9999/999166271/100/1");
    xml.writeAttribute("value", QString("%1 %2")
                                    .arg(QString::number(absX, 'f', 2))
                                    .arg(QString::number(absY, 'f', 2)));
    xml.writeEndElement();

    xml.writeStartElement("text");
    xml.writeStartElement("text-style");
    xml.writeAttribute("ref", QString("ts%1").arg(i + 1));
    xml.writeCharacters(item.text);
    xml.writeEndElement();
    xml.writeEndElement();

    xml.writeStartElement("text-style-def");
    xml.writeAttribute("id", QString("ts%1").arg(i + 1));

    xml.writeStartElement("style");
    xml.writeAttribute("font", item.fontFamily);
    xml.writeAttribute("fontSize", QString::number(item.fontSize));
    xml.writeAttribute("bold", item.bold ? "1" : "0");
    xml.writeAttribute("italic", item.italic ? "1" : "0");
    xml.writeAttribute("strokeColor", "0 0 0 1");
    xml.writeAttribute("strokeWidth", "2");

    QString alignment = "center";
    if (item.alignment & Qt::AlignLeft)
      alignment = "left";
    else if (item.alignment & Qt::AlignRight)
      alignment = "right";
    xml.writeAttribute("alignment", alignment);

    xml.writeEndElement();
    xml.writeEndElement();

    xml.writeEndElement();
  }

  xml.writeEndElement(); // gap
  xml.writeEndElement(); // spine
  xml.writeEndElement(); // sequence
  xml.writeEndElement(); // project
  xml.writeEndElement(); // event
  xml.writeEndElement(); // library
  xml.writeEndElement(); // fcpxml

  xml.writeEndDocument();
  file.close();
  return true;
}
