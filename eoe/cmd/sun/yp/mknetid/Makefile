#
# Makefile for /usr/sbin/mknetid
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr
CFILES= mknetid.c getname.c
TARGETS= mknetid

default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/sbin mknetid
	${INSTALL} -F /etc -idb "config(noupdate)" -m 644 netid
