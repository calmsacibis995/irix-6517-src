#!smake
#
# Makefile for the rpc.statd subdirectory
#
# $Revision: 1.13 $

include	${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

LCDEFS= -D_BSD_SIGNALS
LLDLIBS= -lrpcsvc

HFILES= sm_statd.h
CFILES= sm_svc.c sm_proc.c sm_statd.c

TARGETS= rpc.statd

default: ${TARGETS}

include	${COMMONRULES}

rpc.statd: ${OBJECTS}
	${CCF} ${OBJECTS} ${LCDEFS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -m 755 -F /usr/etc ${TARGETS}
	${INSTALL} -m 755 -F /var/statmon -src rpc.statd.inst.sh \
		-idb "nohist" -idb 'postop($$rbase/var/statmon/.rpc.statd.inst.sh)' \
		.rpc.statd.inst.sh
