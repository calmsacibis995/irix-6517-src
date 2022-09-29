#!smake
#
# Makefile for mount_cachefs(1M).
#
include ${ROOT}/usr/include/make/commondefs

CFILES= mount.c
COMMONOBJ= ../common/subr.o

#LLDOPTS= -Wl,-T,10000000,-D,10040000 -g
LCOPTS= -g
LCINCS= -I../common
LINTFLAGS= -hbax

TARGETS= mount

default: ${TARGETS}

include ${COMMONRULES}

mount: ${OBJECTS} ${COMMONOBJ}
	${CCF} ${OBJECTS} ${COMMONOBJ} ${LDFLAGS} -o $@

install: default
	${INSTALL} -idb onc3.sw.cachefs -src mount -F /usr/etc mount_cachefs

