/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: ftserver.h,v $
 * Revision 65.2  1999/02/04 19:19:40  mek
 * Provide C style include file for IRIX kernel integration.
 *
 * Revision 65.1  1997/10/20 19:20:09  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.62.1  1996/10/02  17:47:36  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:37:53  damon]
 *
 * Revision 1.1.57.2  1994/06/09  14:08:17  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:23:09  annie]
 * 
 * Revision 1.1.57.1  1994/02/04  20:19:02  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:13:13  devsrc]
 * 
 * Revision 1.1.55.1  1993/12/07  17:24:43  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:25:35  jaffe]
 * 
 * Revision 1.1.4.6  1993/01/21  19:36:47  zeliff
 * 	Embedding copyright notices
 * 	[1993/01/19  19:50:44  zeliff]
 * 
 * Revision 1.1.4.5  1992/11/24  17:49:13  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  18:02:22  bolinger]
 * 
 * Revision 1.1.4.4  1992/11/18  20:32:42  jaffe
 * 	Transarc delta: cfe-ot5814-log-cleanup 1.2
 * 	  Selected comments:
 * 	    Clean up some excessive log messages
 * 	    clean up a macro
 * 	    Continuing clean-up.
 * 	[1992/11/17  21:38:37  jaffe]
 * 
 * Revision 1.1.4.3  1992/10/27  21:14:11  jaffe
 * 	Transarc delta: vijay-ot5224-fts-dump-handle-pipe-errors 1.3
 * 	  Selected comments:
 * 	    fts dump uses NCS pipes to dump contents of a fileset to a file. The push
 * 	    routine calls abort upon write syscall failure. This kills the client thread
 * 	    and the corresponding server thread resulting several bad things including
 * 	    a transaction whose ref count is high, a fldb entry that is locked and the
 * 	    fileset state busy. One way to handle pipe errors gracefully would be to
 * 	    store the errno after a syscall failure in the pipe state, and continue pipe
 * 	    data transfer until completion. Once the dump RPC completes, the client
 * 	    could look at the pipe state to determine what error occurred and take
 * 	    appropriate recovery action necessary. The restore case can also utilize a
 * 	    similar mechanism.
 * 	    There were some places in the ftserver RPC routines where FTSERVER_UNLOCK
 * 	    is called in the middle of these routines when segments of code were
 * 	    considered thread safe. The FTSERVER_{LOCK|UNLOCK} macros also disable and
 * 	    enable thread cancels. So we add some conditionality that disables thread
 * 	    cancels through all ftserver RPCs, but allows unlock of mutexes around thread
 * 	    safe code regions.
 * 	    Add the conditionality to the lock and unlock macros.
 * 	    Fix compiler warning on PMAX.
 * 	[1992/10/27  14:26:46  jaffe]
 * 
 * Revision 1.1.4.2  1992/08/31  19:54:39  jaffe
 * 	Transarc delta: cfe-ot4517-new-ftserver-swapid-i-f 1.1
 * 	  Selected comments:
 * 	    Needed a new ftserver RPC call to swap fileset IDs to do cleanup processing
 * 	    correctly after a fileset move.  The case in question was when filesets are
 * 	    being moved between aggregates on the same server and we someday want to
 * 	    preserve local mount points.
 * 	    I also added prototypes to some of the functions in volc.h (functions exported
 * 	    from libvolc.a) so that we wouldn't run into problems with libvolc.a procedure
 * 	    signatures changing again (or not so readily).
 * 	    Added FTSERVER_OP_SWAPIDS bit for use at create-trans time.
 * 	Transarc delta: vijay-ot2674-fileset-states-cleanup 1.6
 * 	  Selected comments:
 * 	    The file system independent layer passes information about fileset operations
 * 	    to the fileset dependent layer to facilitate improved concurrency in vnode
 * 	    operations. Error codes that are more descriptive of the blocking operation
 * 	    is returned to the caller of fileset and vnode operations.
 * 	    misc fixes and cleanup
 * 	    Put in some AFSLOG statements to help debugging
 * 	Transarc delta: vijay-ot2674-vnode-concurrency 1.7
 * 	  Selected comments:
 * 	    Pass the vnode ops type (noop, readonly, readwrite) through to vol_FindVfs
 * 	    and volreg_Lookup. If the fileset is busy, compatibility between the type and
 * 	    the value of the concurrency field is determined. If compatible, the vnode op
 * 	    proceeds, else it sleeps. If the fileset is in a persistent error state, the
 * 	    error is returned back from the vnode op. The call signature for
 * 	    xvfs_GetVolume is changed to return this error value.
 * 	    This delta is checkpoint the changes made before testing.
 * 	    Change the xvfs_VfsGetVolume interface as well along with its variants.
 * 	    Fix lots of old bugs in vol_open's vnode lock-out.
 * 	    Not ready for prime time; closing to upgrade.
 * 	    One big ftserver change is to get the AG_AGGRINFO information before doing
 * 	    a VOL_AGOPEN, so that AG_AGGRINFO's call to VFS_STATFS doesn't deadlock
 * 	    waiting for the VOL_CLOSE (via xglue_root).
 * 	    See above
 * 	    Another checkpoint of on-going work to fix lots of little bugs
 * 	    Had to close to upgrade; these are more fixes that emerged from testing etc.
 * 	    One more round, distinguishing the VOLOP_SETSTATUS calls that can change
 * 	    a fileset's ID, requiring more clearing-the-way in Episode.
 * 	    Define a new bit to allow fts to request setstatus-changes-volid.
 * 	    A previous revision of this delta broke standalone Episode: many of
 * 	    the glue routines try to lookup the volume in the registry, and if the
 * 	    lookup fails (which it will in the standalone case), the glue routine
 * 	    returns failure.  The symptom was failures with ENODEV whenever
 * 	    accessing a standalone Episode fileset, despite a successful mount.
 * 	Transarc delta: vijay-ot4694-dfs-servers-protect-against-cancel 1.1
 * 	  Selected comments:
 * 
 * 	    This delta protects DFS servers from malicious cancels from client threads.
 * 	    Define macros for disabling and enabling cancellability in ftserver procedures.
 * 	[1992/08/30  02:31:16  jaffe]
 * 
 * Revision 1.1.2.2  1992/04/21  13:39:29  mason
 * 	Transarc delta: cburnett-ot2700-ftserver_changes_for_aix32 1.1
 * 	  Files modified:
 * 	    ftserver: ftserver.p.h
 * 	  Selected comments:
 * 	    ftserver changes for AIX 3.2 compile
 * 	    added include of compat_lock.h
 * 	[1992/04/20  22:38:16  mason]
 * 
 * Revision 1.1  1992/01/19  02:55:33  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/*
 * ftserver.h:
 * This file is automatically generated; please do not edit it.
 */
