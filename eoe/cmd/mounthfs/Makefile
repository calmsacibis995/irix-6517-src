#
# Makefile for mounthfs(1M).
#
include $(ROOT)/usr/include/make/commondefs

CVERSION=-xansi -woff 1164 -woff 1185 -woff 1516
CFILES= hfs_main.c\
	nfs_server.c\
	hfs_ar.c\
	hfs_bit.c\
	hfs_io.c\
	hfs_fs.c\
	hfs_bt.c\
	hfs_dir.c\
	hfs_fr.c\
	hfs_hfs.c\
	hfs_hl.c\
	hfs_er.c\
	hfs_hn.c\
	nfs_prot.c\
	cdrom.c
HFILES= hfs.h\
	ancillary.h\
	hfs_ar.h\
	hfs_bit.h\
	hfs_io.h\
	hfs_fs.h\
	hfs_bt.h\
	hfs_dir.h\
	hfs_fr.h\
	hfs_hfs.h\
	hfs_hl.h\
	hfs_er.h\
	hfs_hn.h\
	nfs_prot.h\
	cdrom.h

C++FILES= stress.c++ util.c++

# For debugging, uncomment the following two lines.
# LCOPTS= -g
# LC++OPTS= -g

# To turn off tracing uncomment the following:
#LCDEFS=-NOTRACE

LDIRT= nfs_prot.c nfs_prot.h umounthfs.o
TARGETS= mount_hfs umount_hfs stress util

default: $(TARGETS)

include $(COMMONRULES)

mount_hfs: $(CFILES:c=o)
	$(CCF) $(CFILES:c=o) -ldisk -lds $(LDFLAGS) -o $@

umount_hfs: umounthfs.o
	$(CCF) umounthfs.o $(LDFLAGS) -o $@

stress: stress.o
	$(C++F) stress.o $(LDFLAGS) -o $@ 

util:   util.o
	$(C++F) util.o $(LDFLAGS) -lgen -o $@ 

install: default
	$(INSTALL) -F /usr/etc -m 755 mount_hfs
	$(INSTALL) -F /usr/etc -m 755 umount_hfs

nfs_prot.c: nfs_prot.h nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "$(CCF) -E" -c -o $@ nfs_prot.x

nfs_prot.h: nfs_prot.x
	${TOOLROOTSAFE} $(TOOLROOT)/usr/bin/rpcgen -C "$(CCF) -E" -h -o $@ nfs_prot.x

$(CFILES:c=o): nfs_prot.h















