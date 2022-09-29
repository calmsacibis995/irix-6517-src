#!smake
#
# Makefile for lib4014.
#
#ident	"$Revision: 1.2 $"

BASELIB=lib4014.a

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

CFILES =  arc.c box.c circle.c close.c dot.c erase.c label.c line.c \
	linemod.c move.c open.c point.c scale.c space.c subr.c

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib -m 444 $(LIBS)

include $(LIBROOTRULES)
