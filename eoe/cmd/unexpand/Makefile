#
# Makefile for unexpand
#

include	${ROOT}/usr/include/make/commondefs

CFILES=unexpand.c
TARGETS=$(CFILES:.c=)

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -m 755 -u root -g sys -F usr/bin ${TARGETS}
