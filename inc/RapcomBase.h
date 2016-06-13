#pragma once

#include <string>

#include "Common.h"

#include "internal/LocalServer.h"
#include "internal/PollServer.h"
#include "rapidjson/document.h"
#include "internal/SharedFromThisHelper.h"
#include "IRapcomListener.h"
#include "internal/IRawCommandHandler.h"


namespace Rapcom
{
    DECLARE_SMARTPOINTER(RapcomBase);
    class RapcomBase :
        public IRawCommandHandler,
        public SharedFromThis
    {
    public:
        RapcomBase(std::string channelName, IRapcomListenerWeakPtr listener);

        // Gets the config.
        rapidjson::Document& GetConfig();

        // Saves the config
        void SaveConfig();

        // Starts Rapcom communication
        void Start();
        
        //
        // IRawCommandHandler
        RawCommandResponse OnRawCommand(const char* jsonString, size_t length);

    private:
        // Holds the callback to the listener
        IRapcomListenerWeakPtr m_listener;

        // The current config
        rapidjson::Document m_currentConfig;

        // The channel name we will talk with
        std::string m_channelName;

        // The local server
        LocalServerPtr m_localServer;

        // The poll server
        PollServerPtr m_pollServer;

        // Sets an error to the document
        void SetDocumentError(rapidjson::Document& document, std::string errorText);

        // Sets success on a doc
        void SetDocumentSuccess(rapidjson::Document& document);

        // Gets the config
        void HandleGetConfig(rapidjson::Document& response);

        // Sets the config
        void HandleSetConfig(rapidjson::Document& request);
    };

    static double GetDoubleOrDefault(rapidjson::Document& document, std::string name, double defaultValue)
    {
        auto iter = document.FindMember(name.c_str());
        if (iter != document.MemberEnd())
        {
            if (iter->value.IsDouble())
            {
                return iter->value.GetDouble();
            }            
            else if (iter->value.IsInt())
            {
                return iter->value.GetInt();
            }
        }
        return defaultValue;
    }
}