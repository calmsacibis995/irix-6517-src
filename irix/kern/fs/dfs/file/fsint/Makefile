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
# Revision 65.4  1999/02/04 19:19:39  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.3  1998/04/28 14:23:32  gwehrman
# Fixed the sed scripts to be compatible with sed on 6.5.  Fix for bug
# 595443.
#
# Revision 65.2  1997/12/16 17:05:34  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:20:27  jdoak
# *** empty log message ***
#
# Revision 1.1.104.1  1996/10/02  17:46:56  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:37:25  damon]
#
# Revision 1.1.99.3  1994/09/07  15:48:26  marty
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:22  marty]
# 
# Revision 1.1.99.2  1994/06/09  14:07:48  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:22:46  annie]
# 
# Revision 1.1.99.1  1994/02/04  20:18:41  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:13:02  devsrc]
# 
# Revision 1.1.97.1  1993/12/07  17:24:13  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  15:22:16  jaffe]
# 
# Revision 1.1.6.4  1993/01/21  19:35:42  zeliff
# 	Embedding copyright notices
# 	[1993/01/19  19:49:21  zeliff]
# 
# Revision 1.1.6.3  1992/11/24  17:03:29  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  17:59:02  bolinger]
# 
# Revision 1.1.6.2  1992/08/31  19:45:18  jaffe
# 	Transarc delta: jaffe-3645-remove-hardcoded-idl-path 1.1
# 	  Selected comments:
# 	    Makefile was redefining IDL and thereby requiring the idl program to be
# 	    on the path.
# 	    Removed IDL definition.
# 	[1992/08/30  02:25:50  jaffe]
# 
# Revision 1.1.3.10  1992/06/30  21:43:31  mason
# 	Add the STUB_OPT_LEVEL variable to control optimization of stub files.
# 	[1992/06/30  21:29:32  mason]
# 
# Revision 1.1.3.9  1992/05/20  19:52:25  mason
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
# 	[1992/05/20  11:30:47  mason]
# 
# Revision 1.1.3.8  1992/04/21  13:12:58  mason
# 	Transarc delta: cburnett-ot2690-fsint_changes_for_aix32 1.1
# 	  Files modified:
# 	    fsint: Makefile
# 	  Selected comments:
# 	    Changes for AIX 3.2 compile
# 	    Turned on xlc compiler
# 	[1992/04/20  22:32:56  mason]
# 
# Revision 1.1.3.7  1992/04/16  19:59:31  mckeen
# 	Removed CCTYPE=traditional statement
# 	[1992/04/16  18:38:08  mckeen]
# 
# 	Removed definition of IDL
# 	[1992/04/15  18:41:19  mckeen]
# 
# Revision 1.1.3.6  1992/04/14  04:08:42  mason
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
# 	[1992/04/13  16:04:04  mason]
# 
# Revision 1.1.3.5  1992/01/31  20:02:26  delgado
# 	Add modifications to build libdcedfs.a
# 	[1992/01/31  20:01:36  delgado]
# 
# Revision 1.1.3.4  1992/01/25  20:47:39  zeliff
# 	dfs6.3 merge, part2
# 	[1992/01/25  20:05:35  zeliff]
# 
# $EndLog$
#
#
# Makefile for OSF Distributed File System
#
# $Revision: 65.4 $
DEPTH?=../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/file/fsint
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

RCL_NAME=	libkrepcli.a
RCL_CSRCS=	rep_proc_cstub.c
RCL_OBJS=	$(RCL_CSRCS:.c=.o)
$(RCL_NAME):$(RCL_NAME)($(RCL_OBJS)) MAKELIB

AFSC_NAME=	libkafs4clt.a
AFSC_CSRCS=	afs4int_cstub.c tkn4int_sstub.c
AFSC_OBJS=	$(AFSC_CSRCS:.c=.o)
$(AFSC_NAME):$(AFSC_NAME)($(AFSC_OBJS)) MAKELIB

AFSS_NAME=	libkafs4srv.a
AFSS_CSRCS=	afs4int_sstub.c tkn4int_cstub.c
AFSS_OBJS=	$(AFSS_CSRCS:.c=.o)
$(AFSS_NAME):$(AFSS_NAME)($(AFSS_OBJS)) MAKELIB

TARGETS=	$(RCL_NAME) $(AFSC_NAME) $(AFSS_NAME)
CFILES=		$(RCL_CSRCS) $(AFSC_CSRCS) $(AFSS_CSRCS)
HFILES=		afs4int.h scx_errs.h rep_data.h rep_proc.h tkn4int.h rep_errs.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

