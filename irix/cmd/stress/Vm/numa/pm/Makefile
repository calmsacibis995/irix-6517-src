#!smake
#
# stress/Vm/numa/pm/Makefile
#
# Copyright 1995, Silicon Graphics, Inc.
# ALL RIGHTS RESERVED
#
# UNPUBLISHED -- Rights reserved under the copyright laws of the United
# States.   Use of a copyright notice is precautionary only and does not
# imply publication or disclosure.
#
# U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
# Use, duplication or disclosure by the Government is subject to restrictions
# as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
# in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
# in similar or successor clauses in the FAR, or the DOD or NASA FAR
# Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
# 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
#
# THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
# INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
# DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
# PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
# GRAPHICS, INC.
#
# $Revision: 1.9 $

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit
WANTPARALLEL=yes-please

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS = ccarray crash nmalloc repfactor runtests  ptest
  
LCDEFS+=-g 
OPTIMIZER=

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/numa/pm $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/numa/pm $(COMMANDS)

n32bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/numa/pm/n32 $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/numa/pm/n32 $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/stress/Vm/numa/pm/64bit $(IDB_TAG64) $(COMMANDS)
	$(INSTALL) -idb "noship.sw.stress nostrip" -F /usr/diags/crash/Vm/numa/pm/64bit $(IDB_TAG64) $(COMMANDS)




