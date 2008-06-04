// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice-E is licensed to you under the terms described in the
// ICEE_LICENSE file included in this distribution.
//
// **********************************************************************

public class ChatCallbackI extends Demo._ChatCallbackDisp
{
    public
    ChatCallbackI()
    {
    }

    public void
    message(String data, Ice.Current current)
    {
        System.out.println(data);
    }
}
