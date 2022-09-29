#!smake

include $(ROOT)/usr/include/make/commondefs

CFILES= preg.c
LCOPTS= -fullwarn

TARGETS= preg

targets default: $(TARGETS)

preg: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin preg
