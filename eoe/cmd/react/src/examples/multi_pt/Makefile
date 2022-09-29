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
# Makefile for multi_pt
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lfrs -lpthread

TARGETS= multi_pt

CFILES= multi_pt.c bar.c
HFILES= bar.h
EXTRAS= Makefile 00descr 00runme
 
LDIRT= multi_pt_runme

default: ${TARGETS}

include ${COMMONRULES}

multi_pt: $(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	rm -f multi_pt_runme; cp 00runme multi_pt_runme; chmod +x multi_pt_runme

install: default
	$(INSTALL) -idb "react.sw.src" \
		-F /usr/share/src/react/examples/multi_pt \
		${EXTRAS} ${CFILES} ${HFILES} 
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin \
	${TARGETS} multi_pt_runme