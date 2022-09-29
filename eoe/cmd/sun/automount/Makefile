#!smake
#
# Makefile for automount(1M).
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

CFILES= nfs_prot.c nfs_server.c nfs_trace.c many_cast.c \
	auto_main.c auto_look.c auto_proc.c auto_node.c \
	auto_mount.c auto_all.c nfs_cast.c
HFILES= automount.h

LLDLIBS= -lrpcsvc
LLDOPTS= -Wl,-T,10000000,-D,10040000
LCDEFS= -D_BSD_SIGNALS -D_BSD_TIME
LINTFLAGS= -hbax

TARGETS= automount
LDIRT= nfs_prot.c nfs_prot.h

default: ${TARGETS}

include ${COMMONRULES}

automount: ${OBJECTS}
	${CCF} ${OBJECTS} ${LDFLAGS} -o $@

$(OBJECTS): nfs_prot.h

nfs_prot.c: nfs_prot.x nfs_prot.h
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ nfs_prot.x

nfs_prot.h: nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ nfs_prot.x

install: default
	${INSTALL} -F /usr/etc automount
	${INSTALL} -F /etc/config -m 444 -idb "config(noupdate)" \
		automount.options

