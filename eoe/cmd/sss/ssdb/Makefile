# Makefile for ssdb
#
#       "$Revisions$"
#
include $(ROOT)/usr/include/make/commondefs

EXPORTS_SUBDIRS=ssdbserver
SUBDIRS = ssdbserver ssdb_init

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
