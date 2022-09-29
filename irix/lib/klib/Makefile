#!smake
#
# Makefile for klib libraries
#
#ident "$Header: "

include $(ROOT)/usr/include/make/commondefs

HEADERS_SUBDIRS = include liballoc
EXPORTS_SUBDIRS = liballoc libhwconfig libkern libklib libmem \
				  libswconfig libsym libtrace libutil

SUBDIRS=$(EXPORTS_SUBDIRS)

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

install: headers 

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
