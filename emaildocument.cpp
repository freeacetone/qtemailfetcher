/*
 * This file is part of QtEmailFetcher project: Email parser for C++ Qt.
 *
 * GPLv3+ (c) acetone, 2023
 */

#include "emaildocument.h"

#include <QDebug>
#include <QRegularExpression>

EmailDocument::EmailDocument()
{

}

enum class ParseState {
    subject,
    from,
    other
};

void EmailDocument::parse(const QByteArray &data)
{
    m_rawData = data;

    QTextStream stream(data);
    QString line = stream.readLine();

    QByteArray buffer;
    ParseState state = ParseState::other;

    for (; not stream.atEnd(); line = stream.readLine())
    {
        if (state == ParseState::subject)
        {
            if (not line.startsWith('\t') and not line.startsWith(' '))
            {
                m_subject = buffer.trimmed();
                while (m_subject.contains("  ")) m_subject.replace("  ", " ");
                buffer.clear();
                state = ParseState::other;
            }
            else
            {
                QByteArray tmp = decodeMimeString(line);
                if (tmp.isEmpty())
                {
                    buffer += " " + line.toUtf8().trimmed();
                }
                else
                {
                    buffer += " " + tmp.trimmed();
                }
                continue;
            }
        }
        else if (state == ParseState::from)
        {
            if (not line.startsWith('\t') and not line.startsWith(' '))
            {
                m_from.address = extractAddress(buffer);
                m_from.name = extractName(buffer);
                buffer.clear();
                state = ParseState::other;
            }
            else
            {
                buffer += line.trimmed().toUtf8();
            }
        }

        if (line.isEmpty())
        {
            parseBody();
            return;
        }

        if (line.startsWith("Return-Path:", Qt::CaseInsensitive))
        {
            m_returnPath = extractAddress(line.remove(0,11));
        }

        else if (line.startsWith("From:", Qt::CaseInsensitive))
        {
            buffer = line.remove(0,5).trimmed().toUtf8();
            state = ParseState::from;
        }

        else if (line.startsWith("To:", Qt::CaseInsensitive))
        {
            m_to.address = extractAddress(line.remove(0,3));
            m_to.name = extractName(line);
        }

        else if (line.startsWith("Subject:", Qt::CaseInsensitive))
        {
            buffer = decodeMimeString(line);
            if (buffer.isEmpty()) // not encoded or really empty
            {
                buffer = line.remove(0, 9).toUtf8().trimmed();
            }

            if (state != ParseState::subject)
            {
                state = ParseState::subject;
            }
        }

        else if (line.startsWith("Date:"))
        {
            m_dateTime = decodeTimeString(line.remove(0,5).trimmed());
        }
    }
}

QByteArray EmailDocument::decodeMimeString(const QString &mimeString)
{
    int charset_pos = mimeString.indexOf('?');
    if (charset_pos < 0)
    {
//        qDebug() << __FUNCTION__ << "Missing Q codec separator for charset (first)";
        return QByteArray();
    }

    int method_pos = mimeString.indexOf('?', charset_pos+1);
    if (method_pos < 0)
    {
//        qDebug() << __FUNCTION__ << "Missing Q codec separator for charset (second)";
        return QByteArray();
    }

    QString charset = mimeString.mid(charset_pos + 1, method_pos - charset_pos - 1).toUpper();
    if (charset.isEmpty())
    {
//        qDebug() << __FUNCTION__ << "Missing Q codec charset";
        return QByteArray();
    }

    if (mimeString.size() <= method_pos+1)
    {
        qDebug() << __FUNCTION__ << "Input string too small";
        return QByteArray();
    }
    QChar encodeType = mimeString.at(method_pos+1).toUpper();

    bool base64 = false;
    if (encodeType == 'B')
    {
        base64 = true;
    }
    else if (encodeType != 'Q')
    {
//        qDebug() << __FUNCTION__ << "Unsupported encoding type:" << encodeType;
        return QByteArray();
    }

    int content_pos = mimeString.indexOf('?', method_pos + 1);
    if (content_pos < 0)
    {
//        qDebug() << __FUNCTION__ << "Missing Q codec separator for content (first)";
        return QByteArray();
    }

    int content_end_pos = mimeString.indexOf('?', content_pos + 1);
    if (content_end_pos < 0)
    {
//        qDebug() << __FUNCTION__ << "Missing Q codec separator for content (second)";
        return QByteArray();
    }

    QString input = mimeString.mid(content_pos + 1, content_end_pos - content_pos - 1);
    input.remove("\r");
    input.remove("\n");

    if (base64)
    {
        QByteArray result = QByteArray::fromBase64(input.toUtf8());
        QTextStream stream(result);
        stream.setCodec(charset.toStdString().c_str());
        return stream.readAll().toUtf8();
    }

    while (input.contains("==")) input.remove("==");

    QByteArray decodedText;

    for (int i = 0; i < input.length(); i++) {
        QChar c = input.at(i);

        if (c == '=' and input.length() > i+2)
        {
            QString hexCode = input.mid(i + 1, 2);
            decodedText.push_back(QByteArray::fromHex(hexCode.toUtf8()));
            i += 2;
        }
        else
        {
            decodedText.push_back(c.toLatin1());
        }
    }

    QTextStream stream(decodedText);
    stream.setCodec(charset.toStdString().c_str());
    return stream.readAll().toUtf8();
}

