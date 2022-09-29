#!smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.3  1999/02/04 19:19:43  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.2  1997/12/16 17:05:41  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:20:14  jdoak
# *** empty log message ***
#
# Revision 1.1.931.1  1996/10/02  18:15:10  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:46:23  damon]
#
# Revision 1.1.926.5  1994/08/23  18:58:51  rsarbo
# 	delegation
# 	[1994/08/23  16:31:31  rsarbo]
# 
# Revision 1.1.926.4  1994/07/13  22:26:22  devsrc
# 	merged with bl-10
# 	[1994/06/28  21:11:10  devsrc]
# 
# 	delegation ACL support
# 	[1994/06/07  14:04:50  delgado]
# 
# Revision 1.1.926.3  1994/06/09  14:18:27  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:30:29  annie]
# 
# Revision 1.1.926.2  1994/02/04  20:28:13  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:17:15  devsrc]
# 
# Revision 1.1.925.1  1993/12/07  17:32:28  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  16:42:53  jaffe]
# 
# Revision 1.1.6.5  1993/01/21  15:16:32  cjd
# 	embedded copyright notice
# 	[1993/01/20  15:52:31  cjd]
# 
# Revision 1.1.6.4  1992/11/24  18:27:10  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:19:41  bolinger]
# 
# Revision 1.1.6.3  1992/09/15  13:18:10  jaffe
# 	Transarc delta: jaffe-ot5166-dont-install-testacl.o 1.1
# 	  Selected comments:
# 	    don't install testacl.o.
# 	    put dfsdcl.cat explicitly into the ILIST instead of using OTHERS.
# 	[1992/09/14  20:31:59  jaffe]
# 
# Revision 1.1.6.2  1992/08/31  21:01:01  jaffe
# 	no change
# 	[1992/08/30  04:17:58  jaffe]
# 
# Revision 1.1.3.14  1992/05/27  16:44:22  mason
# 	OT 3833 - fix mismatches between Transarc and OSF source.
# 	[1992/05/27  16:08:37  mason]
# 
# Revision 1.1.3.13  1992/05/20  20:03:48  mason
# 	Transarc delta: cfe-ot3085-fix-msf-dependencies 1.1
# 	  Files modified:
# 	    bak: Makefile; bakserver: Makefile; bosserver: Makefile
# 	    bubasics: Makefile; flserver: Makefile
# 	    flserver.klib: Makefile; fshost: Makefile; fsint: Makefile
# 	    ftserver: Makefile; host: Makefile; ncscompat: Makefile
# 	    ncsubik: Makefile; security/dacl: Makefile
# 	    security/dfsauth: Makefile; sysacl: Makefile; tkm: Makefile
# 	    tools/cmd: Makefile; update: Makefile; xcred: Makefile
# 	    xvolume: Makefile
# 	  Selected comments:
# 	    Make explicit the dependency that builds an .msf file from an .et file.
# 	    Add dependency to build .msf file.
# 	[1992/05/20  11:56:59  mason]
# 
# Revision 1.1.3.12  1992/05/06  23:48:47  jdp
# 	Added "dacl_default.o" to libdacl.a_OFILES.
# 	[1992/05/06  23:48:05  jdp]
# 
# Revision 1.1.3.11  1992/04/21  14:27:29  mason
# 	Transarc delta: bab-ot2379-dacl-build-testacl 1.2
# 	  Files modified:
# 	    security/dacl: Makefile, testacl.c
# 	    security/dacl_lfs: Makefile
# 	  Selected comments:
# 	    Get the user-space testing programs built for the dacl package.
# 	    Get testacl.o built in this directory (w/o the Episode flags), for testacl.
# 	    Error table initialization needed to be removed from testacl.c
# 	[1992/04/20  22:57:23  mason]
# 
# Revision 1.1.3.10  1992/04/17  19:03:17  mason
# 	*** empty log message ***
# 
# Revision 1.1.3.9  1992/04/16  17:19:38  garyf
# 	fix .p.h rule
# 	[1992/04/16  17:16:16  garyf]
# 
# Revision 1.1.3.8  1992/04/15  19:34:54  garyf
# 	auth5b definitions no longer needed
# 	let AFS_DEBUG be controlled by normal mechanisms
# 	[1992/04/15  19:34:23  garyf]
# 
# Revision 1.1.3.7  1992/04/15  16:10:22  mason
# 	Added testacl.o to the OTHERS line so it gets built.
# 	[1992/04/15  16:09:08  mason]
# 
# Revision 1.1.3.6  1992/03/27  21:53:47  toml
# 	Add -Dauth5b -D_AUTH5b_ to $CFLAGS
# 	[1992/03/27  21:53:27  toml]
# 
# Revision 1.1.3.5  1992/01/31  14:52:35  delgado
# 	Add modifications to build libdcedfs.a
# 	[1992/01/31  14:52:15  delgado]
# 
# Revision 1.1.3.4  1992/01/27  22:37:53  treff
# 	1/26/92 DCT @ OSF
# 	RE-add AUTH compile_et changes
# 	[1992/01/27  22:37:26  treff]
# 
# Revision 1.1.3.3  1992/01/24  03:48:03  devsrc
# 	Merging dfs6.3
# 	[1992/01/24  00:18:36  devsrc]
# 
# Revision 1.1.3.2  1992/01/23  20:22:28  zeliff
# 	Moving files onto the branch for dfs6.3 merge
# 	[1992/01/23  18:38:35  zeliff]
# 	Revision 1.1.1.3  1992/01/23  22:21:04  devsrc
# 	Fixed logs
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.3 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/security/dacl
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libkdacl.a
OBJ_CSRCS=	dacl_debug.c \
		dacl_access.c \
		dacl_pac.c \
		dacl_flatten.c \
		dacl_manager.c \
		dacl_mgruuids.c \
		dacl_parse.c \
		dacl_permbits.c \
		dacl_strings.c \
		dacl_default.c \
		dacl_getmode.c \
		dacl_uuidxlat.c \
		dacl_validate.c \
		dacl_init.c \
		dacl_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		dacl.h dacl_mgruuids.h epi_id.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

