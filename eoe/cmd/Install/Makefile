#
# Copyright 1988 Silicon Graphics, Inc. All rights reserved.
#
# Makefile for install.
#
include	${ROOT}/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

CFILES= Install.c
TARGETS= Install

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /sbin -O -src Install install
	${INSTALL} -F /etc -lns ../sbin/install install
