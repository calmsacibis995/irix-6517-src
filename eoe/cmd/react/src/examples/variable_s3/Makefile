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
# Makefile for var_intr1
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/releasedefs

LCDEFS=
LLDLIBS= -lfrs

TARGETS= variable_s3

CFILES= variable_s3.c
EXTRAS= Makefile 00descr 00runme

LDIRT= variable_s3_runme

default: ${TARGETS}

include ${COMMONRULES}

variable_s3:	$(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	rm -f variable_s3_runme; cp 00runme variable_s3_runme; chmod +x variable_s3_runme
install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/variable_s3 \
		 ${EXTRAS} ${CFILES} ${HFILES} 
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin ${TARGETS} variable_s3_runme

