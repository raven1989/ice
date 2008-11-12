# **********************************************************************
#
# Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
#
# This copy of Ice-E is licensed to you under the terms described in the
# ICEE_LICENSE file included in this distribution.
#
# **********************************************************************

top_srcdir	= ..\..\..

CLIENT		= client.exe
SERVER		= server.exe
COLLOCATED	= collocated.exe

TARGETS		= $(CLIENT) $(SERVER) $(COLLOCATED)

OBJS		= Test.obj \
	          Wstring.obj

COBJS           = Client.obj \
		  AllTests.obj \
		  MyByteSeq.obj \
		  StringConverterI.obj

SOBJS           = TestI.obj \
		  WstringI.obj \
		  Server.obj \
		  MyByteSeq.obj \
		  StringConverterI.obj

COLOBJS         = TestI.obj \
		  WstringI.obj \
		  Collocated.obj \
		  AllTests.obj \
		  MyByteSeq.obj \
		  StringConverterI.obj

SRCS		= $(OBJS:.obj=.cpp) \
		  $(COBJS:.obj=.cpp) \
		  $(SOBJS:.obj=.cpp) \
		  $(COLOBJS:.obj=.cpp)

!include $(top_srcdir)/config/Make.rules.mak

CPPFLAGS	= -I. -I../../include $(CPPFLAGS) -WX -Zm200 -DWIN32_LEAN_AND_MEAN
!if "$(ice_bin_dist)" != ""
LDFLAGS		= $(LDFLAGS) /LIBPATH:"$(libdir)"
!endif
!if "$(HAS_OBV)" == "yes"
SLICE2CPPEFLAGS = -DICEE_HAS_OBV $(SLICE2CPPEFLAGS)
!endif

!if "$(OPTIMIZE_SPEED)" != "yes" && "$(OPTIMIZE_SIZE)" != "yes"
CPDBFLAGS        = /pdb:$(CLIENT:.exe=.pdb)
SPDBFLAGS        = /pdb:$(SERVER:.exe=.pdb)
COPDBFLAGS       = /pdb:$(COLLOCATED:.exe=.pdb)
!endif

$(CLIENT): $(OBJS) $(COBJS)
	$(LINK) $(LDFLAGS) $(CPDBFLAGS) TestC.obj WstringC.obj $(COBJS) /out:$@ $(TESTCLIBS)
	@if exist $@.manifest echo ^ ^ ^ Embedding manifest using $(MT) && \
	    $(MT) -nologo -manifest $@.manifest -outputresource:$@;#2 && del /q $@.manifest

$(SERVER): $(OBJS) $(SOBJS)
	$(LINK) $(LDFLAGS) $(SPDBFLAGS) $(OBJS) $(SOBJS) /out:$@ $(TESTLIBS)
	@if exist $@.manifest echo ^ ^ ^ Embedding manifest using $(MT) && \
	    $(MT) -nologo -manifest $@.manifest -outputresource:$@;#2 && del /q $@.manifest

$(COLLOCATED): $(OBJS) $(COLOBJS)
	$(LINK) $(LDFLAGS) $(COPDBFLAGS) $(OBJS) $(COLOBJS) /out:$@ $(TESTLIBS)
	@if exist $@.manifest echo ^ ^ ^ Embedding manifest using $(MT) && \
	    $(MT) -nologo -manifest $@.manifest -outputresource:$@;#2 && del /q $@.manifest

TestC.obj Test.obj: Test.cpp
	$(CXX) /c $(CPPFLAGS) $(CXXFLAGS) Test.cpp
	$(CXX) /c -DICEE_PURE_CLIENT /FoTestC.obj $(CPPFLAGS) $(CXXFLAGS) Test.cpp

WstringC.obj Wstring.obj: Wstring.cpp
	$(CXX) /c $(CPPFLAGS) $(CXXFLAGS) Wstring.cpp
	$(CXX) /c -DICEE_PURE_CLIENT /FoWstringC.obj $(CPPFLAGS) $(CXXFLAGS) Wstring.cpp

clean::
	del /q Test.cpp Test.h
	del /q Wstring.cpp Wstring.h
	del /q WstringAMD.cpp WstringAMD.h

!include .depend
