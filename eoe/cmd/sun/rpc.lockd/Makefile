#!smake
#
# Makefile for the rpc.lockd subdirectory
#
# $Revision: 1.10 $

include	${ROOT}/usr/include/make/commondefs

#CVERSION=-cckr

LCDEFS= -D_BSD_SIGNALS -D_SGI_MP_SOURCE -D_NEW_SYSID
OPTIMIZER= -g
LCOPTS= -fullwarn
LLDLIBS=
LINTFLAGS= -woff 91,102

CFILES= main.c
LDIRT = vers.c
CTAGOPTS=-tw

TARGETS= rpc.lockd

default: ${TARGETS}

include	${COMMONRULES}

rpc.lockd: ${OBJECTS} vers.o
	${CCF} ${LCOPTS} ${OBJECTS} vers.o ${LCDEFS} ${LDFLAGS} -o $@

vers.c:	${CFILES} ${HFILES} newvers.sh
	sh newvers.sh

install: default
	${INSTALL} -F /usr/etc ${TARGETS}
