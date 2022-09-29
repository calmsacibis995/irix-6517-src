#
# Makefile for nettest, a network performance analysis tool developed at Cray.
#
#ident $Revision: 1.9 $

DEPTH=..
include ${ROOT}/usr/include/make/commondefs

CFILES= nettest.c nettestd.c
TARGETS= nettest nettestd
GIFTSLOC= /usr/people/4Dgifts/examples/network

LMKDEPFLAGS= ${NULLSUFFIX_MKDEPFLAG}
VCDEFS=-DNO_ISO 

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway nettest
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway nettestd
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway runnettest
	${INSTALL} -F ${GIFTSLOC} -idb 4Dgifts.src.full nettest.c
	${INSTALL} -F ${GIFTSLOC} -idb 4Dgifts.src.full nettestd.c
	${INSTALL} -F ${GIFTSLOC} -idb 4Dgifts.src.full nettest.h
	${INSTALL} -F ${GIFTSLOC} -idb 4Dgifts.src.full runnettest
