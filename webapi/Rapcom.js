// RapconConnection Object
//     IsConnected (bool) - if we are connected.
//     HasLocalConnection (bool) - if we have a local connection or not.
//     [Internal]
//     ChannelName (string) - the channel name we will connect to
//     HeartbeatObj (bool) - the heartbeat object
//
//     Functions
//          SendCommand(command, waitForResponse, value1, value2, value3, value4)
//              Msg: Sends a message
//              Returns: An object that allows complete(wasSuccess, jsonResponse) to be bound.
//          GetConfig()
//              Msg: Gets the current config and returns it.
//              Returns: An object that allows complete(wasSuccess, config) to be bound.
//          SetConfig()
//              Msg: Sets a config to the server.
//              Returns: An object that allows complete(wasSuccess, config) to be bound which tells if the config was set successfully.
//          KillConnection() - Kills the connection
//     [Internal]
//          InternalDisconnected() - Called when we get disconnected
//      
//     Callbacks
//          OnConnecting(bool isLocal) - Fired when we are trying to connect
//          OnConnected(bool isLocal) - Fired when we are connected
//          OnDisconnected(bool isLocal) - Fired when we lose the connection.

function CreateRapcomConnection(channelName)
{
    // Create the connection object
    var connectionObject = { 
        ChannelName: channelName, 
        IsConnected: false,
        IsConnectedLocally: false, 
        HeartbeatObj : null,
        LocalIp : null,
        LocalPort : null,
        SendCommand : Rapcom_SendCommand,
        GetConfig : Rapcom_GetConfig,
        SetConfig : Rapcom_SetConfig,
        KillConnection : Rapcom_KillConnection,
        InternalDisconnected : Rapcom_InternalDisconnected,
        InternalDisconnectedLocal : Rapcom_InternalDisconnected_Local,
        OnConnecting : null,
        OnConnected : null,
        OnDisconnected : null
    };

    // Start a heartbeat for this connection
    connectionObject.HeartbeatObj = setInterval(Rapcom_Heartbeat, 8000, connectionObject);

    // Kick off a connection now
    Rapcom_Heartbeat(connectionObject);

    // Return the object
    return connectionObject;
}

// Main heartbeat for the object, this should ensure we are connected and try to connect locally
// if possible.
function Rapcom_Heartbeat(connectionObject)
{
    // Only do work if we aren't connected.
    if(!connectionObject.IsConnected)
    {
        // Fire we are trying to connect
        if(connectionObject.OnConnecting != null)
        {
            connectionObject.OnConnecting(false);
        }
        
        // Try to send a heartbeat to the server.
        connectionObject.SendCommand("Heartbeat", true)
            .complete = function(wasSuccess, jsonResponse)
            {
                if(wasSuccess)
                {
                    // We are connected!
                    connectionObject.IsConnected = true;
                    connectionObject.LocalIp = jsonResponse.LocalIp;
                    connectionObject.LocalPort = jsonResponse.LocalPort;

                    if(connectionObject.OnConnected != null)
                    {
                        connectionObject.OnConnected(false);
                    }  

                    // Call us again so we try to connect locally.
                    Rapcom_Heartbeat(connectionObject);
                }             
            };
    }
    // If we are connected try to connect locally
    else if(!connectionObject.IsConnectedLocally)
    {
        // If we have a local ip, try to connect locally.
        if(connectionObject.LocalIp != null && connectionObject.LocalIp.length > 0 && 
           connectionObject.LocalPort != null && connectionObject.LocalPort.length > 0)
        {
            // Fire we are trying to connect locally
            if(connectionObject.OnConnecting != null)
            {
                connectionObject.OnConnecting(true);
            }

            // Try to connect locally
            connectionObject.SendCommand("Heartbeat", true, null, null, null, null, true)
            .complete = function(wasSuccess, jsonResponse)
            {
                if(wasSuccess)
                {
                    // We are connected!
                    connectionObject.IsConnectedLocally = true;
                    if(connectionObject.OnConnected != null)
                    {
                        connectionObject.OnConnected(true);
                    } 
                }              
            };
        }
    }
}

// Called internally when we detect a problem.
function Rapcom_InternalDisconnected()
{
    var wasConnected = this.IsConnected;
    this.IsConnected = false;
    this.LocalIp = null;
    this.LocalPort = null;
    if(wasConnected && this.OnDisconnected != null)
    {
        this.OnDisconnected(false);
    }
}

// Called when we lose our local connection.
function Rapcom_InternalDisconnected_Local()
{
    var wasConnected = this.IsConnectedLocally;
    this.IsConnectedLocally = false;
    if(wasConnected && this.OnDisconnected != null)
    {
        this.OnDisconnected(true);
    }
}

