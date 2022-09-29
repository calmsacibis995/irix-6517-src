#
# Makefile for nettest, a network performance analysis tool developed at Cray.
#
#ident $Revision: 1.1 $

DEPTH=..
include ${ROOT}/usr/include/make/commondefs

CFILES= nettest.c nettestd.c
TARGETS= nettest nettestd

LMKDEPFLAGS= ${NULLSUFFIX_MKDEPFLAG}

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway nettest
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway nettestd
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway runnettest
