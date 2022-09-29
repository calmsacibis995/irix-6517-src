#!smake
#
# Makefile for libplot.
#
#ident	"$Revision: 1.4 $"

OBJECT_STYLE=32
BASELIB=libplot.a
include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

CFILES  =  arc.c box.c circle.c close.c cont.c dot.c erase.c label.c line.c \
	linmod.c move.c open.c point.c putsi.c space.c

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib -m 444 $(LIBS)

include $(LIBROOTRULES)
