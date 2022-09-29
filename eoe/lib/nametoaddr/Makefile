#
#
include ${ROOT}/usr/include/make/commondefs

SUBDIRS = nis resolv straddr tcpip 
EXPORTS_SUBDIRS = $(SUBDIRS)
HEADERS_SUBDIRS = $(SUBDIRS)

all: default

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

headers: $(_FORCE)
	@echo "No header files are made in this directory"
#	$(HEADERS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):

#include $(COMMONRULES)
