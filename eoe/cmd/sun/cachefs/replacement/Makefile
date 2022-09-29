#!smake
#
# Makefile for cfsadmin(1M).
#
include ${ROOT}/usr/include/make/commondefs

CFILES= replacement.c
COMMONOBJ= ../common/subr.o

#LLDOPTS= -Wl,-T,10000000,-D,10046000
LCINCS= -I../common
LINTFLAGS= -hbax

TARGETS= replacement

default: ${TARGETS}

include ${COMMONRULES}

replacement: ${OBJECTS} ${COMMONOBJ}
	${CCF} ${OBJECTS} ${COMMONOBJ} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/etc -idb onc3.sw.cachefs -src replacement cachefs_replacement

