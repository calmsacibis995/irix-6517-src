#!smake
#ident $Revision: 1.6 $
include ../commondefs

CFILES=	growfs.c 
TARGETS= growfs

LLDOPTS= -Wl,-I,$(ROOTRLDNAME)
LLDLIBS= -ldisk

include ../commonrules

install: all
	${INSTALL} -F /sbin -idb std.sw.unix ${TARGETS}
	${INSTALL} -F /etc -idb std.sw.unix -lns ../sbin/${TARGETS} ${TARGETS}

