# @(#)Makefile	1.2 87/02/11 NFSSRC
# to make tests, use 'make'
# to copy tests to another directory, use 'make copy DESTDIR=dir'
# to copy source to another directory, use 'make dist DESTDIR=dir'
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CTFILES= test1.c test2.c test3.c test4.c test5.c test6.c test7.c test8.c test9.c
CFILES= subr.c ${CTFILES}
CTARGETS= ${CTFILES:.c=}
TARGETS= ${CTARGETS} ${CSHFILES:.csh=}

DESTDIR= /no/such/path

LCDEFS = -DSVR4 -D_BSD_TIME

all: ${TARGETS}

include ${COMMONRULES}

${CTARGETS}: $$@.c subr.o
	${CCF} $@.c subr.o ${LDFLAGS} -o $@

copy: ${TARGETS}
	cp runtests defs.csh ${TARGETS} ${DESTDIR}

dist: ${_FORCE}
	cp runtests Makefile *.c *.h *.csh ${DESTDIR}
