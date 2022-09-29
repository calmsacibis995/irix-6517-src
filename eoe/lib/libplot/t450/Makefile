#!smake
#
# Makefile for lib450.
#
#ident	"$Revision: 1.2 $"

BASELIB=lib450.a

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

CFILES =  arc.c box.c circle.c close.c dot.c erase.c label.c line.c \
	linmod.c move.c open.c point.c space.c subr.c

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib -m 444 $(LIBS)

include $(LIBROOTRULES)
