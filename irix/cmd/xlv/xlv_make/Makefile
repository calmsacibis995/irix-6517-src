#!smake
#ident "$Revision: 1.15 $"

include $(ROOT)/usr/include/make/commondefs
LIB_LOCATION=lib
WOFF=
XLVLIB=../lib
TCLLIB=../libtcl

KERN=../../../kern
LCINCS=-I../include -I$(KERN) -I$(ROOT)/usr/include

HFILES = xlv_make_cmd.h
CFILES = xlv_make.c xlv_make_cmd.c xlv_make_int.c
OBJECTS = $(CFILES:.c=.o)

DEBUG = -g -DDEBUG
REGOPTS = -fullwarn -D_SGI_SOURCE
LCOPTS = $(REGOPTS)
#
# There is no libm.so in the miniroot so force the linker to
# look for libm.a by using the "-B static" option.
#
LLDLIBS = -lxlv -ldisk -B static -ltcl7.3 -lm -B dynamic
LINTOPTS = $(XFSDEBUG) -wlint -woff 813,819,826,852

LLDOPTS = -L$(XLVLIB) -L$(TCLLIB)

TARGETS = xlv_make

default: $(TARGETS)

install: default
	$(INSTALL) -F /usr/sbin xlv_make

include $(COMMONRULES)

xlv_make: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

debug:
	exec $(MAKE) 'LCDEFS=$(REGOPTS) $(DEBUG)'
lint:
	exec $(MAKE) LCOPTS='$(REGOPTS) $(LINTOPTS)'
