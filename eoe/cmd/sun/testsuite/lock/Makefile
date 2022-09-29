#
#       @(#)Makefile	1.2 97/01/03 Connectathon Testsuite
#
# to make tests, use 'make'
# to copy tests to another directory, use 'make copy DESTDIR=dir'
# to copy source to another directory, use 'make dist DESTDIR=dir'
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

DESTDIR = /no/such/path
CFILES= tlock.c
TARGETS= tlock tlock32 tlock32lfs tlock64

default: ${TARGETS}

include ${COMMONRULES}

tlock:	tlock.c
	$(CC) $(CFLAGS) -o tlock tlock.c

tlock32:	tlock.c
	$(CC) $(CFLAGS) -o tlock32 tlock.c

tlock32lfs:	tlock.c
	$(CC) $(CFLAGS) -DLF_SUMMIT -o tlock32lfs tlock.c

tlock64:	tlock.c
	$(CC) $(CFLAGS) -DLF_SUMMIT -DLARGE_LOCKS -o tlock64 tlock.c

copy: ${TARGETS}
	cp runtests ${TARGETS} ${DESTDIR}

dist:
	cp runtests Makefile *.c $(DESTDIR)
