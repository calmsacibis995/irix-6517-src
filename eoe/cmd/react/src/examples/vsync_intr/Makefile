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
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lm -lfrs

TARGETS=vsync_intr

CFILES= multi.c sema.c action.c 
HFILES= multi.h
EXTRAS= Makefile 00descr 00runme 

LDIRT= ${TARGETS}_runme

default: ${TARGETS}

include ${COMMONRULES}

vsync_intr: $(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	rm -f ${TARGETS}_runme; cp 00runme ${TARGETS}_runme; chmod +x ${TARGETS}_runme

install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/vsync_intr \
		  ${CFILES} ${HFILES} ${EXTRAS}
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin ${TARGETS} ${TARGETS}_runme
