#ifndef TELEGRAMPOLL_H
#define TELEGRAMPOLL_H

#include "CivetServer.h"
#include <QString>
#include <QByteArray>
#include <QtNetwork/QNetworkAccessManager>
#include "routeshandler.h"

class TelegramHandler: public CivetHandler
{
public:
    TelegramHandler(const QString& vToken, const QString& vStorageFolder, const RoutesHandler& vRoutesh);
    void pingPong();

    bool parseUpdate(QJsonObject& updateObj, QString &httpPath, QJsonDocument &jsonDoc );
    bool replyOnChat(const QString& httpPath, const QJsonDocument& jsonDoc);

    bool setChatParameter(int chatId, const QString& paramValue);
    QString getChatParameter(int chatId, const QString& defaultValue);

protected:
    bool parseGetUpdates(const QByteArray& payload);

    bool parseMessage(const QJsonObject& messageObj, QString &httpPath, QJsonDocument &jsonDoc);
    bool parseInlineQuery(const QJsonObject& messageObj, QString &httpPath, QJsonDocument &jsonDoc);
    bool parseCallbackQuery(const QJsonObject& messageObj, QString &httpPath, QJsonDocument &jsonDoc);
    bool parsePollAnswer(const QJsonObject& messageObj, QString &httpPath, QJsonDocument &jsonDoc);
    bool parsePoll(const QJsonObject& messageObj, QString &httpPath, QJsonDocument &jsonDoc);
    bool parseCommand(int chatId, const QString& first_name, const QString& messageText, bool& unknownCommand, QString &httpPath, QJsonDocument &jsonDoc);

    bool makeMessage(int chatId, const QString& message, const QJsonObject& replyMarkup, QString& httpPath, QJsonDocument& jsonDoc);
    bool makePhoto(int chatId, const QString& photoUrl, QString& httpPath, QJsonDocument& jsonDoc);
    bool makePosition(int chatId, const QPointF& thePosition, QString& httpPath, QJsonDocument& jsonDoc);
    bool makeInlineQueryResult(const QString& inlineQueryId, const QJsonArray& articles, QString& httpPath, QJsonDocument& jsonDoc);
    bool makeAnswerCallbackQuery(const QString &callbackQueryId, const QString& message, QString& httpPath, QJsonDocument& jsonDoc);
    bool makePoll(int chatId, QString& httpPath, QJsonDocument& jsonDoc);

    void createContactKeyboard(QJsonObject& replyMarkup);
    void createInlineKeyboard(QJsonObject& replyMarkup);

    QString token;
    QString storageFolder;
    const RoutesHandler& routesh;

    int lastUpdateId;

    QNetworkAccessManager manager;
};

#endif // TELEGRAMPOLL_H
