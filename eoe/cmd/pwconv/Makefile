#ident	"$Revision: 1.5 $"

include	$(ROOT)/usr/include/make/commondefs

TARGETS= pwconv
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LLDLIBS=
CFILES= pwconv.c htree.c
OBJS=$(CFILES:.c=.o)
HEADERS= htree.h

default: $(TARGETS)

include $(COMMONRULES)

pwconv: $(OBJS) 
	$(CCF) $(OBJS) $(LDFLAGS) -o $@

install: default
	$(INSTALL) -u root -g sys -m 755 -F /sbin $(TARGETS)

