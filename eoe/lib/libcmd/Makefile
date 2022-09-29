#!smake
#
# Makefile for libcmd.
#
# Currently no .so's are shipped
#
#ident	"$Revision: 1.18 $"

BASELIB=libcmd_32.a
ALTLIBS=libcmd_32_ns.a libcmd_64.a libcmd_n32_M3.a

NOSO=no-thank-you

include	$(ROOT)/usr/include/make/librootdefs
LLDLIBS=-lc

default: $(TARGETS)

exports install: default
	$(INST_32_NS) -m 444 -idb "nostrip" -src libcmd_32_ns.a libcmd.a
	$(INST_32) -m 444 -src libcmd_32.a libcmd.a
	$(INST_64) -m 444 -src libcmd_64.a libcmd.a
	$(INST_N32) -m 444 -src libcmd_n32_M3.a libcmd.a

include $(LIBROOTRULES)
