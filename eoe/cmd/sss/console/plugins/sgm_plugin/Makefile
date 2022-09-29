#!smake
#
#
SRC_DEPTH = ../../..

include $(ROOT)/usr/include/make/commondefs
include $(SRC_DEPTH)/include/ssscommondefs

LOCALINCS = -nostdinc -I$(ROOT)/usr/include/sss -I$(ROOT)/usr/include \
	    -I../include 
LLDLIBS=  -delay_load -lssdb -lSSC -leventmon

OBJECT_STYLE=N32
LCDEFS  = 
LCOPTS =  
LCINCS = $(LOCALINCS)
CFILES = uisgmserver.c uisgmevents.c uielements.c 
LD_QUICKSTART_INFO=
COMMONPREF = SSS
LIBPATH = /usr/lib32/internal
TARGETS = libsgmserver.so
IDB_TAG=-idb $(SSS_EOE)

default: $(TARGETS)

include $(COMMONRULES)

clobber clean:
	rm -f *.o $(TARGETS) 
	rm -f so_locations Makedepend

libsgmserver.so: $(OBJECTS) 
	cc $(OBJECTS) ../semd_config/semd_config.o \
	$(LDFLAGS) -shared -all -soname $@ \
	-exports_file $(ROOT)/usr/include/sss/rgPluginServer.export \
	-o $@ -g -woff 1048 -no_unresolved


install: default
	${INSTALL} $(IDB_TAG) -F $(LIBPATH) $(TARGETS)
