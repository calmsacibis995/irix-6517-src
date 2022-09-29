#
# Makefile for mountdos(1M).
#
include ${ROOT}/usr/include/make/commondefs

CFILES= dos_fs.c dos_main.c dos_node.c nfs_prot.c nfs_server.c dos_util.c
HFILES= dos_fs.h nfs_prot.h

LDIRT= nfs_prot.c nfs_prot.h umountdos.o
TARGETS= mount_dos umount_dos

# For debugging, uncomment the following two lines.
#LCOPTS= -g
#LC++OPTS= -g

default: ${TARGETS}

include ${COMMONRULES}

mount_dos: ${OBJECTS}
	${CCF} ${LDLIBS} ${OBJECTS} -ldisk ${LDFLAGS} -o $@

umount_dos: umountdos.o
	${CCF} ${LDLIBS} umountdos.o ${LDFLAGS} -o $@

install: default
	${INSTALL} -F /usr/sbin -m 755 to_dos
	${INSTALL} -F /usr/sbin -m 755 to_unix
	${INSTALL} -F /usr/etc -m 755 mount_dos
	${INSTALL} -F /usr/etc -m 755 umount_dos

nfs_prot.c: nfs_prot.h nfs_prot.x
	/usr/bin/rpcgen -C "${CCF} -E" -c -o $@ nfs_prot.x

nfs_prot.h: nfs_prot.x
	/usr/bin/rpcgen -C "${CCF} -E" -h -o $@ nfs_prot.x

${OBJECTS}: nfs_prot.h
