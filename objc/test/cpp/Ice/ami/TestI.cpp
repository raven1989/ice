// **********************************************************************
//
// Copyright (c) 2003-2012 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#include <ami/TestI.h>
#include <Ice/Ice.h>

using namespace std;
using namespace Ice;
using namespace Test::AMI;

TestIntfI::TestIntfI() :
_batchCount(0)
{
}

void
TestIntfI::op(const Ice::Current&)
{
}

int
TestIntfI::opWithResult(const Ice::Current& current)
{
    return 15;
}

void
TestIntfI::opWithUE(const Ice::Current&)
{
    throw Test::AMI::TestIntfException();
}

void
TestIntfI::opWithPayload(const Ice::ByteSeq&, const Ice::Current& current)
{
}

void
TestIntfI::opBatch(const Ice::Current&)
{
    IceUtil::Monitor<IceUtil::Mutex>::Lock sync(*this);
    ++_batchCount;
    notify();
}

Ice::Int
TestIntfI::opBatchCount(const Ice::Current&)
{
    IceUtil::Monitor<IceUtil::Mutex>::Lock sync(*this);
    return _batchCount;
}

bool
TestIntfI::waitForBatch(Ice::Int count, const Ice::Current&)
{
    IceUtil::Monitor<IceUtil::Mutex>::Lock sync(*this);
    while(_batchCount < count)
    {
        timedWait(IceUtil::Time::milliSeconds(5000));
    }
    bool result = count == _batchCount;
    _batchCount = 0;
    return result;
}

void
TestIntfI::shutdown(const Ice::Current& current)
{
    current.adapter->getCommunicator()->shutdown();
}

void
TestIntfControllerI::holdAdapter(const Ice::Current&)
{
    _adapter->hold();
}

void
TestIntfControllerI::resumeAdapter(const Ice::Current&)
{
    _adapter->activate();
}

TestIntfControllerI::TestIntfControllerI(const Ice::ObjectAdapterPtr& adapter) : _adapter(adapter)
{
}
