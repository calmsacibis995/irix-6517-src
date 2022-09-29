#!smake

include $(ROOT)/usr/include/make/commondefs

CFILES= ypxfr.c
LCOPTS= -fullwarn

TARGETS= ypxfr

targets default: $(TARGETS)

ypxfr: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

include $(COMMONRULES)

install: default
	${INSTALL} -u root -g sys -m 0755 -f /usr/sbin ypxfr
