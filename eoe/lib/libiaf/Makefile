#!smake
#
# Makefile for libiaf.
#
#ident	"$Revision: 1.10 $"

BASELIB=libiaf_32.a
ALTLIBS=libiaf_32_ns.a libiaf_N32_M3.a
NOSO=no-thank-you

include	$(ROOT)/usr/include/make/librootdefs
LLDLIBS=-lc

default: $(TARGETS)

exports install: default
	$(INST_32_NS) -m 444 -idb "nostrip" -src libiaf_32_ns.a libiaf.a
	$(INST_N32) -m 444 -idb "nostrip" -src libiaf_N32_M3.a libiaf.a
	$(INST_32) -m 444 -src libiaf_32.a libiaf.a

include $(LIBROOTRULES)

