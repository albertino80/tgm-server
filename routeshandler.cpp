#include "routeshandler.h"
#include <QDir>
#include <QDebug>
#include <QtXml>

RoutesHandler::RoutesHandler()
{

}

bool RoutesHandler::parseRoutes()
{
    bool allOk(true);

    QDir directory("example");
    QStringList gpxFiles = directory.entryList(QStringList() << "*.gpx",QDir::Files);
    foreach(QString filename, gpxFiles) {

        QDomDocument xmlBOM;
        QFile f(directory.filePath(filename));
        if (!f.open(QIODevice::ReadOnly ))
        {
            // Error while loading file
            qDebug() << "Error while loading file";
            continue;
        }

        xmlBOM.setContent(&f);

        QDomElement root = xmlBOM.documentElement();
        if( root.tagName().compare("gpx") == 0 ) {

            QVector<QPointF> gpxPoint;
            QVector<double>  gpxDist;
            double totalDistance(0.0);

            QDomElement Component=root.firstChild().toElement();
            while(!Component.isNull())
            {
                if(Component.tagName().compare("metadata", Qt::CaseInsensitive) == 0) {
                    QDomElement nameTag = Component.firstChildElement("name");
                    if(!nameTag.isNull()) {
                        qDebug() << "Descrizione: " << nameTag.text();
                    }
                }
                else if(Component.tagName().compare("wpt", Qt::CaseInsensitive) == 0) {
                    //qDebug() << "lat:" << Component.attribute("lat").toDouble() << "lon" << Component.attribute("lon").toDouble();

                    QPointF readPoint(
                        Component.attribute("lat").toDouble(),
                        Component.attribute("lon").toDouble()
                    );

                    if(gpxPoint.isEmpty()) {
                        gpxPoint.push_back( readPoint );
                        gpxDist.push_back( 0.0 );
                    }
                    else {
                        const QPointF& precPoint = gpxPoint.back();
                        double dist = qSqrt( qPow( readPoint.x() - precPoint.x(), 2) + qPow( readPoint.y() - precPoint.y(), 2) );

                        gpxPoint.push_back( readPoint );
                        gpxDist.push_back( dist );
                        totalDistance += dist;
                    }
                }
                Component = Component.nextSibling().toElement();
            }

            if(!gpxPoint.isEmpty()) {
                addRoute(gpxPoint, gpxDist, totalDistance);
            }
        }

        f.close();
    }

    return allOk;
}

bool RoutesHandler::handleGet(CivetServer *server, mg_connection *conn)
{
    QTime now( QTime::currentTime() );

    QJsonArray outArray;
    for(auto& curRoute: routes) {

        int iPoint = now.second() % curRoute.size();

        QJsonObject jPoint;
        jPoint["ordinal"] = iPoint;
        jPoint["lat"] = curRoute[iPoint].rx();
        jPoint["lon"] = curRoute[iPoint].ry();
        outArray.push_back(jPoint);
    }

    QJsonObject outPos;
    outPos["positions"] = outArray;
    QJsonDocument saveDoc(outPos);
    return sendResponse(server, conn, saveDoc.toJson(QJsonDocument::Compact) ) > 0;
}

bool RoutesHandler::getPos(int routeId, QPointF &thePosition) const
{
    if(routeId >= routes.size() || routeId < 0)
        return false;

    const QVector<QPointF>& curRoute = routes[routeId];
    QTime now( QTime::currentTime() );
    int iPoint = now.second() % curRoute.size();

    thePosition.setX( curRoute[iPoint].x() );
    thePosition.setY( curRoute[iPoint].y() );

    return true;
}

int RoutesHandler::sendResponse(CivetServer * /*server*/, struct mg_connection *conn, const QByteArray &reply)
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

QPointF CalculatePoint(QPointF a, QPointF b, double magnitude, double distFromA) {
      QPointF newPoint;
      newPoint.setX( a.x() + (distFromA * ((b.x() - a.x()) / magnitude)) );
      newPoint.setY( a.y() + (distFromA * ((b.y() - a.y()) / magnitude)) );
      return newPoint;
}

void RoutesHandler::addRoute(const QVector<QPointF> &gpxPoint, const QVector<double> &gpxDist, double totalDistance)
{
    int numSteps = 60;

    int remainSteps = 59;
    for(int iSegment = 1; iSegment < gpxPoint.size(); iSegment++){
        double curLength = gpxDist[iSegment];

        int curSegmentSteps = int(floor((curLength * double(numSteps)) / totalDistance));
        if(curSegmentSteps == 0)    curSegmentSteps = 1;

        remainSteps-=curSegmentSteps;
    }


    routes.push_back( QVector<QPointF>() );
    QVector<QPointF>& curRoute = routes.back();

    //qDebug() << QString("lat;lon");
    for(int iSegment = 1; iSegment < gpxPoint.size(); iSegment++){
        const QPointF& startPoint = gpxPoint[iSegment-1];
        const QPointF& endPoint = gpxPoint[iSegment];
        double curLength = gpxDist[iSegment];

        int curSegmentSteps = int(floor((curLength * double(numSteps)) / totalDistance));

        if(curSegmentSteps == 0)    curSegmentSteps = 1;
        if(remainSteps) {
            remainSteps--;
            curSegmentSteps++;
        }

        double realLen = curLength / double(curSegmentSteps);

        for(int i=0; i < curSegmentSteps; i++) {
            double walked( double(i) * realLen );

            QPointF curPos = CalculatePoint(startPoint, endPoint, curLength, walked);
            curRoute.push_back(curPos);
            //qDebug() << QString("%1;%2").arg(curPos.x(),9,'f').arg(curPos.y(),9,'f');
        }
    }
    curRoute.push_back(gpxPoint.back());
    //qDebug() << QString("%1;%2").arg(gpxPoint.back().x(),9,'f').arg(gpxPoint.back().y(),9,'f');

    qDebug() << routes.count() << "Has" << curRoute.count() << "Points";
}
