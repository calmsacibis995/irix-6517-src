#!smake
#
# Makefile for cfsadmin(1M).
#
include ${ROOT}/usr/include/make/commondefs

CFILES= cfsadmin.c
COMMONOBJ= ../common/subr.o

#LLDOPTS= -Wl,-T,10000000,-D,10040000
LCINCS= -I../common
LINTFLAGS= -hbax

TARGETS= cfsadmin

default: ${TARGETS}

include ${COMMONRULES}

cfsadmin: ${OBJECTS} ${COMMONOBJ}
	${CCF} ${OBJECTS} ${COMMONOBJ} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/etc -idb onc3.sw.cachefs cfsadmin

