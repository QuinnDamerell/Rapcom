#pragma once

#include <string>

#include "internal/Common.h"

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

        // The port our local server is bound to.
        std::string m_boundPort;

        // Sets an error to the document
        void SetDocumentError(rapidjson::Document& document, std::string errorText);

        // Sets success on a doc
        void SetDocumentSuccess(rapidjson::Document& document);

        // Gets the config
        void HandleGetConfig(rapidjson::Document& response);

        // Sets the config
        void HandleSetConfig(const char* json, size_t length);

        // Sets the dns of mongoose to the system if possible
        void SetSystemDns();

        // Gets the local IP
        std::string GetLocalIp();
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

        // If not there, add it
        document.AddMember(rapidjson::Value(name.c_str(), document.GetAllocator()), rapidjson::Value(defaultValue), document.GetAllocator());
        return defaultValue;
    }

    static int64_t GetIntOrDefault(rapidjson::Document& document, std::string name, int64_t defaultValue)
    {
        auto iter = document.FindMember(name.c_str());
        if (iter != document.MemberEnd())
        {
            if (iter->value.IsInt())
            {
                return iter->value.GetInt();
            }
            else if (iter->value.IsInt64())
            {
                return iter->value.GetInt64();
            }
        }

        // If not there, add it
        document.AddMember(rapidjson::Value(name.c_str(), document.GetAllocator()), rapidjson::Value(defaultValue), document.GetAllocator());
        return defaultValue;
    }

    static void GetStringOrDefault(rapidjson::Document& document, std::string name, std::string defaultValue, std::string& output)
    {
        auto iter = document.FindMember(name.c_str());
        if (iter != document.MemberEnd())
        {
            if (iter->value.IsString())
            {
                output.assign(iter->value.GetString(), iter->value.GetStringLength());
                return;
            }
        }

        // If not there, add it
        document.AddMember(rapidjson::Value(name.c_str(), document.GetAllocator()), rapidjson::Value(defaultValue.c_str(), document.GetAllocator()), document.GetAllocator());
        output = defaultValue;
        return;
    }

    enum class ConfigChangeType
    {
        Added,
        Removed,
        Updated,
        None
    };

    static ConfigChangeType CheckConfigChange(rapidjson::Document& orgDoc, rapidjson::Document& newDoc, std::string name)
    {
        // Find it in the org
        auto orgIter = orgDoc.FindMember(name.c_str());
        if (orgIter == orgDoc.MemberEnd())
        {
            return ConfigChangeType::Added;
        }

        // Find it in the new
        auto newIter = newDoc.FindMember(name.c_str());
        if (newIter == newDoc.MemberEnd())
        {
            return ConfigChangeType::Removed;
        }

        if (newIter->value.GetType() != orgIter->value.GetType())
        {
            return ConfigChangeType::Updated;
        }

        if (newIter->value != orgIter->value)
        {
            return ConfigChangeType::Updated;
        }

        return ConfigChangeType::None;
    }
}