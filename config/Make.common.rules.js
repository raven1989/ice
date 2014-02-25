# **********************************************************************
#
# Copyright (c) 2003-2014 ZeroC, Inc. All rights reserved.
#
# This copy of Ice is licensed to you under the terms described in the
# ICE_LICENSE file included in this distribution.
#
# **********************************************************************

# ----------------------------------------------------------------------
# Don't change anything below this line!
# ----------------------------------------------------------------------

SHELL			= /bin/sh

# Used for the embedded run path
VERSION_MAJOR           = 0
VERSION_MINOR           = 1

# Used for the library versionning
VERSION                 = 3.5.1js
SHORT_VERSION           = 3.5
SOVERSION               = 35

ICE_VERSION             = 3.5.1
ICEJS_VERSION           = 0.1.0

INSTALL			= cp -fp
INSTALL_PROGRAM		= ${INSTALL}
INSTALL_LIBRARY		= ${INSTALL}
INSTALL_DATA		= ${INSTALL}

OBJEXT			= .o

UNAME			:= $(shell uname)
MACHINE_TYPE		:= $(shell uname -m)

#
# Ensure ice_language has been set by the file that includes this one.
#
ifndef ice_language
$(error ice_language must be defined)
endif

ifeq ($(USE_BIN_DIST),yes)
ice_bin_dist = 1
endif

ifeq ($(UNAME),SunOS)
    ifeq ($(MACHINE_TYPE),sun4u)
       lp64suffix	= /64
       lp64binsuffix	= /sparcv9
    endif
    ifeq ($(MACHINE_TYPE),sun4v)
       lp64suffix       = /64
       lp64binsuffix    = /sparcv9
    endif
    ifeq ($(MACHINE_TYPE),i86pc)
	lp64suffix	= /amd64
	lp64binsuffix	= /amd64
    endif
endif

ifeq ($(UNAME),HP-UX)
    lp64suffix		= /pa20_64
    lp64binsuffix	= /pa20_64
endif

ifeq ($(UNAME),Linux)
   ifeq ($(MACHINE_TYPE),x86_64)
      #
      # Some Linux distributions like Debian/Ubuntu don't use /usr/lib64.
      #
      ifeq ($(shell test -d /usr/lib64 && echo 0),0)
          lp64suffix	= 64
      endif
      ifeq ($(LP64),)
          LP64      	= yes
      endif
   endif
endif

ifeq ($(UNAME),Darwin)
    ifeq ($(CPP11),yes)
      cpp11suffix    = /c++11
    endif
endif

ifeq ($(LP64),yes)
	libsubdir		:= lib$(lp64suffix)
    binsubdir		:= bin$(lp64binsuffix)
else
	libsubdir		:= lib
	binsubdir		:= bin
endif

#
# The following variables might also be defined:
# 
# - slice_translator: the name of the slice translator required for the build.
#   Setting this variable is required when building source trees other than the
#   the source distribution (e.g.: the demo sources).
#                  
# - ice_require_cpp: define this variable to check for the presence of the C++
#   dev kit and check for the existence of the include/Ice/Config.h header.
#

#
# First, check if we're building a source distribution. 
#
# If building from a source distribution, ice_dir is defined to the
# top-level directory of the source distribution and ice_cpp_dir is
# defined to the directory containing the C++ binaries and headers to
# use to build the sources.
#
ifndef ice_bin_dist
    ifeq ($(shell test -d $(top_srcdir)/../$(ice_language) && echo 0),0)
        ice_dir = $(top_srcdir)/..
        ice_src_dist = 1

        #
        # When building a source distribution, if ICE_JS_HOME is specified, it takes precedence over 
        # the source tree for building the language mappings. For example, this can be used to 
        # build the Python language mapping using the translators from the distribution specified
        # by ICE_JS_HOME.
        #
	ifneq ($(ICE_JS_HOME),)
	    ifdef slice_translator
		ifneq ($(shell test -f $(ICE_JS_HOME)/$(binsubdir)/$(slice_translator) && echo 0), 0)
$(error Unable to find $(slice_translator) in $(ICE_JS_HOME)/$(binsubdir), please verify ICE_JS_HOME is properly configured and Ice is correctly installed.)
		endif
		ifeq ($(shell test -f $(ice_dir)/cpp/bin/$(slice_translator) && echo 0), 0)
$(warning Found $(slice_translator) in both ICE_JS_HOME/bin and $(ice_dir)/cpp/bin, ICE_JS_HOME/bin/$(slice_translator) will be used!)
		endif
		ice_cpp_dir = $(ICE_JS_HOME)
	    else
