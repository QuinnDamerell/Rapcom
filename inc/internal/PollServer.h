#pragma once

#include "Common.h"

#include "mongoose.h"
#include "internal/ThreadedObject.h"
#include "internal/SharedFromThisHelper.h"
#include "IRawCommandHandler.h"
#include "rapidjson/document.h"

namespace Rapcom
{
    DECLARE_SMARTPOINTER(PollServer);
    class PollServer :
        public ThreadedObject,
        public SharedFromThis
    {
    public:
        PollServer(std::string channelName, IRawCommandHandlerWeakPtr commandHandler) :
            m_eventManager{ 0 },
            m_isInited(false),
            m_commandHandler(commandHandler),
            m_nextAction(NextAction::MakePollRequest),
            m_shouldErrorSleep(false),
            m_channelName(channelName)
        {
            // Set the poll address
            m_pollAddress = "http://relay.quinndamerell.com/LongPoll.php?key=" + channelName + "Poll&clearValue=&expectingValue=";
        };

        ~PollServer();

        // Sets up the poll server
        void Setup();

        // Starts it
        void Start();

        // Called when we should do work
        virtual bool DoThreadWork();

        // Fired when a web call has been made
        void HandleWebCall(struct mg_connection *nc, int ev, void *ev_data);

    private:
        // Event Manager
        struct mg_mgr m_eventManager;

        // Indicates if we are inited or not
        bool m_isInited;

        // Holds the callback reference
        IRawCommandHandlerWeakPtr m_commandHandler;

        // The channel we will talk on
        std::string m_pollAddress;
        std::string m_channelName;

        // The response we need to send.
        RawCommandResponse m_response;
        
        // Handles a web command
        void HandleWebCommand(const char* jsonStr, size_t length);

        // What action we need to make
        enum class NextAction
        {
            MakePollRequest,
            WaitingOnPollRequest,
            MakeResponseRequest,
            WaitOnResponseRequest
        };
        NextAction m_nextAction;

        // If we should sleep because we are in error
        bool m_shouldErrorSleep;
    };
}