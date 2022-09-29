#
# Makefile for the RAID administrative utility
#

include	$(ROOT)/usr/include/make/commondefs

TARGETS	= raid
CFILES	= raid.c
LCFLAGS	= -g -DDEBUG
LLDOPTS	= -g
LLDLIBS = -ldisk

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -m 755 -F /usr/sbin -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" $(TARGETS)
