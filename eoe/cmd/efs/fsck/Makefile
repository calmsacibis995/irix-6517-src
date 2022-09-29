#!smake
#ident $Revision: 1.13 $
include ../commondefs

CFILES=	fsck.c phase1.c phase2.c phase3.c phase4.c \
	phase5.c phase6.c sblk.c dir.c
TARGETS= fsck

LCDEFS+=-DAFS
LCINCS=-I../lib
LLDOPTS=-L../lib -Wl,-I,$(ROOTRLDNAME)
LLDLIBS=-ldisk

include ../commonrules

fsck: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: all
	${INSTALL} -F /sbin -idb mr ${TARGETS}
	${INSTALL} -F /etc -idb mr -lns ../sbin/${TARGETS} ${TARGETS}