/* Including ftserver.p.h at beginning of ftserver.h file. */

/* Copyright (C) 1990 Transarc Corporation.  All rights reserved. */

#ifndef _FTSERVER_H_
#define _FTSERVER_H_ 1

#ifndef FILE
#include <stdio.h>
#endif /* FILE */

#include <dcedfs/lock.h>
/* #include "ftserver_trans.h" */
#include <dcedfs/icl.h>			/* ICL tracing package */

/*
 * Exported globals/functions of ftserver_main.c
 */
extern int verbose;
extern FILE *logIO;
extern struct icl_set *iclSetP;
extern struct icl_log *iclLogP;

/*
 * Exported globals/functions of volc_vprocs.c
 */

extern long ReceiveRemoteFile(), SendLocalFile();
extern int vols_AggregateInfo();

/* global locking stuff for CMA */
extern osi_dlock_t globalLock;

/* bit mask for ftserver ops */
#define	FTSERVER_OP_DELETE		0x0001
#define	FTSERVER_OP_DUMP		0x0002
#define	FTSERVER_OP_RESTORE		0x0004
#define	FTSERVER_OP_CLONE		0x0008
#define	FTSERVER_OP_RECLONE		0x0010
#define	FTSERVER_OP_GETFLAGS		0x0020
#define	FTSERVER_OP_SETFLAGS		0x0040
#define	FTSERVER_OP_GETSTATUS		0x0080
#define	FTSERVER_OP_SETSTATUS		0x0100
/* if we're setting a new fileset ID */
#define	FTSERVER_OP_SETSTATUS_ID		0x0200
#define	FTSERVER_OP_SWAPIDS		0x0400
/* to guarantee that the file system shuts off all vnode ops */
#define	FTSERVER_OP_NOACCESS		0x0800

/* encoding macro for ftserver ops and error code in the flags parameter - ftserver ops in the upper 2 bytes and the error code the lower 2 bytes
*/
#define	FLAGS_ENCODE(ops, errOffset)  \
	(((ops) << 16) | (((errOffset)-VOLERR_PERS_LOWEST) & 0xffff))

/* decoding macro for ftserver ops and error code from the flags parameter */
#define	FLAGS_DECODE(encodedParam, ops, error)    { \
	(ops) = ((encodedParam) & 0xffff0000)>>16;    \
        (error) = ((encodedParam) & 0xffff)+VOLERR_PERS_LOWEST; \
}

