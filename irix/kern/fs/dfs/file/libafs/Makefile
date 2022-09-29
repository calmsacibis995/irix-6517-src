#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.6  1999/05/21 18:53:49  lauriec
# Add new library libknckstub.a, so that when the .o's are deleted after the
# extraction, the library remains so that the .c files aren't rebuilt.
#
# Revision 65.5  1999/05/20 14:13:41  lauriec
# Remove .o's after extracting them from the archive.
# After removing the .o's from the archives and archiving them into
# new libraries, delete all the .o's.  When left, the timestamps were
# more recent than the .a's they were extracted from, causing a rebuild
# of the libraries each time.  Also, the tkc.o can get picked up in
# a cellular build instead of the cellular tkc.o.
#
# Revision 65.4  1999/02/23 15:24:09  gwehrman
# Removed libkdes.a from KRPCH_LIBS.  This library now goes into krpchd.a.
# The stubs will go into krpchdstubs.a
#
# Revision 65.3  1999/02/04 22:37:18  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:20:39  jdoak
# *** empty log message ***
#
# Revision 1.1.741.1  1996/10/02  17:53:46  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:41:51  damon]
#
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.6 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/libafs
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

CFILES=		px_config.c cm_config.c dfs_idbg.c

OBJS=		$(CFILES:.c=.o)

DFSIDBG_OBJ=	dfsidbg.a
DFSIDBG_FILES=	dfs_idbg.o
DFSIDBG_LIBS=	cm_idbg.a icl_idbg.a

KRPCH_OBJ=	krpch.a
KRPCH_FILES=	
KRPCH_LIBS=	libknck.a libknckstub.a libkidl.a libksec.a

DFS_OBJ=	dfs.a
DFS_FILES=	px_config.o cm_config.o
DFS_LIBS=	libcm.a \
		libkosi.a \
		libkcommondata.a \
		libkdacl.a \
		libkicl.a \
		libkdfsncs.a \
		libkdfskutil.a \
		libkafsutil.a \
		libktkm.a \
		libktkc.a \
		libkfldb.a \
		libkrepcli.a \
		libkafs4clt.a \
		libkafs4srv.a \
		libktpq.a \
		libhost.a \
		libvolume.a \
		libktkset.a \
		libkzlc.a \
		libaggr.a \
		libvolreg.a \
		libkxcred.a \
		libufsops.a \
		libxvnode.a \
		libkolddacl.a \
		libkbomb.a \
		libpx.a

TARGETS=	$(DFSIDBG_OBJ) $(KRPCH_OBJ) $(DFS_OBJ)

$(DFSIDBG_OBJ): $(DFSIDBG_FILES) $(DFSIDBG_LIBS)
	@rm -f $(DFSIDBG_OBJ); \
	echo "Archiving $(DFSIDBG_OBJ)."; \
	ar -cr $(DFSIDBG_OBJ) $(DFSIDBG_FILES); \
	for archive in $(DFSIDBG_LIBS); do \
		objs=`ar -t $$archive`; \
		ar -x $$archive; \
		ar -qc $(DFSIDBG_OBJ) $$objs; \
		rm $$objs;		\
	done

$(KRPCH_OBJ): $(KRPCH_FILES) $(KRPCH_LIBS)
	@rm -f $(KRPCH_OBJ); \
	echo "Archiving $(KRPCH_OBJ)."; \
	ar -cr $(KRPCH_OBJ) $(KRPCH_FILES); \
	for archive in $(KRPCH_LIBS); do \
		objs=`ar -t $$archive`; \
		ar -x $$archive; \
		ar -qc $(KRPCH_OBJ) $$objs; \
		rm $$objs;		\
	done

$(DFS_OBJ): $(DFS_FILES) $(DFS_LIBS)
	@rm -f $(DFS_OBJ); \
	echo "Archiving $(DFS_OBJ)."; \
	ar -cr $(DFS_OBJ) $(DFS_FILES); \
	for archive in $(DFS_LIBS); do \
		objs=`ar -t $$archive`; \
		ar -x $$archive; \
		ar -qc $(DFS_OBJ) $$objs; \
		rm $$objs;		\
	done

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(OBJS) $(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install:$(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}

headers!

