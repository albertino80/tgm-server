#include "telegramhelper.h"

#include <QDebug>
#include <QDateTime>
#include <QDir>

#include <QEventLoop>
#include <QtNetwork/QNetworkReply>
#include <QUrlQuery>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

TelegramHandler::TelegramHandler(const QString& vToken, const QString &vStorageFolder, const RoutesHandler& vRoutesh):
    token(vToken),
    storageFolder(vStorageFolder),
    routesh(vRoutesh),
    lastUpdateId(0)
{

}

void TelegramHandler::pingPong()
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

    parseGetUpdates(reply->readAll());
}

bool TelegramHandler::parseGetUpdates(const QByteArray &payload)
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

                QString httpPath;
                QJsonDocument jsonDoc;

                bool allOk = parseUpdate(updateObj, httpPath, jsonDoc);
                if(allOk) {
                    if(!httpPath.isEmpty()) {
                        if(replyOnChat(httpPath, jsonDoc))
                            qDebug() << "Delivered ok";
                    }
                }
            }
        }
    }

    return messageIsOk;
}

bool TelegramHandler::parseUpdate(QJsonObject &updateObj, QString &httpPath, QJsonDocument &jsonDoc)
{
    bool allOk(true);

    int update_id = updateObj["update_id"].toInt();
    if(update_id > lastUpdateId)    lastUpdateId = update_id;

    if(updateObj.contains("message"))
        allOk = parseMessage( updateObj["message"].toObject(), httpPath, jsonDoc);
    if(updateObj.contains("inline_query"))
        allOk = parseInlineQuery( updateObj["inline_query"].toObject(), httpPath, jsonDoc );
    if(updateObj.contains("callback_query"))
        allOk = parseCallbackQuery( updateObj["callback_query"].toObject(), httpPath, jsonDoc );

    return allOk;
}

bool TelegramHandler::parseMessage(const QJsonObject &messageObj, QString &httpPath, QJsonDocument &jsonDoc)
{
    bool allOk(false);
    if(messageObj.contains("chat")) {
        QString messageText = messageObj["text"].toString();
        QDateTime messageTime = QDateTime::fromTime_t( uint(messageObj["date"].toInt()) );

        QJsonObject chatObj = messageObj["chat"].toObject();

        int chatId = chatObj["id"].toInt();
        QString first_name = chatObj["first_name"].toString();

        qDebug() << "Message from" << first_name;

        QJsonObject replyMarkup;
        QString toSendText;
        QString toSendPic;
        QPointF toSendPos(0,0);
        bool toSendPosReady(false);

        if(messageObj.contains("entities")) {
            QJsonArray nttsArr = messageObj["entities"].toArray();
            for(auto curNtt: nttsArr){
                QJsonObject currNttObj = curNtt.toObject();
                if(currNttObj.contains("type") && currNttObj["type"].toString().compare("bot_command") == 0){
                    if(messageText.startsWith("/start")) {
                        toSendText = QString("%1 welcome to BignoDevBot").arg(first_name);
                        if(messageText.length() > 6){
                            QString userParam = messageText.mid(6).trimmed();
                            setChatParameter(chatId, userParam);
                            if(!userParam.isEmpty()) {
                                toSendText.append("\nWith parameter: ");
                                toSendText.append(userParam);
                            }
                        }
                    }
                    else if(messageText.startsWith("/help")) {
                        toSendText = "Help message";
                        if(messageText.length() > 6) {
                            toSendText.append(" on topic: ");
                            toSendText.append(messageText.mid(6));
                        }

                        QString userParam = getChatParameter(chatId, "");
                        if(!userParam.isEmpty()) {
                            toSendText.append("\nWith parameter: ");
                            toSendText.append(userParam);
                        }
                    }
                    else if(messageText.startsWith("/gatto")) {
                        toSendPic = "https://thiscatdoesnotexist.com/";
                    }
                    else if(messageText.startsWith("/posizione")) {
                        QString userParam = getChatParameter(chatId, "");
                        if(!userParam.isEmpty()) {
                            int routeId = userParam.replace("route", "").toInt();
                            toSendPosReady = routesh.getPos(routeId, toSendPos);
                        }
                        else {
                            toSendText = "Questa chat non è collegata ad alcuna sessione";
                        }
                    }
                    else if(messageText.startsWith("/contattami")) {
                        toSendText = "Chiedo informazioni di contatto";
                        createContactKeyboard(replyMarkup);
                    }
                    else if(messageText.startsWith("/domanda")) {
                        toSendText = "Ti faccio una domanda";
                        createInlineKeyboard(replyMarkup);
                    }
                }
            }
        }

        if(toSendText.isEmpty() && toSendPic.isEmpty() && !toSendPosReady) {

            if(messageObj.contains("contact")) {
                QJsonObject contactObj = messageObj["contact"].toObject();
                toSendText = QString("Received contact: %1, from: %2, at %3")
                    .arg(contactObj["phone_number"].toString())
                    .arg(first_name)
                    .arg(messageTime.toString());
            }
            else if(messageObj.contains("location")) {
                QJsonObject locationObj = messageObj["location"].toObject();
                toSendText = QString("Received location: %1-%2, from: %3, at %4")
                    .arg(locationObj["latitude"].toDouble(),6,'f')
                    .arg(locationObj["longitude"].toDouble(),6,'f')
                    .arg(first_name)
                    .arg(messageTime.toString());
            }
            else {
                toSendText = QString("Received: %1, from: %2, at %3")
                    .arg(messageText)
                    .arg(first_name)
                    .arg(messageTime.toString());
            }
        }

        if(!toSendText.isEmpty())
            allOk = makeMessage(chatId, toSendText, replyMarkup, httpPath, jsonDoc);
        else if(!toSendPic.isEmpty())
            allOk = makePhoto(chatId, toSendPic, httpPath, jsonDoc);
        else if(toSendPosReady)
            allOk = makePosition(chatId, toSendPos, httpPath, jsonDoc);

    }
    return allOk;
}

