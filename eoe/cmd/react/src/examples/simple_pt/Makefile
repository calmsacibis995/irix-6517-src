#**************************************************************************
#*                                                                        *
#*               Copyright (C) 1997, Silicon Graphics, Inc.               *
#*                                                                        *
#*  These coded instructions, statements, and computer programs  contain  *
#*  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
#*  are protected by Federal copyright law.  They  may  not be disclosed  *
#*  to  third  parties  or copied or duplicated in any form, in whole or  *
#*  in part, without the prior written consent of Silicon Graphics, Inc.  *
#*                                                                        *
#**************************************************************************
#
# Makefile for simple
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lfrs -lpthread

TARGETS= simple_pt

CFILES= simple_pt.c
EXTRAS= Makefile 00descr 00runme

LDIRT= simple_pt_runme

default: ${TARGETS}

include ${COMMONRULES}

simple_pt: $(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	rm -f simple_pt_runme; cp 00runme simple_pt_runme; \
	chmod +x simple_pt_runme

install: default
	$(INSTALL) -idb "react.sw.src" \
		-F /usr/share/src/react/examples/simple_pt \
		${EXTRAS} ${CFILES} ${HFILES} 
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin \
	${TARGETS} simple_pt_runme

