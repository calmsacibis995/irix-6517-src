#!smake
#ident "$Revision: 1.6 $"

include $(ROOT)/usr/include/make/commondefs

COMMINCL = \
	path.h \
	types.h

COMMON = \
	path.c

LOCALS = \
	main.c

CFILES = $(COMMON) $(LOCALS)
OFILES = $(CFILES:.c=.o)
LINKS = $(COMMINCL) $(COMMON)

LCOPTS = -fullwarn
LCDEFS = -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DBANYAN -DFICUS -DNDEBUG
LDIRT = $(LINKS)
WOFF =

TARGETS = treecmp

default: $(LINKS) $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)

treecmp: $(OFILES)
	$(CCF) -o $@ $(OFILES) $(LDFLAGS)

$(COMMINCL) $(COMMON):
	@rm -f $@; ln -s ../common/$@ $@
