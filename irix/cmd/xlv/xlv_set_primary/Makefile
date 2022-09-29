#!smake
#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I../include -I$(KERN)

CFILES = xlv_set_primary.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -DSTATIC=static
LCDEFS=$(REGOPTS)
LLDLIBS = -lxlv -ldisk
LLDOPTS = -L$(LIB) -Wl,-I,$(ROOTRLDNAME)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

TARGETS = xlv_set_primary

default: $(TARGETS)

install: default
	$(INSTALL) -F /sbin $(TARGETS)

include $(COMMONRULES)

xlv_set_primary: $(OBJECTS) $(LIB)/libxlv.a
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'

