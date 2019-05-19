#include <string>
#include <iostream>

#include "internal/PollServer.h"

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace Rapcom;
using namespace rapidjson;

// Used to reference the poll server.
static PollServerWeakPtr s_pollServer;

// Callback from mongoose, send it to our poll server
void static event_hander(struct mg_connection *nc, int ev, void *ev_data)
{
    PollServerPtr commandHandler = s_pollServer.lock();
    if (commandHandler)
    {
        commandHandler->HandleWebCall(nc, ev, ev_data);
    }
}

PollServer::~PollServer()
{
    // Uninit
    if (m_isInited)
    {
        mg_mgr_free(&m_eventManager);
    }
}

void PollServer::Setup()
{
    // Init the event manager
    mg_mgr_init(&m_eventManager, NULL);
    m_isInited = true;

    // Set ourself as the static poll server.
    s_pollServer = GetWeakPtr<PollServer>();
}

void PollServer::Start()
{
    StartThread();
}

bool PollServer::DoThreadWork()
{
    if (m_isInited)
    {
        // Check if we errored
        if (m_shouldErrorSleep)
        {
            // We hit an error, try again in 30 seconds.
            m_shouldErrorSleep = false;
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }

        // Do an action if we need to
        switch (m_nextAction)
        {
        case PollServer::NextAction::MakePollRequest:
            mg_connect_http(&m_eventManager, event_hander, m_pollAddress.c_str(), NULL, NULL);
            m_nextAction = NextAction::WaitingOnPollRequest;
            break;
        case PollServer::NextAction::MakeResponseRequest:
            // Build the post request and make the request
            std::string responseUrl = "http://relay.quinndamerell.com/Blob.php?key=" + m_channelName + "_resp" + m_response.responseCode;          
            std::string postData = "data=" + m_response.jsonResponse;
            mg_connect_http(&m_eventManager, event_hander, responseUrl.c_str(), "Content-Type: application/x-www-form-urlencoded\r\n", postData.c_str());
            m_nextAction = NextAction::WaitOnResponseRequest;
            break;
        }        

        // Poll when the timer fires.
        mg_mgr_poll(&m_eventManager, 10000);
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return true;
}

void PollServer::HandleWebCall(struct mg_connection *nc, int ev, void *ev_data)
{
    struct http_message *hm = (struct http_message *) ev_data;

    switch (ev) 
    {
    case MG_EV_CONNECT:
        if (*(int *)ev_data != 0) 
        {
            // If we failed to connect set the flag to sleep and report an error
            std::cout << "Failed to connect to web server for long poll\r\n";
            m_shouldErrorSleep = true;
            m_nextAction = NextAction::MakePollRequest;
        }
        break;
    case MG_EV_HTTP_REPLY:
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
        HandleWebCommand(hm->body.p, hm->body.len);
        break;
    default:
        break;
    }
}

void PollServer::HandleWebCommand(const char* jsonStr, size_t length)
{
    NextAction pastAction = m_nextAction;

    // Set the next action to be a poll request. This will be updated below if we need to send a response.
    m_nextAction = NextAction::MakePollRequest;

    if (pastAction == NextAction::WaitOnResponseRequest)
    {
        // If this a response handle return now.
        return;
    }

    // Parse the input
    Document requestDoc;
    requestDoc.Parse(jsonStr, length);

    if (!requestDoc.HasParseError() && requestDoc.HasMember("Status"))
    {
        Value::ConstMemberIterator itr = requestDoc.FindMember("Status");
        if (itr != requestDoc.MemberEnd() && itr->value.IsString())
        {
            std::string statusValue(itr->value.GetString(), itr->value.GetStringLength());
            if (statusValue == "NewData")
            {
                // We have an update, grab the data.
                Value::ConstMemberIterator dataItr = requestDoc.FindMember("Data");
                if (dataItr != requestDoc.MemberEnd() && dataItr->value.IsString())
                {
                    char* decodedString = new char[dataItr->value.GetStringLength()];
                    mg_url_decode(dataItr->value.GetString(), dataItr->value.GetStringLength(), decodedString, dataItr->value.GetStringLength(), false);
                    if (auto handler = m_commandHandler.lock())
                    {
                        // Send the command and get the response.
                        m_response = handler->OnRawCommand(decodedString, dataItr->value.GetStringLength());

                        // If we have something to respond with and a code make a request to send it.
                        if (m_response.jsonResponse.size() > 0 && m_response.responseCode.size() > 0)
                        {
                            m_nextAction = NextAction::MakeResponseRequest;
                        }
                    }
                    delete[] decodedString;
                }
                else
                {
                    std::cout << "Failed to parse data";
                }                
            }
        }
        else
        {
            std::cout << "Invalid poll data returned";
        }
    }
    else
    {
        std::cout << "Invalid poll data returned";
    }   
}