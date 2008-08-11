// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#import <IceObjC/Config.h>

//
// Forward declarations
//
@class ICEObjectAdapter;
@class ICEIdentity;

typedef enum
{
    Normal,
    Nonmutating,
    Idempotent
}
ICEOperationMode;

@interface ICECurrent : NSObject
{
    ICEObjectAdapter* adapter;
    ICEIdentity* id_objc_;
    NSString* facet;
    NSString* operation;
    ICEOperationMode mode;
    NSDictionary* ctx;
    ICEInt requestId;
}
@property(readonly, nonatomic) ICEObjectAdapter* adapter;
@property(readonly, nonatomic) ICEIdentity* id_objc_;
@property(readonly, nonatomic) NSString* facet;
@property(readonly, nonatomic) NSString* operation;
@property(readonly, nonatomic) ICEOperationMode mode;
@property(readonly, nonatomic) NSDictionary* ctx;
@property(readonly, nonatomic) ICEInt requestId;
@end
