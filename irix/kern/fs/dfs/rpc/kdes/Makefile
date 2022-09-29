#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
# src directory for the full copyright text.
#
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.7  1999/03/02 21:09:33  gwehrman
# Define HFILES to include des.h.  Add a rule to install
# this file in /usr/include/dce/ker.  This file is needed to build the
# encryption library in the DFS product.
#
# Revision 65.6  1999/02/04 22:37:19  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.5  1998/02/27 17:32:05  gwehrman
# Changed definition of EXT_LIBDIRS to use KLIBTYPE
# LIBTYPE does not work because a 64 bit kernel may be build during a 32 bit
# user space compile.  Defined TARGET_FLAGSS to prevent use of user space
# flags in a kernel build.
#
# Revision 65.4  1998/02/27 16:16:04  gwehrman
# Undefined EXPORT_USING_SYMLINKS since the exported files get moved around
# by the makeall script.  Replaced the multiple definition of EXT_LIBDIRS with
# a single definition that uses LIBTYPE and MIPS_VERSION.
#
# Revision 65.3  1998/02/26 23:47:39  lmc
# Deleted setting of TARGET_LDFLAGS because they are already set in
# SGIMIPS/machdep.mk.
#
# Revision 65.2  1997/10/20  19:16:12  jdoak
# Initial IRIX 6.4 code merge
#
# Revision 64.2  1997/03/31  15:37:04  lord
# Make debug compile flags globally switchable
#
# Revision 64.1  1997/02/14  19:45:09  dce
# *** empty log message ***
#
# Revision 1.4  1996/09/11  16:58:24  bhim
# Modified for new build procedure.
#
# Revision 1.3  1996/08/27  15:48:18  bhim
# Modified for new build procedure.
#
# Revision 1.2  1996/05/25  16:44:14  brat
# Modified stuff to build kernel-related stuff to be 32-bit or 64-bit and the
# rest to be n32. This way we have one build instead of multiple builds.
#
# Revision 1.1  1993/12/15  18:33:35  dcebuild
# Original File from OSF
#
# Revision 1.2.4.3  1993/01/03  22:34:26  bbelch
# 	Embedding copyright notice
# 	[1993/01/03  19:49:24  bbelch]
#
# Revision 1.2.4.2  1992/12/23  19:34:57  zeliff
# 	Embedding copyright notice
# 	[1992/12/23  01:15:00  zeliff]
# 
# Revision 1.2.2.2  1992/05/01  17:35:54  rsalz
# 	  27-apr-92 nm    Reformat & remove KERNEL_CC
# 	[1992/05/01  17:33:38  rsalz]
# 
# Revision 1.2  1992/01/19  22:13:29  devrcs
# 	Dropping DCE1.0 OSF1_misc port archive
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.7 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc/kdes
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include \
		-I$(ROOT)/usr/include/dcedfs

OBJ_NAME=	libkdes.a
OBJ_CSRCS=	des_neuter.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		des.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dce/ker $(HFILES)

