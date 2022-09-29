#!smake

#
# FILE: eoe/cmd/miser/lib/libmiser/Makefile
#
# DESCRIPTION:
#	Makefile for libmiser 
#

include $(ROOT)/usr/include/make/commondefs

SUBDIRS	= src test

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
