/*
 * This file is part of QtEmailFetcher project: Email parser for C++ Qt.
 *
 * GPLv3+ (c) acetone, 2023
 */

#pragma once

#include "emaildocument.h"

#include "IMAPClient.h"

#include <QMutex>
#include <QString>
#include <QSharedPointer>

class QtImapClient {

public:
    enum class ConnectionType { START_TLS, SSL, PLAIN_TEXT };

    QtImapClient();
    ~QtImapClient();
    void setHostname(const QString& host)       { m_hostname = host; }
    void setPort(quint16 port)                  { m_port = port; }
    void setConnectionType(ConnectionType type) { m_connectionType = type; }
    void setHttpProxy(const QString& address)   { m_proxy = address; }
    void setPassword(const QString& password)   { m_password = password; }
    void setUsername(const QString& username)   { m_username = username; }

    bool checkUnseen(QList<unsigned int>& result);
    QSharedPointer<EmailDocument> fetch(unsigned int mailIndex);

    QString errorString() const { return m_errorString; }

private:
    bool initConnection();
    bool m_inited = false;
    QMutex m_mtxInit;

    CIMAPClient m_imapClient;

    ConnectionType m_connectionType = ConnectionType::START_TLS;

    QString m_username;
    QString m_password;

    QString m_hostname;
    quint16 m_port = 993;
    QString m_proxy;

    QString m_errorString;
    QStringList m_backEndErrors;
};
