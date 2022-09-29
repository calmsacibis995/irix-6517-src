#!smake
#
# Makefile for autofs(1M).
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

# NAMESVC= ns_nis.c
NAMESVC= ns_uns.c

CAUTOFS= autofs.c 
CAUTOFSD= autod_main.c autod_mount.c autod_nfs.c autod_xdr.c nfs_cast.c
CMOUNTAUTOFS= mount.c
CAUTOCOMMON= auto_mtab.c auto_subr.c ns_files.c ns_generic.c $(NAMESVC)

HFILES= autofs.h autofsd.h
CFILES= ${CAUTOFS} ${CAUTOFSD} ${CMOUNTAUTOFS} ${CAUTOCOMMON}

AUTOCOMMON= ${CAUTOCOMMON:.c=.o}
AUTOFS= ${CAUTOFS:.c=.o} ${AUTOCOMMON}
AUTOFSD= ${CAUTOFSD:.c=.o} ${AUTOCOMMON}
MOUNTAUTOFS= ${CMOUNTAUTOFS:.c=.o}

LLDLIBS= -lrpcsvc
LLDOPTS= -Wl,-T,10000000,-D,10040000
LCDEFS= -D_BSD_SIGNALS -D_BSD_TIME -g -DUSE_UNS
LINTFLAGS= -hbax

TARGETS= autofs autofsd mount_autofs
LDIRT= autofs_prot.c autofs_prot.h autod_xdr.c nfs_prot.h 

default: ${TARGETS}

include ${COMMONRULES}

OPTIMIZER = -g

autofs: ${AUTOFS}
	${CCF} ${AUTOFS} ${LDFLAGS} -o $@

autofsd: ${AUTOFSD}
	${CCF} ${AUTOFSD} ${LDFLAGS} -o $@

mount_autofs: ${MOUNTAUTOFS}
	${CCF} ${MOUNTAUTOFS} ${LDFLAGS} -o $@

$(AUTOFS): autofsd.h
$(AUTOFSD): autofsd.h autofs_prot.h nfs_prot.h

nfs_prot.h: nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ nfs_prot.x

autofs_prot.h: autofs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ autofs_prot.x

autod_xdr.c: autofs_prot.x autofs_prot.h
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ autofs_prot.x

install: default
	${INSTALL} -F /usr/etc autofs
	${INSTALL} -F /usr/etc mount_autofs
	${INSTALL} -F /usr/etc autofsd
	${INSTALL} -F /etc -m 664 -idb 'config(noupdate)' auto_master
	${INSTALL} -F /etc/config -m 444 -idb 'config(suggest)' \
		autofs.options
