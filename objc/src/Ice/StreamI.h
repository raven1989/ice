// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#import <Ice/Stream.h>
#import <Ice/Wrapper.h>

#include <IceCpp/Stream.h>

@interface ICEInputStream : ICEInternalWrapper<ICEInputStream>
{
    Ice::InputStream* is_;
}
+(void)installObjectFactory:(const Ice::CommunicatorPtr&)communicator;
-(Ice::InputStream*) is;
@end

@interface ICEOutputStream : ICEInternalWrapper<ICEOutputStream>
{
    Ice::OutputStream* os_;
    std::map<Ice::ObjectPtr, Ice::ObjectPtr>* objectWriters_;
}
-(Ice::OutputStream*) os;
@end
