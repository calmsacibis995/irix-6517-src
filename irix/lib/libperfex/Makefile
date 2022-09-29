#!smake
#
# Makefile for libperfex.
#

BASELIB=libperfex_n32_M4.a
ALTLIBS=libperfex_64_M4.a libperfex_32_M2.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

LDIRT=so_locations

include $(ROOT)/usr/include/make/librootdefs

LLDLIBS= -lc

DSONAMEOPT=-soname libperfex.so

LLDOPTS= -init __init_counters_thread

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INSTALL) -F /usr/lib32 -m 444 -idb "mach(CPUARCH=R10000)" -src libperfex_n32_M4.so libperfex.so
	$(INSTALL) -F /usr/lib64 -m 444 -idb "mach(CPUARCH=R10000)" -src libperfex_64_M4.so libperfex.so
	$(INSTALL) -F /usr/lib   -m 444 -idb "mach(CPUARCH=R10000)" -src libperfex_32_M2.so libperfex.so
