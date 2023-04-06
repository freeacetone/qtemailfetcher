/*
 * This file is part of QtEmailFetcher project: Email parser for C++ Qt.
 *
 * GPLv3+ (c) acetone, 2023
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QSharedPointer>

class EmailDocumentEntry
{
public:
    struct ContentType
    {
        enum Enum {
            undefined = 0,
            imagePng = 10,
            imageJpeg = 11,
            imageOther,
            textPlain = 20,
            textOther = 21,
            multipartAlternative = 30,
            multipartMixed = 31,
            multipartRelated = 32,
            multipartOther,
            binary = 40
        };

        Enum enumerate = Enum::undefined;
        QString string = "Undefined";
    };
    enum class TransferEncoding
    {
        textPlain,
        base64
    };

    EmailDocumentEntry(QList<QSharedPointer<EmailDocumentEntry>>* attachments = nullptr);
    void parse(const QByteArray& data);

    static ContentType contentTypeFromString(const QString& string);
    static ContentType contentTypeToString(ContentType type);

    static ContentType parseContentType(const QByteArray& data, QString* name = nullptr, QString* boundary = nullptr, QString* charset = nullptr);
    static TransferEncoding parseTransferEncoding(const QByteArray& data);
    static QString primaryLineDelimiter(const QByteArray& data);

    ContentType contentType()               const { return m_contentType; }
    QByteArray content()                    const { return m_content; }
    QString name()                          const { return m_name; }

    QByteArray rawPayload()                 const;
    QByteArray rawHeaders()                 const;

private:
    void multipart(QByteArray section);

    QByteArray m_rawData;
    QList<QSharedPointer<EmailDocumentEntry>>* m_pAttachments;

    ContentType m_contentType;
    QByteArray m_content;
    QString m_name = "Undefined";
};

