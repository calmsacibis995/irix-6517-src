#!smake
#
# Makefile for umount_cachefs(1M).
#
include ${ROOT}/usr/include/make/commondefs

CFILES= umount.c
COMMONOBJ= ../common/subr.o

#LLDOPTS= -Wl,-T,10000000,-D,10040000
LCINCS= -I../common
LCOPTS= -g
LINTFLAGS= -hbax

TARGETS= umount

default: ${TARGETS}

include ${COMMONRULES}

umount: ${OBJECTS} ${COMMONOBJ}
	${CCF} ${OBJECTS} ${COMMONOBJ} ${LDFLAGS} -o $@

install: default
	${INSTALL} -idb onc3.sw.cachefs -src umount -F /usr/etc umount_cachefs

