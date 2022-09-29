#
# Makefile for flashmmsc command
#
include	$(ROOT)/usr/include/make/commondefs

CFILES= flashmmsc.c double_time.c error.c mmsc.c
TARGETS=flashmmsc

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "mach(CPUBOARD=IP27)" -F /usr/sbin flashmmsc

flashmmsc: $(OBJECTS)
	$(CCF) $(OBJECTS) -o $@ $(LDFLAGS)
