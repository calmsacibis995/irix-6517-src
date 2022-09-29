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
# Makefile for r4ktimer_intr_00
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lm -lfrs

TARGETS= frs 

CFILES= r4kfrs.c
HFILES= 

default: ${TARGETS}

include ${COMMONRULES}

frs: $(OBJECTS)
	$(CCF) $(LCOPTS) r4kfrs.o -o $@ $(LDFLAGS)

install: default
	$(INSTALL) -idb "react.sw.frs" -F /usr/react/src/tests/r4ktimer_intr_00 \
		 Makefile 00runme ${CFILES} ${HFILES} frs 


