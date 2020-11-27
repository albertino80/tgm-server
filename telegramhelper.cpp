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

TelegramHelper::TelegramHelper(const QString& vToken, const QString &vStorageFolder, const RoutesHandler& vRoutesh):
    token(vToken),
    storageFolder(vStorageFolder),
    routesh(vRoutesh),
    lastUpdateId(0)
{

}

void TelegramHelper::checkUpdates()
{
    qDebug() << "Called" << __FUNCTION__;

    //compose url to check new messages
    QUrl toCall("https://api.telegram.org");
    QUrlQuery query;

    toCall.setPath(QString("/bot%1/getUpdates").arg(token));
    if(lastUpdateId)
        query.addQueryItem("offset", QString::number(lastUpdateId + 1));
    toCall.setQuery(query);

    //ask the server
    QByteArray recvJson = makeHttpGet(toCall);

    //if there is something, parse the update
    if(!recvJson.isEmpty())
        parseGetUpdates(recvJson);
}

bool TelegramHelper::parseGetUpdates(const QByteArray &payload)
{
    bool messageIsOk(false);

    //decode json
    QJsonDocument d = QJsonDocument::fromJson( payload );
    if(!d.isNull())
    {
        QJsonObject root = d.object();
        messageIsOk = (root.contains("ok") && root["ok"].toBool() == true);
        if(messageIsOk && root.contains("result")) {
            //get result array, that contains our messages
            QJsonArray arrResult = root["result"].toArray();
            for(auto aRes: arrResult){
                QJsonObject updateObj = aRes.toObject();

                QString httpPath; //url to call for sending reply
                QJsonDocument jsonDoc; //body of the reply

                //for each message get a properly reply
                bool allOk = parseUpdate(updateObj, httpPath, jsonDoc);
                if(allOk) {
                    if(!httpPath.isEmpty()) {
                        //if there is something, send reply to the user
                        if(replyOnChat(httpPath, jsonDoc))
                            qDebug() << "Delivered ok";
                    }
                }
            }
        }
    }

    return messageIsOk;
}

bool TelegramHelper::parseUpdate(QJsonObject &updateObj, QString &httpPath, QJsonDocument &jsonDoc)
{
    bool allOk(true);

    //update update_id for next call
    int update_id = updateObj["update_id"].toInt();
    if(update_id > lastUpdateId)    lastUpdateId = update_id;

    //we can receive different type of messages
    if(updateObj.contains("message"))
        allOk = parseMessage( updateObj["message"].toObject(), httpPath, jsonDoc);
    if(updateObj.contains("inline_query"))
        allOk = parseInlineQuery( updateObj["inline_query"].toObject(), httpPath, jsonDoc );
    if(updateObj.contains("callback_query"))
        allOk = parseCallbackQuery( updateObj["callback_query"].toObject(), httpPath, jsonDoc );
    if(updateObj.contains("poll_answer"))
        allOk = parsePollAnswer(updateObj["poll_answer"].toObject(), httpPath, jsonDoc);
    if(updateObj.contains("poll"))
        allOk = parsePoll(updateObj["poll"].toObject(), httpPath, jsonDoc);

    return allOk;
}

bool TelegramHelper::parseMessage(const QJsonObject &messageObj, QString &httpPath, QJsonDocument &jsonDoc)
{
    bool allOk(true);
    if(messageObj.contains("chat")) {
        //get user and chat parameters
        QString messageText = messageObj["text"].toString();
        QJsonObject chatObj = messageObj["chat"].toObject();

        int chatId = chatObj["id"].toInt();
        QString first_name = chatObj["first_name"].toString();
        if(first_name.isEmpty())    first_name = chatObj["title"].toString();

        qDebug() << "Message from" << first_name;

        bool doReplyToChat(true);

        //check if it is a command (starts with slash)
        if(messageObj.contains("entities")) {
            QJsonArray nttsArr = messageObj["entities"].toArray();
            for(auto curNtt: nttsArr){
                QJsonObject currNttObj = curNtt.toObject();
                if(currNttObj.contains("type") && currNttObj["type"].toString().compare("bot_command") == 0){
                    allOk = parseCommand(chatId, first_name, messageText, doReplyToChat, httpPath, jsonDoc);
                }
            }
        }

        //if no command handled compose a reply
        if(doReplyToChat) {
            QDateTime messageTime = QDateTime::fromTime_t( uint(messageObj["date"].toInt()) );
            QString toSendText;

            if(messageObj.contains("contact")) {
                //received user contact informations
                QJsonObject contactObj = messageObj["contact"].toObject();
                toSendText = QString("Received contact: %1, from: %2, at %3")
                    .arg(contactObj["phone_number"].toString())
                    .arg(first_name)
                    .arg(messageTime.toString());
            }
            else if(messageObj.contains("location")) {
                //received user location
                QJsonObject locationObj = messageObj["location"].toObject();
                toSendText = QString("Received location: %1, %2, from: %3")
                    .arg(locationObj["latitude"].toDouble(),6,'f')
                    .arg(locationObj["longitude"].toDouble(),6,'f')
                    .arg(first_name);
            }
            else if(messageObj.contains("new_chat_participant")) {
                //received notification that new user is added to a group
                QJsonObject new_chat_participantObj = messageObj["new_chat_participant"].toObject();
                toSendText = QString("Benvenuto: %1").arg(new_chat_participantObj["first_name"].toString());
            }
            else {
                //simple text message, reply with same message
                toSendText = QString("Received: %1, from: %2, at %3")
                    .arg(messageText)
                    .arg(first_name)
                    .arg(messageTime.toString());
            }

            //if there is some text to send, compose message payload
            if(!toSendText.isEmpty())
                allOk = makeMessage(chatId, toSendText, QJsonObject(), httpPath, jsonDoc);
        }
    }
    return allOk;
}

