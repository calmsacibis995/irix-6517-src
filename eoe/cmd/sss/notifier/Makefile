#
# Makefile for eoe/cmd/sss/notifier
#
# $Revision: 1.1 $

include $(ROOT)/usr/include/make/commondefs

#
# Sub-directories
#
SUBDIRS=qpage ssnotify

HEADERS_SUBDIRS=include

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
