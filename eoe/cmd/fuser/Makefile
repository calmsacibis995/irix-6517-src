#
# Makefile for fuser(1m).
#
#ident "$Revision: 1.9 $"

include	${ROOT}/usr/include/make/commondefs

CFILES=fuser.c
TARGETS=fuser
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default: ${TARGETS}

include ${COMMONRULES}

install: default
	${INSTALL} -idb "mr" -m 2755 -u root -g sys -F /sbin ${TARGETS}
	${INSTALL} -idb "mr" -F /etc -lns ../sbin/${TARGETS} ${TARGETS}
