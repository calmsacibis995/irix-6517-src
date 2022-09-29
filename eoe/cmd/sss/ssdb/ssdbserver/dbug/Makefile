#!smake
# Makefile for dbug under ssdb
#       "$Revision: 1.1 $"
#
OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

SRCDIR = .

LIBS = -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 -L$(ROOT)/usr/lib32/internal \
	-lnsl -lm  -lsocket  -lcrypt -ldl -lgen -lcurses -lpthread

LCINCS = -nostdinc -I$(SRCDIR)/../include -I$(SRCDIR)/.. -I.. -I$(SRCDIR) -I$(ROOT)/usr/include

LCDEFS  = -DDBUG_OFF -DHAVE_CONFIG_H

OPTIMIZER = -O

TARGETS = libdbug.a

CFILES = dbug.c sanity.c

OFILES  = $(CFILES:.c=.o)

OBJECTS = dbug.o sanity.o

COMMONPREF =    SSS

$(COMMONPREF)install default: $(TARGETS) 

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

libdbug.a : $(OBJECTS)
	ar ccurl libdbug.a $(OBJECTS)
