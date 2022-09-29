# Makefile for installing configuration file.
#
#	"$Revision: 1.5 $"

include ../commondefs

FILE	=rfindd

IDBTAG	=-idb "config(noupdate)"

INST	= ${INSTALL} -F /etc/config -m 644

default: ${FILE}

include ../commonrules

install: default
	${INST} ${IDBTAG}    ${FILE}
