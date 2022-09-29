#!smake
#
# Makefile for libvt0.
#
#ident	"$Revision: 1.3 $"

OBJECT_STYLE=32
BASELIB=libvt0.a

include	$(ROOT)/usr/include/make/librootdefs

TARGETS=$(LIBS)

CFILES =  arc.c box.c circle.c close.c dot.c erase.c frame.c label.c line.c \
	linmod.c move.c open.c point.c space.c subr.c

default: $(TARGETS)

exports install: default
	$(INSTALL) -F /usr/lib -m 444 $(LIBS)

include $(LIBROOTRULES)
