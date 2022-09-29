#
#       @(#)Makefile	1.1 97/01/03 Connectathon Testsuite
#	1.5 Lachman ONC Test Suite source
#
#
DESTDIR= /no/such/path
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CFILES= tcp.c tcpd.c udp.c udpd.c dirdmp.c dirprt.c pmaptst.c pmapbrd.c
TARGETS = ${CFILES:.c=}
LLDLIBS= -lm -lbsd

all: ${TARGETS}

include ${COMMONRULES}

copy: ${TARGETS}
	cp ${TARGETS} ${DESTDIR}

dist: ${_FORCE}
	cp Makefile README *.c ${DESTDIR}
