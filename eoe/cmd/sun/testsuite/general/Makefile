# @(#)Makefile	1.2 87/02/11 NFSSRC
DESTDIR=/no/such/path
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CFILES= stat.c
TARGETS= ${CFILES:.c=}

default: ${TARGETS}

include ${COMMONRULES}

copy dist: ${_FORCE}
	cp Makefile runtests runtests.wrk *.sh *.c mkdummy rmdummy \
	   nroff.in makefile.tst \
	   ${DESTDIR}
