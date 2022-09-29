#!smake
#
# Makefile for IRIX crash libraries
#
#ident "$Header: "

DEPTH?=..
include ${DEPTH}/icrash_commondefs

#
# Alphabetized, grouped list of subdirectories.
#
SUBDIRS	= libeval libreadline libhwreg

COMMONPREF=lib_

all:default

include $(COMMONRULES)

default:$(SUBDIRS) $(_FORCE)

$(SUBDIRS):$(_FORCE)
	cd $@; make VERSION="$(VERSION)" 

clean clobber install: $(COMMONPREF)$$@

$(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

