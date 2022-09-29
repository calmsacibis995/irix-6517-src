#!smake
#
# Makefile for ml.
#
#ident "$Revision: 1.7 $"

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=ml

LCOPTS+= -fullwarn

default: $(TARGETS)

include $(CMDCOMMONRULES)

32bitinstall: default
	$(INSTALL) -F /sbin $(IDB_TAG32) $(COMMANDS)
	cd lib; $(MAKE) install

64bitinstall: default
	$(INSTALL) -F /sbin $(IDB_TAG64) $(COMMANDS)
