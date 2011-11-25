# **********************************************************************
#
# Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
#
# This copy of Ice FIX is licensed to you under the terms described in the
# ICE_FIX_LICENSE file included in this distribution.
#
# **********************************************************************

top_srcdir	= ..

!include $(top_srcdir)/config/Make.rules.mak

SUBDIRS		= cpp cs

$(EVERYTHING)::
	@for %i in ( $(SUBDIRS) ) do \
	    @if exist %i \
	        @echo "making $@ in %i" && \
	        cmd /c "cd %i && $(MAKE) -nologo -f Makefile.mak $@" || exit 1
