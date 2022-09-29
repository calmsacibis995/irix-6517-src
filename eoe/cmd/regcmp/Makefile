#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)regcmp:regcmp.mk	1.6"
#	regcmp make file
include	${ROOT}/usr/include/make/commondefs

LLDLIBS= -lgen
CFILES= regcmp.c
TARGETS= ${CFILES:.c=}

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/bin "${TARGETS}"
