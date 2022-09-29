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
# Makefile for variable_pt_m1
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lfrs -lpthread

TARGETS= variable_pt_m1

CFILES= variable_pt_m1.c
EXTRAS= Makefile 00descr 00runme

LDIRT= variable_pt_m1_runme

default: ${TARGETS}

include ${COMMONRULES}

variable_pt_m1:	$(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	rm -f variable_pt_m1_runme; cp 00runme variable_pt_m1_runme; chmod +x variable_pt_m1_runme
install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/variable_pt_m1 \
		 ${EXTRAS} ${CFILES} ${HFILES} 
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin ${TARGETS} variable_pt_m1_runme

