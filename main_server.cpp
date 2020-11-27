#include "CivetServer.h"
#include <string>
#include <thread>

static volatile bool exitNow = false;
//#define MINIMAL_EMPTY_SERVER

#ifndef MINIMAL_EMPTY_SERVER

#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QDebug>
#include <QDir>
#include "routeshandler.h"
#include "telegramhelper.h"
#include "telegramwebhook.h"

bool parseConfig(QString& tgmToken, QString& storageRoot, bool& tgmPolling, std::vector<std::string>& options){
    if(!QFile::exists("server.ini")){
        qDebug() << "Missing server.ini";
        return false;
    }

    QSettings settings("server.ini", QSettings::IniFormat);

    tgmToken = settings.value("telegram/token").toString();

    if(tgmToken.isEmpty()) {
        qDebug() << "Missing telegram/token in server.ini";
        return false;
    }

    tgmPolling = settings.value("telegram/polling", "N").toString().compare("Y", Qt::CaseInsensitive) == 0;
    storageRoot = settings.value("general/sessions", "storage").toString();

    QDir sessions(storageRoot);
    if(!sessions.exists()) {
        qDebug() << "Folder:" << sessions.path() << "does not exists";
        return false;
    }

    auto getOpt = [&](const QString& varName, const QString& defVal = ""){
        QString optStr = settings.value(varName, defVal).toString();
        return optStr.toStdString();
    };

    options.push_back( "document_root" );
    options.push_back( getOpt("general/document_root", "www") );
    options.push_back( "listening_ports" );
    options.push_back( getOpt("general/port", "8081") );

    return true;
}

int main(int argc, char * argv[])
{
    QCoreApplication a(argc, argv);

    //initialize the BOT server
    QSettings settings("server.ini", QSettings::IniFormat);

    QString tgmToken, storageRoot;
    bool tgmPolling(false);
    std::vector<std::string> options;

    if(!parseConfig(tgmToken, storageRoot, tgmPolling, options))
        return 1;

    //start web server
    CivetServer server(options);

    //register web handlers
    RoutesHandler h_r;
    h_r.parseRoutes();
    server.addHandler("/routes", h_r);

    TelegramWebHook tgmHook(tgmToken, storageRoot, h_r);
    server.addHandler("/telegramNewUpdate", tgmHook); //this is for tgm webhook

    //helper object for polling mode
    TelegramHelper pollingLoop(tgmToken, storageRoot, h_r);
    while (!exitNow) {
        std::this_thread::sleep_for (std::chrono::seconds(2));
        if(tgmPolling) //only if polling enabled
            pollingLoop.checkUpdates();
    }

    return 0;
}

#else

class ExampleHandler : public CivetHandler
{
  public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\nContent-Type: "
                  "text/html\r\nConnection: close\r\n\r\n");
        mg_printf(conn,
                  "<html><body>\r\n");
        mg_printf(conn,
                  "<h2>This is an example text from a C++ handler</h2>\r\n");

        std::string s = "";
        if (server->getParam(conn, "param", s))
            mg_printf(conn, "<p>param set to %s</p>", s.c_str());
        else
            mg_printf(conn, "<p>param not set</p>");

        mg_printf(conn, "</body></html>\r\n");
        return true;
    }
};

int main(int /*argc*/, char * /*argv*/[])
{
    CivetServer server({
       "document_root", ".",
       "listening_ports", "8081"
    });

    ExampleHandler h_example;
    server.addHandler("/example", h_example);

    while (!exitNow)
        std::this_thread::sleep_for (std::chrono::seconds(2));

    return 0;
}

#endif
