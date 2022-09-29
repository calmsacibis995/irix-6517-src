#!smake
#ident	"$Revision: 1.6 $"

OBJECT_STYLE=64

include	$(ROOT)/usr/include/make/commondefs

TARGETS=app
LDOPTS=-L -L$(ROOT)/usr/lib64 -lnanothread

CFILES=app.c mp_sugnumthd.c

default: $(TARGETS)

include $(COMMONRULES)

app: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

