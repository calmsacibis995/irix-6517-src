#!smake
#
# Makefile for bufview

include $(ROOT)/usr/include/make/commondefs

COMMANDS=bufview

# Values for the two defaults in "bufview":
#	TOPN	- default number of buffers to display
#	DELAY	- default delay between updates
#
# set TOPN to -1 to indicate infinity (so that bufview will display as many
# as the screen will hold).
TARGETS=bufview

TOPN = -1
DELAY = 3

CFILES = bv.c display.c screen.c utils.c machine.c
OBJECTS = bv.o display.o screen.o utils.o machine.o

LLDLIBS+= -lcurses
LCOPTS+= -fullwarn
LCDEFS+= -D_OLD_TERMIOS -D_BSD_SIGNALS
LCFLAGS = -n32 -mips3

default: $(TARGETS)

bufview: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

clobber:
	rm -f *.o bufview core core.*

install: default
	$(INSTALL) -F /usr/sbin -m 2755 -O $(IDB_TAG32) bufview
