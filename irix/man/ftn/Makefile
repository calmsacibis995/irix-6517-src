#
# Makefile for irix/man/ftn
#
# $Revision: 1.7 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS= man2 man3p

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
