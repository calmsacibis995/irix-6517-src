#
# Makefile for irix/cmd/gnu
#
# $Revision: 1.1 $

include	$(ROOT)/usr/include/make/commondefs

#
# Alphabetized, grouped list of subdirectories.
#
SUBDIRS= \
	bison flex

HEADERS_SUBDIRS=
EXPORTS_SUBDIRS=

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
