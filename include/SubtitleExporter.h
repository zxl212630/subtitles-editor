#pragma once

#include <QSize>
#include <QString>

class SubtitleTrack;

class SubtitleExporter {
public:
  static bool exportToSRT(const SubtitleTrack &track, const QString &filePath);
  static bool exportToTXT(const SubtitleTrack &track, const QString &filePath);
  static bool exportToASS(const SubtitleTrack &track, const QString &filePath,
                          const QSize &videoSize = QSize());
  static bool exportToVTT(const SubtitleTrack &track, const QString &filePath);
  static bool exportToPremiereXML(const SubtitleTrack &track,
                                  const QString &filePath, double fps = 25.0,
                                  const QSize &videoSize = QSize());
  static bool exportToFCPXML(const SubtitleTrack &track,
                             const QString &filePath, double fps = 25.0,
                             const QSize &videoSize = QSize());

private:
  static QString formatSrtTime(qint64 ms);
};
