/*
 * This file is part of QtEmailFetcher project: Email parser for C++ Qt.
 *
 * GPLv3+ (c) acetone, 2023
 */

#pragma once

#include "emaildocumententry.h"

#include <QByteArray>
#include <QString>
#include <QMap>
#include <QDateTime>
#include <QSharedPointer>

class EmailDocument
{
public:
    struct Destination
    {
        QString address;
        QString name;
    };

    EmailDocument();

    void parse(const QByteArray& data);
    static QByteArray decodeMimeString(const QString &mimeString);
    static QString extractAddress(const QString& string);
    static QString extractName(const QString& string);
    static QDateTime decodeTimeString(const QString& stringRFC822_1123);

    Destination to()                                    const { return m_to; }
    Destination from()                                  const { return m_from; }
    QString subject()                                   const { return m_subject; }
    QString returnPath()                                const { return m_returnPath; }
    QDateTime dateTime()                                const { return m_dateTime; }
    QList<QSharedPointer<EmailDocumentEntry>> payload() const { return m_content; }

    QByteArray rawData()                                const { return m_rawData; }

    void setComment(const QString& text) { m_comment = text; }
    QString comment() const { return m_comment; }

private:
    void parseBody();
    QByteArray m_rawData;

    Destination m_to;
    Destination m_from;
    QString m_returnPath;
    QString m_subject;
    QDateTime m_dateTime;
    QList<QSharedPointer<EmailDocumentEntry>> m_content;

    QString m_comment; // for high level usage
};

