#!smake
#
# Makefile for cachefs dev/test tools
#
include ${ROOT}/usr/include/make/commondefs

CFILES= prtheader.c prtdir.c cfsstat.c
COMMONOBJ= ../common/subr.o

#LLDOPTS= -Wl,-T,10000000,-D,10040000 -g
LCOPTS= -g
LCINCS= -I../common
LINTFLAGS= -hbax

TARGETS= prtheader prtdir cfsstat

default: ${TARGETS}

include ${COMMONRULES}

prtheader prtdir: $@.o ${COMMONOBJ}
	${CCF} ${LDFLAGS} -o $@ $@.o ${COMMONOBJ}

cfsstat: $@.o
	${CCF} ${LDFLAGS} -o $@ $@.o -lcurses

install: default
	${INSTALL} -F /usr/etc -idb onc3.sw.cachefs cfsstat

