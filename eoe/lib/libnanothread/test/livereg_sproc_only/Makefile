#!smake
#ident	"$Revision: 1.1 $"

OBJECT_STYLE=64

include	$(ROOT)/usr/include/make/commondefs

LDOPTS=-L -L$(ROOT)/usr/lib64

ASFILES=livereg.s
CFILES=main.c
TARGETS=livereg

default: $(TARGETS)

include $(COMMONRULES)

livereg: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

