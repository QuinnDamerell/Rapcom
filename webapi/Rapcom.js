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
//          OnConnecting() - Fired when we are trying to connect
//          OnConnected(bool isLocal) - Fired when we are connected
//          OnDisconnected() - Fired when we lose the connection.

function CreateRapcomConnection(channelName)
{
    // Create the connection object
    var connectionObject = { 
        ChannelName: channelName, 
        IsConnected: false,
        IsLocalConnection: false, 
        HeartbeatObj : null,
        LocalIp : null,
        SendCommand : Rapcom_SendCommand,
        GetConfig : Rapcom_GetConfig,
        SetConfig : Rapcom_SetConfig,
        KillConnection : Rapcom_KillConnection,
        InternalDisconnected : Rapcom_InternalDisconnected,
        OnConnecting : null,
        OnConnected : null,
        OnDisconnected : null
    };

    // Start a heartbeat for this connection
    connectionObject.HeartbeatObj = setInterval(Rapcom_Heartbeat, 5000, connectionObject);

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
            connectionObject.OnConnecting();
        }
        
        // Try to send a heartbeat to the server.
        connectionObject.SendCommand("Heartbeat", true)
            .complete = function(wasSuccess, jsonResponse)
            {
                // We are connected!
                connectionObject.IsConnected = true;
                connectionObject.LocalIp = jsonResponse.LocalIp;

                if(connectionObject.OnConnected != null)
                {
                    connectionObject.OnConnected();
                }
            };
    }
}

// Called internally when we detect a problem.
function Rapcom_InternalDisconnected()
{
    var wasConnected = this.IsConnected;
    this.IsConnected = false;
    this.LocalIp = null;

    if(wasConnected && this.OnDisconnected != null)
    {
        this.OnDisconnected();
    }
}

// Called externally to stop the connection.
function Rapcom_KillConnection()
{
    clearInterval(this.HeartbeatObj);
    this.InternalDisconnected();
}

// Sends a command to the other side.
function Rapcom_SendCommand(command, waitForResponse, value1, value2, value3, value4)
{
    // Make the json message
    var message = {"Command":command, "Value1":value1,"Value2":value2, "Value3":value3, "Value4":value4};

    // Used to give a callback when the message is sent
    var returnObj = { complete : null};

    // Get our connection object
    var connectionObject = this;

    // This is used to make sure we don't respond back many times
    var hasCalledbackToUser = false;

    // If we are expecting a response generate a response code
    if(waitForResponse)
    {
        // Generate a reponse code
        message.ResponseCode = Math.floor(Math.random() * 100000000000000);
    }
    
    // Send the command
    //var postObj =  $.post("http://localhost/api/v1/command", JSON.stringify(json));
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