// Called externally to stop the connection.
function Rapcom_KillConnection()
{
    clearInterval(this.HeartbeatObj);
    this.InternalDisconnected();
}

// Sends a command to the other side.
function Rapcom_SendCommand(command, waitForResponse, value1, value2, value3, value4, forceLocal)
{
    // Make the json message
    var message = {"Command":command, "Value1":value1,"Value2":value2, "Value3":value3, "Value4":value4};

    // Get our connection object
    var connectionObject = this;

    // If we are connected locally use it
    if(forceLocal || connectionObject.IsConnectedLocally)
    {
        return Rapcom_SendCommand_Local(connectionObject, message);
    }

    // Used to give a callback when the message is sent
    var returnObj = { complete : null};

    // This is used to make sure we don't respond back many times
    var hasCalledbackToUser = false;

    // If we are expecting a response generate a response code
    if(waitForResponse)
    {
        // Generate a reponse code
        message.ResponseCode = Math.floor(Math.random() * 100000000000000);
    }
    
    // Send the command
    $.post("http://relay.quinndamerell.com/Blob.php?key=" + connectionObject.ChannelName + "Poll&data=" + encodeURIComponent(JSON.stringify(message)))
    .done(function(data)
    {
        // If we aren't waiting on a response, return the body now.
        if(!waitForResponse)
        {
            if(returnObj.complete != null)
            {
                returnObj.complete(true, data);
            }  
        }
    })
    .fail(function(data) 
    {
        // Connection failed
        connectionObject.InternalDisconnected();

        if(!hasCalledbackToUser && returnObj.complete != null)
        {
            hasCalledbackToUser = true;
            returnObj.complete(false, data);
        }           
    });

    // Now if we want to wait for a response make a request.
    if(waitForResponse)
    {
        // Make the long poll for the reuest.
        $.get("http://relay.quinndamerell.com/LongPoll.php?expectingValue=clearValue=&key=" + connectionObject.ChannelName + "_resp" + message.ResponseCode)
        .done(function(data)
        {
            // We got a response
            var response = JSON.parse(data);
            if(response.Status == "NewData")
            {
                // We got a response, send it!
                response = JSON.parse(decodeURIComponent(response.Data));
                if(!hasCalledbackToUser && returnObj.complete != null)
                {
                    hasCalledbackToUser = true;
                    returnObj.complete(true, response);
                }
            }
            else
            {
                // We timed out, assume we disconnected
                connectionObject.InternalDisconnected();
                if(!hasCalledbackToUser && returnObj.complete != null)
                {
                    hasCalledbackToUser = true;
                    returnObj.complete(false, {"Status":"Disconnected"});
                }  
            }     
        })
        .fail(function(data) 
        {
            // Connection failed
            connectionObject.InternalDisconnected();
                
            if(!hasCalledbackToUser && returnObj.complete != null)
            {
                hasCalledbackToUser = true;
                returnObj.complete(false, data);
            }  
        });
    }

    return returnObj;
}

// Sends a command to the other side.
function Rapcom_SendCommand_Local(connectionObject, message)
{
    // Used to give a callback when the message is sent
    var returnObj = { complete : null};
  
    // Send the command, note we want to time out after 500ms to ensure the connect stays up. 
    // If we lose this we will fall back to the web.
    $.post({url:"http://" + connectionObject.LocalIp + ":" + connectionObject.LocalPort + "/api/v1/command", data:JSON.stringify(message), timeout:1000})
    .done(function(data)
    {
        var response = JSON.parse(data);
        if(returnObj.complete != null)
        {
            returnObj.complete(true, response);
        } 
    })
    .fail(function(data) 
    {
        // Local Connection failed
        connectionObject.InternalDisconnectedLocal();

        if(returnObj.complete != null)
        { 
            returnObj.complete(false, data);
        }           
    });

    return returnObj; 
}

// Returns the current config
function Rapcom_GetConfig()
{
    var response = { complete : null };

    // Send a message asking for the config.
    this.SendCommand("GetConfig", true)
    .complete = function(wasSuccess, data)
    {
        if(response.complete != null)
        {
            response.complete(wasSuccess, data);
        }
    };
    return response;
}

// Sets a given config
function Rapcom_SetConfig(newConfig)
{
    var response = { complete : null };

    // Make the config into a string and encode it to prevent issues in json
    var stringConfig = encodeURIComponent(JSON.stringify(newConfig));

    // Send the config
    this.SendCommand("SetConfig", true, newConfig)
    .complete = function(wasSuccess, data)
    {
        var wasSet = wasSuccess && data.Status == "Success";
        if(response.complete != null)
        {
            response.complete(wasSet);
        }
    };
    return response;
}