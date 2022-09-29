#!smake
include $(ROOT)/usr/include/make/commondefs

CFILES=tlbstats.c utlbonly.c irix4.0.c
LCOPTS=-fullwarn

TARGETS=tlbstats

default: $(TARGETS)

include $(COMMONRULES)

$(TARGETS):$(OBJECTS)
	$(CCF) -o $(TARGETS) $(OBJECTS) $(LDFLAGS)

install: default
	$(INSTALL) -F /usr/bin tlbstats
