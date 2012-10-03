// **********************************************************************
//
// Copyright (c) 2003-2012 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef HOLD_TEST_I_H
#define HOLD_TEST_I_H

#include <HoldTest.h>

class HoldI : public Test::Hold, public IceUtil::Mutex, public IceUtil::TimerTask
{
public:
    
    HoldI(const IceUtil::TimerPtr&, const Ice::ObjectAdapterPtr&);
    
    virtual void putOnHold(Ice::Int, const Ice::Current&);
    virtual void waitForHold(const Ice::Current&);
    virtual Ice::Int set(Ice::Int, Ice::Int, const Ice::Current&);
    virtual void setOneway(Ice::Int, Ice::Int, const Ice::Current&);
    virtual void shutdown(const Ice::Current&);
    
    virtual void runTimerTask();
    
private:
    
    int _last;
    const IceUtil::TimerPtr _timer;
    const Ice::ObjectAdapterPtr _adapter;
};


#endif
