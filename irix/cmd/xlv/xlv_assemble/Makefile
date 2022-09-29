#!smake
#ident "$Revision: 1.19 $"

include $(ROOT)/usr/include/make/commondefs
WOFF=
LIB=../lib
KERN=../../../kern
LCINCS=-I../include -I$(KERN)

CFILES = xlv_assemble.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE -DSTATIC=static
LCDEFS=$(REGOPTS)
LCOPTS=-Wl,-I,$(ROOTRLDNAME)
MRLLDLIBS = -lxlv -ldisk
LLDLIBS = $(MRLLDLIBS)
ADDLLDLIBS = -ll -llmsgi

LLDOPTS = -L$(LIB)
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

TARGETS = xlv_assemble mrxlv_assemble

EXITOP=exitop("if { test -f $$rbase/etc/fstab; } then { cp $$rbase/etc/fstab $$rbase/etc/fstab.orig; sed -e s+/dev/dsk/xlv/+/dev/xlv/+ -e s+/dev/rdsk/xlv/+/dev/rxlv/+ $$rbase/etc/fstab.orig > $$rbase/etc/fstab; } fi")

default: $(TARGETS)

install: default
	$(INSTALL) -F /sbin -idb mr -src mrxlv_assemble xlv_assemble
	$(INSTALL) -F /sbin -idb '$(EXITOP)' xlv_assemble

include $(COMMONRULES)

xlv_assemble: $(OBJECTS) $(LIB)/libxlv.a
	$(CCF) $(OBJECTS) $(LDFLAGS) $(ADDLLDLIBS) -o $@

mrxlv_assemble: $(OBJECTS) $(LIB)/src/license_check_stub.o $(LIB)/libxlv.a
	$(CCF) $(OBJECTS) $(LIB)/src/license_check_stub.o $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
