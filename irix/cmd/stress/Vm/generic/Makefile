#!smake
#**************************************************************************
#*									  *
#* 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
#*									  *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*									  *
#**************************************************************************/
# $Revision: 1.25 $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS = mwalk plot nwalk nmwalk kids utlb tlbthrash heapdriver swapinc \
           realloc awalk dops runtests mfgtests
MWSRCS = driver.c norm.c drand48.c

LCDEFS+=-g
OPTIMIZER=
#
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,84

default: $(TARGETS)

include $(CMDCOMMONRULES)

#
# Its good to have 1 example of a -n non_shared executable ...
# WAR for the 3.18 compilers - add -Wl,-rdata_shared
#
mwalk:
#	$(CCF) -n -Wl,-rdata_shared -non_shared -o mwalk $(MWSRCS) -nostdlib -L$(ROOT)/usr/lib/nonshared -lm
	$(CCF) -o $@ $(MWSRCS) -lm $(LDFLAGS)

nmwalk: nwalk
	rm -f $@
	ln $? $@

norm.o:	norm.h

32bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/generic $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/generic $(COMMANDS)

n32bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/generic/n32 $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/generic/n32 $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/Vm/generic/64bit $(IDB_TAG64) $(COMMANDS)
	$(INSTALL) -idb "nostrip" -F /usr/diags/crash/Vm/generic/64bit $(IDB_TAG64) $(COMMANDS)

