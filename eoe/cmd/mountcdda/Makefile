#!smake
# Makefile for mountcd(1M).
#
include ${ROOT}/usr/include/make/commondefs

CFILES= player.c cdda.c  main.c nfs_prot.c nfs_server.c remote.c \
	testcd_prot.c util.c 

HFILES= cdda.h iso_types.h nfs_prot.h util.h remote.h main.h

MOUNT_LLDLIBS=  -lds 

#
# By popular demand, include a very gross hack to read messed
# up defense mapping agency discs
#
#LCDEFS=-DWEIRD_DMA_DISC
LDIRT= nfs_prot.c nfs_prot.h testcd_prot.c testcd_prot.h
TARGETS= mount_cdda umount_cdda

default: ${TARGETS}

include ${COMMONRULES}

mount_cdda: ${OBJECTS}
	rm -f $@
	${CCF} ${OBJECTS} ${LDFLAGS} ${MOUNT_LLDLIBS} -o $@

umount_cdda: umountcd.o
	rm -f $@
	${CCF} umountcd.o ${LDFLAGS} -o $@

install: default
	${INSTALL} -m 755 -F /usr/etc mount_cdda
	${INSTALL} -m 755 -F /usr/etc umount_cdda

nfs_prot.h: nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ nfs_prot.x

nfs_prot.c: nfs_prot.h nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ nfs_prot.x

testcd_prot.h: testcd_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ testcd_prot.x

testcd_prot.c: testcd_prot.h testcd_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ testcd_prot.x

${OBJECTS}: nfs_prot.h testcd_prot.h
