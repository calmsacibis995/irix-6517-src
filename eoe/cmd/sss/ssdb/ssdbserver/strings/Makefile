#!smake
# Makefile for strings under ssdb
#       "$Revision: 1.1 $"
#

OBJECT_STYLE=N32
include $(ROOT)/usr/include/make/commondefs

SRCDIR = .

LIBS = -nostdlib -L$(ROOT)/usr/lib32/mips3 -L$(ROOT)/usr/lib32 -L$(ROOT)/usr/lib32/internal \
	-lnsl -lm  -lsocket  -lcrypt -ldl -lgen -lcurses -lpthread

LCINCS = -nostdinc -I$(SRCDIR)/../include -I$(SRCDIR)/.. -I../include -I$(ROOT)/usr/include

LCDEFS  = -DDBUG_OFF -DHAVE_CONFIG_H

OPTIMZER = -O 

TARGETS = libmystrings.a

MYSTRING_OBJS =	bfill.o bmove.o \
		bmove512.o bchange.o strxnmov.o int2str.o str2int.o \
		r_strinstr.o ct_init.o atof.o ptr_cmp.o bcmp.o \
		strtol.o strtoul.o strtoll.o strtoull.o strxmov.o \
		bmove_upp.o strappend.o strcont.o strend.o strfill.o \
		strcend.o is_prefix.o strstr.o strinstr.o strmake.o \
		strnmov.o strmov.o ctype-latin1.o


COMMONPREF =    SSS

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

libmystrings.a : $(MYSTRING_OBJS)
	ar ccurl libmystrings.a $(MYSTRING_OBJS)
