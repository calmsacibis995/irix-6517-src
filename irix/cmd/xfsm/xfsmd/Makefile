#!smake
#ident "$Revision: 1.6 $"

DEPTH=..
include $(DEPTH)/Makedefs
LIB=../lib
KERN=../../../kern
LCINCS=-I../include -I$(KERN)

CFILES = xfs_info_server.c 

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE
LCOPTS = $(REGOPTS)
MYLD_FLAGS=-L$(ROOT)/usr/sgitcl/lib
LLDOPTS= -rpath ${INSTXFSMLIB}:/usr/sgitcl/lib
LLDLIBS = $(MYLD_FLAGS) ../lib/libxfsmserv.a \
	-lw $(LIBXLV) -ldisk -llmsgi -ltcl

LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

TARGETS = xfsmd

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F ${INSTXFSMD} $(TARGETS)

xfsmd: $(OBJECTS) $(LIB)/libxfsmserv.a
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
