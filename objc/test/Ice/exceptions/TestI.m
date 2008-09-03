// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

#import <Ice/Ice.h>

#import <TestI.h>
#import <TestCommon.h>
#import <Foundation/NSException.h>

@interface FooException : NSException
@end

@implementation FooException
-(id)init
{
    if(![super initWithName:@"FooException" reason:@"no reason" userInfo:nil])
    {
        return nil;
    }
    return self;
}
@end

@implementation ThrowerI
+(id) throwerI:(id<ICEObjectAdapter>)adapter
{
     ThrowerI *t = [[[ThrowerI alloc] init] autorelease];
     t->_adapter = adapter;
     return t;
}

-(void) shutdown:(ICECurrent*)current
{
    [[[current adapter] getCommunicator] shutdown];
}

-(BOOL) supportsUndeclaredExceptions:(ICECurrent*)current
{
    return YES;
}

-(BOOL) supportsAssertException:(ICECurrent*)current
{
    return NO;
}

-(void) throwAasA:(ICEInt)a current:(ICECurrent*)current
{
    @throw [TestA a:a];
}

-(void) throwAorDasAorD:(ICEInt)a current:(ICECurrent*)current
{
    if(a > 0)
    {
        @throw [TestA a:a];
    }
    else
    {
        @throw [TestD d:a];
    }
}

-(void) throwBasA:(ICEInt)a b:(ICEInt)b current:(ICECurrent*)current
{
    [self throwBasB:a b:b current:current];
}

-(void) throwCasA:(ICEInt)a b:(ICEInt)b c:(ICEInt) c current:(ICECurrent*)current
{
    [self throwCasC:a b:b c:c current:current];
}

-(void) throwBasB:(ICEInt)a b:(ICEInt)b current:(ICECurrent*)current
{
    @throw [TestB b:a bMem:b];
}

-(void) throwCasB:(ICEInt)a b:(ICEInt)b c:(ICEInt)c current:(ICECurrent*)current
{
    [self throwCasC:a b:b c:c current:current];
}

-(void) throwCasC:(ICEInt)a b:(ICEInt)b c:(ICEInt)c current:(ICECurrent*)current
{
    @throw [TestC c:a bMem:b cMem:c];
}

-(void) throwModA:(ICEInt)a a2:(ICEInt)a2 current:(ICECurrent*)current
{
    @throw [TestModA a:a a2Mem:a2];
}

-(void) throwUndeclaredA:(ICEInt)a current:(ICECurrent*)current
{
    @throw [TestA a:a];
}

-(void) throwUndeclaredB:(ICEInt)a b:(ICEInt)b current:(ICECurrent*)current
{
    @throw [TestB b:a bMem:b];
}

-(void) throwUndeclaredC:(ICEInt)a b:(ICEInt)b c:(ICEInt)c current:(ICECurrent*)current
{
    @throw [TestC c:a bMem:b cMem:c];
}

-(void) throwLocalException:(ICECurrent*)current
{
    @throw [ICETimeoutException timeoutException:__FILE__ line:__LINE__];
}

-(void) throwNonIceException:(ICECurrent*)current
{
    @throw [[[FooException alloc] init] autorelease];
}

-(void) throwAssertException:(ICECurrent*)current
{
    // Not supported.
}
@end
