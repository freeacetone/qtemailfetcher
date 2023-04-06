/*
 * This file is part of QtEmailFetcher project: Email parser for C++ Qt.
 *
 * GPLv3+ (c) acetone, 2023
 */

#include "emaildocumententry.h"
#include "emaildocument.h"

#include <QDebug>

enum class ParseState {
    contentType,
    other
};

EmailDocumentEntry::EmailDocumentEntry(QList<QSharedPointer<EmailDocumentEntry>> *attachments) : m_pAttachments(attachments)
{

}

void EmailDocumentEntry::parse(const QByteArray &data)
{
    m_rawData = data;

    QString boundary;
    QString charset;
    m_contentType = parseContentType(data, &m_name, &boundary, &charset);

    if (boundary.isEmpty())
    {
        if (parseTransferEncoding(data) == TransferEncoding::base64)
        {
            m_content = QByteArray::fromBase64(rawPayload().trimmed()).trimmed();
        }
        else
        {
            m_content = rawPayload().trimmed();
        }

        if (m_contentType.enumerate == EmailDocumentEntry::ContentType::Enum::textOther or
            m_contentType.enumerate == EmailDocumentEntry::ContentType::Enum::textPlain)
        {
            QTextStream stream(m_content);
            stream.setCodec(charset.toStdString().c_str());
            m_content = stream.readAll().toUtf8();
        }

        return;
    }

    if (m_contentType.enumerate == EmailDocumentEntry::ContentType::Enum::multipartRelated or
        m_contentType.enumerate == EmailDocumentEntry::ContentType::Enum::multipartMixed or
        m_contentType.enumerate == EmailDocumentEntry::ContentType::Enum::multipartAlternative or
        m_contentType.enumerate == EmailDocumentEntry::ContentType::Enum::multipartOther)
    {
        multipart(data);
        return;
    }

    // Other MIME types (non-multipart)
    QSharedPointer<EmailDocumentEntry> entry(new EmailDocumentEntry(m_pAttachments));
    entry->parse(rawPayload());
    if (m_pAttachments != nullptr) m_pAttachments->push_back(entry);
}

QByteArray EmailDocumentEntry::rawPayload() const
{
    int pos = m_rawData.indexOf("\n\n");
    if (pos < 0)
    {
        pos = m_rawData.indexOf("\r\n\r\n");
    }
    if (pos < 0)
    {
        return QByteArray();
    }
    return m_rawData.mid(pos).trimmed();
}

QByteArray EmailDocumentEntry::rawHeaders() const
{
    int pos = m_rawData.indexOf("\n\n");
    if (pos < 0)
    {
        pos = m_rawData.indexOf("\r\n\r\n");
    }
    if (pos < 0)
    {
        return QByteArray();
    }
    return m_rawData.mid(0, pos).trimmed();
}

void EmailDocumentEntry::multipart(QByteArray section)
{
    QString lineDelimiter = primaryLineDelimiter(section);
    if (lineDelimiter.isEmpty())
    {
        qWarning() << __FUNCTION__ << "line delimiter not found";
        return;
    }

    QString boundary;
    parseContentType(section, nullptr, &boundary);
    if (boundary.isEmpty())
    {
        qWarning() << __FUNCTION__ << "boundary tag not found" << section.mid(0,100);
        return;
    }

    const QString beginBoundary = "--" + boundary + lineDelimiter;
    const QString endBoundary = "--" + boundary + "--";

    for (qsizetype beginPos = section.indexOf(beginBoundary.toUtf8());
         beginPos > 0;
         beginPos = section.indexOf(beginBoundary.toUtf8(), beginPos+1))
    {
        QByteArray boundaryMarkedPart = section;
        boundaryMarkedPart.remove(0, beginPos);

        qsizetype endPos = boundaryMarkedPart.indexOf(beginBoundary.toUtf8(), 1);
        if (endPos < 0) // last
        {
            endPos = boundaryMarkedPart.indexOf(endBoundary.toUtf8(), 1);
            if (endPos < 0)
            {
                endPos = boundaryMarkedPart.size()-1;
            }
        }

        boundaryMarkedPart.remove(endPos, boundaryMarkedPart.size()-endPos);
        QSharedPointer<EmailDocumentEntry> entry(new EmailDocumentEntry(m_pAttachments));
        entry->parse(boundaryMarkedPart.trimmed());
        if (m_pAttachments != nullptr) m_pAttachments->push_back(entry);
    }
}

