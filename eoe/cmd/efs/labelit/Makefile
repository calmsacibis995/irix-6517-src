#!smake
#ident $Revision: 1.10 $
include ../commondefs

CFILES= labelit.c
TARGETS= labelit

LCINCS= -I../lib
LLDOPTS= -L../lib -Wl,-I,$(ROOTRLDNAME)
LLDLIBS= -lefs

include ../commonrules

install: all
	${INSTALL} -F /sbin -idb "std.sw.unix" ${TARGETS}
	${INSTALL} -F /etc -idb "std.sw.unix" -lns ../sbin/${TARGETS} ${TARGETS}
