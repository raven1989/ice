// **********************************************************************
//
// Copyright (c) 2003-2013 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

(function(){

var Promise = Ice.Promise;
var RouterPrx = Glacier2.RouterPrx;
var ChatSessionPrx = Demo.ChatSessionPrx;
var ChatCallbackPrx = Demo.ChatCallbackPrx;

//
// Servant that implements the ChatCallback interface,
// the message operation just writes the received data
// to output textarea.
//
var ChatCallbackI = Ice.Class(Demo.ChatCallback, {
    message: function(data)
    {
        $("#output").val($("#output").val() + data + "\n");
        $("#output").scrollTop($("#output").get(0).scrollHeight);
    }
});

//
// Chat client state
//
var State = {Disconnected: 0, Connecting: 1, Connected:2 };

var communicator;
var hasError = false;
var state = State.Disconnected;

var signin = function()
{
    Promise.try(
        function()
        {
            state = State.Connecting;
            //
            // Dismiss any previous error message.
            //
            if(hasError)
            {
                $("#signin-alert").click();
            }
            //
            // Transition to loading screen
            //
            return transition("#signin-form", "#loading");
        }
    ).then(
        function()
        {
            startProgress();
            //
            // Initialize the communicator with Ice.Default.Router property
            // set to the chat demo Glacier2 router.
            //
            var id = new Ice.InitializationData();
            id.properties = Ice.createProperties();
            //id.properties.setProperty("Ice.Trace.Protocol", "1");
            id.properties.setProperty("Ice.Default.Router", 
                                "DemoGlacier2/router:ws -p 4063 -h localhost");
            communicator = Ice.initialize(id);
            
            //
            // Get a proxy to the Glacier2 router, using checkedCast to ensure
            // the Glacier2 server is available.
            //
            return RouterPrx.checkedCast(communicator.getDefaultRouter()).then(
                function(r, router)
                {
                    //
                    // Create a session with the Glacier2 router.
                    //
                    return router.createSession(
                        $("#username").val(), $("#password").val()).then(
                            function(r, session)
                            {
                                run(router, ChatSessionPrx.uncheckedCast(session));
                            });
                });
        }
    ).exception(
        function(ex)
        {
            //
            // Handle any exceptions occurred during session creation.
            //
            if(ex instanceof Glacier2.PermissionDeniedException)
            {
                error("permission denied:\n" + ex.reason);
            }
            else if(ex instanceof Glacier2.CannotCreateSessionException)
            {
                error("cannot create session:\n" + ex.reason);
            }
            else if(ex instanceof Ice.ConnectFailedException)
            {
                error("connection to server failed");
            }
            else
            {
                error(ex.toString());
            }
        });
};

var run = function(router, session)
{
    var refreshSession;
    var chat = new Promise();
    //
    // Get the session timeout, the router client category and
    // create the client object adapter.
    //
    // Use Ice.Promise.all to wait for the completion of all the
    // calls.
    //
    Promise.all(
        router.getSessionTimeout(),
        router.getCategoryForClient(),
        communicator.createObjectAdapterWithRouter("", router)
    ).then(
        function()
        {
            var timeout = arguments[0][1];
            var category = arguments[1][1];
            var adapter = arguments[2][1];
            
            //
            // Setup an interval call to router refreshSession 
            // to keep the session alive.
            //
            refreshSession = setInterval(
                function(){
                    router.refreshSession().exception(
                        function(ex){
                            chat.fail(ex);
                        });
                }, (timeout.low * 500));
            
            //
            // Create the ChatCallback servant and add it to the 
            // ObjectAdapter.
            //
            var callback = ChatCallbackPrx.uncheckedCast(
                adapter.add(new ChatCallbackI(), 
                            new Ice.Identity("callback", category)));
            
            //
            // Activate the client object adater before set the session
            // callback.
            //
            adapter.activate();

            //
            // Set the chat session callback.
            //
            return session.setCallback(callback);
        }
    ).then(
        function()
        {
            stopProgress(true);
            return transition("#loading", "#chat-form");
        }
    ).then(
        function()
        {
            $("#loading .meter").css("width", "0%");
            state = State.Connected;
            $("#input").focus();
            //
            // Process input events in the input texbox until
            // chat promise is completed.
            //
            $("#input").keypress(
                function(e)
                {
                    if(!chat.completed())
                    {
                        //
                        // When enter key is pressed we send a new
                        // message using session say operation and
                        // reset the textbox contents.
                        //
                        if(e.which === 13)
                        {
                            var msg = $(this).val();
                            $(this).val("");
                            session.say(msg).exception(
                                function(){
                                    chat.fail(ex);
                                });
                            return false;
                        }
                    }
                });
            
            //
            // Exit the chat loop accepting the chat
            // promise.
            //
            $("#signout").click(
                function(){
                    chat.succeed();
                    return false;
                });
            
            return chat;
        }
    ).finally(
        function()
        {
            //
            // Reset the input text box and chat output
            // textarea.
            //
            $("#input").val("");
            $("#input").off("keypress");
            $("#signout").off("click");
            $("#output").val("");
            
            //
            // Clear the refresh interval and destroy
            // the session.
            //
            clearInterval(refreshSession);
            return router.destroySession();
        }
    ).then(
        function()
        {
            //
            // Destroy the communicator and go back to the 
            // disconnected state.
            //
            var c = communicator;
            communicator = null;
            c.destroy().finally(
                function()
                {
                    transition("#chat-form", "#signin-form").finally(
                        function()
                        {
                            $("#username").focus();
                            state = State.Disconnected;
                        });
                });
        }
    ).exception(
        function(ex)
        {
            //
            // Handle any exceptions occurred while running.
            //
            error(ex);
        });
};

//
// Switch to Disconnected state and display the error 
// message.
//
var error = function(message)
{
    stopProgress(false);
    hasError = true;
    var current = state === State.Connecting ? "#loading" : "#chat-form";
    $("#signin-alert span").text(message);
    
    //
    // First destroy the communicator if needed then do
    // the screen transition.
    //
    Promise.try(
        function()
        {
            if(communicator)
            {
                var c = communicator;
                communicator = null;
                return c.destroy();
            }
        }
    ).finally(
        function()
        {
            transition(current, "#signin-alert").then(
                function(){
                    $("#loading .meter").css("width", "0%");
                    $("#signin-form").css("display", "block")
                        .animo({ animation: "flipInX", keep: true });
                    state = State.Disconnected;
                });
        });
};

//
// Do a transition from "from" screen to "to" screen, return
// a promiset that allows to wait for the transition 
// completion.
//
var transition = function(from, to)
{
    var p = new Ice.Promise();
    
    $(from).animo({ animation: "flipOutX", keep: true },
        function()
        {
            $(from).css("display", "none");
            if(to)
            {
                $(to).css("display", "block")
                    .animo({ animation: "flipInX", keep: true },
                        function(){ p.succeed(); });
            }
            else
            {
                p.succeed();
            }
        });
    return p;
};

//
// Event handler for Sign in button
//
$("#signin").click(function(){
    signin();
    return false;
});

//
// Dismiss error message on click.
//
$("#signin-alert").click(function(){
    transition("#signin-alert");
    hasError = false;
    return false;
});

//
// Set default height of output textarea
//
$("#output").height(300);

//
// Animate the loading progress bar.
//
var w = 0;
var progress;

var startProgress = function()
{
    progress = setInterval(
        function()
        {
            w = w === 100 ? 0 : w + 5;
            $("#loading .meter").css("width", w.toString() + "%");
        }, 
        20);
};

var stopProgress = function(completed)
{
    clearInterval(progress);
    progress = null;
    if(completed)
    {
        $("#loading .meter").css("width", "100%");
    }
};

$("#username").focus();

}());