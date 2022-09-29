#!smake
#ident "$Revision: 1.16 $"

BASELIB=libw_32.a
ALTLIBS=libw_32_ns.a libw_64.a libw_64_ns.a libw_n32.a libw_n32_ns.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

PRIVATEFILES=

include $(ROOT)/usr/include/make/librootdefs
DSONAMEOPT=-soname libw.so
LLDLIBS=-lc
LLDDSOOPTS=-LD_LAYOUT:segalign=0x4000

default!$(TARGETS)

include ${LIBROOTRULES}

install!headers exports

headers!
	$(INSTALL) -F $(INSTINC) -m 444 -src inc/getwidth.h getwidth.h
	$(INSTALL) -F $(INSTINC) -m 444 -src inc/libw.h libw.h

exports!default
	$(INST_32) -m 444 -src libw_32.so libw.so
	$(INST_32_NS) -m 444 -idb "nostrip" -src libw_32_ns.a libw.a
	$(INST_32) -m 444 -src libw_32.a libw.a
	# 64bit shared libw
	$(INST_64) -m 444 -src libw_64.so libw.so
	$(INST_64) -m 444 -src libw_64.a libw.a
	# 64bit non_shared libw
	$(INST_64_NS) -m 444 -idb "nostrip" -src libw_64_ns.a libw.a
	# n32 shared libw
	$(INST_N32) -m 444 -src libw_n32.so libw.so
	$(INST_N32) -m 444 -src libw_n32.a libw.a
	# n32 non_shared libw
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libw_n32_ns.a libw.a