$(warning Ignoring ICE_JS_HOME environment variable to build current source tree.)
		ice_cpp_dir = $(ice_dir)/cpp
	    endif
	else
	    ice_cpp_dir = $(ice_dir)/cpp
	endif

    endif
    ifeq ($(shell test -d $(top_srcdir)/ice/$(ice_language) && echo 0),0)
        ice_dir = $(top_srcdir)/ice
        ice_src_dist = 1

        #
        # When building a source distribution, if ICE_JS_HOME is specified, it takes precedence over 
        # the source tree for building the language mappings. For example, this can be used to 
        # build the Python language mapping using the translators from the distribution specified
        # by ICE_JS_HOME.
        #
	ifneq ($(ICE_JS_HOME),)
	    ifdef slice_translator
		ifneq ($(shell test -f $(ICE_JS_HOME)/$(binsubdir)$(cpp11suffix)/$(slice_translator) && echo 0), 0)
$(error Unable to find $(slice_translator) in $(ICE_JS_HOME)/$(binsubdir)$(cpp11suffix), please verify ICE_JS_HOME is properly configured and Ice is correctly installed.)
		endif
		ifeq ($(shell test -f $(ice_dir)/cpp/bin$(cpp11suffix)/$(slice_translator) && echo 0), 0)
$(warning Found $(slice_translator) in both ICE_JS_HOME/bin and $(ice_dir)/cpp/bin, ICE_JS_HOME/bin/$(slice_translator) will be used!)
		endif
		ice_cpp_dir = $(ICE_JS_HOME)
	    else
$(warning Ignoring ICE_JS_HOME environment variable to build current source tree.)
		ice_cpp_dir = $(ice_dir)/cpp
	    endif
	else
	    ice_cpp_dir = $(ice_dir)/cpp
	endif
    endif
endif

ifndef ice_src_dist

    ifndef slice_translator
$(error slice_translator must be defined)
    endif
    
    ifneq ($(ICE_JS_HOME),)
        ifneq ($(shell test -f $(ICE_JS_HOME)/$(binsubdir)$(cpp11suffix)/$(slice_translator) && echo 0), 0)
$(error Unable to find $(slice_translator) in $(ICE_JS_HOME)/$(binsubdir)$(cpp11suffix), please verify ICE_JS_HOME is properly configured and IceJS is correctly installed.)
        endif
        ice_js_dir = $(ICE_JS_HOME)
        
        ifneq ($(shell test -f $(ICE_HOME)/$(binsubdir)$(cpp11suffix)/slice2cpp && echo 0), 0)
$(error Unable to find a valid Ice distribution, please verify ICE_HOME is properly configured and Ice is correctly installed.)
        endif
        ice_dir = $(ICE_HOME)
    else
        ifeq ($(shell test -f $(top_srcdir)/bin/$(slice_translator) && echo 0), 0)
            ice_dir = $(top_srcdir)
        else 
                ifeq ($(shell test -f /usr/bin/$(slice_translator) && echo 0), 0)
                ice_dir = /usr
                ifeq ($(shell test -f /opt/Ice-$(VERSION)/bin/$(slice_translator) && echo 0), 0)
                $(warning Found $(slice_translator) in both /usr/bin and /opt/Ice-$(VERSION)/bin, /usr/bin/$(slice_translator) will be used!)
                endif
                ifeq ($(shell test -f /Library/Developer/Ice-$(VERSION)/bin/$(slice_translator) && echo 0), 0)
                $(warning Found $(slice_translator) in both /usr/bin and /Library/Developer/Ice-$(VERSION)/bin, /usr/bin/$(slice_translator) will be used!)
                endif
            else
                ifeq ($(shell test -f /opt/Ice-$(VERSION)/$(binsubdir)/$(slice_translator) && echo 0), 0)
                    ice_dir = /opt/Ice-$(VERSION)
                else
                    ifeq ($(shell test -f /Library/Developer/Ice-$(VERSION)/$(binsubdir)/$(slice_translator) && echo 0), 0)
                        ice_dir = /Library/Developer/Ice-$(VERSION)
                    endif
                endif
            endif
        endif
    endif
    
    ifndef ice_dir
$(error Unable to find a valid Ice distribution, please verify ICE_JS_HOME is properly configured and Ice is correctly installed.)
    endif
    ice_bin_dist = 1
    ice_cpp_dir = $(ice_dir)
endif
#
# If ice_require_cpp is defined, ensure the C++ headers exist
#
ifeq ($(ice_require_cpp), "yes")
    ifdef ice_src_dist
        ice_cpp_header = $(ice_cpp_dir)/include/Ice/Ice.h
    else
        ice_cpp_header = $(ice_dir)/include/Ice/Ice.h
    endif
    ifneq ($(shell test -f $(ice_cpp_header) && echo 0),0)
