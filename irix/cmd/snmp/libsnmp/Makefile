#!smake
#
# Makefile for snmp.
#
#ident	"$Revision: 1.2 $"

BASELIB=libsnmp.a
ALTLIBS=libsnmp_n32.a libsnmp_64.a
NOSO=no-thank-you

include	$(ROOT)/usr/include/make/librootdefs
LLDLIBS=-lc

default: $(TARGETS)

exports install: default
	$(INST_64) -m 444 -idb "nostrip" -src libsnmp_64.a libsnmp.a
	$(INST_N32) -m 444 -idb "nostrip" -src libsnmp_n32.a libsnmp.a
	$(INST_32) -m 444 -idb "nostrip" libsnmp.a

include $(LIBROOTRULES)

