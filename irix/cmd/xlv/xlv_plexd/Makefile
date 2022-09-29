#!smake
#ident "$Revision: 1.10 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I$(ROOT)/usr/include -I../include -I$(KERN)

HFILES = 
CFILES = xlv_plexd.c xlv_plexd_utils.c
TCFILES = xlv_plexd_test.c
OBJECTS = $(CFILES:.c=.o)
TOBJECTS = $(TCFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE

LLDLIBS = -lxlv -ldisk
LLDOPTS = -L$(LIB) -Wl,-I,$(ROOTRLDNAME)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852
LCOPTS = $(REGOPTS)

TARGETS = xlv_plexd
TST_TARGETS = xlv_plexd_test

default: $(TARGETS) $(TST_TARGETS)

install: default
	$(INSTALL) -F /sbin $(TARGETS)

include $(COMMONRULES)

xlv_plexd: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

xlv_plexd_test: $(TOBJECTS)
	$(CCF) $(TOBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) LCDEFS='$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
