#!smake
#
# $Revision: 1.8 $

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

CFILES= istress.c mstress.c ran.c stk.c stk2.c
SHFILES= runtests.sh mfgtests.sh
COMMANDS= $(CFILES:.c=) $(SHFILES:.sh=)
OPTIMIZER=

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) $(IDB_TAG32) -F /usr/stress/Istress $(COMMANDS)
	$(INSTALL) $(IDB_TAG32) -F /usr/diags/crash/Istress $(COMMANDS)

64bitinstall: default
	$(INSTALL) $(IDB_TAG64) -F /usr/stress/Istress $(COMMANDS)
	$(INSTALL) $(IDB_TAG64) -F /usr/diags/crash/Istress $(COMMANDS)
