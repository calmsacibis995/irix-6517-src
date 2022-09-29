#!smake
#
# Makefile for rcp.bootparamd, the boot parameters daemon.
#
# $Revision: 1.16 $

include	$(ROOT)/usr/include/make/commondefs

LCDEFS+= -D_BSD_TIME
LLDLIBS+= -lrpcsvc
CVERSION=-cckr

CFILES= bootparam_svc.c bootparam_subr.c bootparam_lib.c
D_FILE= bootparams
TARGETS= rpc.bootparamd

default: ${TARGETS}

include	${COMMONRULES}

rpc.bootparamd: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/etc -idb nfs.sw.nfs ${TARGETS}
	${INSTALL} -idb "config(noupdate)" -u root -g sys \
		-m 644 -F /etc $(D_FILE)
