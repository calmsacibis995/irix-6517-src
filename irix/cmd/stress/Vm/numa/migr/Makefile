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
# $Revision: 1.7 $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS = mapcnt mcontrol muser runtests
  
LCDEFS+=-g 
OPTIMIZER=

default: $(TARGETS)

include $(CMDCOMMONRULES)


#
# Installation
# 

32bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/numa/migr $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/numa/migr $(COMMANDS)

n32bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/numa/migr/n32 $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/numa/migr/n32 $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/numa/migr/64bit $(IDB_TAG64) $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/numa/migr/64bit $(IDB_TAG64) $(COMMANDS)


