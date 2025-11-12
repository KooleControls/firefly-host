#pragma once
#include "WebServer.h"
#include "FileController.h"
#include "api/GuestSseEndpoint.h"
#include "api/PostScoreEndpoint.h"

class WebManager {
public:
    WebManager(EspNowManager& espNowManager)    
        : espNowManager(espNowManager)
    {}
    ~WebManager() = default;

    WebManager(const WebManager&) = delete;
    WebManager& operator=(const WebManager&) = delete;
    WebManager(WebManager&&) = delete;
    WebManager& operator=(WebManager&&) = delete;

    void init() {
        server.start();
        
        server.registerHandler("/api/guests/events", HTTP_GET, guestSse);
        server.registerHandler("/api/score", HTTP_POST, postScoreEndpoint);
        

        // These need last!
        fileController.init();
        server.EnableCors();
    }



private:
    WebServer server;   
    FileController fileController{server};

    EspNowManager& espNowManager;

    GuestSseEndpoint guestSse {espNowManager};
    PostScoreEndpoint postScoreEndpoint {espNowManager};

};


