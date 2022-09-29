#!smake
#
# Makefile for libdprof.
#

BASELIB=libdprof_32_M2.a
ALTLIBS=libdprof_64_M4.a libdprof_n32_M4.a libdprof_64_M3.a libdprof_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports
LDIRT=so_locations

include $(ROOT)/usr/include/make/librootdefs

#LLDLIBS=-exports -lc
LLDLIBS= -lc

INSOPT=-m 444 -idb "nostrip"
INSOPT4=-idb "mach(CPUARCH=R4000)"
INSOPT4_64=-idb "mach(CPUARCH=R4000 MODE=64bit)"
INSOPT8=-idb "mach(CPUARCH=R8000 CPUARCH=R10000 CPUARCH=R5000)"
INSOPT8_64=-idb "mach(CPUARCH=R8000 CPUARCH=R10000 MODE=64bit)"

DSONAMEOPT=-soname libdprof.so

LLDOPTS= -init __init_dprof

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INST_32_M2)	$(INSOPT) -src libdprof_32_M2.so	libdprof.so
	$(INST_N32_M3)	$(INSOPT) -src libdprof_n32_M3.so	libdprof.so
	$(INST_64_M3)	$(INSOPT) -src libdprof_64_M3.so	libdprof.so
	$(INST_N32_M4)	$(INSOPT) -src libdprof_n32_M4.so	libdprof.so
	$(INST_64_M4)	$(INSOPT) -src libdprof_64_M4.so	libdprof.so
	$(I_N32_M3) (cd libdprof_n32_M3; $(INST_N32) $(INSOPT4) -lns mips3/libdprof.so libdprof.so)
	$(I_64_M3) (cd libdprof_64_M3; $(INST_64) $(INSOPT4) -lns mips3/libdprof.so libdprof.so)
	$(I_N32_M4) (cd libdprof_n32_M4; $(INST_N32) $(INSOPT8) -lns mips4/libdprof.so libdprof.so)
	$(I_64_M4) (cd libdprof_64_M4; $(INST_64) $(INSOPT8_64) -lns mips4/libdprof.so libdprof.so)
	


