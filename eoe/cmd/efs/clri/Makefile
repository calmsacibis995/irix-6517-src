#!smake
#ident $Revision: 1.12 $
include ${ROOT}/usr/include/make/commondefs

CFILES= clri.c
TARGETS= clri

LCINCS= -I../nlib
LCOPTS= -fullwarn
LLDOPTS= -L../nlib -Wl,-I,$(ROOTRLDNAME)
LLDLIBS= -lefs -ldisk

all: ${TARGETS}

include ${COMMONRULES}

install: all
	${INSTALL} -F /sbin -idb "std.sw.unix mr" ${TARGETS}
	${INSTALL} -F /etc -idb "std.sw.unix mr" -lns ../sbin/${TARGETS} ${TARGETS}
