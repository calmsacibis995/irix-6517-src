include	$(ROOT)/usr/include/make/commondefs

TARGETS=flashbuild flashinst

LCDEFS=-D_KMEMUSER

default: $(TARGETS)

include $(COMMONRULES)

flashbuild: flashbuild.o flashcommon.o flashcommon.h
	$(CCF) -o flashbuild flashbuild.o flashcommon.o $(LDFLAGS)

flashinst: flashinst.o flashcommon.o flashcommon.h
	$(CCF) -o flashinst flashinst.o flashcommon.o $(LDFLAGS)

install: default
	$(INSTALL) -idb "mach(CPUBOARD=IP32)" -F /sbin flashinst
	$(INSTALL) -idb "mach(CPUBOARD=IP32) noship" -F /sbin flashbuild
