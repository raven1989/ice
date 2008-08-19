// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#import <Hello_S.h>

#import <Ice/LocalException.h>

@implementation DemoHello

static const char* DemoHello_ids__[2] =
{
    "::Demo::Hello",
    "::Ice::Object"
};

static const char* DemoHello_all__[5] =
{
    "ice_id",
    "ice_ids",
    "ice_isA",
    "ice_ping",
    "sayHello"
};

+(const char**) staticIds__:(int*)count
{
    *count = sizeof(DemoHello_ids__) / sizeof(const char*);
    return DemoHello_ids__;
}

-(BOOL) sayHello___:(ICECurrent*)current is:(id<ICEInputStream>)is os:(id<ICEOutputStream>)os
{
    [self checkModeAndSelector__:ICEIdempotent selector:@selector(sayHello:) current:current];
    [(id<DemoHello>)self sayHello:current];
    return TRUE;
}
-(BOOL) dispatch__:(ICECurrent*)current is:(id<ICEInputStream>)is os:(id<ICEOutputStream>)os
{
    switch(ICELookupString(DemoHello_all__, sizeof(DemoHello_all__) / sizeof(const char*), [[current operation] UTF8String]))
    {
    case 0:
        return [self ice_id___:current is:is os:os];
    case 1:
        return [self ice_ids___:current is:is os:os];
    case 2:
        return [self ice_isA___:current is:is os:os];
    case 3:
        return [self ice_ping___:current is:is os:os];
    case 4:
        return [self sayHello___:current is:is os:os];
    default:
        @throw [ICEOperationNotExistException requestFailedException:__FILE__ 
                                              line:__LINE__ 
                                              id_:current.id_ 
                                              facet:current.facet
                                              operation:current.operation];
    }
}

@end
