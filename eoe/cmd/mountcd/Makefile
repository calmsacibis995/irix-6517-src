#!smake
# Makefile for mountcd(1M).
#
include ${ROOT}/usr/include/make/commondefs

CFILES=cdrom.c iso.c main.c nfs_prot.c nfs_server.c remote.c \
	testcd_prot.c util.c
HFILES=cdrom.h iso.h iso_types.h nfs_prot.h util.h remote.h main.h

MOUNT_LLDLIBS= -ldisk -lds

#
# By popular demand, include a very gross hack to read messed
# up defense mapping agency discs
#
LCDEFS=-DWEIRD_DMA_DISC
LDIRT= nfs_prot.c nfs_prot.h testcd_prot.c testcd_prot.h
TARGETS= mount_iso9660 umount_iso9660

default: ${TARGETS}

include ${COMMONRULES}

mount_iso9660: ${OBJECTS}
	rm -f $@
	${CCF} ${OBJECTS} ${LDFLAGS} ${MOUNT_LLDLIBS} -o $@

umount_iso9660: umountcd.o
	rm -f $@
	${CCF} umountcd.o ${LDFLAGS} -o $@

install: default
	${INSTALL} -m 755 -F /usr/etc mount_iso9660
	${INSTALL} -m 755 -F /usr/etc umount_iso9660
	${INSTALL} -lns mount_iso9660 -F /usr/etc mount_cdfs
	${INSTALL} -lns umount_iso9660 -F /usr/etc umount_cdfs

nfs_prot.c: nfs_prot.h nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ nfs_prot.x

nfs_prot.h: nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ nfs_prot.x

testcd_prot.c: testcd_prot.h testcd_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ testcd_prot.x

testcd_prot.h: testcd_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ testcd_prot.x

${OBJECTS}: nfs_prot.h testcd_prot.h
