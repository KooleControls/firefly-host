#pragma once
#include "WebServer.h"
#include "FileController.h"


class WebManager {
public:
    WebManager() = default;
    ~WebManager() = default;

    WebManager(const WebManager&) = delete;
    WebManager& operator=(const WebManager&) = delete;
    WebManager(WebManager&&) = delete;
    WebManager& operator=(WebManager&&) = delete;

    void init() {
        server.start();
        fileController.init();
    }



private:
    WebServer server;
    FileController fileController{server};
};


