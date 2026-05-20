#include "SubtitleExporter.h"
#include "SubtitleTrack.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

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

bool SubtitleExporter::exportToASS(const SubtitleTrack &track,
                                   const QString &filePath) {
  Q_UNUSED(track)
  Q_UNUSED(filePath)
  qWarning() << "exportToASS not implemented";
  return false;
}

bool SubtitleExporter::exportToVTT(const SubtitleTrack &track,
                                   const QString &filePath) {
  Q_UNUSED(track)
  Q_UNUSED(filePath)
  qWarning() << "exportToVTT not implemented";
  return false;
}

bool SubtitleExporter::exportToPremiereXML(const SubtitleTrack &track,
                                           const QString &filePath) {
  Q_UNUSED(track)
  Q_UNUSED(filePath)
  qWarning() << "exportToPremiereXML not implemented";
  return false;
}