$(error Unable to find the C++ header file $(ice_cpp_header), please verify ICE_JS_HOME is properly configured and Ice is correctly installed.)
    endif
endif

#
# Clear the embedded runpath prefix if building against RPM distribution.
#
ifeq ($(ice_dir), /usr)
    embedded_runpath_prefix =
endif

#
# Set slicedir to the path of the directory containing the Slice files.
#
ifeq ($(ice_dir), /usr)
    slicedir = /usr/share/Ice-$(VERSION)/slice
else
    slicedir = $(ice_dir)/slice
endif

ifeq ($(prefix), /usr)
    install_slicedir = /usr/share/Ice-$(VERSION)/slice
else
    install_slicedir = $(prefix)/slice
endif

#
# Set environment variables for the Slice translator.
#
ifneq ($(ice_dir), /usr)
    ifdef ice_src_dist
	ifeq ($(ice_cpp_dir), $(ice_dir)/cpp)
	    ice_lib_dir = $(ice_cpp_dir)/lib
	else
    	ice_lib_dir = $(ice_cpp_dir)/$(libsubdir)
	endif
    else
        ice_lib_dir = $(ice_js_dir)/$(libsubdir)
    endif

    ifeq ($(UNAME),Linux)
        export LD_LIBRARY_PATH := $(ice_lib_dir):$(LD_LIBRARY_PATH)
    endif

    ifeq ($(UNAME),SunOS)
        ifeq ($(LP64),yes)
            export LD_LIBRARY_PATH_64 := $(ice_lib_dir):$(LD_LIBRARY_PATH_64)
        else
            export LD_LIBRARY_PATH := $(ice_lib_dir):$(LD_LIBRARY_PATH)
	endif
    endif

    ifdef ice_src_dist
	    ifeq ($(UNAME),Darwin)
	        export DYLD_LIBRARY_PATH := $(ice_lib_dir):$(DYLD_LIBRARY_PATH)
	    endif
    endif

    ifeq ($(UNAME),AIX)
        export LIBPATH := $(ice_lib_dir):$(LIBPATH)
    endif

    ifeq ($(UNAME),HP-UX)
        ifeq ($(LP64),yes)
	    export LD_LIBRARY_PATH := $(ice_lib_dir)$(lp64dir):$(SHLIB_PATH)
	else
	    export SHLIB_PATH := $(ice_lib_dir):$(LD_LIBRARY_PATH)
	endif
    endif

    ifeq ($(UNAME),FreeBSD)
        export LD_LIBRARY_PATH := $(ice_lib_dir):$(LD_LIBRARY_PATH)
    endif

    ifeq ($(UNAME),OSF1)
        export LD_LIBRARY_PATH := $(ice_lib_dir):$(LD_LIBRARY_PATH)
    endif

    ifneq ($(findstring MINGW,$(UNAME)),)
	set PATH := $(ice_lib_dir);$(PATH)
    endif
endif

ifeq ($(installdata),)
    installdata		= $(INSTALL_DATA) $(1) $(2); \
			  chmod a+r $(2)/$(notdir $(1))
endif

ifeq ($(installprogram),)
    installprogram	= $(INSTALL_PROGRAM) $(1) $(2); \
			  chmod a+rx $(2)/$(notdir $(1))
endif

ifeq ($(mkdir),)
    mkdir		= $(if $(2),mkdir $(2) $(1),mkdir $(1)); \
			  chmod a+rx $(1)
endif

all::

ifeq ($(wildcard $(top_srcdir)/../ICE_LICENSE.txt),) 
    TEXT_EXTENSION =
else 
    TEXT_EXTENSION = .txt 
endif

install-common::
	@if test ! -d $(prefix) ; \
	then \
	    echo "Creating $(prefix)..." ; \
	    $(call mkdir,$(prefix), -p) ; \
	fi

	@if test ! -f $(DESTDIR)$(prefix)/ICE_LICENSE$(TEXT_EXTENSION) ; \
	then \
	    $(call installdata,$(top_srcdir)/../ICE_LICENSE$(TEXT_EXTENSION),$(DESTDIR)$(prefix)) ; \
	fi

	@if test ! -f $(DESTDIR)$(prefix)/LICENSE$(TEXT_EXTENSION) ; \
    then \
        $(call installdata,$(top_srcdir)/../LICENSE$(TEXT_EXTENSION),$(DESTDIR)$(prefix)) ; \
    fi
