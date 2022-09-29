#
# Makefile for irix/man
#
# $Revision: 1.2 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS= man3

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
