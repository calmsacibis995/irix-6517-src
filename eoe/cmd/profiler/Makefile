#!smake
# 
# Makefile for kernel profiler utilities
#
#ident $Revision: 1.11 $

BASEVERSION=32bit
ALTVERSIONS=64bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=prfld prfdc prfpr prfsnap prfstat

CFILES=prfld.c prfdc.c prfpr.c prfsnap.c prfstat.c

SUPPORT_CFILES=prfsubr.c elfsubr.c
SUPPORT_OBJECTS=${SUPPORT_CFILES:.c=.o}

LCOPTS+= -fullwarn

default: $(TARGETS)

include	$(CMDCOMMONRULES)

prfld: prfld.o ${SUPPORT_OBJECTS}
	$(CCF) prfld.o ${SUPPORT_OBJECTS} $(LDFLAGS) -o $@

prfpr: prfpr.o ${SUPPORT_OBJECTS}
	$(CCF) prfpr.o ${SUPPORT_OBJECTS} $(LDFLAGS) -o $@

32bitinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG32) $(COMMANDS)

64bitinstall: default
	$(INSTALL) -F /usr/sbin $(IDB_TAG64) $(COMMANDS)
