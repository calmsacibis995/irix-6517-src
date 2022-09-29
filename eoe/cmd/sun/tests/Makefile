#
# Makefile for Sun-derived executable components and configuration files
# for onc3
#
include	${ROOT}/usr/include/make/commondefs

TARGETS= 

SUBDIRS= cachefs tree

HEADERS_SUBDIRS=

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)

exports: $(_FORCE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
