#!smake
# Makefile for readline under ssdb
#       "$Revision: 1.1 $"
#
OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

SRCDIR = .

LLIBS   = -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 -L$(ROOT)/usr/lib32/internal \
	-lnsl -lm  -lsocket  -lcrypt -ldl -lgen -lcurses -lpthread

LCINCS = -nostdinc -I$(SRCDIR)/../include -I../include -I. -I$(SRCDIR) -I.. \
	 -I$(ROOT)/usr/include

LCDEFS  = -DDBUG_OFF -DHAVE_CONFIG_H

LCOPTS = -O

TARGETS = libregex.a

CFILES  = debug.c main.c regerror.c regexp.c reginit.c engine.c regcomp.c \
	  regexec.c regfree.c split.c

OBJECTS = regerror.o regcomp.o regexec.o regfree.o reginit.o

COMMONPREF =    SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

libregex.a : $(OBJECTS)
		ar ccurl libregex.a $(OBJECTS)
