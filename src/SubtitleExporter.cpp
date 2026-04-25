#include "SubtitleExporter.h"
#include "SubtitleTrack.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

bool SubtitleExporter::exportToSRT(const SubtitleTrack &track,
                                   const QString &filePath) {
  Q_UNUSED(track)
  Q_UNUSED(filePath)
  qWarning() << "exportToSRT not implemented";
  return false;
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
