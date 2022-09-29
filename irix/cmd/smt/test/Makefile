#!smake
#
# Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for standard SMT applications
#
#
DEPTH=..
include ${DEPTH}/Makedefs

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

LLDLIBS	= ${LIBSMT}

CFILES	=conf.c mib.c parse.c \
	smtgc.c smtvers.c \
	smtget.c smtgetnext.c smtwalk.c \
	setmaddr.c

TARGETS	=${CFILES:.c=}

default: ${TARGETS}

include ${COMMONRULES}

# Do not ship these files.  
# IMPORTANT:  Do not ship setmaddr.
install: default

${TARGETS}:$$@.c ${LIBSMT}

smtgc: $$@.c ${LIBSMT}
	${CCF} $@.c ${LDOPTS} -lcurses ${LDLIBS} -o $@

afluff:
	for d in ${TARGETS}; do \
		lint ${CFLAGS} $$d.c -lsmt -lc ; \
	done
