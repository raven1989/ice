// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef LIBRARY_SESSION_ICE
#define LIBRARY_SESSION_ICE

module Demo
{

/* Forward declaration. */
interface Library;

/**
 *
 * The session object. This is used to retrieve a per-session library
 * on behalf of the client. If the session is not refreshed on a
 * periodic basis, it will be automatically destroyed.
 *
 */
["ami"] interface Session
{
    /**
     *
     * Get the library object.
     *
     * @return A proxy for the new library.
     *
     **/
    Library* getLibrary();

    /**
     *
     * Refresh a session. If a session is not refreshed on a regular
     * basis by the client, it will be automatically destroyed.
     *
     **/
    idempotent void refresh();

    /**
     *
     * Destroy the session.
     *
     **/
    void destroy();
};

/**
 *
 * Interface to create new sessions.
 *
 **/
["ami"] interface SessionFactory
{
    /**
     *
     * Create a session. 
     *
     * @return A proxy to the session.
     *
     **/
    Session* create();

    /**
     *
     * Get the value of the session timeout. Sessions are destroyed
     * if they see no activity for this period of time.
     *
     * @return The timeout (in seconds).
     *
     **/
    ["nonmutating", "cpp:const"] idempotent long getSessionTimeout();
};

};

#endif