EmailDocumentEntry::ContentType EmailDocumentEntry::contentTypeFromString(const QString &string)
{
    ContentType result;

    if      (string.trimmed() == "text/plain")            result.enumerate = ContentType::Enum::textPlain;
    else if (string.trimmed().startsWith("text/"))        result.enumerate = ContentType::Enum::textOther;
    else if (string.trimmed() == "image/jpeg")            result.enumerate = ContentType::Enum::imageJpeg;
    else if (string.trimmed() == "image/png")             result.enumerate = ContentType::Enum::imagePng;
    else if (string.trimmed().startsWith("image/"))       result.enumerate = ContentType::Enum::imageOther;
    else if (string.trimmed() == "multipart/mixed")       result.enumerate = ContentType::Enum::multipartMixed;
    else if (string.trimmed() == "multipart/related")     result.enumerate = ContentType::Enum::multipartRelated;
    else if (string.trimmed() == "multipart/alternative") result.enumerate = ContentType::Enum::multipartAlternative;
    else if (string.trimmed().startsWith("multipart/"))   result.enumerate = ContentType::Enum::multipartOther;
    else                                                  result.enumerate = ContentType::Enum::binary;

    result.string = string.trimmed();
    return result;
}

EmailDocumentEntry::ContentType EmailDocumentEntry::contentTypeToString(ContentType type)
{
    ContentType result;
    result.enumerate = type.enumerate;

    if      (type.enumerate == ContentType::Enum::textPlain)            result.string = "text/plain";
    else if (type.enumerate == ContentType::Enum::textOther)            result.string = "text/";
    else if (type.enumerate == ContentType::Enum::imageJpeg)            result.string = "image/jpeg";
    else if (type.enumerate == ContentType::Enum::imagePng)             result.string = "image/png";
    else if (type.enumerate == ContentType::Enum::imageOther)           result.string = "image/";
    else if (type.enumerate == ContentType::Enum::multipartMixed)       result.string = "multipart/mixed";
    else if (type.enumerate == ContentType::Enum::multipartRelated)     result.string = "multipart/related";
    else if (type.enumerate == ContentType::Enum::multipartAlternative) result.string = "multipart/alternative";
    else if (type.enumerate == ContentType::Enum::multipartOther)       result.string = "multipart/";
    else     result.string = "application/octet-stream" ;

    return result;
}

EmailDocumentEntry::ContentType EmailDocumentEntry::parseContentType(const QByteArray &data, QString* name, QString* boundary, QString* charset)
{
    ContentType result;

    ParseState state = ParseState::other;
    QStringList contentTypeRaw;

    QTextStream stream(data.trimmed());
    QByteArray line = stream.readLine().toUtf8();

    QByteArray buffer;

    for (; not stream.atEnd(); line = stream.readLine().toUtf8())
    {
        if (state == ParseState::contentType)
        {
            if (not line.startsWith('\t') and not line.startsWith(' '))
            {
                while (buffer.contains(";;")) buffer.replace(";;", ";");
                contentTypeRaw = QString(buffer.trimmed()).split(';');

                result = contentTypeFromString(contentTypeRaw.first());
                QStringListIterator iter(contentTypeRaw);
                while (iter.hasNext())
                {
                    auto attr = iter.next();
                    if (attr.trimmed().startsWith("boundary=") and boundary != nullptr)
                    {
                        *boundary = attr.trimmed().remove(0,9).trimmed();
                        boundary->remove('"');
                    }

                    else if (attr.trimmed().startsWith("name=") and name != nullptr)
                    {
                        QByteArray decoded = EmailDocument::decodeMimeString(attr.remove(0,5).trimmed());
                        if (decoded.isEmpty())
                        {
                            *name = attr.trimmed();
                            name->remove('"');
                        }
                        else
                        {
                            *name = decoded;
                        }
                    }

                    else if (attr.trimmed().startsWith("charset=") and charset != nullptr)
                    {
                        *charset = attr.trimmed().remove(0,8).trimmed();
                        charset->remove('"');
                    }
                }

                break;
            }
            else
            {
                buffer += ';' + line;
                continue;
            }
        }

        if (line.startsWith("Content-Type:"))
        {
            state = ParseState::contentType;
            buffer = line.remove(0,13).trimmed();
        }

        else if (line.isEmpty())
        {
            break;
        }
    }

    return result;
}

EmailDocumentEntry::TransferEncoding EmailDocumentEntry::parseTransferEncoding(const QByteArray &data)
{
    TransferEncoding result = TransferEncoding::textPlain;

    QTextStream stream(data.trimmed());
    QString line = stream.readLine();

    for (; not stream.atEnd(); line = stream.readLine())
    {
        if (line.startsWith("Content-Transfer-Encoding:"))
        {
            if (line.contains("base64", Qt::CaseInsensitive))
            {
                result = TransferEncoding::base64;
            }
        }

        else if (line.isEmpty())
        {
            break;
        }
    }

    return result;
}

QString EmailDocumentEntry::primaryLineDelimiter(const QByteArray &data)
{
    QString lineDelimiter;
    int rnPos = data.indexOf("\r\n");
    int nPos = data.indexOf("\n");
    if (rnPos < 0 and nPos < 0)
    {
        qWarning() << __FUNCTION__ << "Invalid input: no line delimiters found";
        return QString();
    }
    else if (rnPos < 0)
    {
        lineDelimiter = "\n";
    }
    else if (nPos < 0)
    {
        lineDelimiter = "\r\n";
    }
    else
    {
        lineDelimiter = (rnPos < nPos ? "\r\n" : "\n");
    }
    return lineDelimiter;
}
