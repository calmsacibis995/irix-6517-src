include	$(ROOT)/usr/include/make/commondefs

TARGETS=flashbuild

LCDEFS=-D_KMEMUSER

default: $(TARGETS)

include $(COMMONRULES)

flashbuild: flashbuild.o flashcommon.o flashcommon.h
	$(CCF) $(LDFLAGS) -o flashbuild flashbuild.o flashcommon.o

install: default
