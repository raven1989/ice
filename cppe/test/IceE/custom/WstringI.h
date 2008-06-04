// **********************************************************************
//
// Copyright (c) 2003-2007 ZeroC, Inc. All rights reserved.
//
// This copy of Ice-E is licensed to you under the terms described in the
// ICEE_LICENSE file included in this distribution.
//
// **********************************************************************

#ifndef WSTRING_I_H
#define WSTRING_I_H

#include <IceE/Config.h>

#ifdef ICEE_HAS_WSTRING

#include <Wstring.h>

namespace Test1
{

class WstringClassI : virtual public WstringClass
{
public:

    virtual ::std::wstring opString(const ::std::wstring&,
                                    ::std::wstring&,
                                    const Ice::Current&);

    virtual ::Test1::WstringStruct opStruct(const ::Test1::WstringStruct&,
                                            ::Test1::WstringStruct&,
                                            const Ice::Current&);

    virtual void throwExcept(const ::std::wstring&,
                             const Ice::Current&);
};

}

namespace Test2
{

class WstringClassI : virtual public WstringClass
{
public:

    virtual ::std::wstring opString(const ::std::wstring&,
                                    ::std::wstring&,
                                    const Ice::Current&);

    virtual ::Test2::WstringStruct opStruct(const ::Test2::WstringStruct&,
                                            ::Test2::WstringStruct&,
                                            const Ice::Current&);

    virtual void throwExcept(const ::std::wstring&,
                             const Ice::Current&);
};

}

#endif

#endif
