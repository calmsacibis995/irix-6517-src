#!smake

include $(ROOT)/usr/include/make/commondefs

CFILES= mdbm_remove.c
LCOPTS= -fullwarn

TARGETS= mdbm_remove

targets default: $(TARGETS)

ypxfr: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

include $(COMMONRULES)

install: default
	${INSTALL} -u root -g sys -m 0755 -f /sbin mdbm_remove
