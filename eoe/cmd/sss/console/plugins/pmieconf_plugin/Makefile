#!smake
#
#
SRC_DEPTH = ../../..

include $(ROOT)/usr/include/make/commondefs
include $(SRC_DEPTH)/include/ssscommondefs

LOCALINCS = -nostdinc -I$(ROOT)/usr/include/sss -I$(ROOT)/usr/include \
	    -I../include  -I ../../include 
LLDLIBS=  -delay_load -lSSC 

OBJECT_STYLE=N32
LCDEFS  = -DOS_UNIX -DPTHREAD_THREAD -DAMR_DEBUG=1
IDB_TAG=-idb $(SSS_EOE)
LCOPTS =  -g
LCINCS = $(LOCALINCS)
CFILES = pmieconfsrvr.c
LD_QUICKSTART_INFO=
COMMONPREF = SSS
LIBPATH = /usr/lib32/internal
TARGETS = pmiesrvr.so

default: $(TARGETS)

include $(COMMONRULES)

clobber clean:
	rm -f *.o $(TARGETS) so_locations Makedepend

pmiesrvr.so: $(OBJECTS)
	cc  $(OBJECTS) $(LDFLAGS) -shared -all -soname $@ \
	-exports_file $(ROOT)/usr/include/sss/rgPluginServer.export \
	-o $@ -g -woff 1048 -no_unresolved 


install: default
	${INSTALL} $(IDB_TAG) -F $(LIBPATH) $(TARGETS)
