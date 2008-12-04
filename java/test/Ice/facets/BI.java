// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package test.Ice.facets;

import test.Ice.facets.Test._BDisp;

public final class BI extends _BDisp
{
    public
    BI()
    {
    }

    public String
    callA(Ice.Current current)
    {
        return "A";
    }

    public String
    callB(Ice.Current current)
    {
        return "B";
    }
}
