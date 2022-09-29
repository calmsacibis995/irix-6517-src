#
# Makefile for ttcp, a network performance analysis tool from BRL.
#
#ident $Revision: 1.12 $

DEPTH=..
include ${ROOT}/usr/include/make/commondefs

CFILES= ttcp.c
TARGETS= ttcp
GIFTSLOC= /usr/people/4Dgifts/examples/network

LCDEFS= -D_BSD_TIME

LMKDEPFLAGS= ${NULLSUFFIX_MKDEPFLAG}

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway ttcp
	${INSTALL} -F /usr/etc -idb std.sw.ipgateway runttcp
	${INSTALL} -F ${GIFTSLOC} -idb 4Dgifts.src.full ttcp.c
	${INSTALL} -F ${GIFTSLOC} -idb 4Dgifts.src.full runttcp
