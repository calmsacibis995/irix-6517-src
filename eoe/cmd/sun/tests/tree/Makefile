#
# Makefile for directory tree tests
#
include	${ROOT}/usr/include/make/commondefs

LCOPTS = -g

TARGETS= checktree maketree exercise walktree

LLDLIBS=

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/local/bin checktree
	${INSTALL} -F /usr/local/bin maketree
	${INSTALL} -F /usr/local/bin exercise
	${INSTALL} -F /usr/local/bin walktree

${TARGETS}:	tests.h subr.o
	$(CCF) $(CFLAGS) $(LDFLAGS) -o $@ $@.c subr.o $(LDLIBS)
