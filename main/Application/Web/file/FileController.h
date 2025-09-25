#pragma once
#include "WebServer.h"
#include "FileGetEndpoint.h"

class FileController {
public:
    FileController(WebServer& server) 
        : server(server) {}

    void init()
    {
        server.registerHandler("/*", HTTP_GET, getFile);
    }

private:
    WebServer& server;
    FileGetEndpoint getFile {"/fat"};

};
