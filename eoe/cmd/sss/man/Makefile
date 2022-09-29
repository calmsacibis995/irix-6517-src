#!smake
#
# Makefile for availmon's man pages
#
# ident "$Revision: 1.1 $"

include $(ROOT)/usr/include/make/commondefs

SUBDIRS=  man1 man1m man5

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
