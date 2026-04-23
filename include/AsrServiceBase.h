#pragma once

#include <QObject>
#include <QString>
#include <QList>

class AsrServiceBase : public QObject
{
    Q_OBJECT

public:
    struct TranscriptSegment {
        QString text;
        qint64 startMs = 0;
        qint64 endMs = 0;
    };

    struct TranscriptResult {
        bool success = false;
        QString errorMessage;
        QList<TranscriptSegment> segments;
    };

    explicit AsrServiceBase(QObject* parent = nullptr);
    virtual ~AsrServiceBase();

    virtual void transcribe(const QString& audioFilePath) = 0;

signals:
    void transcribeFinished(const AsrServiceBase::TranscriptResult& result);
    void transcribeProgress(int percent);
};
