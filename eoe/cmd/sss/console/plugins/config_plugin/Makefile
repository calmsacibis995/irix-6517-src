#!smake
#
# Makefile for ConfigMon examples
#
#ident "$Header: "

SRC_DEPTH = ../../..
OBJECT_STYLE=n32
include /usr/include/make/commondefs

include $(SRC_DEPTH)/include/ssscommondefs

LLDLIBS=  -L. -delay_load -lSSC -lm -lconfigmon -lhwconfig -lswconfig \
	  -lklib -lutil -lalloc -lssdb
LD_QUICKSTART_INFO=

VERSION   	= 1.0.a
LCDEFS    	= -I. -I$(ROOT)/usr/include -I $(ROOT)/usr/include/sss \
		  -g -DVERSION=$(VERSION) -DCM_DEBUG -DALLOC_DEBUG
TARGETS		= cmreport.so
CFILES          = cmreport.c
OPTIMIZER       = -g
OBJECTS         = cmreport.o
COMMONPREF      = SSS
LDIRT           = so_locations
all:default 

include $(COMMONRULES)

clean clobber install: $(COMMONPREF)$$@

#cmreport.c:	cmreport.rgl
#	$(TOOLROOT)/usr/bin/rgl cmreport.rgl cmreport.c

cmreport.so: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -shared -all \
	-soname $@ \
	-exports_file $(ROOT)/usr/include/sss/rgPluginServer.export \
	-o $@ -g -woff all -no_unresolved

default: $(TARGETS) $(_FORCE)

$(COMMONPREF)install: default
	${INSTALL} -idb $(SSS_EOE) -F /usr/lib32/internal cmreport.so