bool TelegramHandler::parseInlineQuery(const QJsonObject &messageObj, QString &httpPath, QJsonDocument &jsonDoc)
{
    bool allOk(false);
    if(messageObj.contains("from")) {
        QString queryText = messageObj["query"].toString();
        QString queryId = messageObj["id"].toString();

        QJsonObject fromObj = messageObj["from"].toObject();

        //int chatId = fromObj["id"].toInt();
        QString first_name = fromObj["first_name"].toString();

        qDebug() << "Query from" << first_name;

        QVector<QString> allResults;
        allResults.push_back("casa al mare");
        allResults.push_back("casa in montagna");
        allResults.push_back("casa in collina");
        allResults.push_back("hotel al mare");
        allResults.push_back("hotel in montagna");
        allResults.push_back("hotel in collina");

        QJsonArray articles;

        for(int i=0; i<allResults.size(); i++){
            if(!queryText.isEmpty()){
                if(!allResults[i].contains( queryText ,Qt::CaseInsensitive))
                    continue;
            }
            QJsonObject mess;

            mess["message_text"] = allResults[i];

            QJsonObject aSingleResult;
            aSingleResult["type"] = "article";
            aSingleResult["id"] = QString::number(i);
            aSingleResult["title"] = QString("Titolo: %1").arg(allResults[i]);
            aSingleResult["description"] = QString("Descr: %1").arg(allResults[i]);
            aSingleResult["input_message_content"] = mess;
            articles.append(aSingleResult);
        }

        allOk = makeInlineQueryResult(queryId, articles, httpPath, jsonDoc);
    }
    return allOk;
}

bool TelegramHandler::parseCallbackQuery(const QJsonObject &messageObj, QString &httpPath, QJsonDocument &jsonDoc)
{
    bool allOk(false);
    if(messageObj.contains("from")) {
        QString callbackId = messageObj["id"].toString();
        QString callbackData = messageObj["data"].toString();
        QJsonObject fromObj = messageObj["from"].toObject();

        //int chatId = fromObj["id"].toInt();
        QString first_name = fromObj["first_name"].toString();

        qDebug() << "Callback" << callbackData << "from" << first_name;

        QString messageText = QString("You sent: %1").arg(callbackData);

        allOk = makeAnswerCallbackQuery(callbackId, messageText, httpPath, jsonDoc);
    }
    return allOk;
}

bool TelegramHandler::makeMessage(int chatId, const QString &message, QJsonObject& replyMarkup, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/sendMessage").arg(token);

    QJsonObject root;
    root["method"] = "sendMessage";
    root["chat_id"] = chatId;
    root["text"] = message;
    root["reply_markup"] = replyMarkup;

    /*
    QJsonObject forceReply;
    forceReply["force_reply"] = true;
    root["reply_markup"] = forceReply;
    */

    jsonDoc.setObject(root);

    return true;
}

