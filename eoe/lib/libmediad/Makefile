#!smake
#
# Makefile for libmediad
#
#ident	"$Revision: 1.9 $"

BASELIB=libmediad.a
ALTLIBS=libmediad_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_N32=common.exports


include	$(ROOT)/usr/include/make/librootdefs

LLDLIBS=-lc

default: $(TARGETS)

include $(LIBROOTRULES)

install: headers exports

exports: default $(_FORCE)
	$(INST_32) -m 444 libmediad.a
	$(INST_N32) -m 444 -src libmediad_n32_M3.a libmediad.a

headers: $(_FORCE)
	$(INSTALL) -F /usr/include -idb "irix_root.sw.public" -m 444 mediad.h
