#!smake

# $Revision: 1.14 $


include	$(ROOT)/usr/include/make/commondefs

LCDEFS = -DSTANDALONE
LCOPTS=-fullwarn

UUSRCS	=callers.c conn.c uucpdefs.c ulockf.c getargs.c interface.c \
		strsave.c sysfiles.c strecpy.c
CUSRCS	=cu.c altconn.c culine.c
CFILES	=$(UUSRCS) $(CUSRCS)

TARGETS	=cu

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin -idb "std.sw.uucp" -m 4111 -u uucp -g uucp cu

cu:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o cu
