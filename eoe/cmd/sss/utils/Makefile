# Makefile for ssdb
#
#       "$Revisions$"
#
include $(ROOT)/usr/include/make/commondefs

SUBDIRS = sslogger db_utils


exports default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
