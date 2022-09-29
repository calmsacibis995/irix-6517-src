#
# Makefile for the RAID status monitoring hack
#

include	$(ROOT)/usr/include/make/commondefs

TARGETS	= raid_monitor
CFILES	= window.c status.c
LCFLAGS	= -g -DDEBUG
LLDOPTS	= -g
LLDLIBS = -lXm -lXt -lX11

default: $(TARGETS)

include $(COMMONRULES)

$(TARGETS):	$(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
