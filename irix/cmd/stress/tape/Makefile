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
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=bash vartape

LCOPTS+=-cckr
LLDLIBS+=-lm

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -F /usr/stress/tape $(IDB_TAG32) $(COMMANDS)

64bitinstall: default
	$(INSTALL) -F /usr/stress/tape $(IDB_TAG64) $(COMMANDS)
