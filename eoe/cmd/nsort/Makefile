#!smake
#
# Top-Level Makefile for Ordinal's nsort command
#
# $Revision: 1.1 $

include	$(ROOT)/usr/include/make/commondefs

#
# Alphabetized, grouped list of subdirectories.
#
SUBDIRS= examples man src 

#HEADERS_SUBDIRS=
#EXPORTS_SUBDIRS=

COMMONPREF=nsort

default install: $(_FORCE)
	$(SUBDIRS_MAKERULE)

include	$(COMMONRULES)

# XXX No headers or exports needed - null these two rules out?
headers: $(_FORCE)
#	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
#	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

clobber: nsortclobber
	cd src; make clobber
