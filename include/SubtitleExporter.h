#pragma once

#include <QString>

class SubtitleTrack;

class SubtitleExporter
{
public:
    static bool exportToSRT(const SubtitleTrack& track, const QString& filePath);
    static bool exportToASS(const SubtitleTrack& track, const QString& filePath);
    static bool exportToVTT(const SubtitleTrack& track, const QString& filePath);
    static bool exportToPremiereXML(const SubtitleTrack& track, const QString& filePath);
};
