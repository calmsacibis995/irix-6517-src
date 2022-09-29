#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/false/RCS/Makefile,v 1.12 1993/07/16 23:30:35 raghav Exp $"
#
# Makefile for /bin/false and its links.
#
include	${ROOT}/usr/include/make/commondefs

SHFILES= false.sh
LINKS= u3b u3b2 u3b5 u3b15 vax pdp11 m68000 m68k
TARGETS= false ${LINKS}

default: false

include ${COMMONRULES}

${LINKS}: false

install: default
	${INSTALL} -F /usr/bin ${IDBTAG} false
	${INSTALL} -lns false -F /usr/bin ${IDBTAG} "${LINKS}"
