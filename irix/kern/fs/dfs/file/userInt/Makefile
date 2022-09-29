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
# Revision 65.2  1999/02/04 19:19:45  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.1  1997/10/20 19:19:59  jdoak
# *** empty log message ***
#
# Revision 1.1.9.1  1996/10/02  21:06:01  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:50:08  damon]
#
# Revision 1.1.4.4  1994/09/16  21:26:58  ruby
# 	add fts to SAMIDL_SUBDIRS
# 	[1994/09/16  21:26:22  ruby]
# 
# 	Added fts to SAMIDL_SUBDIRS list
# 
# Revision 1.1.4.3  1994/08/24  17:57:43  sasala
# 	Implement partial S12Y for DFS - bos
# 	[1994/08/24  17:42:57  sasala]
# 
# Revision 1.1.4.2  1994/08/08  17:05:12  sasala
# 	Implement partial S12Y (Internationalization) for cm command, including general Makefile changes
# 	[1994/08/08  15:30:00  sasala]
# 
# Revision 1.1.4.1  1994/06/09  14:23:51  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:35:45  annie]
# 
# Revision 1.1.2.3  1993/01/21  16:28:15  cjd
# 	embedded copyright notice
# 	[1993/01/20  18:21:22  cjd]
# 
# Revision 1.1.2.2  1992/08/31  21:41:47  jaffe
# 	Transarc delta: jaffe-ot3211-fix-prototypes 1.1
# 	  Selected comments:
# 	    Remove ANSI-C declaration files which contained prototypes for standard
# 	    functions.  We expect these prototypes to be defined by the system include
# 	    files.
# 	    Don't build the base directory anymore since its contents have been removed.
# 	[1992/08/30  13:30:21  jaffe]
# 
# Revision 1.1  1992/01/19  02:50:44  devrcs
# 	Initial revision
# 
# $EndLog$
#
#!smake
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.2 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/userInt
.PATH:$(KPATH)

HFILES = volc.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install:

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

