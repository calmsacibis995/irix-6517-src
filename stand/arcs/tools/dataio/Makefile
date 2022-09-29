#
# Makefile for nprom (DATA I/O PROM/PAL programmer interface tool).
#
include ${ROOT}/usr/include/make/commondefs
MKDEPFILE= Makedepend

INSTIDB= ${INSTALL} -idb "noship.prom"
INSTDIR=/usr/local/bin/dataio

CFILES= nprom.c autolabel.c small_label.c
TARGETS= nprom autolabel small_label
LCFLAGS=-DSVR3

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTIDB} -F ${INSTDIR} ${TARGETS}
	${INSTIDB} -F ${INSTDIR} -m 444 devicecodes
