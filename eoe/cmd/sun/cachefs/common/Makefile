#!smake
#
# Makefile for cachefs common functions
#
include ${ROOT}/usr/include/make/commondefs

CVERSION= -xansi

CFILES= subr.c
COMMONOBJ= subr.o

LCINCS= -I.
LINTFLAGS= -hbax

default: ${COMMONOBJ}

include ${COMMONRULES}

install: ${COMMONOBJ}
