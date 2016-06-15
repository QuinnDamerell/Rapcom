#include <iostream>
#include <fstream>
#include <string>

#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
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

    // Set the dns to the system's dns if possible.
    SetSystemDns();
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
            if (responseItr != requestDoc.MemberEnd())
            {
                if (responseItr->value.IsString())
                {
                    responseCode = responseItr->value.GetString();
                }
                if (responseItr->value.IsNumber())
                {
                    responseCode = std::to_string(responseItr->value.GetInt64());
                }
            }

            // Try to match the command
            std::string command(itr->value.GetString(), itr->value.GetStringLength());
            if (command == "SetConfig")
            {
                Value::ConstMemberIterator configItr = requestDoc.FindMember("Value1");
                if (configItr != requestDoc.MemberEnd() && configItr->value.IsObject())
                {
                    // Since the new config is a subnode of this tree, we will 
                    // just write it to a string.
                    StringBuffer strBuf;
                    Writer<StringBuffer> jsonWriter(strBuf);
                    configItr->value.Accept(jsonWriter);

                    // Now handle the new config.
                    HandleSetConfig(strBuf.GetString(), strBuf.GetSize());

                    // And set the success response.
                    SetDocumentSuccess(responseDoc);
                }
                else
                {
                    SetDocumentError(responseDoc, "Failed to find new config");
                }
            }
            // Handle get config
            else if (command == "GetConfig")
            {
                HandleGetConfig(responseDoc);
            }
            // Handle heartbeats
            else if (command == "Heartbeat")
            {
                // Add our current IP to the message.
                responseDoc.AddMember("LocalIp", Value(GetLocalIp().c_str(), responseDoc.GetAllocator()), responseDoc.GetAllocator());
                responseDoc.AddMember("LocalPort", 80, responseDoc.GetAllocator());
                SetDocumentSuccess(responseDoc);
            }
            else
            {
                // This isn't a command for us, send it along.
                if (auto listener = m_listener.lock())
                {
                    listener->OnCommand(requestDoc, responseDoc);
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
    document.AddMember("ErrorText", Value(errorText.c_str(), document.GetAllocator()), document.GetAllocator());
}
    
void RapcomBase::SetDocumentSuccess(Document& document)
{
    document.AddMember("Status", "Success", document.GetAllocator());
}

void RapcomBase::HandleGetConfig(rapidjson::Document& response)
{
    // Copy the config to the response
    response.CopyFrom(m_currentConfig, response.GetAllocator());
}

void RapcomBase::HandleSetConfig(const char* json, size_t length)
{
    // Copy the old config
    rapidjson::Document oldConfig;
    oldConfig.CopyFrom(m_currentConfig, oldConfig.GetAllocator());

    // Parse the new config
    m_currentConfig.Parse(json, length);

    // Save the new config
    SaveConfig();

    // Fire the event listener if we have one.
    if (auto listener = m_listener.lock())
    {
        listener->OnConfigChange(oldConfig, m_currentConfig);
    }
}

void RapcomBase::SetSystemDns()
{
    // For mongoose we need to overwrite the default DNS (8.8.8.8) because sometimes we can't connect
    // to it if behind a firewall. If we can, we will try to get the system dns and use it.

#ifdef _MSC_VER
    // This is really bad, but the quickest way I would find to get the dns server
    // of a windows box was to run ipconfig and parse it. If you can find better let me know.    
    char lineBuffer[2000];
    // Try to run the command
    std::shared_ptr<FILE> pipe(_popen("ipconfig -all", "r"), _pclose);
    if (pipe)
    {
        // Run through the output
        while (!feof(pipe.get()))
        {
            if (fgets(lineBuffer, 2000, pipe.get()) != NULL)
            {
                if (strncmp(lineBuffer, "   DNS Servers", 14) == 0)
                {
                    // We found it!

                    // Parse out the IP.
                    int bufferSize = strlen(lineBuffer);
                    int start = 0;
                    while (start < bufferSize && lineBuffer[start] != ':')
                    {
                        start++;
                    }

                    // Add 2 to account for the ': '
                    start += 2;

                    if (start < bufferSize)
                    {
                        // Find the end
                        int end = start;
                        while (end < bufferSize && lineBuffer[end] != '\n')
                        {
                            end++;
                        }

                        // Now trim the string
                        char *dnsIp = lineBuffer + start;
                        dnsIp[(end - start)] = '\0';

                        // Create a new buffer for the string and set the new IP.
                        mg_set_dns_server(dnsIp, end - start);         

                        // We are done.
                        break;
                    }
                }
            }
        }
    }
#endif
}

std::string RapcomBase::GetLocalIp()
{
    std::string localIp = "";

#ifdef  _MSC_VER  

    ADDRINFOW   info = { 0 };
    PADDRINFOW  more = { 0 };
    info.ai_family = AF_UNSPEC;
    info.ai_socktype = SOCK_STREAM;
    info.ai_protocol = IPPROTO_TCP;

    // Get the host name.    
    WCHAR hostname[500];
    if (GetHostNameW(hostname, sizeof(hostname)) != 0)
    {
        return localIp;
    }

    // Get the info
    if (GetAddrInfoW(hostname, NULL, &info, &more) != 0)
    {
        return localIp;
    }

    // Find the ip
    char ipstringbuffer[46];
    DWORD ipbufferlength = 46;
    LPSOCKADDR sockaddr_ip;
    ADDRINFOW *ptr = NULL;
    for (ptr = more; ptr != NULL; ptr = ptr->ai_next) 
    {
        switch (ptr->ai_family) 
        {
        case AF_INET:
            sockaddr_ip = (LPSOCKADDR)ptr->ai_addr;
            ipbufferlength = 46;
            if (WSAAddressToString(sockaddr_ip, (DWORD)ptr->ai_addrlen, NULL, ipstringbuffer, &ipbufferlength) == 0)
            {
                localIp.assign(ipstringbuffer);
                break;
            }
        }
    }
#endif

    return localIp;
}