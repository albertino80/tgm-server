#include "telegrampoll.h"

#include <QDebug>
#include <QDateTime>

#include <QEventLoop>
#include <QtNetwork/QNetworkReply>
#include <QUrlQuery>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

TelegramPoll::TelegramPoll(const QString& vToken):
    token(vToken),
    lastUpdateId(0)
{

}

void TelegramPoll::pingPong()
{
    qDebug() << "Called" << __FUNCTION__;

    QNetworkRequest request;
    QEventLoop synchronous;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &synchronous, SLOT(quit()));

    QUrl toCall("https://api.telegram.org");
    QUrlQuery query;

    toCall.setPath(QString("/bot%1/getUpdates").arg(token));
    if(lastUpdateId)
        query.addQueryItem("offset", QString::number(lastUpdateId + 1));
    toCall.setQuery(query);

    request.setUrl(toCall);

    QNetworkReply *reply = manager.get(request);

    synchronous.exec();

    if (reply->error()) {
        qDebug() << reply->errorString();
        return;
    }

    if( parseGetUpdates(reply->readAll()) ) {
        qDebug() << "Delivered ok";
    }
}

bool TelegramPoll::parseGetUpdates(const QByteArray &payload)
{
    bool messageIsOk(false);

    QJsonDocument d = QJsonDocument::fromJson( payload );
    if(!d.isNull())
    {
        QJsonObject root = d.object();
        messageIsOk = (root.contains("ok") && root["ok"].toBool() == true);
        if(messageIsOk && root.contains("result")) {
            QJsonArray arrResult = root["result"].toArray();
            for(auto aRes: arrResult){
                QJsonObject updateObj = aRes.toObject();

                if(updateObj.contains("message")) {

                    int update_id = updateObj["update_id"].toInt();
                    if(update_id > lastUpdateId)
                        lastUpdateId = update_id;

                    QJsonObject messageObj = updateObj["message"].toObject();
                    if(messageObj.contains("chat")) {
                        QString messageText = messageObj["text"].toString();
                        QDateTime messageTime = QDateTime::fromTime_t( uint(messageObj["date"].toInt()) );

                        QJsonObject chatObj = messageObj["chat"].toObject();

                        int chatId = chatObj["id"].toInt();
                        QString first_name = chatObj["first_name"].toString();

                        qDebug() << "Message from" << first_name;

                        QString toSend = QString("Received: %1, from: %2, at %3")
                                .arg(messageText)
                                .arg(first_name)
                                .arg(messageTime.toString());

                        sendMessage(chatId, toSend);
                    }
                }
            }
        }
    }

    return messageIsOk;
}

bool TelegramPoll::sendMessage(int chatId, const QString &message)
{
    QNetworkRequest request;
    QEventLoop synchronous;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &synchronous, SLOT(quit()));

    QUrl toCall("https://api.telegram.org");
    QUrlQuery query;

    toCall.setPath(QString("/bot%1/sendMessage").arg(token));
    query.addQueryItem("chat_id", QString::number(chatId));
    query.addQueryItem("text", QUrl::toPercentEncoding(message));
    toCall.setQuery(query);
    request.setUrl(toCall);

    QNetworkReply *reply = manager.get(request);

    synchronous.exec();

    if (reply->error()) {
        qDebug() << reply->errorString();
        return false;
    }

    return true;
}
