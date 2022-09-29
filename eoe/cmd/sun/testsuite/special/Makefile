#
#       @(#)Makefile	1.1 97/01/03 Connectathon Testsuite
#	1.4 Lachman ONC Test Suite source
#
DESTDIR=/no/such/path
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CFILES= dupreq.c excltest.c fstat.c holey.c negseek.c nfsidem.c nstat.c	\
	open-chmod.c open-rename.c open-unlk.c rename.c rewind.c stat.c	\
	stat2.c touchn.c truncate.c

TARGETS= ${CFILES:.c=}
LCDEFS = -D_BSD_TIME

default: ${TARGETS}

include ${COMMONRULES}

copy: ${TARGETS}
	cp runtests runtests.wrk ${TARGETS} ${DESTDIR}

dist: ${_FORCE}
	cp runtests runtests.wrk Makefile *.c ${DESTDIR}
