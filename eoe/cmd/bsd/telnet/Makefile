#!smake
#
# Make the telnet command
#
# $Revision: 1.8 $

include $(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

CFILES=	commands.c main.c network.c ring.c sys_bsd.c \
	telnet.c terminal.c tn3270.c utilities.c
TARGETS	=telnet

LCOPTS	=-signed -woff 1116
LCDEFS	=-D_BSD_SIGNALS -DUSE_TERMIO -DSRCRT -DKLUDGELINEMODE \
	-DNEED_GETTOS -Dvfork=fork
LLDLIBS	=-ltermlib

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/bsd $(TARGETS)

include $(COMMONRULES)

telnet: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
