#
# Makefile for irix/cmd/ns
#
# $Revision: 1.7 $

include	$(ROOT)/usr/include/make/commondefs

#
# Alphabetized, grouped list of subdirectories.
#

# Turn off directories :
#	NONE
#
SUBDIRS= makemdbm mdbm_remove nsadmin nsd nsmount ypxfr ypinit

HEADERS_SUBDIRS= NONE
EXPORTS_SUBDIRS= NONE

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
