// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICETOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#import <Ice/Config.h>

#import <Ice/Communicator.h>
#import <Ice/Properties.h>
#import <Ice/Stream.h>

//
// Forward declarations.
//
@protocol ICELogger;

@interface ICEInitializationData : NSObject
{
@private
    id<ICEProperties> properties;
    id<ICELogger> logger;
    NSDictionary* prefixTable;
}
@property(retain, nonatomic) id<ICEProperties> properties;
@property(retain, nonatomic) id<ICELogger> logger;
@property(retain, nonatomic) NSDictionary* prefixTable;

-(id) init:(id<ICEProperties>)properties logger:(id<ICELogger>)logger prefixTable:(NSDictionary*)prefixTable;
+(id) initializationData;
+(id) initializationData:(id<ICEProperties>)properties logger:(id<ICELogger>)logger prefixTable:(NSDictionary*)prefixTable;
// This class also overrides copyWithZone:, hash, isEqual:, and dealloc.
@end

@interface ICEUtil : NSObject
+(id<ICEProperties>) createProperties;
+(id<ICEProperties>) createProperties:(int*)argc argv:(char*[])argv;
+(id<ICECommunicator>) createCommunicator;
+(id<ICECommunicator>) createCommunicator:(ICEInitializationData *)initData;
+(id<ICECommunicator>) createCommunicator:(int*)argc argv:(char*[])argv;
+(id<ICECommunicator>) createCommunicator:(int*)argc argv:(char*[])argv initData:(ICEInitializationData *)initData;
+(id<ICEInputStream>) createInputStream:(id<ICECommunicator>)communicator data:(NSData*)data;
+(id<ICEOutputStream>) createOutputStream:(id<ICECommunicator>)communicator;
+(NSString*) generateUUID;
+(NSArray*)argsToStringSeq:(int)argc argv:(char*[])argv;
+(void)stringSeqToArgs:(NSArray*)args argc:(int*)argc argv:(char*[])argv;
@end
