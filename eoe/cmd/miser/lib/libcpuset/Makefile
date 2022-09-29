#!smake

#
# FILE: eoe/cmd/miser/lib/libcpuset
#
# DESCRIPTION:
#	Makefile for libcpuset
#

include $(ROOT)/usr/include/make/commondefs

SUBDIRS	= src

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
