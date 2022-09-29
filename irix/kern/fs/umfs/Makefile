#!smake
# Copyright 1997 Silicon Graphics Inc.
#
# Makefile for the User Mode File System.
#
#ident "$Revision: 1.4 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/fs/umfs
.PATH:$(KPATH)

KCOPTS=	-fullwarn $(XFSCOPTS)

UMFS_NAME=umfs.a
UMFS_CSRCS=umfs_entry.c umfs_vfsops.c umfs_vnodeops.c

UMFS_OBJS=${UMFS_CSRCS:.c=.o}
$(UMFS_NAME):$(UMFS_NAME)($(UMFS_OBJS)) MAKELIB

UMFSIDBG_NAME=umfsidbg.o
UMFSIDBG_CSRCS=umfsidbg.c

TARGETS=$(UMFS_NAME) $(UMFSIDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=umfs
CFILES=$(UMFS_CSRCS) $(UMFSIDBG_CSRCS)

FRIEND_HEADERS= umfs_kern.h

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${UMFS_NAME}
	${KINSTALL} ${UMFSIDBG_NAME}
	${KDEVINSTALL} ${TARGETS}

umfsidbg.o: umfsidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/umfsidbg.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

headers!
	${INSTALL} -idb "noship" -m 444 \
		-F /usr/include/sys/fs ${FRIEND_HEADERS}
