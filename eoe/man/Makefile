#!smake
#
# Makefile for irix/man
#
# $Revision: 1.13 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS= man1   man1m  man2   man3   man3b  man3c  man3g  man3n \
	 man3p  man3s  man3t  man3w  man3x  man4   man5  \
	 man7m  manD ftn

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
