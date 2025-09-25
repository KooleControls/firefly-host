#pragma once
#include "WebServer.h"
#include "FileController.h"
#include "ApiController.h"



class WebManager {
public:
    WebManager(GuestManager& guestManager)
        : guestManager(guestManager) {}
    ~WebManager() = default;

    WebManager(const WebManager&) = delete;
    WebManager& operator=(const WebManager&) = delete;
    WebManager(WebManager&&) = delete;
    WebManager& operator=(WebManager&&) = delete;

    void init() {
        server.start();
        apiController.init();
        

        // These need last!
        fileController.init();
        server.EnableCors();
    }



private:
    WebServer server;
    GuestManager& guestManager;
    ApiController apiController{server, guestManager};
    
    FileController fileController{server};

};


