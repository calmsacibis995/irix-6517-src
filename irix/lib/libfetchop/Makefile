#!smake

BASELIB = libfetchop_sn0_64.a
ALTLIBS = libfetchop_sn0_n32.a \
	  libfetchop_nonsn0_64.a libfetchop_nonsn0_n32.a

FETCHOP_SO_BASE = ../libfetchop.so
FETCHOP_SO = libfetchop.so

include $(ROOT)/usr/include/make/librootdefs

LLDLIBS = -lc

DSONAMEOPT = -soname libfetchop.so

include $(LIBROOTRULES)

default: $(TARGETS)

exports install! default
	$(INST_64) -idb "mach(CPUBOARD!=IP27)" -m 444 \
		-src libfetchop_nonsn0_64.so libfetchop.so
	$(INST_64) -idb "mach(CPUBOARD=IP27)" -m 444 \
		-src libfetchop_sn0_64.so libfetchop.so
	$(INST_64_M3) -lns $(FETCHOP_SO_BASE) $(FETCHOP_SO) 
	$(INST_64_M4) -lns $(FETCHOP_SO_BASE) $(FETCHOP_SO) 
	$(INST_N32) -idb "mach(CPUBOARD!=IP27)" -m 444 \
		-src libfetchop_nonsn0_n32.so libfetchop.so
	$(INST_N32) -idb "mach(CPUBOARD=IP27)" -m 444 \
		-src libfetchop_sn0_n32.so libfetchop.so
	$(INST_N32_M3) -lns $(FETCHOP_SO_BASE) $(FETCHOP_SO) 
	$(INST_N32_M4) -lns $(FETCHOP_SO_BASE) $(FETCHOP_SO) 


