#!smake
#
#ident	"$Id: Makefile,v 1.10 1997/05/25 14:48:40 rbeck Exp $"
#

BASELIB=libgrio_32.a
ALTLIBS=libgrio_64.a libgrio_n32.a


DSOEXPORTS=common.exports
DSOEXPORTS_32=common.exports
DSOEXPORTS_N32=common.exports
DSOEXPORTS_64=common.exports

include	$(ROOT)/usr/include/make/librootdefs
LLDLIBS=-lc
DSONAMEOPT = -soname libgrio.so

default! $(TARGETS)

include $(LIBROOTRULES)

install!exports

exports!default
	$(INST_32) -m 444 -idb "nostrip" -src libgrio_32.so libgrio.so
	$(INST_32) -lns libgrio.so libprio.so
	# 64bit shared libgrio
	$(INST_64) -m 444 -idb "nostrip" -src libgrio_64.so libgrio.so
	$(INST_64) -lns libgrio.so libprio.so
	# n32 shared libgrio
	$(INST_N32) -m 444 -idb "nostrip" -src libgrio_n32.so libgrio.so
	$(INST_N32) -lns libgrio.so libprio.so
