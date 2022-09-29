#!smake
#
# Make the talk command
#
# $Revision: 1.6 $

include	$(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

CFILES	= talk.c get_names.c display.c io.c ctl.c init_disp.c \
	  msgs.c get_addrs.c ctl_transact.c invite.c look_up.c

TARGETS	=talk

LCDEFS	=-D_BSD_COMPAT
LCOPTS	=-signed
LLDLIBS	=-lcurses -ltermlib

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/bsd $(TARGETS)

include $(COMMONRULES)

talk:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
