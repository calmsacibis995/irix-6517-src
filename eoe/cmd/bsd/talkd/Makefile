#!smake
#
# Make the talkd daemon
#
# $Revision: 1.7 $

include	$(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

CFILES	=talkd.c announce.c process.c table.c print.c

TARGETS	=talkd

LCOPTS	=-signed
LCDEFS	=-D_BSD_SIGNALS

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/etc $(TARGETS)

include $(COMMONRULES)

talkd:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
