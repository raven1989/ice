// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>

@interface Ice_Properties : NSObject
{
    void* properties__;
}
-(NSString*) getProperty:(NSString*)key;
-(NSString*) getPropertyWithDefault:(NSString*)key value:(NSString*)value;
-(int) getPropertyAsInt:(NSString*)key;
-(int) getPropertyAsIntWithDefault:(NSString*)key value:(int)value;
-(NSArray*) getPropertyAsList:(NSString*)key;
-(NSArray*) getPropertyAsListWithDefault:(NSString*)key value:(NSArray*)value;
-(NSDictionary*) getPropertiesForPrefix:(NSString*)prefix;
-(void) setProperty:(NSString*)key value:(NSString*)value;
-(NSArray*) getCommandLineOptions;
-(NSArray*) parseCommandLineOptions:(NSString*)prefix options:(NSArray*)options;
-(NSArray*) parseIceCommandLineOptions:(NSArray*)options;
-(void) load:(NSString*)file;
-(Ice_Properties*) clone;

@end
