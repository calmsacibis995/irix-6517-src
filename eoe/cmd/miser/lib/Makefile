#!smake

#
# FILE: eoe/cmd/miser/lib/Makefile
#
# DESCRIPTION:
#	Makefile for miser and cpuset libraries.
#

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = libmiser libcpuset

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
