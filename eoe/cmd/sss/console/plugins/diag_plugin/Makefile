#!smake
#
#
SRC_DEPTH = ../../..

include $(ROOT)/usr/include/make/commondefs
include $(SRC_DEPTH)/include/ssscommondefs

LOCALINCS = -nostdinc -I$(ROOT)/usr/include/sss -I$(ROOT)/usr/include \
	    -I../include 
LLDLIBS=  -delay_load -lssdb -lSSC 

OBJECT_STYLE=N32
LCDEFS  = 
LCOPTS =  
LCINCS = $(LOCALINCS)
CFILES = uidiags.c
LD_QUICKSTART_INFO=
COMMONPREF = SSS
LIBPATH = /usr/lib32/internal
TARGETS = rgpdiag.so
IDB_TAG=-idb $(SSS_EOE)

default: $(TARGETS)

include $(COMMONRULES)

clobber clean:
	rm -f *.o $(TARGETS) 
	rm -f *.so so_locations Makedepend

$(TARGETS): $(OBJECTS) 
	cc $(OBJECTS) \
	$(LDFLAGS) -shared -all -soname $@ \
	-exports_file $(ROOT)/usr/include/sss/rgPluginServer.export \
	-o $@ -g -woff 1048 -no_unresolved


install: default
	${INSTALL} $(IDB_TAG) -F $(LIBPATH) $(TARGETS)
