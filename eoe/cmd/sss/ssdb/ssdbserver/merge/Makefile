#!smake
# Makefile for merge under mysql
#	"$Revision: 1.1 $"
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

TARGETS = libmerge.a

CFILES  = _locking.c close.c create.c delete.c extra.c info.c open.c \
	panic.c rrnd.c rsame.c static.c update.c

OFILES  = $(CFILES:.c=.o)

LIBMERGE_OBJS = open.o extra.o info.o _locking.o rrnd.o update.o delete.o \
		rsame.o panic.o close.o create.o static.o

COMMONPREF = SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

libmerge.a : $(LIBMERGE_OBJS)
		ar ccurl libmerge.a $(LIBMERGE_OBJS)
