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
# $Revision: 1.24 $

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=runtests \
	 siginfo sig64 sigaltstack sigstack pollrestart itimer sched isolate \
	 adjtime fselect autolock sigwait i2u_go i2u_viewer
# NOTYET= lowestpri wantdisp BSDtime
LCDEFS+=-g
OPTIMIZER=
#
# 84 - stop whining about not using anything from libstress.a
LLDOPTS=-Wl,-woff,84

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/rt $(COMMANDS)

64bitinstall: default
	$(INSTALL) -idb "nostrip" -F /usr/stress/rt/64bit $(IDB_TAG64) $(COMMANDS)
