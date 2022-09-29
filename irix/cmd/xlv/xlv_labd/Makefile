#!smake
#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I$(ROOT)/usr/include -I../include -I$(KERN)

HFILES = 
CFILES = xlv_labd.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE
LCOPTS = $(REGOPTS)
LLDLIBS = -lxlv -ldisk
LLDOPTS = -L$(LIB) -Wl,-I,$(ROOTRLDNAME)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

TARGETS = xlv_labd

default: $(TARGETS)

install: default
	$(INSTALL) -F /sbin $(TARGETS)

include $(COMMONRULES)

xlv_labd: $(OBJECTS) $(LIB)/libxlv.a
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
