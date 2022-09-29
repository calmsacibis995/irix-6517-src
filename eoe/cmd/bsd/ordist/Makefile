#!smake
#
# Makefile for the 4.3BSD rdist command
#
# $Revision: 1.8 $$

include	$(ROOT)/usr/include/make/commondefs

TARGETS	=ordist
CFILES	=docmd.c expand.c lookup.c main.c server.c
YFILES	=gram.y

LCDEFS	= -D_BSD_SIGNALS -D_BSD_TIME
LCOPTS	= -signed
I_FLAGS	= -m 4755 -u root -g sys

default:$(TARGETS)

install:default
	$(INSTALL) $(I_FLAGS) -F /usr/bsd $(TARGETS)

include $(COMMONRULES)

ordist: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
