#!smake
#
# Makefile for my_ism
#
#
#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs

HEADERS_SUBDIRS=include
EXPORTS_SUBDIRS=libsgmtask
SUBDIRS=libsgmtask rpcserver confupdttask

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

#install:$(_FORCE)
#	$(SUBDIRS_MAKERULE)

$(_FORCE):
