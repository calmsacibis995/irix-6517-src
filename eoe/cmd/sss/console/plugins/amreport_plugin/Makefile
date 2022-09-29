#!smake
#
#
SRC_DEPTH = ../../..

include $(ROOT)/usr/include/make/commondefs
include $(SRC_DEPTH)/include/ssscommondefs

LOCALINCS = -nostdinc -I$(ROOT)/usr/include/sss -I$(ROOT)/usr/include \
	    -I../include 
LLDLIBS=  -delay_load -lssdb -lSSC -llmsgi

OBJECT_STYLE=N32
LCDEFS  = -DOS_UNIX -DPTHREAD_THREAD -DAMR_DEBUG=1
LCOPTS =  -g
LCINCS = $(LOCALINCS)
CFILES = amreport.c
LD_QUICKSTART_INFO=
COMMONPREF = SSS
LIBPATH = /usr/lib32/internal
TARGETS = amrserver.so
IDB_TAG = -idb $(SSS_EOE)
LDIRT = so_locations
default: $(TARGETS)

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

#amreport.c:	amreport.rgl
#	$(TOOLROOT)/usr/bin/rgl amreport.rgl amreport.c

amrserver.so: $(OBJECTS) amreport.c
	$(CCF) $(OBJECTS) $(LDFLAGS) -shared -all -soname $@ \
	-exports_file $(ROOT)/usr/include/sss/rgPluginServer.export \
	-o $@ -g -woff 1048 -no_unresolved


$(COMMONPREF)install: default
	${INSTALL} $(IDB_TAG) -F $(LIBPATH) $(TARGETS)
