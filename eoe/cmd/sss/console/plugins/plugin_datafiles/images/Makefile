#!smake
#
#
SRC_DEPTH = ../../../..

include $(ROOT)/usr/include/make/commondefs
include $(SRC_DEPTH)/include/ssscommondefs

HEADERS_SUBDIRS=
EXPORTS_SUBDIRS=
SUBDIRS = images

DESTPATH = /var/esp/ssc/images
HTMLFILES = *.gif
IDB_TAG = -idb $(SSS_EOE)

default: $(TARGETS)

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)
	${INSTALL} $(IDB_TAG) -F $(DESTPATH) $(HTMLFILES)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)


clobber clean:

$(_FORCE):