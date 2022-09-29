#!smake
#ident $Revision: 1.14 $
include ${ROOT}/usr/include/make/commondefs

CFILES= ncheck.c
TARGETS= ncheck

LCINCS= -I../nlib
LCOPTS= -fullwarn
LLDOPTS= -L../nlib -Wl,-I,$(ROOTRLDNAME)
LLDLIBS= -lefs -ldisk

all: ${TARGETS}

include ${COMMONRULES}

install: all
	${INSTALL} -F /sbin -idb "std.sw.unix" ${TARGETS}
	${INSTALL} -F /etc -idb "std.sw.unix" -lns ../sbin/${TARGETS} ${TARGETS} 

