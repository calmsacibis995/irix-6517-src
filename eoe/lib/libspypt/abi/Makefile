#!smake

include $(ROOT)/usr/include/make/librootdefs

LCINCS		+= -I../inc
LCOPTS		+= -fullwarn
LCDEFS		+=
LDIRT		+= $(ABIFILES:S/$/.NEW/g) ptABIGen

ABIFILES	= ./ptABI.h ./ptABID.h
HDRINST		= $(INSTALL) -F $(INSTINC) -m 444 -idb "noship"

default: abifiles

include $(LIBROOTRULES)

.NOTPARALLEL:	abifiles

abifiles:
	$(HOST_CC) -o32 $(LCFLAGS) $(GCINCS) ptABIGen.c -o ptABIGen
	./ptABIGen > ptABID.h.NEW

	$(HOST_CC) -n32 -mips3 $(LCFLAGS) $(GCINCS) ptABIGen.c -o ptABIGen
	./ptABIGen >> ptABID.h.NEW

	$(HOST_CC) -64 -mips3 $(LCFLAGS) $(GCINCS) ptABIGen.c -o ptABIGen
	./ptABIGen >> ptABID.h.NEW
	./ptABIGen -c >> ptABID.h.NEW
	./ptABIGen -h > ptABI.h.NEW
