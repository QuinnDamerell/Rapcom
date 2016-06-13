#include <iostream>
#include <fstream>
#include <string>

#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "RapcomBase.h"

using namespace std;
using namespace rapidjson;
using namespace Rapcom;

RapcomBase::RapcomBase(std::string channelName, IRapcomListenerWeakPtr listener) :
    m_listener(listener),
    m_channelName(channelName)
{
    // Try to open the config file
    ifstream confiFile("rapcomConfig.json", ios::out | ios::in);

    if (confiFile.is_open())
    {
        // Read in the file
        string jsonString;
        string line;
        while (getline(confiFile, line))
        {
            jsonString += line;
        }

        // Read and parse the config
        m_currentConfig.Parse(jsonString.c_str(), jsonString.size());
    }

    // Close the file
    confiFile.close();

    // If it failed to parse clear the config
    if (m_currentConfig.HasParseError())
    {
        m_currentConfig.Clear();
    }

    // Make sure the root is an object
    if (!m_currentConfig.IsObject())
    {
        m_currentConfig.SetObject();
    }

    m_currentConfig.AddMember("test", 34879374, m_currentConfig.GetAllocator());
}

Document& RapcomBase::GetConfig()
{
    return m_currentConfig;
}

void RapcomBase::SaveConfig()
{
    // Try to open the file.
    ofstream confiFile("rapcomConfig.json", ios::out | ios::trunc);

    // Serialize the json
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    m_currentConfig.Accept(writer);
    string jsonString = sb.GetString();

    // Write the json to the file.
    confiFile << jsonString;

    // Close the file.
    confiFile.close();
}

void RapcomBase::Start()
{
    // Make the local server
    m_localServer = std::make_shared<LocalServer>(GetSharedPtr<IRawCommandHandler>());
    // Setup and start
    m_localServer->Setup();
    m_localServer->Start();

    // Make the poll server
    m_pollServer = std::make_shared<PollServer>(m_channelName, GetSharedPtr<IRawCommandHandler>());
    // Setup and start
    m_pollServer->Setup();
    m_pollServer->Start();
}

// Fired when we get a command
RawCommandResponse RapcomBase::OnRawCommand(const char* jsonString, size_t length)
{
    // Setup the response
    Document responseDoc;
    responseDoc.SetObject();
    std::string responseCode = "";
    
    // Parse the input
    Document requestDoc;
    requestDoc.Parse(jsonString, length);
    
    if (!requestDoc.HasParseError() && requestDoc.HasMember("Command"))
    {
        Value::ConstMemberIterator itr = requestDoc.FindMember("Command");
        if (itr != requestDoc.MemberEnd() && itr->value.IsString())
        {
            // Check if we have a response code
            Value::ConstMemberIterator responseItr = requestDoc.FindMember("ResponseCode");
            if (responseItr != requestDoc.MemberEnd() && responseItr->value.IsString())
            {
                responseCode = responseItr->value.GetString();
            }

            // Try to match the command
            std::string command(itr->value.GetString(), itr->value.GetStringLength());
            if (command == "SetConfig")
            {
                Value::ConstMemberIterator configItr = requestDoc.FindMember("Value1");
                if (configItr != requestDoc.MemberEnd() && configItr->value.IsObject())
                {
                    //HandleSetConfig(configItr->value.GetObjectA());
                }
                else
                {
                    SetDocumentError(responseDoc, "Failed to find new config");
                }
            }
            else if (command == "GetConfig")
            {
                HandleGetConfig(responseDoc);
            }
            else
            {
                // This isn't a command for us, send it along.
                if (auto listener = m_listener.lock())
                {
                    listener->OnCommand(requestDoc);
                }
            }
        }
        else
        {
            SetDocumentError(responseDoc, "Failed to find Command");
        }
    }
    else
    {
        SetDocumentError(responseDoc, "Parse Error");
    }
    
    // Write the output string
    StringBuffer sb;
    Writer<StringBuffer> writer(sb);
    responseDoc.Accept(writer);

    // Send the response
    RawCommandResponse response;
    response.jsonResponse = sb.GetString();
    response.responseCode = responseCode;
    return response;
}
    
void RapcomBase::SetDocumentError(Document& document, std::string errorText)
{
    document.AddMember("Status", "Error", document.GetAllocator());
    document.AddMember("ErrorText", StringRef(errorText.c_str()), document.GetAllocator()); //todo fix me
}
    
void RapcomBase::SetDocumentSuccess(Document& document)
{
    document.AddMember("Status", "Success", document.GetAllocator());
}

void RapcomBase::HandleGetConfig(rapidjson::Document& response)
{
    // Copy the config
    response.CopyFrom(m_currentConfig, response.GetAllocator());
}

void RapcomBase::HandleSetConfig(rapidjson::Document& request)
{

}