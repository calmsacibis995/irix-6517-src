#**************************************************************************
#*                                                                        *
#*               Copyright (C) 1994, Silicon Graphics, Inc.               *
#*                                                                        *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*                                                                        *
#**************************************************************************
#
# Makefile for mprogs
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lm -lfrs

TARGETS=master processA processB

CFILES= ipc.c error.c master.c processA.c processB.c
HFILES= ipc.h

LDIRT=mprogs_runme

default: ${TARGETS}

include ${COMMONRULES}

MOBJECTS= ipc.o error.o master.o
AOBJECTS= ipc.o error.o processA.o
BOBJECTS= ipc.o error.o processB.o

master:		$(MOBJECTS)
		$(CCF) $(LCOPTS) $(MOBJECTS) -o $@ $(LDFLAGS)
		rm -f mprogs_runme; cp 00runme mprogs_runme; chmod +x mprogs_runme
processA:	$(AOBJECTS)
		$(CCF) $(LCOPTS) $(AOBJECTS) -o $@ $(LDFLAGS)
processB:	$(BOBJECTS)
		$(CCF) $(LCOPTS) $(BOBJECTS) -o $@ $(LDFLAGS)



install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/mprogs \
		 Makefile ${CFILES} ${HFILES} ipclean 00runme
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin ${TARGETS} mprogs_runme ipclean

