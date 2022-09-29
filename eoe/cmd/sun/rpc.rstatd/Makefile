#!smake
#
# Makefile for the rpc.rstatd subdirectory
#
# $Revision: 1.3 $

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	${ROOT}/usr/include/make/cmdcommondefs

COMMANDS= rpc.rstatd

CVERSION=-cckr

LCDEFS+= -D_BSD_TIME
LCOPTS+= -fullwarn
LLDLIBS+= -lrpcsvc -ldisk
CFLAGS+= -DHWG
default: ${TARGETS}

include	${CMDCOMMONRULES}

32bitinstall: default
	${INSTALL} -F /usr/etc ${IDB_TAG32} ${COMMANDS}

64bitinstall: default
	${INSTALL} -F /usr/etc ${IDB_TAG64} ${COMMANDS}
