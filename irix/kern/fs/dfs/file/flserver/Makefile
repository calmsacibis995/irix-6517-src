#smake
#
# @OSF_COPYRIGHT@
# COPYRIGHT NOTICE
# Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
# ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
# the full copyright text.
#
# HISTORY
# $Log: Makefile,v $
# Revision 65.6  1999/02/04 19:19:38  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.5  1998/05/04 15:16:47  bdr
# Cleanup the Makefile so that message cat *.cat files are in the
# SAMS_TARGETS and the *.cat.m  files are on the ILIST.  This fixes
# a build problem introduced by the last mod.
#
# Revision 65.4  1998/05/01  22:00:59  tdm
# Added CC_OPT_LEVEL to CFLAGS.
# Moved dfslsv.cat from ILIST to OTHERS.
# added dfslsv.cat.m, dfsvls.cat.m, and dfszfl.cat.m to ILIST.
#
# Revision 65.3  1998/03/16  15:37:17  gwehrman
# Cleanup ld warnings.
#
# Revision 65.2  1998/03/04 21:57:47  gwehrman
# Initial merge.
#
# Revision 65.1  1997/10/20 19:20:05  jdoak
# *** empty log message ***
#
# Revision 1.1.24.1  1994/10/26  15:45:47  sasala
# 	UHYP: CR 12618 Implement S12Y in DFS flserver
# 	[1994/10/26  15:43:33  sasala]
#
# Revision 1.1.22.4  1994/09/07  15:48:24  marty
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:20  marty]
# 
# Revision 1.1.22.3  1994/07/13  22:19:18  devsrc
# 	merged with bl-10
# 	[1994/06/28  18:03:05  devsrc]
# 
# 	Added  afsvl_s2c.h afsvl_data.h to ILIST
# 	[1994/03/08  21:20:05  mckeen]
# 
# Revision 1.1.22.2  1994/06/09  14:06:46  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:22:02  annie]
# 
# Revision 1.1.22.1  1994/02/04  20:17:44  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:12:34  devsrc]
# 
# Revision 1.1.20.1  1993/12/07  17:23:24  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  15:14:44  jaffe]
# 
# Revision 1.1.9.6  1993/01/21  19:33:24  zeliff
# 	Embedding copyright notices
# 	[1993/01/19  19:46:57  zeliff]
# 
# Revision 1.1.9.5  1992/11/24  17:01:19  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  17:56:09  bolinger]
# 
# Revision 1.1.9.4  1992/09/15  13:12:05  jaffe
# 	Transarc delta: jaffe-sync-with-aug-31-osf-sources 1.4
# 	  Selected comments:
# 	    Pick up changes from last submission to the OSF.
# 	    remove commondefs.idl.  This was done incompletely in another delta
# 	[1992/09/14  19:34:33  jaffe]
# 
# Revision 1.1.9.3  1992/09/01  15:36:19  jaffe
# 	Remove reference to commondefs
# 	[1992/09/01  13:56:47  jaffe]
# 
# Revision 1.1.9.2  1992/08/31  19:32:30  jaffe
# 	Transarc delta: jess-ot2869-com_err-usage-cleanup 1.5
# 	  Selected comments:
# 	    The use of com_err in dfs is inconsistent and in some cases
# 	    incorrect.
# 	    1) there are many sources which include <com_err.h> but don't
# 	    use any definitions from it.
# 	    2) there are sources which use com_err, but don't include the
# 	    <com_err.h>
# 	    3) there are subdirectories which do not need libcom_err.a for
# 	    any reason that link with it.
# 	    This delta is created to clean up the usage of com_err in dfs.
# 	    Remove -lcom_err in the var flserver_LIBS
# 	    Add -lcom_err to LIBS.
# 	Transarc delta: vijay-ot2172-cmdproc-into-flserver 1.2
# 	  Selected comments:
# 	    1. delete some obsolete options
# 	    2. Use the standard cmd command parser
# 	    no change here, -lcmd already in
# 	    accidentally erase admin list file name
# 	[1992/08/30  02:17:53  jaffe]
# 
# Revision 1.1.3.17  1992/06/30  21:43:20  mason
# 	Add the STUB_OPT_LEVEL variable to control optimization of stub files.
# 	[1992/06/30  21:28:49  mason]
# 
# Revision 1.1.3.16  1992/06/08  22:45:08  mason
# 	Transarc delta: mason-ot4102-fix-libbsd-in-Makefiles 1.1
# 	  Selected comments:
# 	    Replaced -lbsd with ${BSD_LIBS} variable
# 	[1992/06/08  21:34:46  mason]
# 
# Revision 1.1.3.15  1992/05/20  19:50:52  mason
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
# 	[1992/05/20  11:12:55  mason]
# 
# Revision 1.1.3.14  1992/05/05  19:00:37  rec
# 	Removed dependence on libkrb5.a by allowing libdce.a
# 	to export required symbol (krb5_cksumarray).
# 	[1992/05/05  18:54:45  rec]
# 
# Revision 1.1.3.13  1992/04/16  19:59:24  mckeen
# 	Removed definition of IDL
# 	[1992/04/15  19:23:28  mckeen]
# 
# Revision 1.1.3.12  1992/04/16  17:18:52  garyf
# 	fix .p.h rules
# 	[1992/04/16  17:12:57  garyf]
# 
# Revision 1.1.3.11  1992/04/14  04:08:02  mason
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
# 	[1992/04/13  16:00:17  mason]
# 
# Revision 1.1.3.10  1992/04/07  20:27:11  rec
# 	Have -lkrb5 on link line depend on RIOS platform since
# 	it is not needed with a shared libdce on OSF/1.
# 	[1992/04/07  19:58:38  rec]
# 
# Revision 1.1.3.9  1992/01/31  16:44:28  delgado
# 	Add modifications to build libdcedfs.a
# 	[1992/01/31  16:44:09  delgado]
# 
# Revision 1.1.3.8  1992/01/28  20:43:20  cfe
# 	No changes necessary
# 	[1992/01/28  20:42:44  cfe]
# 
# Revision 1.1.3.7  1992/01/26  00:26:20  zeliff
# 	Merging OSF porting changes:  added -lafssys to end of
# 	LIBS lines.
# 	[1992/01/25  22:46:13  zeliff]
# 
# Revision 1.1.3.6  1992/01/25  20:47:30  zeliff
# 	dfs6.3 merge, part2
# 	[1992/01/25  20:05:14  zeliff]
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
KPATH=$(TOP)/fs/dfs/file/flserver
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libkfldb.a
OBJ_CSRCS=	afsvl_proc_cstub.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		afsvl_data.h \
		afsvl_proc.h \
		fldb_data.h \
		fldb_proc.h \
		flclient.h \
		flserver.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

