#!smake
# Copyright 1989 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile to make all SGI file systems.
#
#ident	"$Revision: 3.28 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/fs
.PATH:$(KPATH)

SUBDIRS=autofs cachefs dfs efs fdfs fifofs hwgfs lofs pipefs namefs nfs \
	procfs specfs xfs umfs
HEADERS_SUBDIRS=autofs cachefs dfs efs fifofs pipefs namefs nfs lofs \
	procfs specfs xfs umfs

#if	defined(CELL_IRIX) || defined(CELL_ARRAY)
SUBDIRS+=cfs
#endif

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	${HEADERS_SUBDIRS_MAKERULE}
