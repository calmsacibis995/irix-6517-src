# Make the compress command
# $Revision: 1.10 $ 

include	$(ROOT)/usr/include/make/commondefs

CFILES	= compress.c
# OPTIMIZER=-g -DDEBUG
OPTIMIZER=-O

TARGETS	= compress uncompress zcat

LCOPTS = -signed

I_FLAGS=-idb "EOE MR"

LLDLIBS	= -lcomp

default: $(TARGETS)

install:default exports headers
	$(INSTALL) $(I_FLAGS) -F /usr/bsd compress
	$(INSTALL) $(I_FLAGS) -F /usr/bsd -lns compress uncompress
	$(INSTALL) $(I_FLAGS) -F /usr/bsd -lns compress zcat

exports:

headers:

include $(COMMONRULES)

compress:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

uncompress:	compress
	ln -f compress uncompress

zcat:	compress
	ln -f compress zcat
