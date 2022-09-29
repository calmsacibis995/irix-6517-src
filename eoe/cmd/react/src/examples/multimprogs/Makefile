#**************************************************************************
#*                                                                        *
#*               Copyright (C) 1995, Silicon Graphics, Inc.               *
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

LCDEFS= -DMESSAGES_ON
LLDLIBS= -lm -lfrs

TARGETS=multimprogs

LDIRT= ${TARGETS}_runme  *~ mmcontrol mmprocA mmprocB mmprocC mmprocD

CFILES= ipc.c error.c mmcontrol.c enqueue.c mmprocA.c mmprocB.c \
	 process_support.c mmprocC.c mmprocD.c
HFILES= ipc.h defs.h

default: ${TARGETS}

include ${COMMONRULES}

MOBJECTS= ipc.o error.o enqueue.o mmcontrol.o
AOBJECTS= ipc.o error.o process_support.o mmprocA.o 
BOBJECTS= ipc.o error.o process_support.o mmprocB.o
COBJECTS= ipc.o error.o process_support.o mmprocC.o
DOBJECTS= ipc.o error.o process_support.o mmprocD.o

multimprogs:	mmcontrol mmprocA mmprocB mmprocC mmprocD
		rm  -f ${TARGETS}_runme; cp 00runme ${TARGETS}_runme; chmod +x ${TARGETS}_runme		
mmcontrol:		$(MOBJECTS)
		$(CCF) $(LCOPTS) $(MOBJECTS) -o $@ $(LDFLAGS)
mmprocA:	$(AOBJECTS)
		$(CCF) $(LCOPTS) $(AOBJECTS) -o $@ $(LDFLAGS)
mmprocB:	$(BOBJECTS)
		$(CCF) $(LCOPTS) $(BOBJECTS) -o $@ $(LDFLAGS)
mmprocC:	$(COBJECTS)
		$(CCF) $(LCOPTS) $(COBJECTS) -o $@ $(LDFLAGS)
mmprocD:	$(DOBJECTS)
		$(CCF) $(LCOPTS) $(DOBJECTS) -o $@ $(LDFLAGS)



install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/multimprogs \
		 Makefile ${CFILES} ${HFILES} 00runme 00descr
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin mmcontrol mmprocA \
		 mmprocB mmprocC mmprocD ${TARGETS}_runme 

