#pragma once
#include "WebServer.h"
#include "GuestGetAllEndpoint.h"



class ApiController {
public:
    ApiController(WebServer& server, GuestManager& guestManager) 
        : server(server), getGuests(guestManager)  {}

    void init()
    {
        server.registerHandler("/api/guests", HTTP_GET, getGuests);
    }

private:
    WebServer& server;
    GuestGetAllEndpoint getGuests;


};
