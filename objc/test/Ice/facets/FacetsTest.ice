// **********************************************************************
//
// Copyright (c) 2003-2011 ZeroC, Inc. All rights reserved.
//
// This copy of Ice Touch is licensed to you under the terms described in the
// ICE_TOUCH_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef TEST_ICE
#define TEST_ICE

["objc:prefix:TestFacets"]
module Test
{

interface Empty
{
};

interface A
{
    string callA();
};

interface B extends A
{
    string callB();
};

interface C extends A
{
    string callC();
};

interface D extends B, C
{
    string callD();
};

interface E
{
    string callE();
};

interface F extends E
{
    string callF();
};

interface G
{
    void shutdown();
    string callG();
};

interface H extends G
{
    string callH();
};

};

#endif
