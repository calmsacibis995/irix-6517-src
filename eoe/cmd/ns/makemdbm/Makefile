#!smake

include $(ROOT)/usr/include/make/commondefs

CFILES= makemdbm.c
LCINCS= -I../ypxfr

TARGETS= makemdbm

targets default: $(TARGETS)

makemdbm: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

include $(COMMONRULES)

install: default
	${INSTALL} -u root -g sys -m 0755 -f /usr/sbin makemdbm
	${INSTALL} -f /usr/sbin -lns makemdbm makedbm
