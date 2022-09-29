#
# Makefile for irix/man/ftn
#
# $Revision: 1.6 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS= man2   man3c  man3p man3s

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
