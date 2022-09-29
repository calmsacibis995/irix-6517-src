#!smake

include $(ROOT)/usr/include/make/commondefs

CFILES= mount.c
LCOPTS= -fullwarn

TARGETS= nsmount

targets default: $(TARGETS)

nsmount: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

include $(COMMONRULES)

install: default
	${INSTALL} -u root -g sys -m 0755 -f /sbin nsmount
	${INSTALL} -u root -g sys -m 0755 -dir /ns
