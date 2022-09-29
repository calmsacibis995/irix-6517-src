#!smake
#
# Makefile for libdplace.
#

BASELIB=libdplace_32_M2.a
ALTLIBS=libdplace_64_M4.a libdplace_n32_M4.a libdplace_64_M3.a libdplace_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

LDIRT=so_locations

include $(ROOT)/usr/include/make/librootdefs

LLDLIBS= -lc

INSOPT=-m 444 -idb "nostrip"
INSOPT4=-idb "mach(CPUARCH=R4000)"
INSOPT4_64=-idb "mach(CPUARCH=R4000 MODE=64bit)"
INSOPT8=-idb "mach(CPUARCH=R8000 CPUARCH=R10000 CPUARCH=R5000)"
INSOPT8_64=-idb "mach(CPUARCH=R8000 CPUARCH=R10000 MODE=64bit)"

DSONAMEOPT=-soname libdplace.so

LLDOPTS= -init __init_dplace

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INST_32_M2)	$(INSOPT) -src libdplace_32_M2.so	libdplace.so
	$(INST_N32_M3)	$(INSOPT) -src libdplace_n32_M3.so	libdplace.so
	$(INST_64_M3)	$(INSOPT) -src libdplace_64_M3.so	libdplace.so
	$(INST_N32_M4)	$(INSOPT) -src libdplace_n32_M4.so	libdplace.so
	$(INST_64_M4)	$(INSOPT) -src libdplace_64_M4.so	libdplace.so
	$(I_N32_M3) (cd libdplace_n32_M3; $(INST_N32) $(INSOPT4) -lns mips3/libdplace.so libdplace.so)
	$(I_64_M3) (cd libdplace_64_M3; $(INST_64) $(INSOPT4) -lns mips3/libdplace.so libdplace.so)
	$(I_N32_M4) (cd libdplace_n32_M4; $(INST_N32) $(INSOPT8) -lns mips4/libdplace.so libdplace.so)
	$(I_64_M4) (cd libdplace_64_M4; $(INST_64) $(INSOPT8_64) -lns mips4/libdplace.so libdplace.so)
	


