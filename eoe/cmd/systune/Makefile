#!smake
#
#ident $Revision: 1.6 $

BASEVERSION=n32bit
ALTVERSIONS=64bit

include	$(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=systune 

LCOPTS+= -fullwarn 

CFILES=systune.c 

default: $(TARGETS)

include $(CMDCOMMONRULES)

n32bitinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG32) $(COMMANDS)
	$(INSTALL) -F /usr/sbin edstune

64bitinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG64) $(COMMANDS)
# DO NOT DELETE
