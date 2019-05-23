#pragma once

#include "Common.h"

#include <string>

#include "mongoose.h"
#include "internal/ThreadedObject.h"
#include "internal/SharedFromThisHelper.h"
#include "IRawCommandHandler.h"
#include "rapidjson/document.h"

namespace Rapcom
{
    DECLARE_SMARTPOINTER(LocalServer);
    class LocalServer :
        public SharedFromThis,
        ThreadedObject
    {
    public:
        LocalServer(IRawCommandHandlerWeakPtr commandHandler) :
            m_http_server_opts{ 0 },
            m_eventManager{ 0 },
            m_connection(nullptr),
            m_isInited(false),
            m_commandHandler(commandHandler)
        {};

        ~LocalServer();

        // Sets up the web server
        std::string Setup();

        // Starts it
        void Start();

        // Called when we should do work
        virtual bool DoThreadWork();

        // Fired when a web call has been made
        void HandleWebCall(struct mg_connection *nc, int ev, void *ev_data);

    private:

		void WriteJsonResponse(struct mg_connection* nc, std::string& json);

        // The port we will try to bind.
        const char* m_http_port = "80";
        const char* m_http_port_backup = "8356";

        // Our current server options
        struct mg_serve_http_opts m_http_server_opts;

        // Event Manager
        struct mg_mgr m_eventManager;

        // A pointer to our connection
        struct mg_connection* m_connection;

        // Indicates if we are inited or not
        bool m_isInited;

        // Holds the callback reference
        IRawCommandHandlerWeakPtr m_commandHandler;
    };
}