#!smake
# Makefile for heap under mysql
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

TARGETS = libheap.a

CFILES  = _check.c _rectest.c hp_block.c hp_clear.c hp_close.c hp_create.c \
	hp_delete.c hp_extra.c hp_hash.c hp_info.c hp_open.c hp_panic.c \
	hp_rename.c hp_rfirst.c hp_rkey.c hp_rlast.c hp_rnext.c hp_rprev.c \
	hp_rrnd.c hp_rsame.c hp_static.c hp_test1.c hp_test2.c hp_update.c \
	hp_write.c


OFILES  = $(CFILES:.c=.o)

OBJECTS = hp_open.o hp_extra.o hp_close.o hp_panic.o \
	hp_info.o hp_rrnd.o hp_update.o hp_write.o hp_delete.o hp_rsame.o \
	hp_create.o hp_rename.o hp_rfirst.o hp_rnext.o hp_rlast.o hp_rprev.o \
	hp_clear.o hp_rkey.o hp_block.o hp_hash.o _check.o _rectest.o \
	hp_static.o

COMMONPREF = SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

libheap.a : $(OBJECTS)
	ar ccurl libheap.a $(OBJECTS)
