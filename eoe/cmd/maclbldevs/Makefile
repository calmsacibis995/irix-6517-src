#
# Makefile 
#
#ident	"$Revision: 1.1 $"

include	${ROOT}/usr/include/make/commondefs

LLDLIBS=
LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

CFILES= maclbldevs.c
TARGETS= maclbldevs

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/sbin maclbldevs
