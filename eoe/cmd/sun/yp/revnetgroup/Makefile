#!smake
#
# cmd/sun/yp/revnetgroup makefile (reverse a netgroup!).
#
include	${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

HFILES= getgroup.h table.h util.h
CFILES= revnetgroup.c getgroup.c table.c util.c
TARGETS= revnetgroup

default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/sbin ${TARGETS}
