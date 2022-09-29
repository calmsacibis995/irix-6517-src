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
# Revision 1.1.11.1  1996/10/02  18:15:07  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:46:17  damon]
#
# Revision 1.1.6.1  1994/06/09  14:18:24  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:30:26  annie]
# 
# Revision 1.1.4.3  1993/01/28  18:13:00  shl
# 	Transarc delta: jaffe-ot6338-cleanup-passes-on-dfs-makefiles 1.2
# 	  Selected comments:
# 	    By being more carefull about what we do when, we can reduce the compile
# 	    time on the DFS component.
# 	    Clean out all subdirs that were noops.
# 	[1993/01/27  19:41:24  shl]
# 
# Revision 1.1.4.2  1993/01/21  15:16:23  cjd
# 	embedded copyright notice
# 	[1993/01/20  15:52:20  cjd]
# 
# Revision 1.1.2.3  1992/06/08  22:46:10  mason
# 	Transarc delta: mason-ot4102-fix-libbsd-in-Makefiles 1.1
# 	  Selected comments:
# 	    Replaced -lbsd with ${BSD_LIBS} variable
# 	[1992/06/08  21:49:42  mason]
# 
# Revision 1.1.2.2  1992/01/22  19:57:27  zeliff
# 	dce.77c file overlay
# 	[1992/01/22  19:15:44  zeliff]
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.3 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/security
.PATH:$(KPATH)

HEADERS_SUBDIRS = dacl dfsauth

SUBDIRS = dacl

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	${HEADERS_SUBDIRS_MAKERULE}

