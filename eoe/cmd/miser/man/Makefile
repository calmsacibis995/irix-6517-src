#!smake

#
# FILE: eoe/cmd/miser/man/Makefile
#
# DESCRIPTION:
#	Makefile for root level miser man.
#

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS= man1 man4 man5

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