bool TelegramHelper::parseInlineQuery(const QJsonObject &messageObj, QString &httpPath, QJsonDocument &jsonDoc)
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

bool TelegramHelper::parseCallbackQuery(const QJsonObject &messageObj, QString &httpPath, QJsonDocument &jsonDoc)
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

bool TelegramHelper::parsePollAnswer(const QJsonObject &messageObj, QString & /*httpPath*/, QJsonDocument & /*jsonDoc*/)
{
    bool allOk(false);
    if(messageObj.contains("user")) {
        QString first_name = messageObj["user"]["first_name"].toString();
        QJsonArray option_ids = messageObj["option_ids"].toArray();

        qDebug() << "Poll answer from" << first_name << "options:";
        for(const auto curOpt: option_ids){
            qDebug() << curOpt.toInt();
        }

        allOk = true;
    }
    return allOk;
}

bool TelegramHelper::parsePoll(const QJsonObject &messageObj, QString & /*httpPath*/, QJsonDocument & /*jsonDoc*/)
{
    bool allOk(false);
    if(messageObj.contains("question")) {
        QString question = messageObj["question"].toString();
        QJsonArray options = messageObj["options"].toArray();

        qDebug() << "Poll " << question << "summary:";
        for(const auto curOpt: options){
            QJsonObject curOptObj = curOpt.toObject();
            qDebug() << curOptObj["text"].toString() << curOptObj["voter_count"].toInt() << "votes";
        }

        allOk = true;
    }
    return allOk;
}

bool TelegramHelper::parseCommand(int chatId, const QString &first_name, const QString &messageText, bool &unknownCommand, QString &httpPath, QJsonDocument &jsonDoc)
{
    bool allOk(true);

    unknownCommand = false;
    if(messageText.startsWith("/start")) {
        //first time I see this user
        QString toSendText = QString("%1 welcome to BignoDevBot").arg(first_name);
        if(messageText.length() > 6){
            //strip text after /start, the parameter and store it
            QString userParam = messageText.mid(6).trimmed();
            setChatParameter(chatId, userParam);
            if(!userParam.isEmpty()) {
                toSendText.append("\nWith parameter: ");
                toSendText.append(userParam);
            }
        }
        allOk = makeMessage(chatId, toSendText, QJsonObject(), httpPath, jsonDoc);
    }
    else if(messageText.startsWith("/help")) {
        //ask for help
        QString toSendText = "Help message";
        if(messageText.length() > 6) {
            //strip text after /help
            toSendText.append(" on topic: ");
            toSendText.append(messageText.mid(6));
        }

        //if this user has a starting parameter display it
        QString userParam = getChatParameter(chatId, "");
        if(!userParam.isEmpty()) {
            toSendText.append("\nWith parameter: ");
            toSendText.append(userParam);
        }

        allOk = makeMessage(chatId, toSendText, QJsonObject(), httpPath, jsonDoc);
    }
    else if(messageText.startsWith("/gatto")) {
        //send a cat to the user
        QString toSendPic = "https://thiscatdoesnotexist.com/";
        allOk = makePhoto(chatId, toSendPic, httpPath, jsonDoc);
    }
    else if(messageText.startsWith("/posizione")) {
        //send a position to the user
        QString userParam = getChatParameter(chatId, "");
        if(!userParam.isEmpty()) {
            int routeId = userParam.replace("route", "").toInt();
            QPointF toSendPos(0,0);
            bool toSendPosReady = routesh.getPos(routeId, toSendPos);
            if(toSendPosReady) {
                allOk = makePosition(chatId, toSendPos, httpPath, jsonDoc);
            }
            else {
                QString toSendText = "Error fetching position";
                allOk = makeMessage(chatId, toSendText, QJsonObject(), httpPath, jsonDoc);
            }
        }
        else {
            QString toSendText = "Questa chat non è collegata ad alcuna sessione";
            allOk = makeMessage(chatId, toSendText, QJsonObject(), httpPath, jsonDoc);
        }
    }
    else if(messageText.startsWith("/contattami")) {
        //ask for contact informations
        QJsonObject replyMarkup;
        QString toSendText = "Chiedo informazioni di contatto";
        createContactKeyboard(replyMarkup);
        allOk = makeMessage(chatId, toSendText, replyMarkup, httpPath, jsonDoc);
    }
    else if(messageText.startsWith("/domanda")) {
        //ask for a question
        QJsonObject replyMarkup;
        QString toSendText = "Ti faccio una domanda";
        createInlineKeyboard(replyMarkup);
        allOk = makeMessage(chatId, toSendText, replyMarkup, httpPath, jsonDoc);
    }
    else if(messageText.startsWith("/sondaggio")) {
        //make a poll
        allOk = makePoll(chatId, httpPath, jsonDoc);
    }
    else
        unknownCommand = true;

    return allOk;
}

