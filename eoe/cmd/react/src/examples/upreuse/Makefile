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

LCDEFS=-mips2 -DDEBUG -D_BSD_SIGNALS -D_SGI_MP_SOURCE
LLDLIBS= -lm -lfrs

TARGETS=upreuse

CFILES= sema.c pqueue.c pdesc.c main.c atomic_ops.s \
	loop.c work.c config.c test_hello.c test_singlefrs.c controller.c
HFILES= sema.h pqueue.h pdescdef.h pdesc.h atomic_ops.h \
	loop.h work.h config.h controller.h
EXTRAS= Makefile 00descr 00runme 

LDIRT= ${TARGETS}_runme

default: ${TARGETS}

include ${COMMONRULES}

upreuse: $(OBJECTS)
	$(CCF) $(LCOPTS) $(OBJECTS) -o $@ $(LDFLAGS)
	rm -f ${TARGETS}_runme; cp 00runme ${TARGETS}_runme; chmod +x ${TARGETS}_runme

install: default
	$(INSTALL) -idb "react.sw.src" -F /usr/share/src/react/examples/upreuse \
		  ${CFILES} ${HFILES} ${EXTRAS}
	$(INSTALL) -idb "react.sw.demos" -F /usr/react/bin ${TARGETS} ${TARGETS}_runme

