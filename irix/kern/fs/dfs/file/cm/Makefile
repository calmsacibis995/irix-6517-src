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
# Revision 65.14  1999/03/23 22:57:00  gwehrman
# Added special rules for cm_idgb.c and dfs_icrash.c so that they are compiled
# with symbols.  Fix for 680479.
#
# Revision 65.13  1999/02/23 15:24:09  gwehrman
# Added cm.h to HFILES for exporting.
#
# Revision 65.12  1999/02/04 19:19:37  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.11  1998/09/22 20:31:59  gwehrman
# Added cm_idbg.h to INCLUDES.
#
# Revision 65.10  1998/07/15 16:36:25  bdr
# Add additional idbg lib target.  This is a dyamicly loadable kernel
# module that contains the DFS cm idbg commands.
#
# Revision 65.9  1998/05/06  19:00:24  lmc
# Add dfs_icrash.o to makefile.
#
# Revision 65.8  1998/05/06  17:35:36  gwehrman
# Changed cm_idbg0.c to cm_idbg0.o in OFILES definiton.
#
# Revision 65.7  1998/05/01  21:59:57  tdm
# Added dfszcm.cat.m to ILIST.  Added dfszcm.cat.m_IDIR.
#
# Revision 65.6  1998/03/31  18:07:48  lmc
# Add support for idbg commands used to print the cm_scache
# and cm_dcache structures.
#
# Revision 65.5  1998/03/03  15:51:36  gwehrman
# Added CPUBOARD to libcm.a_EXPDIR definition.
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.4  1998/02/26 20:54:32  lmc
# Remove the setting of TARGET_FLAGS to 32 or 64 bit.  It gets these
# from the machdep.mk file.
#
# Revision 65.1  1997/10/20  19:17:21  jdoak
# *** empty log message ***
#
# Revision 1.1.73.1  1996/10/02  17:06:32  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:05:36  damon]
#
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.14 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/cm
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

CM_NAME=	libcm.a
CM_CSRCS=	cm_aclent.c \
		cm_async.c \
		cm_bkg.c \
		cm_cell.c \
		cm_conn.c \
		cm_daemons.c \
		cm_dnamehash.c \
		cm_dcache.c \
		cm_init.c \
		cm_lockf.c \
		cm_pcache.c \
		cm_pioctl.c \
		cm_rrequest.c \
		cm_scache.c \
		cm_server.c \
		cm_serverpref.c \
		cm_shutdown.c \
		cm_site.c \
		cm_subr.c \
		cm_tknimp.c \
		cm_tokens.c \
		cm_vdirent.c \
		cm_vcache.c \
		cm_vfsops.c \
		cm_vnodeops.c \
		cm_volume.c \
		cm_xsyscalls.c \
		dfs_icrash.c
CM_OBJS=	$(CM_CSRCS:.c=.o)
$(CM_NAME):$(CM_NAME)($(CM_OBJS)) MAKELIB

IDBG_NAME=	cm_idbg.a
IDBG_CSRCS=	cm_idbg.c
IDBG_OBJS=	$(IDBG_CSRCS:.c=.o)
$(IDBG_NAME):$(IDBG_NAME)($(IDBG_OBJS)) MAKELIB

TARGETS=	$(CM_NAME) $(IDBG_NAME)
CFILES=		$(CM_CSRCS) $(IDBG_CSRCS)

HFILES=		cm.h \
		cm_chunk.h \
		cm_idbg.h \
		cm_serverpref.h \
		cm_stats.h \
		cm_sysname.h \
		cm_trace.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

cm_idbg.o: cm_idbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/cm_idbg.c && \
	$(LDF) $(PRODOBJECT) -G 0 -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

dfs_icrash.o: dfs_icrash.c
	$(CCF) -g3 -c $(JALR) $(KPATH)/dfs_icrash.c && \
	$(LDF) $(PRODOBJECT) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