bool TelegramHelper::makeMessage(int chatId, const QString &message, const QJsonObject& replyMarkup, QString &httpPath, QJsonDocument &jsonDoc)
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

bool TelegramHelper::makePhoto(int chatId, const QString &photoUrl, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/sendPhoto").arg(token);

    QJsonObject root;
    root["method"] = "sendPhoto";
    root["chat_id"] = chatId;
    root["photo"] = photoUrl;

    jsonDoc.setObject(root);

    return true;
}

bool TelegramHelper::makePosition(int chatId, const QPointF &thePosition, QString &httpPath, QJsonDocument &jsonDoc)
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

bool TelegramHelper::makeInlineQueryResult(const QString &inlineQueryId, const QJsonArray &articles, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/answerInlineQuery").arg(token);

    QJsonObject root;
    root["method"] = "answerInlineQuery";
    root["inline_query_id"] = inlineQueryId;
    root["results"] = articles;

    jsonDoc.setObject(root);

    return true;
}

bool TelegramHelper::makeAnswerCallbackQuery(const QString& callbackQueryId, const QString &message, QString &httpPath, QJsonDocument &jsonDoc)
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

bool TelegramHelper::makePoll(int chatId, QString &httpPath, QJsonDocument &jsonDoc)
{
    httpPath = QString("/bot%1/sendPoll").arg(token);

    QJsonObject root;
    root["method"] = "sendPoll";
    root["chat_id"] = chatId;
    root["question"] = "Ti piace il c++?";
    root["is_anonymous"] = false;

    QJsonArray optArray;
    optArray.append("Si");
    optArray.append("No");
    optArray.append("Preferisco non rispondere");

    root["options"] = optArray;

    jsonDoc.setObject(root);

    return true;
}

void TelegramHelper::createContactKeyboard(QJsonObject &replyMarkup)
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

void TelegramHelper::createInlineKeyboard(QJsonObject &replyMarkup)
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

QByteArray TelegramHelper::makeHttpGet(const QUrl &toCall)
{
    QNetworkRequest request;
    QEventLoop synchronous;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &synchronous, SLOT(quit()));

    request.setUrl(toCall);

    QNetworkReply *reply = manager.get(request);

    synchronous.exec();

    if (reply->error()) {
        qDebug() << reply->errorString();
        return QByteArray();
    }
    return reply->readAll();
}

QByteArray TelegramHelper::makeHttpPost(const QUrl &toCall, const QString &contentType, const QByteArray &toSend)
{
    QNetworkRequest request;
    QEventLoop synchronous;

    QObject::connect(&manager, SIGNAL(finished(QNetworkReply*)), &synchronous, SLOT(quit()));

    request.setUrl(toCall);
    request.setHeader(QNetworkRequest::ContentTypeHeader,QVariant(contentType));

    QNetworkReply *reply = manager.post(request, toSend);

    synchronous.exec();

    if (reply->error()) {
        qDebug() << reply->errorString();
        return QByteArray();
    }

    return reply->readAll();
}

bool TelegramHelper::replyOnChat(const QString &httpPath, const QJsonDocument &jsonDoc)
{
    //compose url
    QUrl toCall("https://api.telegram.org");
    toCall.setPath( httpPath );

    //send jsonDoc to the server
    QByteArray sended = makeHttpPost(toCall, "application/json", jsonDoc.toJson(QJsonDocument::Compact));

    //if not empty it's ok (not very accurate)
    return !sended.isEmpty();
}

bool TelegramHelper::setChatParameter(int chatId, const QString &paramValue)
{
    QDir sessions(storageFolder);
    if(!sessions.exists())
        qDebug() << "Folder:" << sessions.path() << "does not exists";

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

QString TelegramHelper::getChatParameter(int chatId, const QString &defaultValue)
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
