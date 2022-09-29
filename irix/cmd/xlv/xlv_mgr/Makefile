#!smake
#ident "$Revision: 1.8 $"

include $(ROOT)/usr/include/make/commondefs
LIB_LOCATION=lib
WOFF=
XLVLIB=../lib
TCLLIB=../libtcl
KERN=../../../kern
LCINCS =-I../include -I$(ROOT)/usr/include -I$(KERN)

HFILES = xlv_mgr.h
CFILES = xlv_mgr.c xlv_mgr_cmd.c xlv_mgr_int.c \
	 name.c prompt.c table.c utils.c \
	 vol.c plex.c ve.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE
LCOPTS = $(REGOPTS)
#
# There is no libm.so in the miniroot so force the linker to
# look for libm.a by using the "-B static" option.
#
LLDLIBS = -lxlv -ldisk -B static -ltcl7.3 -lm -B dynamic
ADDLLDLIBS = -llmsgi
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852
LLDOPTS = -L$(XLVLIB) -L$(TCLLIB) -Wl,-I,$(ROOTRLDNAME)

TARGETS = xlv_mgr mrxlv_mgr

default: $(TARGETS)

install: default
	$(INSTALL) -F /sbin -idb mr -src mrxlv_mgr xlv_mgr
	$(INSTALL) -F /sbin xlv_mgr

include $(COMMONRULES)

xlv_mgr: $(OBJECTS) $(XLVLIB)/libxlv.a $(TCLLIB)/libtcl7.3.a
	$(CCF) $(OBJECTS) $(LDFLAGS) $(ADDLLDLIBS) -o $@

mrxlv_mgr: $(OBJECTS) $(XLVLIB)/src/license_check_stub.o $(XLVLIB)/libxlv.a $(TCLLIB)/libtcl7.3.a
	$(CCF) $(OBJECTS) $(XLVLIB)/src/license_check_stub.o $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
