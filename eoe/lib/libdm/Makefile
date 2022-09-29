#!smake
# Generate Data Management library .
#
#ident	"$Revision: 1.3 $"

BASELIB=libdm_32.a
ALTLIBS=libdm_32_ns.a libdm_64.a libdm_64_ns.a libdm_n32.a libdm_n32_ns.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include	$(ROOT)/usr/include/make/librootdefs

DSONAMEOPT = -soname libdm.so
LLDLIBS=-lc

default! $(TARGETS)

include $(LIBROOTRULES)

install!exports

exports!default
	$(INST_32) -m 444 -idb "nostrip" -src libdm_32.so libdm.so
	$(INST_32_NS) -m 444 -idb "nostrip" -src libdm_32_ns.a libdm.a
	$(INST_32) -m 444 -idb "nostrip" -src libdm_32.a libdm.a
	# 64bit shared libdm
	$(INST_64) -m 444 -idb "nostrip" -src libdm_64.so libdm.so
	$(INST_64) -m 444 -idb "nostrip" -src libdm_64.a libdm.a
	# 64bit nonshared libdm
	$(INST_64_NS) -m 444 -idb "nostrip" -src libdm_64_ns.a libdm.a
	# n32 shared libdm
	$(INST_N32) -m 444 -idb "nostrip" -src libdm_n32.so libdm.so
	$(INST_N32) -m 444 -idb "nostrip" -src libdm_n32.a libdm.a
	# n32 non_shared libdm
	$(INST_N32_NS) -m 444 -idb "nostrip" -src libdm_n32_ns.a libdm.a
