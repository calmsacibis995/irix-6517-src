#!smake
#
# $Revision: 1.3 $
#

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS= mem splocks ttcp

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) $(IDB_TAG32) -F /usr/stress/mfg/bin $(COMMANDS)

64bitinstall: default
	$(INSTALL) $(IDB_TAG64) -F /usr/stress/mfg/bin $(COMMANDS)

mem: memory.o
	$(CCF) -o $@ memory.o $(LDFLAGS)
