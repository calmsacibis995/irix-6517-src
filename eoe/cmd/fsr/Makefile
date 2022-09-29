#!smake
#ident "$Id: Makefile,v 1.2 1999/11/16 16:14:32 clk Exp $"
include ${ROOT}/usr/include/make/commondefs

CFILES= fsr.c
TARGETS= fsr

LCOPTS= -fullwarn

default: ${TARGETS}

include ${COMMONRULES}

fsr:	${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/etc -idb "std.sw.unix" ${TARGETS}

