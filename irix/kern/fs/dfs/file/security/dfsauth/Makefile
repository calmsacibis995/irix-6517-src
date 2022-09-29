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
# Revision 65.6  1999/02/04 23:15:10  mek
# Makefile tuning.
#
# Revision 65.5  1999/02/04 19:19:44  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.4  1998/12/04 17:26:37  bobo
# O32 build changes
#
# Revision 65.3  1998/03/16  15:37:19  gwehrman
# Cleanup ld warnings.
#
# Revision 65.2  1998/02/26 21:55:44  lmc
# Changes for SGIMIPS for multiple defines and install list including cat.m files.
#
# Revision 65.1  1997/10/20  19:20:19  jdoak
# *** empty log message ***
#
# Revision 1.5  1996/12/24  19:12:57  vrk
# PV Incidence - 451963. Added *.cat to OTHERS macro and *.cat.m to ILIST.
#
# Revision 1.4  1996/12/17  02:29:38  bhim
# Added *.cat.m files to the INSTALL LIST.
#
# Revision 1.3  1996/09/27  16:32:41  bhim
# Improved SGIMIPS install condition checks.
#
# Revision 1.2  1996/09/11  16:57:25  bhim
# Modified for new build procedure.
#
# Revision 1.1  1994/06/09  14:19:11  dcebuild
# Original File from OSF
#
# Revision 1.1.996.3  1994/06/09  14:19:11  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:31:10  annie]
#
# Revision 1.1.996.2  1994/02/04  20:29:19  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:17:43  devsrc]
# 
# Revision 1.1.995.1  1993/12/07  17:33:08  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  16:52:07  jaffe]
# 
# Revision 1.1.6.3  1993/01/21  15:18:27  cjd
# 	embedded copyright notice
# 	[1993/01/20  15:55:56  cjd]
# 
# Revision 1.1.6.2  1992/11/24  18:30:56  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:22:45  bolinger]
# 
# Revision 1.1.3.10  1992/06/08  22:46:18  mason
# 	Transarc delta: mason-ot4102-fix-libbsd-in-Makefiles 1.1
# 	  Selected comments:
# 	    Replaced -lbsd with ${BSD_LIBS} variable
# 	[1992/06/08  21:51:29  mason]
# 
# Revision 1.1.3.9  1992/05/20  20:04:03  mason
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
# 	[1992/05/20  11:57:51  mason]
# 
# Revision 1.1.3.8  1992/05/05  04:34:09  mason
# 	delta mason-fix-security-dfsauth-Makefile 1.1
# 	[1992/04/27  12:15:56  mason]
# 
# Revision 1.1.3.7  1992/04/14  18:06:24  mason
# 	auth5b/AUTH5B defines no longer needed.
# 	[1992/04/14  18:02:51  mason]
# 
# Revision 1.1.3.6  1992/04/14  03:35:26  mason
# 	Transarc delta: bab-ot2394-change-catalog-names 1.3
# 	  Files modified:
# 	    bak: Makefile; bakserver: Makefile; bosserver: Makefile
# 	    bubasics: Makefile; flserver: Makefile; fshost: Makefile
# 	    fsint: Makefile; ftserver: Makefile; host: Makefile
# 	    ncscompat: Makefile; ncsubik: Makefile
# 	    security/dacl: Makefile; security/dfsauth: Makefile
# 	    tkm: Makefile; tools/cmd: Makefile; update: Makefile
# 	    xcred: Makefile; xvolume: Makefile
# 	  Selected comments:
# 	    The dfs error catalogs were generated with names that made them
# 	    dce_error_inq_text unable to locate them. (dce_error_inq_text pulls
# 	    a facility code out of the error code and uses this to construct a
# 	    filename in which to look for the error message.  Our (dfs) facility
# 	    code is such that our error catalog filenames must start with "dfs",
# 	    rather then "dce".)
# 	    Change occurrences of "dce...\.cat" to "dfs...\.cat"
# 	    ot 2394
# 	    The sed script missed one occurrence of "dfs...\.cat"
# 	    the first time through because it was hidden by another
# 	    occurrence earlier on the line.
# 	[1992/04/13  17:23:11  mason]
# 
# Revision 1.1.3.5  1992/01/31  16:07:29  delgado
# 	Add modifications to build libdcedfs.a
# 	[1992/01/31  16:06:48  delgado]
# 
# Revision 1.1.3.4  1992/01/29  19:08:56  cfe
# 	Get the security flags right--in particular, define auth5b stuff
# 	and don't define FORCE_NOAUTH.
# 	[1992/01/29  19:08:32  cfe]
# 
# Revision 1.1.3.3  1992/01/24  03:48:14  devsrc
# 	Merging dfs6.3
# 	[1992/01/24  00:18:54  devsrc]
# 
# Revision 1.1.3.2  1992/01/23  20:23:01  zeliff
# 	Moving files onto the branch for dfs6.3 merge
# 	[1992/01/23  18:39:02  zeliff]
# 	Revision 1.1.1.3  1992/01/23  22:21:22  devsrc
# 	Fixed logs
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.6 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/security/dfsauth
.PATH:$(KPATH)

HFILES = dfsauth.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

