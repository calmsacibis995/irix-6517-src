#!smake
#ident "$Revision: 1.10 $"

BASEVERSION=n32bit
ALTVERSIONS=troot
WANTPARALLEL=yes-please
include	$(ROOT)/usr/include/make/cmdcommondefs

CFILES=	ed.c textmem.c

COMMANDS=ed 

default: $(TARGETS)

include $(CMDCOMMONRULES)

ed: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

n32bitinstall: default 
	$(INSTALL) -F /sbin ed
	$(INSTALL) -F /usr/bin -lns ../../sbin/ed ed
	$(INSTALL) -F /usr/bin -lns ../../sbin/ed red

trootinstall: default
	$(INSTALL) -F /sbin ed
