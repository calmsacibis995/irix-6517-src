#!smake
#
#ident	"$Revision: 1.24 $"
#

BASELIB=libdisk_32.a
ALTLIBS=libdisk_32_ns.a libdisk_64.a libdisk_64_ns.a \
	libdisk_n32.a libdisk_n32_ns.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include	$(ROOT)/usr/include/make/librootdefs
#TARGETS=$(LIBS)

LLDLIBS=-lc

DSONAMEOPT=-soname libdisk.so

LLCOPTS = -g

default: $(TARGETS)

include $(LIBROOTRULES)

install: exports

exports: default
	$(I_32_M2) $(INSTALL) -F /lib -m 444 -idb stripdso -src libdisk_32.so libdisk.so
	$(INST_32_NS) -m 444 -idb stripdso -src libdisk_32_ns.a libdisk.a
	$(INST_64) -m 444 -idb stripdso -src libdisk_64.so libdisk.so
	$(INST_64_NS) -m 444 -idb "nostrip" -src libdisk_64_ns.a libdisk.a
	$(I_N32_M3) $(INSTALL) -F /lib32 -m 444 -idb stripdso -src libdisk_n32.so libdisk.so
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libdisk_n32_ns.a libdisk.a
	$(I_32) $(INSTALL) -F ${INSTDIR} -lns ../../lib/libdisk.so libdisk.so
	$(I_N32_M3) $(INSTALL) -F ${INSTDIR_N32} -lns ../../lib32/libdisk.so libdisk.so
