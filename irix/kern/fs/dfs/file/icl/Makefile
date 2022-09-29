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
# Revision 65.7  1999/02/04 19:19:41  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.6  1998/12/04 17:26:37  bobo
# O32 build changes
#
# Revision 65.5  1998/09/22  20:33:15  gwehrman
# Added icl_idbg.h to INCLUDES.  Defined icl_idbg.h_EXPDIR.  Added idbglog to
# PROGRAMS.  Defined idbglog_OFILES and idbglog_LIBS.
#
# Revision 65.4  1998/03/16 15:37:18  gwehrman
# Cleanup ld warnings.
#
# Revision 65.3  1998/01/20 16:43:17  lmc
# Removed optimization flag of "-0" because its not valid
# for us.
#
# Revision 65.1  1997/10/20  19:17:38  jdoak
# *** empty log message ***
#
# Revision 1.1.753.1  1996/10/02  17:51:54  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:40:36  damon]
#
# Revision 1.1.748.4  1994/09/07  15:48:28  marty
# 	CR # 11497 - Remove '-v' option from IDLFLAGS defineittion.
# 	[1994/09/06  21:39:24  marty]
# 
# Revision 1.1.748.3  1994/06/09  14:11:06  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:25:03  annie]
# 
# Revision 1.1.748.2  1994/02/04  20:20:52  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:14:07  devsrc]
# 
# Revision 1.1.74.1  1993/12/07  17:26:45  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  15:36:06  jaffe]
# 
# Revision 1.1.4.3  1993/05/12  16:02:24  jaffe
# 	Correct library ordering
# 	[1993/05/12  15:45:25  jaffe]
# 
# Revision 1.1.4.2  1993/05/11  14:09:51  jaffe
# 	Transarc delta: comer-db3378-add-rpc-interface-to-user-level-icl 1.2
# 	  Selected comments:
# 	    This delta implements an RPC interface to the user-level ICL tracing
# 	    package.  The idea is that each server will export this interface to
# 	    CDS under a meaningful entry.  For example, the flserver would export
# 	    it under /.:/hosts/<hostname>/flserver by making the call:
# 	    code = icl_ExportIF("flserver");
# 	    Each command in the dfstrace suite now has a -cdsentry that, if
# 	    specified, will cause dfstrace to connect to the specified server
# 	    instead of the kernel.
# 	    Add new RPC files, new error table.
# 	    The dump file is now restricted to be in /opt/dcelocal/var/dfs/adm.
# 	Transarc delta: comer-db3396-make-icl-dump-timestamps 1.1
# 	  Selected comments:
# 	    Some have claimed that the timestamp dumped by ICL is not all that
# 	    terribly useful for absolute time.  This delta makes a change to ICL
# 	    so that it will dump a unix time timestamp.
# 	    Fix a problem with message catalog creation.
# 	[1993/05/10  18:06:52  jaffe]
# 
# Revision 1.1.2.9  1993/01/19  16:04:42  cjd
# 	embedded copyright notice
# 	[1993/01/19  14:14:51  cjd]
# 
# Revision 1.1.2.8  1993/01/13  18:08:13  shl
# 	Transarc delta: comer-ot6251-move-icldump-into-file-tree 1.1
# 	  Selected comments:
# 	    This deltat moves the icldump command back into the file tree.  It
# 	    also adds a command parser interface.
# 	    Build and install icldump.
# 	Transarc delta: comer-ot6297-icl-user-interface 1.5
# 	  Selected comments:
# 	    Added some functioanlity and a user interface to the ICL package.  The
# 	    current command is called dfstrace; the current interface is:
# 	    ./dfstrace: Commands are:
# 	    apropos         search by help text
# 	    clear           clear logs by logname or by event set
# 	    dump            dump DFS trace logs from the kernel
# 	    help            get help on commands
# 	    lslogs          list available logs
# 	    lssets          list available event sets
# 	    setlog          set the size of a log
# 	    setset          set state of event sets
# 	    Usage: dfstrace clear  [{-set <set_name>... | -log <log_name>... | -all}]
# 	    [-help]
# 	    Usage: dfstrace dump  [{-set <set_name>... | -all | -follow <log_name>}]
# 	    [-file <output_filename>]
# 	    [-sleep <seconds_between_reads>] [-help]
# 	    To get a continuous dump of a log, for example, do:
# 	    dfstrace -follow cmfx -sleep 5
# 	    Usage: dfstrace lslogs  [{-set <set_name>... | -log <log_name>... | -all}]
# 	    [-long][-help]
# 	    Lists logs, their sizes, and whether or not they are currently
# 	    allocated.
# 	    Usage: dfstrace lssets  [{-set <set_name>... | -all}] [-help]
# 	    Lists available sets and their states.
# 	    Usage: dfstrace setlog  -log <log_name> -buffersize <4-kilobyte_units>
# 	    Set log size.
# 	    Usage: dfstrace setset  {-activate | -deactivate | -release}
# 	    [{-set <set_name>... | -all}] [-help]
# 	    Change the state of an event set.  In active state, logging
# 	    occurs to log.  In inactive state, no loggin occurs but the
# 	    reference to the log persists.  In the released state, the
# 	    reference to the log is gone, so the log may be deallocated.
# 	    Changed name from icldump to dfstrace.
# 	    Misc bug fixes.
# 	    logp->logElements wasn't getting reset on clear.
# 	    By default icl tracing is turned on.  This change makes it a little
# 	    easier to turn it on and off when compiling.  The change adds 2 flags:
# 	    ICL_DEFAULT_ENABLED and ICL_DEFAULT_DISABLED.  There is an accompanying
# 	    -D in icl and icl.klib Makefiles, currently both enabled.
# 	    Added -DICL_DEFAULT_ENABLED to CFLAGS.
# 	[1993/01/12  21:06:42  shl]
# 
# Revision 1.1.2.7  1992/12/09  20:20:07  jaffe
# 	Transarc delta: comer-ot6129-user-level-icl-dumping 1.5
# 	  Selected comments:
# 	    Added code to ICL trace logs at user level.  The guinea pig for this
# 	    change was dfsbind, also modified as part of this delta.  Most of the
# 	    actual tracing and some of the ICL work was done at IBM by Jean
# 	    Pehkonen.
# 	    Force the program to dump its log: kill -30 <pid>
# 	    Added icl_signal.o.
# 	    Added dfsbind to EXPINC_SUBDIRS
# 	    In files where both osi.h and compat.h are included, the reference
# 	    to osi.h must be first.  In each of these files, osi.h was implicitly
# 	    included, so I added an explicit reference before compat.h.
# 	    Installed message catalog.
# 	[1992/12/04  16:43:00  jaffe]
# 
# Revision 1.1.2.6  1992/11/24  17:53:25  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  18:06:58  bolinger]
# 
# Revision 1.1.2.5  1992/10/27  21:14:27  jaffe
# 	Transarc delta: fred-icl-move-dump-to-test 1.1
# 	  Selected comments:
# 	    To move icldump to the test tree.
# 	    Removed references to icl_dumpcmd.c.
# 	[1992/10/27  14:28:08  jaffe]
# 
# Revision 1.1.2.4  1992/09/25  18:23:10  jaffe
# 	Transarc delta: jess-ot5380-fix-cm-px-makefile 1.1
# 	  Selected comments:
# 	    The catalog files "cm/dfszcm.cat" and "px/dfszpx.cat" have not been installed.
# 	    No change.
# 	Transarc delta: jess-ot5381-install-icldump 1.2
# 	  Selected comments:
# 	    Recently, fs and low tests have been involved with Kernel trace command
# 	    and function to get kernel logs in case of error. The kernel trace
# 	    interface command "icldump' thus needs to be installed in the install
# 	    tree.
# 	    Fix the problem.
# 	    Minor change.
# 	    See above.
# 	[1992/09/23  19:12:48  jaffe]
# 
# Revision 1.1.2.3  1992/09/15  20:31:23  jaffe
# 	sync with Transarc
# 	[92/09/15            jaffe]
# 
# Revision 1.1.2.2  1992/09/15  13:14:38  jaffe
# 	Transarc delta: kazar-ot4921-add-in-core-logging-package 1.5
# 	  Selected comments:
# 	    Start coding in-core logging package.  Not ready for export yet,
# 	    as mostly untested code.
# 	    makefile for basic stuff.
# 	    added missing file and cleaned up Makefile
# 	    removed old stuff from previous copy.
# 	Transarc delta: kazar-ot4921-basic-cmfx-events 1.1
# 	  Selected comments:
# 	    add some trace points, fix up bugs in logging package.
# 	    add trace points
# 	[1992/09/14  19:54:53  jaffe]
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
KPATH=$(TOP)/fs/dfs/file/icl
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

ICL_NAME=	libkicl.a
ICL_CSRCS=	icl_event.c icl_log.c icl_set.c icl_control.c
ICL_OBJS=	$(ICL_CSRCS:.c=.o)
$(ICL_NAME):$(ICL_NAME)($(ICL_OBJS)) MAKELIB

IDBG_NAME=	icl_idbg.a
IDBG_CSRCS=	icl_idbg.c
IDBG_OBJS=	$(IDBG_CSRCS:.c=.o)
$(IDBG_NAME):$(IDBG_NAME)($(IDBG_OBJS)) MAKELIB

TARGETS=	$(ICL_NAME) $(IDBG_NAME)
CFILES=		$(ICL_CSRCS) $(IDBG_CSRCS)
HFILES=		icl.h icl_idbg.h icl_macros.h icl_rpc.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

