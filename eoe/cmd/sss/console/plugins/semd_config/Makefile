#!smake
#
#
SRC_DEPTH = ../../..

include $(ROOT)/usr/include/make/commondefs
include $(SRC_DEPTH)/include/ssscommondefs

LOCALINCS = -nostdinc -I$(ROOT)/usr/include/sss -I$(ROOT)/usr/include \
	    -I../include 
LLDLIBS=  -delay_load -lssdb -lSSC -leventmon
IDB_TAG=-idb $(SSS_EOE)
OBJECT_STYLE=N32
LCDEFS  = 
LCOPTS =  
LCINCS = $(LOCALINCS)
CFILES = semd_config.c
LD_QUICKSTART_INFO=
COMMONPREF = SSS
LIBPATH = /usr/lib32/internal
TARGETS = semd_config.o

install default: $(TARGETS)

include $(COMMONRULES)

clobber clean:
	rm -f *.o $(TARGETS) 
	rm -f so_locations Makedepend

