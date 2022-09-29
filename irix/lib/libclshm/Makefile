#!smake

BASELIB = libclshm_sn0_64.a
ALTLIBS = libclshm_sn0_n32.a

CLSHM_SO_BASE = ../libclshm.so
CLSHM_SO = libclshm.so

include $(ROOT)/usr/include/make/librootdefs

LLDLIBS = -lc

DSONAMEOPT = -soname libclshm.so

include $(LIBROOTRULES)

default: $(TARGETS)

exports install! default
	$(INST_64) -idb "mach(CPUBOARD=IP27)" -m 444 \
		-src libclshm_sn0_64.so libclshm.so
	$(INST_64_M3) -lns $(CLSHM_SO_BASE) $(CLSHM_SO) 
	$(INST_64_M4) -lns $(CLSHM_SO_BASE) $(CLSHM_SO) 
	$(INST_N32) -idb "mach(CPUBOARD=IP27)" -m 444 \
		-src libclshm_sn0_n32.so libclshm.so
	$(INST_N32_M3) -lns $(CLSHM_SO_BASE) $(CLSHM_SO) 
	$(INST_N32_M4) -lns $(CLSHM_SO_BASE) $(CLSHM_SO) 


