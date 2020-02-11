#ifndef ROUTESHANDLER_H
#define ROUTESHANDLER_H

#include "CivetServer.h"
#include <QVector>
#include <QPointF>

class RoutesHandler: public CivetHandler
{
public:
    RoutesHandler();

    bool parseRoutes();
    bool handleGet(CivetServer *server, struct mg_connection *conn);
    bool getPos(int routeId, QPointF& thePosition) const;

protected:
    int sendResponse(CivetServer *server, struct mg_connection *conn, const QByteArray& reply);
    void addRoute( const QVector<QPointF>& gpxPoint, const QVector<double>& gpxDist, double totalDistance );
    QVector<QVector<QPointF> > routes;
};

#endif // ROUTESHANDLER_H
