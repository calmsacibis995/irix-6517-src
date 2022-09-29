#!smake
#
# Makefile for libsnmp.a
#
#ident	"$Revision: 1.9 $"

BASELIB=libsnmp.a
ALTLIBS=libsnmp_64.a

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

default: $(TARGETS)

include $(LIBROOTRULES)

install: exports

exports: default $(_FORCE)
	$(INST_N32) -m 444 -idb "nostrip" -src libsnmp_n32.a $(BASELIB)
	$(INST_64) -m 444 -idb "nostrip" -src libsnmp_64.a $(BASELIB)
