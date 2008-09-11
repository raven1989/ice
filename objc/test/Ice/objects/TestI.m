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


@implementation BI
-(BOOL) postUnmarshalInvoked:(ICECurrent*)current
{
    return _postUnmarshalInvoked;
}
-(void) ice_preMarshal
{
    preMarshalInvoked = YES;
}
-(void) ice_postUnmarshal
{
    _postUnmarshalInvoked = YES;
}
@end

@implementation CI
-(BOOL) postUnmarshalInvoked:(ICECurrent*)current
{
    return _postUnmarshalInvoked;
}
-(void) ice_preMarshal
{
    preMarshalInvoked = YES;
}
-(void) ice_postUnmarshal
{
    _postUnmarshalInvoked = YES;
}
@end

@implementation DI
-(BOOL) postUnmarshalInvoked:(ICECurrent*)current
{
    return _postUnmarshalInvoked;
}
-(void) ice_preMarshal
{
    preMarshalInvoked = YES;
}
-(void) ice_postUnmarshal
{
    _postUnmarshalInvoked = YES;
}
@end

@implementation EI
-(id) init
{
    if(![super init:1 s:@"hello"])
    {
        return nil;
    }
    return self;
}
-(BOOL) checkValues:(ICECurrent*)current
{
    return i == 1 && [s isEqualToString:@"hello"];
}
@end

@implementation FI
-(id) init:(TestE*)e1_ e2:(TestE*)e2_
{
    if(![super init:e1_ e2:e2_])
    {
        return nil;
    }
    return self;
}
-(BOOL) checkValues:(ICECurrent*)current
{
    return e1 && e1 == e2;
}
@end

@implementation HI
@end

@implementation II
@end

@implementation JI
@end

@implementation InitialI

-(id) init
{
    if(![super init])
    {
        return nil;
    }

    _b1 = [[BI alloc] init];
    _b2 = [[BI alloc] init];
    _c = [[CI alloc] init];
    _d = [[DI alloc] init];
    _e = [[EI alloc] init];
    _f = [[FI alloc] init:_e e2:_e];

    _b1.theA = _b2; // Cyclic reference to another B
    _b1.theB = _b1; // Self reference.
    _b1.theC = nil; // Null reference.

    _b2.theA = _b2; // Self reference, using base.
    _b2.theB = _b1; // Cyclic reference to another B
    _b2.theC = _c; // Cyclic reference to a C.

    _c.theB = _b2; // Cyclic reference to a B.

    _d.theA = _b1; // Reference to a B.
    _d.theB = _b2; // Reference to a B.
    _d.theC = nil; // Reference to a C.
    return self;
}
-(void) dealloc
{
    _b1.theA = nil; // Break cyclic reference.
    _b1.theB = nil; // Break cyclic reference.

    _b2.theA = nil; // Break cyclic reference.
    _b2.theB = nil; // Break cyclic reference.
    _b2.theC = nil; // Break cyclic reference.

    _c.theB = nil; // Break cyclic reference.

    _d.theA = nil; // Break cyclic reference.
    _d.theB = nil; // Break cyclic reference.
    _d.theC = nil; // Break cyclic reference.

    [_b1 release];
    [_b2 release];
    [_c release];
    [_d release];
    [_e release];
    [_f release];
    [super dealloc];
}

-(void) shutdown:(ICECurrent*)current
{
    [[current.adapter getCommunicator] shutdown];
}

-(TestB*) getB1:(ICECurrent*)current
{
    _b1.preMarshalInvoked = NO;
    _b2.preMarshalInvoked = NO;
    _c.preMarshalInvoked = NO;
    return _b1;
}

-(TestB*) getB2:(ICECurrent*)current
{
    _b1.preMarshalInvoked = NO;
    _b2.preMarshalInvoked = NO;
    _c.preMarshalInvoked = NO;
    return _b2;
}


-(TestC*) getC:(ICECurrent*)current
{
    _b1.preMarshalInvoked = NO;
    _b2.preMarshalInvoked = NO;
    _c.preMarshalInvoked = NO;
    return _c;
}


-(TestD*) getD:(ICECurrent*)current
{
    _b1.preMarshalInvoked = NO;
    _b2.preMarshalInvoked = NO;
    _c.preMarshalInvoked = NO;
    _d.preMarshalInvoked = NO;
    return _d;
}


-(TestE*) getE:(ICECurrent*)current
{
    return _e;
}


-(TestF*) getF:(ICECurrent*)current
{
    return _f;
}


-(void) getAll:(TestB **)b1 b2:(TestB **)b2 theC:(TestC **)theC theD:(TestD **)theD current:(ICECurrent *)current;
{
    _b1.preMarshalInvoked = NO;
    _b2.preMarshalInvoked = NO;
    _c.preMarshalInvoked = NO;
    _d.preMarshalInvoked = NO;
    *b1 = _b1;
    *b2 = _b2;
    *theC = _c;
    *theD = _d;
}

-(TestI*) getI:(ICECurrent*)current
{
    return [[[TestI alloc] init] autorelease];
}

-(TestI*) getJ:(ICECurrent*)current
{
    return [[[TestJ alloc] init] autorelease];
}

-(TestI*) getH:(ICECurrent*)current
{
    return [[[TestH alloc] init] autorelease];
}

-(void) setI:(TestI*)i current:(ICECurrent*)current
{
}

-(TestObjectSeq *) getObjectSeq:(TestMutableObjectSeq *)s current:(ICECurrent*)current
{
    return s;
}

-(TestObjectDict *) getObjectDict:(TestMutableObjectDict *)d current:(ICECurrent*)current
{
    return d;
}
@end

@implementation UnexpectedObjectExceptionTestI
-(BOOL)ice_invoke:(NSData*)inParams outParams:(NSData**)outParams current:(ICECurrent*)current
{
    id<ICECommunicator> communicator = [[current adapter] getCommunicator];
    id<ICEOutputStream> o = [ICEUtil createOutputStream:communicator];
    TestAlsoEmpty* ae = [[TestAlsoEmpty alloc] init];
    [o writeObject:ae];
    [o writePendingObjects];
    *outParams = [o finished];
    return YES;
}
@end

