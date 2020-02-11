#include "telegramwebhook.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "telegramhelper.h"

TelegramWebHook::TelegramWebHook(const QString &vToken, const QString &vStorageFolder, const RoutesHandler &vRoutesh):
    token(vToken),
    storageFolder(vStorageFolder),
    routesh(vRoutesh)
{

}

bool TelegramWebHook::handlePost(CivetServer *server, mg_connection *conn)
{
    const std::string payloadStr = server->getPostData(conn);

    if(payloadStr.empty())
        return false;

    bool allOk(false);

    QByteArray playloadBytes( payloadStr.c_str(), int(payloadStr.size()) );

    QJsonDocument d = QJsonDocument::fromJson( playloadBytes );
    //QJsonDocument d = QJsonDocument::fromRawData( payloadStr.c_str(), int(payloadStr.size()) );
    if(!d.isNull()){

        TelegramHandler tgmHelp(token, storageFolder, routesh);

        QJsonObject root = d.object();

        QString httpPath;
        QJsonDocument jsonDoc;

        allOk = tgmHelp.parseUpdate(root, httpPath, jsonDoc);
        if(allOk) {
            if(!httpPath.isEmpty()) {
                if(tgmHelp.replyOnChat(httpPath, jsonDoc)) {
                    qDebug() << "Delivered ok";
                    allOk = true;
                }
            }
        }
    }

    QJsonObject outPos;
    outPos["ok"] = allOk;
    QJsonDocument saveDoc(outPos);
    return sendResponse(server, conn, saveDoc.toJson(QJsonDocument::Compact) ) > 0;
}

int TelegramWebHook::sendResponse(CivetServer * /*server*/, struct mg_connection *conn, const QByteArray &reply)
{
    int httpReturnCode = 200;
    size_t len( size_t(reply.size()) );
    QString contentType = "application/json; charset=utf-8";
    const char *connectionFlag("close");

    const char *headerConnection = CivetServer::getHeader(conn, "param");
    if (headerConnection) {
        if( strncmp(headerConnection, "keep-alive", 10 ) == 0)
            connectionFlag = "keep-alive";
        else if( strncmp(headerConnection, "Keep-Alive", 10 ) == 0)
            connectionFlag = "keep-alive";
    }

    int sentData = mg_printf(conn,"HTTP/1.1 %d OK\r\n"
                   "Content-Length: %lu\r\n"
                   "Content-Type: %s\r\n"
                   "Connection: %s\r\n"
                   "\r\n",
                   httpReturnCode, len, contentType.toUtf8().data(), connectionFlag
    );

    sentData += mg_write(conn, reply.data(), len);

    return sentData;
}
