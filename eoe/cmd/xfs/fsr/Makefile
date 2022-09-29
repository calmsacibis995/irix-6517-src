#!smake
#ident "$Id: Makefile,v 1.2 1999/03/06 18:22:28 cwf Exp $"
include ${ROOT}/usr/include/make/commondefs

CFILES= fsr_xfs.c jdm.c
TARGETS= fsr_xfs

LCDEFS = -D_SGI_SOURCE -D_SGI_MP_SOURCE -D_SGI_REENTRANT_FUNCTIONS -DCLRMTAUD -DEXTATTR -DBASED -DDOSOCKS -DINVCONVFIX -DSIZEEST -DPIPEINVFIX -DBANYAN -DFICUS -DNDEBUG
LLDLIBS= -B static -ldm
LCOPTS= -fullwarn

default: ${TARGETS}

include ${COMMONRULES}

fsr_xfs:	${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

fsrtst:	fsrtst.c
	${CCF} fsrtst.c -o $@

install: $(TARGETS)
	$(INSTALL) -u root -g sys -m 755 -F /usr/etc $(TARGETS)

