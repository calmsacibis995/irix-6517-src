#!smake
#
# idbg - a tool for kernel debugging
#

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	$(ROOT)/usr/include/make/cmdcommondefs

LCOPTS+=-fullwarn

COMMANDS=idbg

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) $(IDB_TAG32) -F /usr/sbin idbg

64bitinstall: default
	$(INSTALL) $(IDB_TAG64) -F /usr/sbin idbg
