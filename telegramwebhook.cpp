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
    bool allOk(false);

    //get http payload, our json with message
    const std::string payloadStr = server->getPostData(conn);
    if(payloadStr.empty()) return allOk;
    QByteArray playloadBytes( payloadStr.c_str(), int(payloadStr.size()) );

    //decode as json
    QJsonDocument d = QJsonDocument::fromJson( playloadBytes );
    if(!d.isNull()){
        QJsonObject root = d.object();

        //instatiate helper class (same as polling mode)
        TelegramHelper tgmHelp(token, storageFolder, routesh);

        QString httpPath; //url to call for sending reply
        QJsonDocument jsonDoc; //body of the reply

        //for each message get a properly reply
        allOk = tgmHelp.parseUpdate(root, httpPath, jsonDoc);
        if(allOk) {
            if(!httpPath.isEmpty()) {
                //if there is something, send reply to the user
                if(tgmHelp.replyOnChat(httpPath, jsonDoc)) {
                    qDebug() << "Delivered ok";
                    allOk = true;
                }
            }
        }
    }

    //reply to telegram call, { "ok"=true }
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

    //check if caller support keep-alive
    const char *headerConnection = CivetServer::getHeader(conn, "param");
    if (headerConnection) {
        if( mg_strncasecmp(headerConnection, "keep-alive", 10 ) == 0)
            connectionFlag = "keep-alive";
    }

    //send http header
    int sentData = mg_printf(conn,"HTTP/1.1 %d OK\r\n"
                   "Content-Length: %lu\r\n"
                   "Content-Type: %s\r\n"
                   "Connection: %s\r\n"
                   "\r\n",
                   httpReturnCode, len, contentType.toUtf8().data(), connectionFlag
    );

    //send http payload
    sentData += mg_write(conn, reply.data(), len);

    return sentData;
}
