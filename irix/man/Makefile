#!smake
#
# Makefile for irix/man
#
# $Revision: 1.15 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS= man1 man2 man3p man5 man1m man3c man4 man7m ftn


default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
