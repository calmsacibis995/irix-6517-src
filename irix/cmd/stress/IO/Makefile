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

# $Revision: 1.14 $

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=runtests copy dkstress sio siomgr disktest rawdisk sectorcheck \
         rdwr diskrand mfgtests diocmpall genfile
CFILES=dkstress.c sio.c siomgr.c disktest.c rawdisk.c sectorcheck.c \
       rdwr.c diskrand.c diocmpall.c

LCDEFS+=-D_BSD_TIME

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) $(IDB_TAG32) -F /usr/stress/IO '$(COMMANDS) README'
	$(INSTALL) $(IDB_TAG32) -F /usr/diags/crash/IO '$(COMMANDS) README'

64bitinstall: default
	$(INSTALL) $(IDB_TAG64) -F /usr/stress/IO '$(COMMANDS) README'
	$(INSTALL) $(IDB_TAG64) -F /usr/diags/crash/IO '$(COMMANDS) README'
