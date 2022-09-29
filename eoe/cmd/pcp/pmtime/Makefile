#
# Makefile for pmtime
#
# $Revision: 2.9 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	= archiveTime common liveTime pixmaps timer main

LDIRT	=

HEADERS_SUBDIRS=
EXPORTS_SUBDIRS=

default:
	$(SUBDIRS_MAKERULE)

install:	default
	$(SUBDIRS_MAKERULE)

clobber clean rmtargets fluff:
	rm -f ${LDIRT}
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; PWD=`pwd`; export PWD; $(MAKE)

$(_FORCE):
