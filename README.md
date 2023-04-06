# QtEmailFetcher
IMAP client and e-mail document parser for C++ Qt5.

Dependenсies: libcurl, Qt.

A little example which will fetch all your unreaded messages with short summary:

```
#include "qtimapclient.h"
#include <QDebug>
#include <QThread>

int main() 
{
    QtImapClient imap;
    imap.setConnectionType(QtImapClient::ConnectionType::SSL);
    imap.setHostname("imap.server.com");
    imap.setPort(993);
    imap.setUsername("yournick@server.com");
    imap.setPassword("IHateTheC++");
    
    QList<unsigned int> unreaded;
    if (not imap.checkUnseen(unreaded))
    {
        qInfo() << "Fetch failed:" << imap.errorString();
        return 1;
    }
    qInfo() << "Unreaded emails:" << unreaded;
    
    for (const auto& un: unreaded)
    {
        qInfo().noquote() << "\nFetching:" << un;
        
        auto email = imap.fetch(un);
        if (email == nullptr)
        {
            qInfo() << "Hmmm. Email is nullptr...";
            continue;
        }
        
        qInfo() << email->from().address << email->from().name;
        qInfo() << email->subject();
        qInfo() << email->dateTime();
        qInfo() << "Payload size:" << email->payload().size();
        
        for (const auto& part: email->payload())
        {
            qInfo() << part->contentType().string << part->content().size();
        }
        
        QThread::sleep(2);
    }
    return 0;
}
```
