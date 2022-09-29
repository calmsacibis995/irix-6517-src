#!smake
#ident "$Revision: 1.5 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I../include -I$(KERN)

CFILES = getlvent.c main.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -DSTATIC=static
LCDEFS=$(REGOPTS)
LLDLIBS = -lxlv -ldisk
LLDOPTS = -L$(LIB)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

TARGETS = lv_to_xlv

default: $(TARGETS)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)

include $(COMMONRULES)

lv_to_xlv: $(OBJECTS) $(LIB)/libxlv.a
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
