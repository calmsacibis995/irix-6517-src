#**************************************************************************
#*                                                                        *
#*               Copyright (C) 1998, Silicon Graphics, Inc.               *
#*                                                                        *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*                                                                        *
#**************************************************************************
#
# Makefile for insert
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lfrs

TARGETS=insert

CFILES= sema.c simple.c
HFILES= sema.h
EXTRAS= Makefile 00descr 00runme

LDIRT=insert_runme

default: ${TARGETS}

include ${COMMONRULES}

insert:	$(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	rm -f insert_runme; cp 00runme insert_runme; chmod +x insert_runme
install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/insert \
		 ${EXTRAS} ${CFILES} ${HFILES} 
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin ${TARGETS} insert_runme

