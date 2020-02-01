#include "CivetServer.h"
#include <cstring>
#include "routeshandler.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#define DOCUMENT_ROOT "."
#define PORT "8081"
#define EXAMPLE_URI "/example"
#define EXIT_URI "/exit"

/* Exit flag for main loop */
static volatile bool exitNow = false;

class ExampleHandler : public CivetHandler
{
  public:
    bool
    handleGet(CivetServer * /*server*/, struct mg_connection *conn)
    {
        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\nContent-Type: "
                  "text/html\r\nConnection: close\r\n\r\n");
        mg_printf(conn, "<html><body>\r\n");
        mg_printf(conn,
                  "<h2>This is an example text from a C++ handler</h2>\r\n");
        mg_printf(conn,
                  "<p>To see a page from the A handler <a "
                  "href=\"a\">click here</a></p>\r\n");
        mg_printf(conn,
                  "<form action=\"a\" method=\"get\">"
                  "To see a page from the A handler with a parameter "
                  "<input type=\"submit\" value=\"click here\" "
                  "name=\"param\" \\> (GET)</form>\r\n");
        mg_printf(conn,
                  "<form action=\"a\" method=\"post\">"
                  "To see a page from the A handler with a parameter "
                  "<input type=\"submit\" value=\"click here\" "
                  "name=\"param\" \\> (POST)</form>\r\n");
        mg_printf(conn,
                  "<p>To see a page from the A/B handler <a "
                  "href=\"a/b\">click here</a></p>\r\n");
        mg_printf(conn,
                  "<p>To see a page from the *.foo handler <a "
                  "href=\"xy.foo\">click here</a></p>\r\n");
        mg_printf(conn,
                  "<p>To see a page from the WebSocket handler <a "
                  "href=\"ws\">click here</a></p>\r\n");
        mg_printf(conn,
                  "<p>To exit <a href=\"%s\">click here</a></p>\r\n",
                  EXIT_URI);
        mg_printf(conn, "</body></html>\r\n");
        return true;
    }
};

class ExitHandler : public CivetHandler
{
  public:
    bool handleGet(CivetServer *, struct mg_connection *conn)
    {
        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\nContent-Type: "
                  "text/plain\r\nConnection: close\r\n\r\n");
        mg_printf(conn, "Bye!\n");
        exitNow = true;
        return true;
    }
};

class AHandler : public CivetHandler
{
  private:
    bool handleAll(const char *method, CivetServer * /*server*/, struct mg_connection *conn)
    {
        std::string s = "";
        mg_printf(conn,
                  "HTTP/1.1 200 OK\r\nContent-Type: "
                  "text/html\r\nConnection: close\r\n\r\n");
        mg_printf(conn, "<html><body>");
        mg_printf(conn, "<h2>This is the A handler for \"%s\" !</h2>", method);
        if (CivetServer::getParam(conn, "param", s)) {
            mg_printf(conn, "<p>param set to %s</p>", s.c_str());
        } else {
            mg_printf(conn, "<p>param not set</p>");
        }
        mg_printf(conn, "</body></html>\n");
        return true;
    }

  public:
    bool handleGet(CivetServer *server, struct mg_connection *conn)
    {
        return handleAll("GET", server, conn);
    }
    bool handlePost(CivetServer *server, struct mg_connection *conn)
    {
        return handleAll("POST", server, conn);
    }
};

int main(int /*argc*/, char * /*argv*/[])
{
    const char *options[] = {
        "document_root", DOCUMENT_ROOT,
        "listening_ports", PORT,
        nullptr
    };

    CivetServer server(options); // <-- C style start

    ExampleHandler h_ex;
    server.addHandler(EXAMPLE_URI, h_ex);

    ExitHandler h_exit;
    server.addHandler(EXIT_URI, h_exit);

    AHandler h_a;
    server.addHandler("/a", h_a);

    RoutesHandler h_r;
    h_r.parseRoutes();
    server.addHandler("/routes", h_r);

    printf("Run example at http://localhost:%s%s\n", PORT, EXAMPLE_URI);
    printf("Exit at http://localhost:%s%s\n", PORT, EXIT_URI);

    while (!exitNow) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }

    printf("Bye!\n");

    return 0;
}
