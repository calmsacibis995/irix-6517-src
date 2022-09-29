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
# Revision 65.6  1999/02/04 19:19:46  mek
# Convert from ODE format to IRIX smake format for IRIX kernel integration.
#
# Revision 65.5  1998/05/01 22:02:32  tdm
# Moved dfsxag.cat from ILIST to OTHERS.
# Added dfszxa.cat.m to ILIST.
# Added dfszxa.cat.m_IDIR.
#
# Revision 65.4  1998/03/16  15:37:21  gwehrman
# Cleanup ld warnings.
#
# Revision 65.3  1998/03/04 22:03:50  gwehrman
# Changed agclient_LIBS definition to fix unresolved symbols.
#
# Revision 65.2  1997/12/16 17:05:49  lmc
#  1.2.2 initial merge of file directory
#
# Revision 65.1  1997/10/20  19:20:31  jdoak
# *** empty log message ***
#
# Revision 1.1.153.1  1996/10/02  21:11:07  damon
# 	Newest DFS from Transarc
# 	[1996/10/01  18:51:19  damon]
#
# Revision 1.1.148.4  1994/08/30  15:56:18  sasala
# 	DFS S12Y general cleanup - rename ftutil and tools cmd catalogs, add ftutil msgs to xaggr dfsexport, and episode salvage and vnops subcomponents
# 	[1994/08/30  00:19:59  sasala]
# 
# Revision 1.1.148.3  1994/08/01  21:19:18  sasala
# 	Implement partial S12Y (Internationalization) for DFS commands and daemons
# 	[1994/08/01  15:08:15  sasala]
# 
# 	Implement partial S12Y (internationalization) for DFS commands and daemons
# 	[1994/07/21  14:52:09  sasala]
# 
# Revision 1.1.148.2  1994/06/09  14:25:16  annie
# 	fixed copyright in src/file
# 	[1994/06/09  13:37:14  annie]
# 
# Revision 1.1.148.1  1994/02/04  20:35:51  devsrc
# 	Merged from 1.0.3a to 1.1
# 	[1994/02/04  15:20:29  devsrc]
# 
# Revision 1.1.146.1  1993/12/07  17:37:58  jaffe
# 	1.0.3a update from Transarc
# 	[1993/12/03  18:11:11  jaffe]
# 
# Revision 1.1.9.3  1993/05/12  16:02:28  jaffe
# 	Fix library ordering
# 	[1993/05/12  15:34:36  jaffe]
# 
# Revision 1.1.9.2  1993/05/11  15:20:49  jaffe
# 	Transarc delta: jdp-db3358-add-bomb-point-utilities 1.5
# 	  Selected comments:
# 	    This delta adds a bomb point (error injection) package to DFS.  The
# 	    README file in src/file/bomb describes the package in detail.
# 	    Made a variety of improvements like including the proper copyright/history
# 	    header at the top of each file, fixing initialization problems,
# 	    changing the delay and wait bomb points to affect the thread instead
# 	    of the process, and making bomb points cheaper when none are set to go off.
# 	    Fix an oversight that caused compilation errors.
# 	    Integrate the package into the big build.
# 	    Include -lbomb
# 	[1993/05/10  20:09:05  jaffe]
# 
# Revision 1.1.6.8  1993/04/01  22:01:15  jaffe
# 	Transarc delta: vijay-db3329-ubik-add-dfstrace 1.6
# 	  Selected comments:
# 	    Add ICL tracing to the ubik library. Also convert the locking macros into
# 	    function calls to assist in debugging and tracking any locking problems.
# 	    All components that link with the ubik library will now have to also link in
# 	    the ICL library.
# 	    All hex values have a 0x prefix in the trace dump.
# 	    Add a few casts in order to satisfy the compiler.
# 	[1993/04/01  21:48:13  jaffe]
# 
# Revision 1.1.6.7  1993/02/16  16:14:14  jaffe
# 	Transarc delta: jdp-db3072-aggregate-interlocking 1.2
# 	  Selected comments:
# 	    This delta implements an interlocking feature that keeps
# 	    administrative commands like salvage and newaggr from stomping on an
# 	    attached (or mounted) fileset.  Two new primitives,
# 	    ftu_LockAggrDevice() and ftu_UnlockAggrDevice(), are provided by the
# 	    ftutil library and are used in both newaggr and salvage.
# 	    Two new test cases were added to the fset (ftutil) tests suite.  The first,
# 	    lock00.itl, tests the primitives directly. The second, lock01.itl, tests
# 	    interaction with newaggr and salvage for both attached aggregates and
# 	    mounted filesets.
# 	    Add ${BSD_LIBS} to the list of dfsexport's libraries.
# 	    Fix things up for OSF/1.
# 	Transarc delta: jdp-ot1051-revoke-tokens-on-detach 1.3
# 	  Selected comments:
# 	    This delta makes "dfsexport -detach" revoke tokens for all filesets contained
# 	    in any of the aggregates that are being detached.
# 	    This may be the one and only revision of this delta, but we're really
# 	    closing it now to fix another bug (OT 6914).
# 	    Add to the list of dfsexport's libraries.  The list got quite a bit longer
# 	    because of all of the stuff pulled in by libvolc.a.
# 	    Did some minor reorganization.
# 	    Fix up usage message to be consistent with other DFS commands.
# 	[1993/02/15  18:57:11  jaffe]
# 
# Revision 1.1.6.6  1993/01/21  16:32:24  cjd
# 	embedded copyright notice
# 	[1993/01/20  18:26:11  cjd]
# 
# Revision 1.1.6.5  1992/11/24  20:43:23  bolinger
# 	Change include file install directory from .../afs to .../dcedfs.
# 	[1992/11/22  19:39:09  bolinger]
# 
# Revision 1.1.6.4  1992/11/20  20:19:54  rsarbo
# 	agclient needs to link libdce rather than libafsys
# 	on PMAX
# 	[1992/11/20  19:32:57  rsarbo]
# 
# Revision 1.1.6.3  1992/11/18  21:14:24  jaffe
# 	Transarc delta: jdp-ot5415-mount-aggregate-on-attach 1.4
# 	  Selected comments:
# 	    This delta contains the bulk of the changes necessary to have aggregates
# 	    mounted at attach time.  The purpose of this feature is to provide a
# 	    means by which exported filesets (with no local mount points) will be
# 	    sync'd periodically.  To accomplish this, we have created a new VFS type
# 	    (agfs).  Whenever an aggregate is exported, a local mount of the aggregate
# 	    is also done; then, when the sync daemon does its thing, the aggregate's sync
# 	    vfsop will be called.  The agfs sync then calls the new sync aggrop, which
# 	    in the case of Episode, calls efs_sync().  The UFS sync aggrop doen't do
# 	    anything, since we know the UFS filesystem is mounted, and so, its sync vfsop
# 	    will get called anyway.
# 	    This delta must be bundled with the corresponding deltas in the Episode and
# 	    test configurations.
# 	    Remove leftover junk from $OFILES.
# 	    Rearrange things slightly to make use of a vendor-supplied definition
# 	    of MOUNT_AGFS.
# 	    Add "-losi" to list of libs for dfsexport.
# 	    Fix bogus linker warnings about shared libraries when compiling .o files.
# 	[1992/11/17  22:38:58  jaffe]
# 
# Revision 1.1.6.2  1992/08/31  22:06:43  jaffe
# 	Transarc delta: jdp-ftutils 1.12
# 	  Selected comments:
# 	    Checkpoint this delta to do other work.
# 	    Checkpoint while upgrading to the latest release
# 	    Yet another checkpoint.
# 	    Another checkpoint.
# 	    With this revision, the new ftutil code should be ready to be released.
# 	    Add "-lcom_err" and "-ldce" to link line for dfsexport.
# 	    Fix compilation problems on OSF/1.
# 	    Move "-lftutil" before "-lafssys" in dfsexport_LIBS.
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
# 	    Remove -lcom_err in the var LIBS
# 	[1992/08/30  13:50:50  jaffe]
# 
# Revision 1.1.3.4  1992/07/02  18:06:31  jdp
# 	Transarc delta: jdp-ot4353-remove-recovery 1.3
# 	  Selected comments:
# 	    This delta removes Episode recovery from the ftutil layer.  Epimount and
# 	    dfsexport will no longer automatically do recovery; the salvager should be
# 	    used instead.
# 	    dfsexport no longer needs to be linked with episode libraries, since recovery
# 	    has been removed.
# 	    Remove episode libraries from dfsexport's link line.
# 	    Added a more informative message about trying recovery when the attach fails
# 	    with ENXIO on an LFS fileset.
# 	[1992/07/02  16:40:10  jdp]
# 
# Revision 1.1.3.3  1992/06/22  16:15:35  mason
# 	Transarc delta: jdp-ot3839-xaggr-add-recovery-to-dfsexport 1.5
# 	  Selected comments:
# 	    This delta adds recovery to dfsexport, fixing OT defect #3839.
# 	    Added all sorts of stuff to dfsexport's list of libraries to support recovery.
# 	    Fix problem where dfsexport would sometimes fail to export an aggregate
# 	    because of an uninitialized variable in ftu_AttachAggrWithAstab().
# 	    [This is me (jdp) typing at rajesh's terminal.]
# 	    Fix merge problems
# 	    Ran into the "can't open a mounted block device on OSF/1" problem in the
# 	    ftutil interlocking code.  Now, we only do the interlock if we are
# 	    attaching an Episode aggregate.
# 	    Correct compilation problems on OSF/1.
# 	[1992/06/22  15:59:15  mason]
# 
# Revision 1.1.3.2  1992/05/20  20:21:26  mason
# 	Transarc delta: cfe-ot2442-cmdproc-into-dfsexport 1.2
# 	  Files modified:
# 	    xaggr: Makefile, export.c
# 	  Selected comments:
# 	    Integrated command processor into dfsexport.
# 	    Link with command-processor library.
# 	    Fixed a compile-time problem.
# 	[1992/05/20  16:52:13  mason]
# 
# Revision 1.1  1992/01/19  02:51:32  devrcs
# 	Initial revision
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
KPATH=$(TOP)/fs/dfs/file/xaggr
.PATH:$(KPATH)

KCINCS=		-I$(KPATH) \
		-I$(ROOT)/usr/include

OBJ_NAME=	libaggr.a
OBJ_CSRCS=	ag_init.c ag_registry.c xaggr_linkage.c
OBJS=		$(OBJ_CSRCS:.c=.o)
$(OBJ_NAME):$(OBJ_NAME)($(OBJS)) MAKELIB

TARGETS=	$(OBJ_NAME)
CFILES=		$(OBJ_CSRCS)
HFILES=		aggr.h ag_init.h astab.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install: $(KCOMMONPREF)default

headers!
	$(INSTALL) -m 444 -F /usr/include/dcedfs $(HFILES)