#endif /* _FTSERVER_H_ */

/* End of prolog file ftserver.p.h. */

#define FTS_ERR_RCSID                            (572825600L)
#define FTS_ERR_ATTACH                           (572825601L)
#define FTS_ERR_DETACH                           (572825602L)
#define FTS_ERR_BADAGGREGATE                     (572825603L)
#define FTS_ERR_BADNAME                          (572825604L)
#define FTS_ERR_BADACCESS                        (572825605L)
#define FTS_ERR_VLDB                             (572825606L)
#define FTS_ERR_MOVED                            (572825607L)
#define FTS_ERR_BADOP                            (572825608L)
#define FTS_ERR_DUMP                             (572825609L)
#define FTS_ERR_BADDUMP                          (572825610L)
#define FTS_ERR_BADRELEASE                       (572825611L)
#define FTS_ERR_BUSY                             (572825612L)
#define FTS_ERR_NOMEM                            (572825613L)
#define FTS_ERR_NOVOL                            (572825614L)
#define FTS_ERR_MULTIRWVOL                       (572825615L)
#define FTS_ERR_FAILEDOP                         (572825616L)
#define FTS_ERR_IO                               (572825617L)
#define FTS_ERR_NOTIMP                           (572825618L)
#define FTS_ERR_INTERNAL_FLDB                    (572825619L)
#define FTS_ERR_NEED_RW                          (572825620L)
#define FTS_ERR_NEED_RO                          (572825621L)
#define FTS_ERR_NEED_BK                          (572825622L)
#define FTS_ERR_BADCONN                          (572825623L)
#define FTS_ERR_BADSERVADDR                      (572825624L)
#define FTS_ERR_PIPE                             (572825625L)
#define FTS_ERR_INCONSISTENT                     (572825626L)
#define FTS_ERR_BADGROUP                         (572825627L)
#define FTS_ERR_FLDBENT_INCONSISTENT             (572825628L)
#define FTS_ERR_FSET_EXISTS                      (572825629L)
#define FTS_ERR_FSET_REPLICATED                  (572825630L)
#define FTS_ERR_SPARE3                           (572825631L)
#define FTS_ERR_SPARE4                           (572825632L)
#define FTSERVER_ERR_ATTACH                      (572825633L)
#define FTSERVER_ERR_DETACH                      (572825634L)
#define FTSERVER_ERR_BADAGGREGATE                (572825635L)
#define FTSERVER_ERR_BADNAME                     (572825636L)
#define FTSERVER_ERR_BADACCESS                   (572825637L)
#define FTSERVER_ERR_VLDB                        (572825638L)
#define FTSERVER_ERR_MOVED                       (572825639L)
#define FTSERVER_ERR_BADOP                       (572825640L)
#define FTSERVER_ERR_DUMP                        (572825641L)
#define FTSERVER_ERR_BADDUMP                     (572825642L)
#define FTSERVER_ERR_BADRELEASE                  (572825643L)
#define FTSERVER_ERR_BUSY                        (572825644L)
#define FTSERVER_ERR_OFFLINE                     (572825645L)
#define FTSERVER_ERR_NOMEM                       (572825646L)
#define FTSERVER_ERR_NOVOL                       (572825647L)
#define FTSERVER_ERR_MULTIRWVOL                  (572825648L)
#define FTSERVER_ERR_IO                          (572825649L)
#define FTSERVER_ERR_TRANSEXISTS                 (572825650L)
#define FTSERVER_ERR_NOTRANS                     (572825651L)
#define FTSERVER_ERR_TRELE                       (572825652L)
#define FTSERVER_ERR_DELETED                     (572825653L)
#define FTSERVER_ERR_LCLMOUNT                    (572825654L)
#define FTSERVER_ERR_DISJOINT_DATE               (572825655L)
#define FTSERVER_ERR_DISJOINT_VV                 (572825656L)
#define FTSERVER_ERR_DISJOINT2_DATE              (572825657L)
#define FTSERVER_ERR_DISJOINT2_VV                (572825658L)
#define FTSERVER_ERR_BAD_ACL_RESTORE             (572825659L)
#define FTSERVER_ERR_BAD_ACL_ENTRYTYPE_RESTORE   (572825660L)
#define FTSERVER_ERR_BAD_PROTOCOL                (572825661L)
#define initialize_fts_error_table()
#define ERROR_TABLE_BASE_fts (572825600L)

/* for compatibility with older versions... */
#define init_fts_err_tbl initialize_fts_error_table
#define fts_err_base ERROR_TABLE_BASE_fts
