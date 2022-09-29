#!make
#ident "$Revision: 1.14 $"
include $(ROOT)/usr/include/make/commondefs
WOFF=
LIB=../lib
KERN=../../../kern
LCINCS =-I$(ROOT)/usr/sgitcl/include -I../include -I$(KERN) -I../xlv_make \
	-I$(ROOT)/usr/include

HFILES =
CFILES = swalk.c xlv_make_int.c choice.c plex.c find.c do.c ve.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -D_SGI_SOURCE
LCOPTS = $(REGOPTS)
##LLDLIBS = -ltcl -lxlv -ldisk -lm
LLDLIBS = $(ROOT)/usr/sgitcl/lib/libtcl.a ../lib/libxlv.a -ldisk -llmsgi
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

TARGETS = xlv_admin mrxlv_admin

default: $(TARGETS)

install: default
#	$(INSTALL) -F /usr/sbin -idb mr -src mrxlv_admin xlv_admin
#	$(INSTALL) -F /usr/sbin xlv_admin

include $(COMMONRULES)

xlv_admin: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

mrxlv_admin: $(OBJECTS) $(LIB)/src/license_check_stub.o
	$(CCF) $(OBJECTS) $(LIB)/src/license_check_stub.o $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
