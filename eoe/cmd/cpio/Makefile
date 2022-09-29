#!smake

#ident "$Revision: 1.20 $"

include $(ROOT)/usr/include/make/commondefs

CFILES = cpio.c newstat.c g_init.c g_read.c g_write.c

TARGETS=cpio


LCDEFS = -DRMT
LCOPTS = -fullwarn
LLDLIBS=-lrmt -lgen
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /usr/bin -lns ../../sbin/$(TARGETS) $(TARGETS)

cpio:	$(OBJECTS) 
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