bool TelegramHandler::makePhoto(int chatId, const QString &photoUrl, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/sendPhoto").arg(token);

    QJsonObject root;
    root["method"] = "sendMessage";
    root["chat_id"] = chatId;
    root["photo"] = photoUrl;

    jsonDoc.setObject(root);

    return true;
}

bool TelegramHandler::makePosition(int chatId, const QPointF &thePosition, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/sendLocation").arg(token);

    QJsonObject root;
    root["method"] = "sendLocation";
    root["chat_id"] = chatId;
    root["latitude"] = thePosition.x();
    root["longitude"] = thePosition.y();

    jsonDoc.setObject(root);

    return true;
}

bool TelegramHandler::makeInlineQueryResult(const QString &inlineQueryId, const QJsonArray &articles, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/answerInlineQuery").arg(token);

    QJsonObject root;
    root["method"] = "answerInlineQuery";
    root["inline_query_id"] = inlineQueryId;
    root["results"] = articles;

    jsonDoc.setObject(root);

    return true;
}

bool TelegramHandler::makeAnswerCallbackQuery(const QString& callbackQueryId, const QString &message, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/answerCallbackQuery").arg(token);

    QJsonObject root;
    root["method"] = "answerCallbackQuery";
    root["callback_query_id"] = callbackQueryId;
    root["text"] = message;
    root["show_alert"] = true;

    jsonDoc.setObject(root);

    return true;
}

void TelegramHandler::createContactKeyboard(QJsonObject &replyMarkup)
{
    QJsonArray arrRows;
    QJsonArray arrButtons1;
    QJsonArray arrButtons2;

    {
        QJsonObject curButton;
        curButton["text"] = "Manda la mia posizione";
        curButton["request_location"] = true;
        curButton["request_contact"] = false;
        arrButtons1.append(curButton);
    }

    {
        QJsonObject curButton;
        curButton["text"] = "Manda il mio contatto";
        curButton["request_contact"] = true;
        arrButtons1.append(curButton);
    }

    {
        QJsonObject curButton;
        curButton["text"] = "Manda qualcosa";
        arrButtons2.append(curButton);
    }

    arrRows.append( arrButtons1 );
    arrRows.append( arrButtons2 );
    replyMarkup["keyboard"] = arrRows;
}

void TelegramHandler::createInlineKeyboard(QJsonObject &replyMarkup)
{
    QJsonArray arrRows;
    QJsonArray arrButtons;

    {
        QJsonObject curButton;
        curButton["text"] = "Bottone 1";
        curButton["callback_data"] = "data for bottone 1";
        arrButtons.append(curButton);
    }

    {
        QJsonObject curButton;
        curButton["text"] = "Bottone 2";
        curButton["callback_data"] = "other data";
        arrButtons.append(curButton);
    }

    arrRows.append( arrButtons );
    replyMarkup["inline_keyboard"] = arrRows;
}

bool TelegramHandler::replyOnChat(const QString &httpPath, const QJsonDocument &jsonDoc)
{
    QNetworkRequest request;
    QEventLoop synchronous;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &synchronous, SLOT(quit()));

    QUrl toCall("https://api.telegram.org");
    toCall.setPath( httpPath );

    request.setUrl(toCall);
    request.setHeader(QNetworkRequest::ContentTypeHeader,QVariant("application/json"));

    QNetworkReply *reply = manager.post(request, jsonDoc.toJson(QJsonDocument::Compact));

    synchronous.exec();

    if (reply->error()) {
        qDebug() << reply->errorString();
        return false;
    }

    return true;
}

bool TelegramHandler::setChatParameter(int chatId, const QString &paramValue)
{
    QDir sessions(storageFolder);
    if(!sessions.exists())
        sessions.mkdir(".");

    bool allOk(false);
    QFile file(sessions.filePath(QString("chat_%1.dat").arg(chatId)));
    if(file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&file);
        stream << paramValue;
        file.close();
        allOk = true;
    }

    return allOk;
}

QString TelegramHandler::getChatParameter(int chatId, const QString &defaultValue)
{
    QDir sessions(storageFolder);

    QString paramValue;

    QFile file(sessions.filePath(QString("chat_%1.dat").arg(chatId)));
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream stream(&file);
        paramValue = stream.readLine();
        file.close();
    }

    if(paramValue.isEmpty())
        paramValue = defaultValue;

    return paramValue;
}
