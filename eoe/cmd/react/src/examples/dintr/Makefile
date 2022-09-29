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

TARGETS=dintr

CFILES= sema.c dintr.c sendintr.c
HFILES= sema.h
EXTRAS= Makefile 00descr 00runme 

LDIRT= ${TARGETS}_runme sendintr

default: ${TARGETS}

include ${COMMONRULES}

dintr: $(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	$(CCF) $(LCOPTS) sendintr.c -o sendintr $(LDFLAGS)
	rm -f ${TARGETS}_runme; cp 00runme ${TARGETS}_runme; chmod +x ${TARGETS}_runme

install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/dintr \
		 ${CFILES} ${HFILES} ${EXTRAS}
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin ${TARGETS} sendintr ${TARGETS}_runme

