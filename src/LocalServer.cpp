#include <string>
#include <iostream>

#include "internal/LocalServer.h"

using namespace Rapcom;
using namespace rapidjson;

// Used to reference web servers.
static LocalServerWeakPtr s_webServer;

void static event_hander(struct mg_connection *nc, int ev, void *ev_data)
{
    LocalServerPtr commandHandler = s_webServer.lock();
    if (commandHandler)
    {
        commandHandler->HandleWebCall(nc, ev, ev_data);
    }    
}
    
LocalServer::~LocalServer()
{
    // Uninit
    if (m_isInited)
    {
        mg_mgr_free(&m_eventManager);
    }
}

void LocalServer::Setup()
{
    // Init the event manager
    mg_mgr_init(&m_eventManager, NULL);
    m_isInited = true;

    static const char *root = "c:/";
    m_http_server_opts.document_root = root;

    // Bind
    m_connection = mg_bind(&m_eventManager, m_http_port, event_hander);
    if (m_connection == nullptr)
    {
        std::cout << "Failed to bind port 80, trying 3008\r\n";
        m_connection = mg_bind(&m_eventManager, m_http_port_backup, event_hander);
        if (m_connection == nullptr)
        {
            std::cout << "Failed to bind port 3008, killing the webserver\r\n";
            m_connection = nullptr;
            mg_mgr_free(&m_eventManager);
            m_isInited = false;
            return;
        }
        else
        {
            std::cout << "Web server started on port 3008\r\n";
        }
    }
    else
    {
        std::cout << "Web server started on port 80\r\n";

    }

    // Finish the setup
    mg_set_protocol_http_websocket(m_connection);

    // Set ourself as the static web server.
    s_webServer = GetWeakPtr<LocalServer>();
}

void LocalServer::Start()
{
    StartThread();
}

bool LocalServer::DoThreadWork()
{
    if (m_isInited)
    {
        // Poll when the timer fires.
        mg_mgr_poll(&m_eventManager, 10000);
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return true;
}

void LocalServer::HandleWebCall(struct mg_connection *nc, int ev, void *ev_data)
{
    struct http_message *hm = (struct http_message *) ev_data;

    switch (ev) {
    case MG_EV_HTTP_REQUEST:
        // Check for an incoming command
        if (mg_vcmp(&hm->uri, "/api/v1/command") == 0) 
        {
            // Handle the command
            RawCommandResponse response;
            if (auto handler = m_commandHandler.lock())
            {
                response = handler->OnRawCommand(hm->body.p, hm->body.len);
            }
 
            // Send the headers            
            mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent - Type: text / json\r\nAccess-Control-Allow-Origin: * \r\n\r\n");

            // Send the result
            mg_printf_http_chunk(nc, "%s", response.jsonResponse.c_str());
            mg_send_http_chunk(nc, "", 0);
        }
        else if (mg_vcmp(&hm->uri, "/local") == 0)
        {
            /* Serve static content */
            mg_serve_http(nc, hm, m_http_server_opts); 
        }
        else 
        {
            // Redirect to the public website.
            mg_printf(nc, "%s", "HTTP/1.1 301 Moved Permanently\r\nLocation: http://prism.quinndamerell.com/\r\nContent - Type : text / html\r\nContent - Length : 0\r\n\r\n");           
        }
        break;
    default:
        break;
    }
}