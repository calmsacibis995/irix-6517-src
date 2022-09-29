#!smake

#
# FILE: eoe/cmd/miser/lib/libmiser/src/Makefile
#
# DESCRIPTION:
#	Makefile for libmiser
#

include $(ROOT)/usr/include/make/commondefs

#DEBUG		= -g

BISON_FLAGS	= -d
FLEX_FLAGS	=

TARGETS		= ../libmiser.a
CFILES		= cmd.c miser.c parse.c parse_gram.c parse_scan.c print.c util.c
OFILES		= $(CFILES:.c=.o)

LCFLAGS		= $(DEBUG)
LDLIBS		= $(TARGETS)
LDIRT		= parse_gram.c parse_gram.h parse_scan.c
LWOFF		= ,1110,1199,1178

include $(COMMONRULES)

all:     default
default: $(TARGETS)
install: default

$(TARGETS): $(OFILES)
	$(AR) c$(ARFLAGS) $(TARGETS) $(OFILES)

parse_gram.c:	parse_gram.y
	$(BISON) $(BISON_FLAGS) $*.y
	mv $*.tab.c $*.c
	mv $*.tab.h $*.h

parse_scan.c:	parse_scan.l
	$(FLEX) $(FLEX_FLAGS) $*.l
	mv lex.yy.c $*.c

parse_gram.h:	parse_gram.c

parse_gram.o:	parse_gram.c

parse_scan.o:	parse_scan.c parse_gram.h
