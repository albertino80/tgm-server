#ifndef TELEGRAMWEBHOOK_H
#define TELEGRAMWEBHOOK_H

#include "CivetServer.h"
#include <QString>
#include <QByteArray>
#include "routeshandler.h"

class TelegramWebHook: public CivetHandler
{
public:
    TelegramWebHook(const QString& vToken, const QString& vStorageFolder, const RoutesHandler& vRoutesh);

    bool handlePost(CivetServer *server, struct mg_connection *conn);

protected:
    int sendResponse(CivetServer *server, struct mg_connection *conn, const QByteArray& reply);

    QString token;
    QString storageFolder;
    const RoutesHandler& routesh;
};


#endif // TELEGRAMWEBHOOK_H
