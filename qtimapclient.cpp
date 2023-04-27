/*
 * This file is part of QtEmailFetcher project: Email parser for C++ Qt.
 *
 * GPLv3+ (c) acetone, 2023
 */

#include "qtimapclient.h"

#include <QDebug>
#include <QRegularExpression>
#include <QDebug>

QtImapClient::QtImapClient() : m_imapClient ([this](const std::string& strLogMsg) {
        qWarning() << "QtImapClient backend:" << strLogMsg.c_str();
        m_backEndErrors.push_back(strLogMsg.c_str());
    })
{}

QtImapClient::~QtImapClient()
{
    m_imapClient.CleanupSession();
}

bool QtImapClient::checkUnseen(QList<unsigned int> &result)
{
    if (not initConnection())
    {
        m_errorString = "Connection initialize failed";
        return false;
    }

    std::string out;

    bool status = m_imapClient.Search(out, CIMAPClient::SearchOption::UNSEEN);
    if (not status)
    {
        m_errorString = "Fetch unseen messages failed";
        return false;
    }

    static const QRegularExpression onlyNumbers("[^ 0-9]");
    QString string = QString::fromStdString(out.c_str());
    string.remove(onlyNumbers);
    while (string.contains("  ")) string.remove("  ");
    QStringList strList = string.split(' ');

    if (not result.isEmpty()) result.clear();

    QStringListIterator iter(strList);
    while (iter.hasNext())
    {
        bool ok = false;
        auto val = iter.next().toUInt(&ok);
        if (ok)
        {
            result.push_back(val);
        }
    }
    return true;
}

QSharedPointer<EmailDocument> QtImapClient::fetch(unsigned int mailIndex)
{
    if (not initConnection())
    {
        m_errorString = "Connection initialize failed";
        return nullptr;
    }

    std::string output;
    bool fetchStatus = m_imapClient.GetString(std::to_string(mailIndex), output);

    if (not fetchStatus)
    {
        m_errorString = "Fetching failed";
        return nullptr;
    }

    QSharedPointer<EmailDocument> document(new EmailDocument);
    document->parse(output.c_str());
    return document;
}

bool QtImapClient::initConnection()
{
    QMutexLocker lock (&m_mtxInit);

    if (m_inited) return true;

    CMailClient::SslTlsFlag ssl;
    if (m_connectionType == ConnectionType::PLAIN_TEXT)
    {
        ssl = CMailClient::SslTlsFlag::NO_SSLTLS;
    }
    else if (m_connectionType == ConnectionType::SSL)
    {
        ssl = CMailClient::SslTlsFlag::ENABLE_SSL;
    }
    else if (m_connectionType == ConnectionType::START_TLS)
    {
        ssl = CMailClient::SslTlsFlag::ENABLE_TLS;
    }
    else
    {
        qCritical() << __FUNCTION__ << "Unknown SSL status";
    }

    if (not m_proxy.isEmpty())
    {
        m_imapClient.SetProxy(m_proxy.toStdString());
    }

    m_inited = m_imapClient.InitSession((m_hostname + ":" + QString::number(m_port)).toStdString(),
                                     m_username.toStdString(), m_password.toStdString(),
                                     CMailClient::SettingsFlag::ALL_FLAGS, ssl);

    return m_inited;
}
