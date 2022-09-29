#!smake

#ident	"$Revision: 1.14 $"

BASELIB=libudma.a
ALTLIBS=libudma_64.a libudma_n32_M3.a libudma_ev.a libudma_ev_64.a libudma_ev_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include	$(ROOT)/usr/include/make/librootdefs

#TARGETS=libudma.a libudma.so libudma_64.a libudma_64.so \
#		libudma_n32_M3.a libudma_n32_M3.so

DSONAMEOPT=-soname libudma.so

LLDLIBS=-lc

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
#	inst will default to idb entry without a machtag (see gendist(1M))
	$(INSTALL) -idb "mach(CPUBOARD!=IP19 && CPUBOARD!=IP21 && CPUBOARD!=IP25)" \
		   -F $(INSTDIR) -m 444 libudma.so
	$(INSTALL) -idb "mach(CPUBOARD!=IP19 && CPUBOARD!=IP21 && CPUBOARD!=IP25)" \
		   -F $(INSTDIR_64) -m 444 -src libudma_64.so libudma.so
	$(INSTALL) -idb "mach(CPUBOARD!=IP19 && CPUBOARD!=IP21 && CPUBOARD!=IP25)" \
		   -F $(INSTDIR_N32) -m 444 -src libudma_n32_M3.so libudma.so

#	install the Challenge version of libudma on IP19 | IP21 | IP25
	$(INSTALL) -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" \
		   -F $(INSTDIR) -m 444 -src libudma_ev.so libudma.so
	$(INSTALL) -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" \
		   -F $(INSTDIR_64) -m 444 -src libudma_ev_64.so libudma.so
	$(INSTALL) -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" \
		   -F $(INSTDIR_N32) -m 444 -src libudma_ev_n32_M3.so libudma.so

	$(INSTALL) -F /usr/include -m 444 -src src/udmalib.h udmalib.h 





