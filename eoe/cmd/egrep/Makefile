#ident	"$Revision: 1.12 $"

include	$(ROOT)/usr/include/make/commondefs

LLDLIBS = -lw

TARGETS = egrep

LDIRT = egrep.c

default: $(TARGETS)

install: default
	$(INSTALL) -F /usr/bin egrep

egrep:	egrep.c	
	$(CCF) $@.c -o $@ $(LDFLAGS)

egrep:	egrep.y $(FRC) $(INCLDIR)/stdio.h $(INCLDIR)/ctype.h 

include $(COMMONRULES)
