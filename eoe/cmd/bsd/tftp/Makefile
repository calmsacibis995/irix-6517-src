#!smake
#
# Make the tftp command
#
# $Revision: 1.10 $

include	$(ROOT)/usr/include/make/commondefs

CVERSION=-cckr

CFILES	=main.c tftp.c tftpsubs.c
TARGETS	=tftp

LCOPTS	=-signed
LCDEFS	=-D_BSD_SIGNALS

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/bsd $(TARGETS)

include $(ROOT)/usr/include/make/commonrules

tftp:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
