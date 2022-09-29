#!smake
#
# Makefile for IRIX crash libraries
#
#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/lib/RCS/Makefile,v 1.1 1999/05/25 19:19:20 tjm Exp $"

include $(ROOT)/usr/include/make/commondefs
#include ./commondefs

#
# General definitions.
#
VERSION	= 2.5.3
GCDEFS	= -DICRASH_DEBUG -DI_TEMP
LCDEFS	= -I. -g -n32 -Iinclude -DIRIX$(OSVERS) -DVERSION=\"$(VERSION)\"
LLDLIBS	= -L. -ldwarf -lelf

#
# Alphabetized, grouped list of subdirectories.
#
SUBDIRS	= libeval libhwreg libklib libreadline libsym libutil 

COMMONPREF=lib_

all:default

include $(COMMONRULES)

default:$(SUBDIRS) $(_FORCE)

$(SUBDIRS):$(_FORCE)
	cd $@; make VERSION="$(VERSION)" 

clean clobber install: $(COMMONPREF)$$@

$(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

