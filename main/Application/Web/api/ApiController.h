#pragma once
#include "WebServer.h"
#include "GuestGetAllEndpoint.h"
#include "GuestSseEndpoint.h"



class ApiController {
public:
    ApiController(WebServer& server, GuestManager& guestManager) 
        : server(server),
          getGuests(guestManager),
          guestSse(guestManager) {}

    void init() {
        server.registerHandler("/api/guests", HTTP_GET, getGuests);
        server.registerHandler("/api/guests/events", HTTP_GET, guestSse);
    }

private:
    WebServer& server;
    GuestGetAllEndpoint getGuests;
    GuestSseEndpoint guestSse;
};
