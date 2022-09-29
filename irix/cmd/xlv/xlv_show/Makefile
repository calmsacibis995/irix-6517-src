#!smake
#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I../include -I$(KERN)

CFILES = xlv_show.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -DSTATIC=static
LCDEFS=$(REGOPTS)
LLDLIBS = -lxlv
LLDOPTS = -L$(LIB)

TARGETS = xlv_show

default: $(TARGETS)

install: default
	$(INSTALL) -F /sbin $(TARGETS)

include $(COMMONRULES)

xlv_show: $(OBJECTS) $(LIB)/libxlv.a
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