QString EmailDocument::extractAddress(const QString &string)
{
    if (string.contains('<'))
    {
        static const QRegularExpression rgxBegin("^.*<");
        static const QRegularExpression rgxEnd(">.*$");
        return QString(string).remove(rgxBegin).remove(rgxEnd).trimmed();
    }
    else
    {
        static const QRegularExpression rgx("^.* ");
        return QString(string).remove(rgx);
    }
}

QString EmailDocument::extractName(const QString &string)
{
    if (not string.contains('<') or not string.contains('>')) return QString();

    static const QRegularExpression rgx("<.*$");
    QString raw = QString(string).remove(rgx).trimmed();
    QString decoded = decodeMimeString(raw);
    return decoded.isEmpty() ? raw : decoded;
}

QDateTime EmailDocument::decodeTimeString(const QString &stringRFC822_1123)
{
    if (stringRFC822_1123.size() < 25) return QDateTime();
    QString timeString (stringRFC822_1123);

    timeString.remove(0,4); // "ddd,
    int timeShortWordPos = timeString.indexOf('(');
    if (timeShortWordPos > 0)
    {
        timeString.remove(timeShortWordPos, timeString.size()-timeShortWordPos);
    }

    int plusPos = timeString.indexOf('+');
    if (plusPos < 0 or plusPos >= timeString.size()+3) return QDateTime();
    int receivedUtcOffset = timeString.mid(plusPos+1).trimmed().remove("0").toInt() * 60 * 60;

    timeString.remove(plusPos-1, timeString.size()-plusPos+1);

    static const std::map<const char*, const char*> MONTH_CODES {
        {"Jan", "01"},
        {"Feb", "02"},
        {"Mar", "03"},
        {"Apr", "04"},
        {"May", "05"},
        {"Jun", "06"},
        {"Jul", "07"},
        {"Aug", "08"},
        {"Sep", "09"},
        {"Oct", "10"},
        {"Nov", "11"},
        {"Dec", "12"}
    };

    for (const auto& m: MONTH_CODES)
    {
        if (timeString.contains(m.first))
        {
            timeString.replace(m.first, m.second);
            break;
        }
    }

    auto result = QDateTime::fromString(timeString.trimmed(), "d MM yyyy hh:mm:ss");
    result.setOffsetFromUtc(receivedUtcOffset);
    return result;
}

void EmailDocument::parseBody()
{
    QSharedPointer<EmailDocumentEntry> entry(new EmailDocumentEntry(&m_content));
    entry->parse(m_rawData);
    m_content.push_front(entry); // main object to begin

    QList<int> toRemove;
    int counter = 0;
    for (QListIterator<QSharedPointer<EmailDocumentEntry>> iter(m_content); iter.hasNext(); counter++)
    {
        auto n = iter.next()->contentType();
        auto type = n.enumerate;
        if (type == EmailDocumentEntry::ContentType::Enum::multipartRelated or
            type == EmailDocumentEntry::ContentType::Enum::multipartMixed or
            type == EmailDocumentEntry::ContentType::Enum::multipartAlternative or
            type == EmailDocumentEntry::ContentType::Enum::multipartOther)
        {
            toRemove.push_front(counter);
        }
    }
    // Store only text/files, not a abstract structures
    for (QListIterator<int> iter(toRemove); iter.hasNext(); )
    {
        m_content.removeAt(iter.next());
    }
}
