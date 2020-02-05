#ifndef TELEGRAMPOLL_H
#define TELEGRAMPOLL_H
#include <QString>
#include <QByteArray>
#include <QtNetwork/QNetworkAccessManager>

class TelegramPoll
{
public:
    TelegramPoll(const QString& vToken);
    void pingPong();

protected:
    bool parseGetUpdates(const QByteArray& payload);
    bool sendMessage(int chatId, const QString& message);

    QString token;
    int lastUpdateId;

    QNetworkAccessManager manager;
};

#endif // TELEGRAMPOLL_H
