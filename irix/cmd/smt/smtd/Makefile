#!smake
#
# Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for SMT daemon. 
#
#
DEPTH=..
include ${DEPTH}/Makedefs

LLDLIBS=${LIBSMT}
LCDEFS+=${SMTVERS}

TARGETS=smtd
CFILES=	smtd.c smtd_svc.c smtd_snmp.c smtd_conf.c \
	smtd_fs.c smtd_nn.c smtd_event.c smtd_rmt.c

default:${TARGETS}

include ${COMMONRULES}

smtd: ${OBJECTS} ${LIBSMT}
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	${INSTALL} -F /usr/etc -idb fddi.sw.fddi -u root -g sys ${TARGETS}

# recompile if the version number changes
smtd_conf.o:${DEPTH}/Makedefs
