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

@implementation MyDerivedClassI
-(void) opVoid:(ICECurrent*)current
{
}

-(void) opDerived:(ICECurrent*)current
{
}

-(ICEByte) opByte:(ICEByte)p1 p2:(ICEByte)p2 p3:(ICEByte *)p3 current:(ICECurrent *)current
{
    *p3 = p1 ^ p2;
    return p1;
}

-(BOOL) opBool:(BOOL)p1 p2:(BOOL)p2 p3:(BOOL *)p3 current:(ICECurrent*)current
{
    *p3 = p1;
    return p2;
}

-(ICELong) opShortIntLong:(ICEShort)p1 p2:(ICEInt)p2 p3:(ICELong)p3
                       p4:(ICEShort *)p4 p5:(ICEInt *)p5 p6:(ICELong *)p6
		       current:(ICECurrent *)current
{
    *p4 = p1;
    *p5 = p2;
    *p6 = p3;
    return p3;
}

-(ICEDouble) opFloatDouble:(ICEFloat)p1 p2:(ICEDouble)p2 p3:(ICEFloat *)p3 p4:(ICEDouble *)p4
                   current:(ICECurrent *)current
{
    *p3 = p1;
    *p4 = p2;
    return p2;
}

-(NSString *) opString:(NSMutableString *)p1 p2:(NSMutableString *)p2 p3:(NSString **)p3 current:(ICECurrent *)current
{
    NSMutableString *sout = [NSMutableString stringWithCapacity:([p2 length] + 1 + [p1 length])];
    [sout appendString:p2];
    [sout appendString:@" "];
    [sout appendString:p1];
    *p3 = sout;

    NSMutableString *ret = [NSMutableString stringWithCapacity:([p1 length] + 1 + [p2 length])];
    [ret appendString:p1];
    [ret appendString:@" "];
    [ret appendString:p2];
    return ret;
}

-(TestMyEnum) opMyEnum:(TestMyEnum)p1 p2:(TestMyEnum *)p2 current:(ICECurrent *)current
{
    *p2 = p1;
    return Testenum3;
}

// TODO: opMyClass

-(TestStructure *) opStruct:(TestStructure *)p1 p2:(TestStructure *)p2 p3:(TestStructure **)p3
                    current:(ICECurrent *)current;
{
    *p3 = [[p1 copy] autorelease];
    [[*p3 s] setS:@"a new string"];
    return p2;
}

-(TestByteS *) opByteS:(TestMutableByteS *)p1 p2:(TestMutableByteS *)p2 p3:(TestByteS **)p3 current:(ICECurrent *)current
{
    *p3 = [TestMutableByteS dataWithLength:[p1 length]];
    ICEByte *target = (ICEByte *)[*p3 bytes];
    ICEByte *src = (ICEByte *)[p1 bytes] + [p1 length];
    int i;
    for(i = 0; i != [p1 length]; ++i)
    {
        *target++ = *--src;
    }
    TestMutableByteS * r = [TestMutableByteS dataWithData:p1];
    [r appendData:p2];
    return r;
}

-(TestBoolS *) opBoolS:(TestMutableBoolS *)p1 p2:(TestMutableBoolS *)p2 p3:(TestBoolS **)p3 current:(ICECurrent *)current
{
    *p3 = [TestMutableBoolS dataWithData:p1];
    [(TestMutableBoolS *)*p3 appendData:p2];

    TestMutableBoolS * r = [TestMutableBoolS dataWithLength:[p1 length] * sizeof(BOOL)];
    BOOL *target = (BOOL *)[r bytes];
    BOOL *src = (BOOL *)([p1 bytes] + [p1 length]);
    int i;
    for(i = 0; i != [p1 length]; ++i)
    {
        *target++ = *--src;
    }
    return r;
}

-(TestLongS *) opShortIntLongS:(TestMutableShortS *)p1 p2:(TestMutableIntS *)p2 p3:(TestMutableLongS *)p3
                            p4:(TestShortS **)p4 p5:(TestIntS **)p5 p6:(TestLongS **)p6 current:(ICECurrent *)current
{
    *p4 = [TestMutableShortS dataWithData:p1];
    *p5 = [TestMutableIntS dataWithLength:[p2 length]];
    ICEInt *target = (ICEInt *)[*p5 bytes];
    ICEInt *src = (ICEInt *)([p2 bytes] + [p2 length]);
    int i;
    for(i = 0; i != [p2 length] / sizeof(ICEInt); ++i)
    {
        *target++ = *--src;
    }
    *p6 = [TestMutableLongS dataWithData:p3];
    [(TestMutableLongS *)*p6 appendData:p3];
    return p3;
}

-(TestDoubleS *) opFloatDoubleS:(TestMutableFloatS *)p1 p2:(TestMutableDoubleS *)p2
                             p3:(TestFloatS **)p3 p4:(TestDoubleS **)p4 current:(ICECurrent *)current
{
    *p3 = [TestMutableFloatS dataWithData:p1];
    *p4 = [TestMutableDoubleS dataWithLength:[p2 length]];
    ICEDouble *target = (ICEDouble *)[*p4 bytes];
    ICEDouble *src = (ICEDouble *)([p2 bytes] + [p2 length]);
    int i;
    for(i = 0; i != [p2 length] / sizeof(ICEDouble); ++i)
    {
        *target++ = *--src;
    }
    TestDoubleS *r = [TestMutableDoubleS dataWithLength:([p2 length]
                                                         + ([p1 length] / sizeof(ICEFloat) * sizeof(ICEDouble)))];
    ICEDouble *rp = (ICEDouble *)[r bytes];
    ICEDouble *p2p = (ICEDouble *)[p2 bytes];
    for(i = 0; i < [p2 length] / sizeof(ICEDouble); ++i)
    {
        *rp++ = *p2p++;
    }
    ICEFloat *bp1 = (ICEFloat *)[p1 bytes];
    for(i = 0; i < [p1 length] / sizeof(ICEFloat); ++i)
    {
        *rp++ = bp1[i];
    }
    return r;
}

-(TestStringS *) opStringS:(TestMutableStringS *)p1 p2:(TestMutableStringS *)p2
                        p3:(TestStringS **)p3 current:(ICECurrent *)current
{
    *p3 = [TestMutableStringS arrayWithArray:p1];
    [(TestMutableStringS *)*p3 addObjectsFromArray:p2];
    TestMutableStringS *r = [TestMutableStringS arrayWithCapacity:[p1 count]];
    NSEnumerator *enumerator = [p1 reverseObjectEnumerator];
    for(NSString *element in enumerator)
    {
        [r addObject:element];
    } 
    return r;
}

-(void) shutdown:(ICECurrent*)current
{
    [[[current adapter] getCommunicator] shutdown];
}
@end
