/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: px_intops.c,v 65.8 1999/02/05 16:54:31 mek Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 * HISTORY
 * $Log: px_intops.c,v $
 * Revision 65.8  1999/02/05 16:54:31  mek
 * Cleanup build warnings for IRIX kernel integration.
 *
 * Revision 65.7  1998/04/01 14:16:08  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Turned off 'pointless comparison of unsigned integer with zero'
 * 	errors.  Changed the cast of a pointer to a long from an int.
 *
 * Revision 65.6  1998/03/23  16:24:44  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.5  1998/02/09 15:54:53  lmc
 * Changes to get paramaeters right to VOPX_xxx calls.
 *
 * Revision 64.5  1997/08/22  23:00:45  bdr
 * Fixup umask stuff.  Irix expects the umask bits to have been applied
 * above the VOP interface and the final file mode to be held in the
 * va_mode field.  For an IRIX DFS client we will be folding in the
 * umask twice once by the system call code (mkdir for example) and  then
 * once here in the DFS server.  This is ok.  We need to do this so that
 * we can honor umask values from non-Irix DFS client boxes that do not
 * fold in the umask into the va_mode for us on the client side.
 *
 * Revision 64.4  1997/07/10  15:47:18  gw
 * Added new style VOPX calls to support behaviors.
 *
 * Revision 64.3  1997/06/24  18:01:40  lmc
 * Changes for memory mapped files and the double locking problem which
 * is seen when dirty pages are hung off the vnode for memory mapped files.
 * The scp and dcp locks are dropped in cm_read.  Locks that are held
 * by a thread as a write lock are allowed to be recursively locked by
 * that same thread as a write lock.  Memory mapped files changed fsync
 * to add a flag telling whether or not it came from an msync.  Various
 * other changes to flushing and tossing pages.
 *
 * Revision 64.2  1997/04/16  18:52:05  lmc
 * Fix a uprintf for spelling and added a parameter.
 *
 * Revision 64.1  1997/02/14  19:46:20  dce
 * *** empty log message ***
 *
 * Revision 1.5  1996/05/09  18:56:40  bhim
 * Changed function definitions for all SAFS_* functions to match the
 * .idl generated prototypes.
 *
 * Revision 1.4  1996/05/06  21:01:08  brat
 * 64-bit bugfix. Pointers were being typecast to ints.
 *
 * Revision 1.3  1996/04/30  21:42:43  vrk
 * Handled some 64 bit porting issues for functions call thru ubik_call.
 *
 * Revision 1.2  1996/04/05  18:50:03  vrk
 * Redefine numByte in SAFS_StoreData() as idl_ulong_int.
 *
 * Revision 1.1  1994/08/29  21:38:45  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.946.4  1994/08/29  21:38:42  rsarbo
 * 	return error on fetch acl request from pre 1.1
 * 	acl_edit if acl has delegate entries.
 * 	[1994/08/29  20:07:27  rsarbo]
 *
 * Revision 1.1.946.2  1994/06/09  14:17:36  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:29:45  annie]
 * 
 * Revision 1.1.946.1  1994/02/04  20:27:16  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:16:44  devsrc]
 * 
 * Revision 1.1.944.1  1993/12/07  17:31:46  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  16:17:25  jaffe]
 * 
 * Revision 1.1.5.12  1993/01/21  14:53:58  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:58:02  cjd]
 * 
 * Revision 1.1.5.11  1993/01/13  18:18:44  shl
 * 	Transarc delta: cfe-ot6052-tkm-forced-revoke-protocol 1.1
 * 	Transarc delta: cfe-ot6054-genl-and-i18n-spares 1.1
 * 	Transarc delta: comer-ot6063-make-px-always-get-write-status-on-rename 1.1
 * 	Transarc delta: jaffe-cleanup-2.3-compiler-warnings 1.8
 * 	Transarc delta: kazar-ot6436-zap-unused-trunclength-refs 1.1
 * 	Transarc delta: kazar-ot6523-uuid-encodes-primary-secondary-pool-selection 1.3
 * 	Transarc delta: mcinerny-ot6067-rename-spares-in-vol_stat_st 1.2
 * 	Transarc delta: tu-ot5849a-ot5849b-ot6048-super-delta 1.2
 * 	Transarc delta: tu-ot5968-remove-obsolete-api 1.1
 * 	Transarc delta: tu-ot6421-px-tsr-ping-request 1.1
 * 	Transarc delta: tu-ot6669-tsr-px-output-status 1.1
 * 	[1993/01/12  21:48:35  shl]
 * 
 * Revision 1.1.5.10  1992/12/09  20:34:45  jaffe
 * 	Transarc delta: kazar-ot6305-increase-px-call-timeout 1.1
 * 	  Selected comments:
 * 	    file server hard timeout wasn't big enough
 * 	    increase file server call timeout to match the general "2 minutes" rule we've been
 * 	    using in DFS.
 * 	[1992/12/09  18:41:43  jaffe]
 * 
 * 	Transarc delta: kazar-ot6084-tkset-locking-problems 1.2
 * 	  Selected comments:
 * 	    tkset had a # of problems: didn't handle revoke during grant
 * 	    race condition, had deadlock during certain other races.
 * 	    use TKSET_RELOCK instead of TKM_RELOCK, since latter was just
 * 	    garbage definition, anyway.
 * 	    still could hang
 * 	[1992/12/04  17:57:14  jaffe]
 * 
 * Revision 1.1.5.9  1992/11/24  18:24:49  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:17:06  bolinger]
 * 
 * Revision 1.1.5.8  1992/11/18  20:55:40  jaffe
 * 	Transarc delta: kazar-ot5382-fix-cm-dir-open-access-check 1.1
 * 	  Selected comments:
 * 	    ensure that x and r rights for dirs are truly independently handled.
 * 	    fix incorrect check for enumerating dirs: "r" is the only right required
 * 	    for fetching a dir listing.
 * 	Transarc delta: kazar-ot5680-fix-copy-acl 1.1
 * 	  Selected comments:
 * 	    copy acl was assuming that null pointers could be passed across
 * 	    the wire
 * 	    use flag to see where to get ACL.
 * 	[1992/11/17  22:00:08  jaffe]
 * 
 * Revision 1.1.5.7  1992/11/04  18:32:10  jaffe
 * 	Transarc delta: cburnett-ot5713-fix-safs-gettoken-merge-bug 1.1
 * 	  Selected comments:
 * 	    SAFS_GetToken() had a merge error from the TSR merge into DFS 1.52
 * 	    Corrected merge error.
 * 	Transarc delta: cfe-ot5755-fshost-panics 1.2
 * 	  Selected comments:
 * 	    Track the panics from the fshost package.
 * 	    They seemed to be due to bzero'd callbackAddr structures on input.
 * 	    Discovered a reference problem!
 * 	    Clean up the binding generated by AFS_SetContext if its ownership has
 * 	    not been transferred.
 * 	[1992/11/04  17:10:39  jaffe]
 * 
 * Revision 1.1.5.6  1992/10/27  21:25:39  jaffe
 * 	Transarc delta: comer-ot4971-pass-gettoken-flags-to-token-manager 1.5
 * 	  Selected comments:
 * 	    tkset_AddTokenSet() was not passing all the appropriate flags to
 * 	    the token manager through ObtainSet().  I cleaned up the AddTokenSet
 * 	    interface and passed the right flags.
 * 	    Cleaned up the tkset_AddTokenSet inteface.
 * 	    Missed a place the first time.
 * 	    Don't want token optimism if looking for a WVT.
 * 	    Don't always request TKSET optimism.
 * 	    Fix merge problems.
 * 	    Choose the exported versions.
 * 	    Compilation errors.
 * 	Transarc delta: comer-ot5608-fix-merge-of-pxintops 1.1
 * 	  Selected comments:
 * 	    Fixed a merge problem in px_intops.c
 * 	    Spurious call to tkc_AddTokenSet().
 * 	Transarc delta: comer-ot5610-enable-fileset-move-tsr 1.3
 * 	  Selected comments:
 * 	    The code to enable the file exporter to go into move token state recovery
 * 	    was disabled by #ifdefs.  This delta removes them.
 * 	    See above.
 * 	    Extend this delta to debug a snafu in the original TSR spec: even if
 * 	    a fileset is marked with VOL_MOVE_SOURCE, the fts command itself wants to
 * 	    obtain tokens on it.
 * 	    Implement the new AFS_FLAG_MOVE_SOURCE_OK flag.
 * 	    Need to turn off the VOL_MOVE_SOURCE and VOL_MOVE_TARGET flags.
 * 	Transarc delta: kazar-ot5556-pass-umask-to-create-ops 1.2
 * 	  Selected comments:
 * 	    pass umask to create ops for default acl handling
 * 	    new interface for creating objects
 * 	    Fix unresolved symbol during link.
 * 	Transarc delta: tu-5547-fx-check-conflict-flags 1.1
 * 	  Selected comments:
 * 	    fx server should check the token request flag to make sure that caller would
 * 	    not use both AFS_FLAG_SKIPTOKEN and AFS_FLAG_RETURNTOKEN.
 * 	Transarc delta: tu-ot4507-ot4713-ot4824-superdelta 1.1
 * 	  Selected comments:
 * 	    This super-delta was created from configuration dfs-102, revision 1.48,
 * 	    by importing the following deltas:
 * 	    kazar-ot5055-cm-conn-bad-returns 1.1
 * 	    tu-ot4507-after-tsr-merge 1.1
 * 	    tu-ot4507-cm-tsr-part-2 1.9
 * 	    tu-ot4713-add-one-more-tokenflag 1.2
 * 	    tu-ot4834-cm-using-a-new-tgt 1.4
 * 	    From tu-ot4507-cm-tsr-part-2 1.9:
 * 	    An enhancement to the "Token State Recovery" 1) after a particular fileset is
 * 	    moved to another server or 2) after a server becomes accessible to the CM.
 * 	    1) Tokens after a Server Crash:
 * 	    The fundamental difference of the TSR strategy between DCE 1.0.1 and DCE 1.0.2
 * 	    is that the CM, in 1.0.2, is to try to "keep alive" existing running programs
 * 	    or other users applications. That is, when the server comes back the CM will
 * 	    allow users to continue using these running programs transparantly. An good
 * 	    example is that users will be able to continue using "emacs" after the server
 * 	    comes back.
 * 	    Moreover, the CM preserves changes made to any files at the time that
 * 	    the server is detected unreachable and tries to send, if still valid, the
 * 	    modified data back to the server.
 * 	    2) Tokens after a Fileset Move
 * 	    This is a new feature starting in 1.0.2.
 * 	    Upon detecting one particular fileset is moved to another machine, the CM
 * 	    will try its best to reestablish all tokens pertaining to this fileset from
 * 	    the target server. In essence, the CM is "moving" tokens from the source
 * 	    machine to the target marchine without disturbing end users' applications
 * 	    running on the CM.
 * 	    same as above
 * 	    More fixes
 * 	    more fixes
 * 	    Used a new fucntion, px_ComputeTokenRecoveryTime() to compute the remaining
 * 	    time for the recovery period.
 * 	    tyops
 * 	    From tu-ot4713-add-one-more-tokenflag 1.2:
 * 	    Our DFS should allow its client (ie., CM) to have an option of asking the
 * 	    server not to return the "optimistic" tokens if the CM  only wants
 * 	    a particular token.
 * 	    Made change to utilize the new flag.
 * 	    same
 * 	    more fixes
 * 	[1992/10/27  14:48:28  jaffe]
 * 
 * Revision 1.1.5.5  1992/09/25  18:51:40  jaffe
 * 	Transarc delta: cfe-ot5296-rpc-bounds 1.1
 * 	  Selected comments:
 * 	    Many RPC interfaces that used varying arrays were erring in one of two
 * 	    ways:
 * 	    (a) using last_is() attributes for the array control variable
 * 	    in the .idl file, but treating the boundary as if it were
 * 	    specified with length_is()
 * 	    (b) not always initializing the array control variable, particularly
 * 	    when an RPC returned a varying array but took an error exit.
 * 	    Either of these situations could produce a fault_invalid_bound exception
 * 	    from the RPC runtime, when the control variable was given a value outside
 * 	    the allowed range.  In case (a), this could happen when an RPC value was
 * 	    packed full (e.g. an entire [0..24] range was used in a 25-element-max array,
 * 	    and 25 was stored in the control value).  This would be a correct length_is()
 * 	    value, but one too large for a last_is() value.  In case (b), uninitialized
 * 	    storage could take any value, including values outside the [0..24] range.
 * 	    Case (b) might have occurred in Rx-era code, since Rx didn't send bulk
 * 	    return values when any non-zero result was returned from the RPC (unlike
 * 	    DCE RPC, which ships these varying-array OUT parameters regardless of
 * 	    procedure result).
 * 	    Case-(b) error in returning afsBulkVVs, plus some very weird-looking
 * 	    errors in that procedure eliminated (e.g. freeing unallocated memory).
 * 	Transarc delta: kazar-ot4474-sys-v-locking-async-grant 1.2
 * 	  Selected comments:
 * 	    start work on async grant
 * 	    finish work on async grant
 * 	    support async grant flag to gettokens
 * 	Transarc delta: kazar-ot5331-afs-gettoken-needs-status-read 1.1
 * 	  Selected comments:
 * 	    AFS_GetToken shouldn't return file status info unless
 * 	    a read token has been obtained
 * 	    Don't return file status from AFS_GetToken if status/read token wasn't
 * 	    obtained in this call.  Prevents bogus status from being seen and
 * 	    returned by VOPX_GETATTR call.
 * 	[1992/09/23  19:27:17  jaffe]
 * 
 * Revision 1.1.5.4  1992/09/23  15:40:26  delgado
 * 	 Fix error path in SAFS_Lookup so it will call VN_RELE
 * 	 on vnode (if there is one)
 * 	[1992/09/23  15:39:48  delgado]
 * 
 * Revision 1.1.5.3  1992/09/15  13:16:39  jaffe
 * 	Transarc delta: cfe-ot5030-more-rep-fixes 1.2
 * 	  Selected comments:
 * 	    Fixing more rep (and scaffolding) errors.
 * 	    Still doesn't work.
 * 	    Disable updates made to R/O filesets.
 * 	Transarc delta: kazar-ot4921-basic-cmfx-events 1.1
 * 	  Selected comments:
 * 	    add some trace points, fix up bugs in logging package.
 * 	    add trace points
 * 	Transarc delta: kazar-ot5227-set-times-on-new-obj-creates 1.1
 * 	  Selected comments:
 * 	    use client times when creating new dirs, symlinks and files
 * 	    set fields that VOPX_CREATE doesn't set
 * 	[1992/09/14  20:24:11  jaffe]
 * 
 * Revision 1.1.5.2  1992/08/31  20:50:39  jaffe
 * 	Transarc delta: cfe-ot4029-portable-rpc-data-types 1.2
 * 	  Selected comments:
 * 	    If ``long'' could possibly mean ``64 bits'' any time soon, we need to keep
 * 	    our RPC interfaces from breaking.
 * 	    see above
 * 	    More of the same.  Forgot a couple of .idl files, and needed to change
 * 	    a couple of procedure signatures to match.
 * 	    trivial signature change
 * 	Transarc delta: comer-ot3950-additional-px-vopx-protection-checks 1.1
 * 	  Selected comments:
 * 	    The PX does not do protection checks for vnode operations beyond
 * 	    what is done by the underlying file system.  This means that a
 * 	    program that knows the DFS protocol can do things it should not be
 * 	    able to.  The operations in question that are currently not
 * 	[1992/08/30  03:46:10  jaffe]
 * 
 * Revision 1.1.3.17  1992/07/03  01:54:15  mason
 * 	Transarc delta: cburnett-ot1000-fix-fclear-errs-regarding-devices 1.1
 * 	  Selected comments:
 * 	    Fix IBM sitb fclear test case failures.  The main fix was to
 * 	    add support for creating FIFO files.  This required adding support
 * 	    in cm_create and SAFS_CreateFile.  Also a small change was required
 * 	    to naix_fclear() to return to proper error code.
 * 	    Add support to SAFS_CreateFile for fifos and fix minor bug.
 * 	Transarc delta: kazar-ot3933-mtime-update-on-close 1.1
 * 	  Selected comments:
 * 	    mtime wasn't being updated properly on close
 * 	    have afs_storedata preserve mtime
 * 	Transarc delta: kazar-ot4358-token-return-etc-idl-change 1.1
 * 	  Selected comments:
 * 	    New IDL for ACL cache entry expiration, and for piggybacked token return.
 * 	    return token passed in for rmdir, rm and rename
 * 	[1992/07/02  19:33:42  mason]
 * 
 * Revision 1.1.3.16  1992/07/02  21:52:47  mason
 * 	Transarc delta: cburnett-ot1000-fix-fclear-errs-regarding-devices 1.1
 * 	  Selected comments:
 * 	    Fix IBM sitb fclear test case failures.  The main fix was to
 * 	    add support for creating FIFO files.  This required adding support
 * 	    in cm_create and SAFS_CreateFile.  Also a small change was required
 * 	    to naix_fclear() to return to proper error code.
 * 	    Add support to SAFS_CreateFile for fifos and fix minor bug.
 * 	Transarc delta: kazar-ot3933-mtime-update-on-close 1.1
 * 	  Selected comments:
 * 	    mtime wasn't being updated properly on close
 * 	    have afs_storedata preserve mtime
 * 	Transarc delta: kazar-ot4358-token-return-etc-idl-change 1.1
 * 	  Selected comments:
 * 	    New IDL for ACL cache entry expiration, and for piggybacked token return.
 * 	    return token passed in for rmdir, rm and rename
 * 	[1992/07/02  19:33:42  mason]
 * 
 * Revision 1.1.3.15  1992/06/22  16:14:10  mason
 * 	Transarc delta: kazar-ot4085-token-discretion 1.1
 * 	  Selected comments:
 * 	    don't always return tokens when not really needed
 * 	    check for tokens early
 * 	[1992/06/22  15:52:23  mason]
 * 
 * Revision 1.1.3.14  1992/06/10  21:32:22  mason
 * 	Transarc delta: kazar-otosfxx-dir-list-byte-order 1.2
 * 	  Selected comments:
 * 	    used a byte-swapped # after byte swapping it
 * 	    fix recordlen computation to use host order, not net order.
 * 	[1992/06/10  21:28:41  mason]
 * 
 * Revision 1.1.3.13  1992/06/10  19:03:55  rsarbo
 * 	put dir_namelen into network byte order AFTER using locally in NDIRSIZE
 * 	[1992/06/10  18:42:55  rsarbo]
 * 
 * Revision 1.1.3.12  1992/06/08  17:54:13  mason
 * 	Transarc delta: kazar-otosfxx-dir-list-byte-order 1.1
 * 	  Selected comments:
 * 	    dir byte streams were transmitted in host byte order
 * 	    write dir byte stream in net byte order.
 * 	[1992/06/08  17:49:12  mason]
 * 
 * Revision 1.1.3.11  1992/06/01  20:41:45  mason
 * 	Transarc delta: tu-ot3458-fshost-stale-etc 1.4
 * 	  Selected comments:
 * 	    Removed the use of activeCall field.
 * 	[1992/06/01  17:41:21  mason]
 * 
 * Revision 1.1.3.10  1992/05/28  17:24:36  garyf
 * 	fix OSF/1 compilation warnings
 * 	[1992/05/28  17:23:35  garyf]
 * 
 * Revision 1.1.3.9  1992/05/20  20:00:01  mason
 * 	Transarc delta: comer-ot2672-tsr-fshost-px 1.4
 * 	  Files modified:
 * 	    cm: cm_express.c; config: common_def.h
 * 	    fshost: fshs.h, fshs_host.c, fshs_host.h, fshs_hostops.c
 * 	    fshost: fshs_subr.c; host: hs_errs.et
 * 	    px: px.h, px_init.c, px_intops.c, px_remove.c, px_repops.c
 * 	    pxd: pxd.c
 * 	    tkc: tkc.c, tkc.h, tkc_cache.c, tkc_locks.c, tkc_tokens.c
 * 	    tkset: tkset.c, tkset.h; volreg: volreg.c; xaggr: ag_init.c
 * 	  Selected comments:
 * 	    File server, fshost, and tkset changes to support changes
 * 	    for improved token-state recovery.  Specific changes are
 * 	    documented in "File Exporter Token State Recovery".
 * 	    Added code to all SAFS_xxx functions to make them support
 * 	    a post-recovery period when only token reestabllishment is
 * 	    allowed.  Also, changed volreg_Lookup calls to understand
 * 	    ENODEV return codes and also to handle token-move recovery
 * 	    semantics.
 * 	    Fixed compile errors.
 * 	    Changed TKC code to use new parameter for tkn_ReturnToken
 * 	    CFE: fix typos in a couple of files
 * 	Transarc delta: kazar-ot3034-bad-time-mgmt 1.1
 * 	  Files modified:
 * 	    cm: cm_dcache.c, cm_scache.c, cm_scache.h, cm_subr.c
 * 	    cm: cm_vnodeops.c; fsint: afs4int.idl
 * 	    px: px.h, px_intops.c, px_subr.c
 * 	  Selected comments:
 * 	    mtime and ctime are version numbers as well as timestamps,
 * 	    and must be managed carefully (only increase, except during
 * 	    utimes call)
 * 	    stuff
 * 	Transarc delta: tu-ot2642-fix-incomplete 1.3
 * 	  Files modified:
 * 	    cm: cm_daemons.c, cm_dcache.c, cm_dnamehash.c, cm_pioctl.c
 * 	    cm: cm_rrequest.c, cm_scache.c, cm_scache.h, cm_server.c
 * 	    cm: cm_server.h, cm_tknimp.c, cm_tokens.c, cm_vnodeops.c
 * 	    px: px.h, px_intops.c, px_remove.c, px_repops.c
 * 	    tkm: tkm_afsFile.c, tkm_control.c, tkm_dblULongs.h
 * 	    tkm: tkm_exceptions.c, tkm_fidHash.c, tkm_fidHash.h
 * 	  Selected comments:
 * 	    Fix the merge problem occurred between cm lock and tsr-cm.
 * 	    more fixes
 * 	    typo
 * 	    Also fix bugs introduced by the TSR.
 * 	    Must adjust the cell id before calling tkm_ReturnToken().
 * 	    SAFS_RemoveFile should return an encoded error.
 * 	[1992/05/20  11:52:06  mason]
 * 
 * Revision 1.1.3.8  1992/05/05  04:31:44  mason
 * 	delta kazar-ot2816-core-leaks 1.1
 * 	[1992/04/27  22:04:08  mason]
 * 
 * 	delta cburnett-ot2742-fix_px_intops_merge_conflicts 1.1
 * 	[1992/04/27  12:15:31  mason]
 * 
 * Revision 1.1.3.7  1992/04/21  14:21:45  mason
 * 	Transarc delta: cburnett-ot2594-change_assert_to_osi_assert 1.1
 * 	  Files modified:
 * 	    fp: fp.c; fshost: fshs_princ.c; px: px_intops.c
 * 	    tkc: tkc_cache.c
 * 	    tkm: tkm_fidHash.c, tkm_getToken.c, tkm_grid.c
 * 	    tkm: tkm_hostHash.c, tkm_locks.c, tkm_tokenList.c
 * 	    tkm: tkm_tokens.c; ufsops: ufs_volops.c; xvolume: vol_desc.c
 * 	  Selected comments:
 * 	    [ replace assert with portable osi_assert.  This also is part of the
 * 	      preperation for the removal of the assert redefination in config/debug.h
 * 	      and the eventual move of DFS to AIX 3.2]
 * 	    [ assert to osi_assert ]
 * 	Transarc delta: tu-1143-error-mapping 1.8
 * 	  Files modified:
 * 	    cm: cm.h, cm_cell.c, cm_conn.c, cm_daemons.c, cm_dcache.c
 * 	    cm: cm_dnamehash.c, cm_init.c, cm_pioctl.c, cm_rrequest.c
 * 	    cm: cm_scache.c, cm_server.c, cm_subr.c, cm_tokens.c
 * 	    cm: cm_vnodeops.c, cm_volume.c; fsprobe: fsprobe.c
 * 	    libafs/RIOS: dfscore.exp; osi: Makefile, osi_dfserrors.h
 * 	    osi.klib/OSF1: Makefile; osi.klib/RIOS: Makefile
 * 	    osi/OSF1: Makefile, osi_dfs2osferr.c, osi_osf2dfserr.c
 * 	    osi/RIOS: Makefile, osi_aix2dfserr.c, osi_dfs2aixerr.c
 * 	    px: px_init.c, px_intops.c; rep: rep_main.c
 * 	    userInt/fts: volc_tokens.c
 * 	  Selected comments:
 * 	    An enhancement to allow the DFS server to map returned kernel errors to the
 * 	    canonical ones and to allow the CM to map the canonical ones to the
 * 	    appropriate local kernel errors.
 * 	    Please refer to the design memo, Error Conversion Protocol, for more detail.
 * 	    For each manager routine, added code to map a returned kernel error to an
 * 	    appropriate canonical (DFS) error.
 * 	    The same as above.
 * 	    Made osi_dfserrors.h and osi_dfs2aixerr.c osi_aix2dfserr.c osi_osf2dfserr.c and
 * 	    osi_dfs2osferr.c avaliable in the user space. So, whoever references AFS_xxx
 * 	    APIs should use the error conversion protocol.
 * 	    Added error conversion call to those un-implemented SAFS manager routines.
 * 	    fixed some typo errors and others
 * 	    Changes did not compile on the PMAX.
 * 	    DFS servers now maps errors to the canonical ones before returning to theirs
 * 	    clients. Therefore, those clients who use AFS_xxx rpc calls must do the
 * 	    error decoding work to get the appropriate error back.
 * 	    CFE adds osi_initDecodeTable() and osi_errDecode() to repserver and fts.
 * 	[1992/04/20  22:54:12  mason]
 * 
 * Revision 1.1.3.6  1992/04/14  03:51:43  mason
 * 	Transarc delta: tu-1143-error-mapping 1.6
 * 	  Files modified:
 * 	    cm: cm.h, cm_cell.c, cm_conn.c, cm_daemons.c, cm_dcache.c
 * 	    cm: cm_dnamehash.c, cm_init.c, cm_pioctl.c, cm_rrequest.c
 * 	    cm: cm_scache.c, cm_server.c, cm_subr.c, cm_tokens.c
 * 	    cm: cm_vnodeops.c, cm_volume.c; libafs/RIOS: dfscore.exp
 * 	    osi: Makefile, osi_dfserrors.h; osi.klib/OSF1: Makefile
 * 	    osi.klib/RIOS: Makefile
 * 	    osi/OSF1: Makefile, osi_dfs2osferr.c, osi_osf2dfserr.c
 * 	    osi/RIOS: Makefile, osi_aix2dfserr.c, osi_dfs2aixerr.c
 * 	    px: px_init.c, px_intops.c
 * 	  Selected comments:
 * 	    An enhancement to allow the DFS server to map returned kernel errors to the
 * 	    canonical ones and to allow the CM to map the canonical ones to the
 * 	    appropriate local kernel errors.
 * 	    Please refer to the design memo, Error Conversion Protocol, for more detail.
 * 	    For each manager routine, added code to map a returned kernel error to an
 * 	    appropriate canonical (DFS) error.
 * 	    The same as above.
 * 	    Made osi_dfserrors.h and osi_dfs2aixerr.c osi_aix2dfserr.c osi_osf2dfserr.c and
 * 	    osi_dfs2osferr.c avaliable in the user space. So, whoever references AFS_xxx
 * 	    APIs should use the error conversion protocol.
 * 	    Added error conversion call to those un-implemented SAFS manager routines.
 * 	    fixed some typo errors and others
 * 	    Changes did not compile on the PMAX.
 * 	Transarc delta: tu-ot1155-px-intops-set-mask 1.3
 * 	  Files modified:
 * 	    osi: osi_port.h; px: px_intops.c
 * 	  Selected comments:
 * 	    Fixed OT-1155 (Orbit#4532)
 * 	    The FX server must set the u.u_cmask to 0 first before starting any services.
 * 	    Otherwise, the file creation mask bit would be incorrectly set.
 * 	    Added a new osi_SetCMASK macro to do this.
 * 	    Call osi_SetCMASK(0) at AFS_CreateFile, AFS_Symlink and AFS_MakeDir.
 * 	    cosmetic
 * 	    Same as above
 * 	    include osi_user.h
 * 	Transarc delta: tu-ot2329-px-pipe-error 1.3
 * 	  Files modified:
 * 	    px: px_intops.c, px_subr.c
 * 	  Selected comments:
 * 	    PX should not return the "pipe_disipline" error" (should return a proper one)
 * 	    when failed at pipe-related rpc calls such as AFS_Store, Readdir or FetchData
 * 	    before the manager routine starts pushing/pulling the data.
 * 	    Verify that the pipe is empty or not
 * 	    same as above
 * 	    cosmetic
 * 	    same as above
 * 	    using osi_AllocBuffer to get pbuf instead of a static one.
 * 	[1992/04/13  17:08:13  mason]
 * 
 * Revision 1.1.3.5  1992/03/17  16:32:04  delgado
 * 	Remove assignment statements from within assert macros
 * 	[1992/03/17  16:31:26  delgado]
 * 
 * Revision 1.1.3.4  1992/01/25  20:48:56  zeliff
 * 	dfs6.3 merge, part2
 * 	[1992/01/25  20:07:33  zeliff]
 * 
 * $EndLog$
 */
/*
 *      Copyright (C) 1990 Transarc Corporation
 *      All rights reserved.
 */
#include <dcedfs/param.h>
#include <dcedfs/stds.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/scx_errs.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/aggr.h>
#include <dcedfs/volume.h>
#include <dcedfs/vol_errs.h>
#include <dcedfs/xcred.h>
#include <dcedfs/xvfs_export.h>
#include <dcedfs/dacl.h>
#include <dcedfs/aclint.h>
#include <dcedfs/tkset.h>
#include <dcedfs/fshs.h>
#include <dcedfs/osi_user.h>
#include <dcedfs/osi_dfserrors.h>
#include <dcedfs/icl.h>
#include <dcedfs/syscall.h>
#include <dcedfs/volreg.h>
#include <dcedfs/vol_init.h>
#include <dcedfs/tkc.h>			/* gluevn functions */
#include <px.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/px/RCS/px_intops.c,v 65.8 1999/02/05 16:54:31 mek Exp $")

#define PX_OPEN_TOKENS	(TKN_OPEN_READ | TKN_OPEN_WRITE | TKN_OPEN_SHARED | \
			 TKN_OPEN_EXCLUSIVE | TKN_OPEN_DELETE | \
			 TKN_OPEN_PRESERVE)

/*
 * check for tokens on store RPCs ?
 */

#define PX_NO_TOKEN_CHECK 0
#define PX_CHECK_AND_TRACE 1
#define PX_CHECK_AND_REJECT 2

#define InitToken(_tokenp, _type) \
MACRO_BEGIN     \
    afs_token_t *tkp = _tokenp; \
    bzero((caddr_t)tkp, sizeof (afs_token_t)); \
    AFS_hset64(tkp->type, 0, _type); \
    tkp->endRange = osi_hMaxint64; \
MACRO_END

int px_checkStoreTokens = 1;

struct afsIntStats afsStats;	/* Keeps global exporter stats */
extern char protSeq[];

/*
 * Macros to wrap the volreg_Lookup & export check together
 */
/*
 * This proc and the following one contain changes to support
 * token recovery after a fileset move.  They basically do the
 * following:
 * if lookup fails with ENODEV, return VOLERR_PERS_DELETED
 * else if fileset is busy, return EBUSY
 * else if VOL_MOVE_SOURCE set in the volume status,
 *	attempt to time it out, if can't return VOLERR_PERS_DELETED
 * else if VOL_MOVE_TARGET set in volume status,
 *	attempt to time out, if can't and call is not
 *	AFS_GetToken(), return EBUSY
 * lookup succeeded
 *
 */
static int pxreg_Lookup(
  afsFid *fidp,
  int type,
  opaque *dmptrp,
  struct volume **ovolp,
  struct vnode **ovp)
{
#ifdef SGIMIPS
    int code;
#else
    long code;
#endif
    struct volume *volp;
    struct vnode *vp;

#ifdef SGIMIPS
    code = (int)volreg_Lookup(fidp, &volp);
#else
    code = volreg_Lookup(fidp, &volp);
#endif
    if (code == ENODEV) code = VOLERR_PERS_DELETED;
    else if (!code) {
	lock_ObtainRead(&volp->v_lock);
	if (!VOL_EXPORTED(volp)) {
	    lock_ReleaseRead(&volp->v_lock);
	    code = EACCES;
	    VOL_RELE(volp);
	} else
	    lock_ReleaseRead(&volp->v_lock);
    }
    if (code) return code;

    /* get vnode after pinning volume down */
    code = vol_StartVnodeOp(volp, type, VOL_XCODE_NOWAIT, dmptrp);
    if (code) {
	VOL_RELE(volp);
	return code;
    }
    code = VOL_VGET(volp, fidp, &vp);
    if (code) {
	vol_EndVnodeOp(volp, dmptrp);
	VOL_RELE(volp);
	return code;
    }

    icl_Trace3(px_iclSetp,PX_TRACE_FID_LOOKUP, 
	       ICL_TYPE_FID, fidp,
	       ICL_TYPE_POINTER, volp,
	       ICL_TYPE_POINTER, vp);
    *ovolp = volp;
    *ovp = vp;
    return 0;
}

/* same as above, only don't bump the vnode count (it'll be done
 * manually by our caller, if need be), don't return a vnode pointer,
 * and, if moveSourceOK is set, ignore the VOL_MOVE_SOURCE flag
 * (this last is a feature used by SAFS_GetToken).
 */
static int pxreg_Lookup2(
  afsFid *fidp,
  struct volume **ovolp)
{
#ifdef SGIMIPS
    int code;
#else
    long code;
#endif
    struct volume *volp;
    struct vnode *vp;

#ifdef SGIMIPS
    code = (int)volreg_Lookup(fidp, &volp);
#else
    code = volreg_Lookup(fidp, &volp);
#endif
    if (code == ENODEV) code = VOLERR_PERS_DELETED;
    else if (!code) {
	lock_ObtainRead(&volp->v_lock);
	if (!VOL_EXPORTED(volp)) {
	    lock_ReleaseRead(&volp->v_lock);
	    code = EACCES;
	    VOL_RELE(volp);
	} else
	    lock_ReleaseRead(&volp->v_lock);
    }
    if (code) return code;

    /* return the volume */
    *ovolp = volp;
    return 0;
}

void SetClientSize(struct fshs_host *hostp, unsigned value)
{
    if (hostp == NULL || value == 0)
	return;

    hostp->maxFileParm = value;
    hostp->maxFileSize = osi_MaxFileParmToHyper(value);
    /*
     * If the remote host didn't set a value or if it is bogus (code may not
     * implemented on server yet), set maxFileSize to the default.  Also check
     * for anything but 2^31-2^0, which was the value originally used, or the
     * new flag bit.  This detects when the remote system needs the backward
     * compatibility features for 32bit servers.
     */
    if (!AFS_hiszero(hostp->maxFileSize) &&
	((value & AFS_CONN_PARAM_SUPPORTS_64BITS) ||
	 (AFS_hcmp(hostp->maxFileSize, osi_hMaxint32) != 0))) {
	hostp->supports64bit = 1;
	icl_Trace2(px_iclSetp, PX_TRACE_SETCLIENTSIZE,
		   ICL_TYPE_LONG, hostp,
		   ICL_TYPE_HYPER, &hostp->maxFileSize);
    } else {
	hostp->maxFileSize = osi_maxFileSizeDefault;
	icl_Trace2(px_iclSetp, PX_TRACE_SETCLIENTSIZE_OLD,
		   ICL_TYPE_LONG, hostp,
		   ICL_TYPE_HYPER, &hostp->maxFileSize);
    }
}

/*
 * 32/64-bit interoperability: make sure we don't return a handle for a
 * file too long for an old client.
 */
#define DoLargeFileOldClient(statusP, princp) \
    ((!(princp)->hostp->supports64bit && \
      AFS_hcmp((statusP)->length, osi_hMaxint32) > 0) ? EOVERFLOW : 0)

/*
 * Make sure that caller would not pass both AFS_FLAG_SKIPTOKEN and
 * AFS_FLAG_RETURNTOKEN at the same time.
 */

#define InvalidTokenFlags(Flags)   \
      ((Flags & AFS_FLAG_RETURNTOKEN)   && \
       (Flags & AFS_FLAG_SKIPTOKEN)) \


#ifdef SGIMIPS
error_status_t
SAFS_SetContext (
     handle_t      h,			/* in: the RPC binding */
     unsigned32 epochTime,		/* in: client's start time of rpc */
     afsNetData  *callbackAddr,		/* in: ptr to the generic net addr */
     unsigned32 Flags,			/* in: 1 is requesting for secondary
                                         * svc, also AFS_FLAG_CONTEXT_xxx
                                         * options */
     afsUUID *otherUUID,		/* secondary service uuid */
     unsigned32 clientSizesAttrs,	/* Client declarations */
     unsigned32 parm7)			/* not used */
#else
unsigned32
SAFS_SetContext (h, epochTime, callbackAddr, Flags, otherUUID,
		 clientSizesAttrs, parm7)
     handle_t      h;			/* in: the RPC binding */
     unsigned32 epochTime;		/* in: client's start time of rpc */
     afsNetData  *callbackAddr;		/* in: ptr to the generic net addr */
     unsigned32 Flags;			/* in: 1 is requesting for secondary
                                         * svc, also AFS_FLAG_CONTEXT_xxx
                                         * options */
     afsUUID *otherUUID;		/* secondary service uuid */
     unsigned32 clientSizesAttrs;	/* Client declarations */
     unsigned32 parm7;			/* not used */
#endif /* SGIMIPS */
{
    int preempting;			/* kernel preemption lock */
    struct context *cookie;
    struct fshs_principal *princp;
    struct fshs_host *hostp;
    struct sockaddr_in *ipaddrPtr;
    u_char *princNamePtr;
#ifdef SGIMIPS
    int errorCode = 0;
    int normalCode = 0;
#else
    long errorCode = 0;
    long normalCode = 0;
#endif

    preempting = osi_PreemptionOff();
    icl_Trace1(px_iclSetp, PX_TRACE_SETCONTEXT, ICL_TYPE_LONG, Flags);
    afsStats.SetContext++, afsStats.TotalCalls++;

    if (fshs_postRecovery)
	px_ComputeTokenRecoveryTime(0);
    /*
     * NOTE: We don't check whether the server is in post-crash period or
     *       not in this particular function. Otherwise, clients could not
     *       start the TSR work.
     */
    if (epochTime == 0) {
       errorCode = EINVAL;
       goto out1;
    }
    /*
     * This called is to establish a context for the remote user whenever the
     * user initiates a new rpc binding to the server. A "context" for the
     * remote end user is composed of two basic data structures; the "host"
     * and "principal". The "host" is used, by PX, to identify a particular
     * instance of a client from which the rpc call originated. The "principal"
     * is to represent the remote caller from that client. These two objects
     * will be GCed if they are not referenced for a period of time.
     */
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
        goto out1;

    /*
     * See if host needs to be created.  It is only during creation that
     * token state is initiated, so if the guy is down, but a host struct
     * exists, we won't create a new entry now, but will go down to
     * fshs_GetPrincipal below, and return an error code that says in
     * effect to try again with a new context.
     */
    hostp = fshs_GetHost(cookie);
    /*
     * Check to see if this host needs to be reinitialized because the local
     * port changed on the client side.
     */
    if (hostp && callbackAddr->sockAddr.type == AF_INET
	&& hostp->clientAddr.sin_port !=
	((struct sockaddr_in *)&callbackAddr->sockAddr)->sin_port) {
	/* Check authn/authz before changing the host pointer. */
	errorCode = fshs_CheckAuthn(cookie, hostp);
	fshs_PutHost(hostp);
	hostp = NULL;
	if (errorCode)
	    goto out;
    }
    if (hostp == NULL) {
	princNamePtr = (u_char *) 0;
	if (callbackAddr->sockAddr.type != 0) {
	    ipaddrPtr = (struct sockaddr_in *) &callbackAddr->sockAddr;
	    if (callbackAddr->principalName[0] == '/')
		princNamePtr = (u_char *) &callbackAddr->principalName;
	} else {
	    ipaddrPtr = (struct sockaddr_in *) 0;
	}
	if (Flags & AFS_FLAG_SEC_SERVICE) {
	    cookie->needSecondarySvc = 1;
	}
	cookie->epochTime  = epochTime;
	cookie->initContext = 1;
	/* CreateHost has two suboptions.  If ipaddrPtr->type == 0,
	 * then we don't need to setup a callback binding.  If
	 * cookie->needSecondarySvc is not set, then we are a primary
	 * interface call and we don't make calls on the secondary
	 * interface.  I believe that these are independent parameters.
	 * Note that otherUUID is really the *other* uuid, and refers
	 * to the primary if this is a secondary call.
	 */
#ifdef SGIMIPS
	errorCode = (int)fshs_CreateHost(cookie, ipaddrPtr, otherUUID, h,
#else
	errorCode = fshs_CreateHost(cookie, ipaddrPtr, otherUUID, h,
#endif
				    princNamePtr, Flags, &hostp);

	if (hostp) {
	    SetClientSize(hostp, clientSizesAttrs);
	    fshs_PutHost(hostp);
	}

	if (errorCode && errorCode != TKN_ERR_NEW_IF)
	    goto out;
	normalCode = errorCode;

    } else {
	SetClientSize(hostp, clientSizesAttrs);
	fshs_PutHost(hostp);
	normalCode = (Flags & AFS_FLAG_CONTEXT_NEW_IF)?
#ifdef SGIMIPS
	    (int)TKN_ERR_NEW_IF : 0;
#else
	    TKN_ERR_NEW_IF : 0;
#endif
    }


    /* when we get here, host has been created; return proper error
     * code if there's something wrong with it, for example, if it is
     * down.
     */
    errorCode = fshs_GetPrincipal (cookie, &princp, 0);
    if (errorCode == 0) {
	fshs_PutPrincipal(princp);
	errorCode = normalCode;
    }
out:
    osi_Free((char *)cookie, sizeof (struct context));
out1:
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * Get root vnode id and unique of a particular fileset.
 */
#ifdef SGIMIPS
error_status_t 
SAFS_LookupRoot(
     handle_t h,
     afsFid *InFidp,			/* incomplete afsfid for the fileset */
     afs_hyper_t *minVVp,		/* min acceptable volume version num */
     unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
     afsFid *OutFidp,			/* Fid with complete info of vnode */
     afsFetchStatus *OutFidStatusp,	/* Output status for new file */
     afs_token_t *OutTokenp,		/* Return Token promise for new file */
     afsVolSync  *Syncp)		/* volume synchronization parm */
#else
unsigned32 
SAFS_LookupRoot(h, InFidp, minVVp, Flags, OutFidp, OutFidStatusp,
                           OutTokenp, Syncp)
     handle_t h;
     afsFid *InFidp;                    /* incomplete afsfid for the fileset */
     afs_hyper_t *minVVp;               /* min acceptable volume version num */
     unsigned long   Flags;             /* Skip Tokens, SYNC or ZERO, etc.. */
     afsFid *OutFidp;                   /* Fid with complete info of vnode */
     afsFetchStatus *OutFidStatusp;     /* Output status for new file */
     afs_token_t *OutTokenp;            /* Return Token promise for new file */
     afsVolSync  *Syncp;                /* volume synchronization parm */
#endif /* SGIMIPS */

{

    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with InFid */
    struct vnode *rootvp;		/* Vnode associated with InFid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    struct vattr vattr;			/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    int preempting;			/* kernel preemption lock */
    struct context *cookie;
    long retryCount = 0;
    int code = 0, errorCode = 0;
    int alreadyHasTokens;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    icl_Trace1(px_iclSetp, PX_TRACE_LOOKUPROOT, ICL_TYPE_HYPER, &InFidp->Volume);
    afsStats.LookupRoot++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
        errorCode = EINVAL;
	goto Bad_LookupRoot1;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_LookupRoot1;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(InFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
        goto Bad_LookupRoot1;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
        goto Bad_LookupRoot2;
    /*
     * Get unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with the fileset.
     * The returned vnode should point to the appropriate (VnodeId, Unique).
     */
  Retry_LookupRoot:
    tset = (struct tkset_set *) 0;	/* move initialization here so that any loop pass is OK */
    rootvp = (struct vnode *) 0;
    volp = (struct volume *) 0;
    errorCode = pxreg_Lookup2(InFidp, &volp);
    if (errorCode)
        goto Bad_LookupRoot3;
    errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READONLY, VOL_XCODE_NOWAIT,
				 &dmptr);
    if (errorCode) {
	VOL_RELE(volp);
	goto Bad_LookupRoot3;
    }
    errorCode = VOL_ROOT(volp, &rootvp);
    if (errorCode) {
        goto Bad_LookupRoot3b;
    }
    XVFS_CONVERT(rootvp);
    FidCopy(OutFidp, InFidp); /* Copy Cell & Volume info */
#ifdef SGIMIPS
    VOPX_AFSFID(rootvp, OutFidp, 0, errorCode);
#else
    errorCode = VOPX_AFSFID(rootvp, OutFidp, 0);
#endif
    if (errorCode)
        goto Bad_LookupRoot4;

   /*
    * Get read status tokens for the root of fileset.
    * IF the caller has tokens already, we will NOT request them.
    * Do check before adding token to token set, or we'll find our new
    * token!
    */
    alreadyHasTokens = px_ClientHasTokens(fshs_GetPrincHost(princp), OutFidp);
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(retryCount);
	tkset_SetVolSync(tset, volp, VNOP_TYPE_READONLY);
	InitToken(&ltoken, TKN_STATUS_READ | TKN_OPEN_PRESERVE);
	errorCode = tkset_AddTokenSet
	    (tset, OutFidp, &ltoken, NULL, fshs_GetPrincHost(princp),
	     (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
	if (errorCode) {
	    /* volume released by tkset, so cleanup and retry */
	    if (errorCode != TKSET_RELOCK) {
		volp = (struct volume *) 0;
		goto Bad_LookupRoot5;	/* go delete the tset */
	    }
	    else {
		/* need to release volume ourselves */
		OSI_VN_RELE(rootvp);
		vol_EndVnodeOp(volp, &dmptr);
		VOL_RELE(volp);
		tkset_Delete(tset);
		retryCount++;	/* tells tkset we're looping */
		goto Retry_LookupRoot;
	    }
	}
    }
#ifdef SGIMIPS
    VOPX_GETATTR(rootvp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(rootvp, &xvattr, 1, credp);
#endif
    if (errorCode)
        goto Bad_LookupRoot5;
    px_xvattr_to_afsstatus(&xvattr, OutFidStatusp, princp);
    px_SetSync(volp, Syncp, 0, credp);

    if (!errorCode) {
	if ((Flags & AFS_FLAG_RETURNTOKEN) && !alreadyHasTokens &&
	    !DoLargeFileOldClient(OutFidStatusp, princp))
	    errorCode = px_SetTokenStruct(tset, OutFidp, OutTokenp, princp);
	else
	    px_ClearTokenStruct(OutTokenp);
    }

Bad_LookupRoot5:
    if (code = tkset_Delete(tset)) {
	uprintf("fx: failed on tkset_Delete; code = %d\n", code);
	if (errorCode == 0)
	    errorCode = code;    /* To preserve incoming errorCode */
    }
Bad_LookupRoot4:
    OSI_VN_RELE(rootvp);
Bad_LookupRoot3b:
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_LookupRoot3:
    fshs_PutPrincipal(princp);
Bad_LookupRoot2:
    osi_Free((char *)cookie, sizeof (struct context));
Bad_LookupRoot1:
    icl_Trace3(px_iclSetp, PX_TRACE_LOOKUPROOTEND,
	       ICL_TYPE_LONG, OutFidp->Vnode,
	       ICL_TYPE_LONG, OutFidp->Unique,
	       ICL_TYPE_LONG, errorCode);
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);

}

/*
 * FetchData RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_FetchData (
    handle_t h,
    afsFid *Fidp,			/* Fid of file to fetch */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    afs_hyper_t *Position,		/* Offset in the file */
    signed32 Length,			/* Amount to read */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp,		/* Returned status for Fid */
    afs_token_t *OutTokenp,		/* If r/w return Token for Fid */
    afsVolSync *Syncp,			/* volume synchronization info */
    pipe_t  *streamPtr)			/* ptr to input data string pipe */
#else
unsigned32
SAFS_FetchData (h, Fidp, minVVp, Position, Length, Flags,
			   OutStatusp, OutTokenp, Syncp, streamPtr)
    handle_t h;
    afsFid *Fidp;			/* Fid of file to fetch */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    afs_hyper_t *Position;		/* Offset in the file */
    long Length;			/* Amount to read */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp;		/* Returned status for Fid */
    afs_token_t *OutTokenp;		/* If r/w return Token for Fid */
    afsVolSync *Syncp;			/* volume synchronization info */
    pipe_t  *streamPtr;			/* ptr to input data string pipe */
#endif /* SGIMIPS */
{
    volatile int code, errorCode = 0;	/* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with Fid */
    struct vnode *vp;			/* Vnode associated with Fid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset=0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;		        /* Token for this Fid */
    volatile int preempting;            /* kernel preemption lock */
    int pipeDrained = 0;		/* whether the pipe is drained */
    int volpDeactivated = 0;		/* whether the activeVnop count is down already */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace4(px_iclSetp, PX_TRACE_FETCHDATA, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, AFS_hgetlo(*Position), ICL_TYPE_LONG, Length,
	       ICL_TYPE_LONG, Flags);
    afsStats.FetchData++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
	errorCode = EINVAL;
	goto Bad_FetchData4;
    }

    {   /* Bounds check *Position and Length against max supported file size;
         * truncate requested Length if necessary. */

	afs_hyper_t temp;

	if (AFS_hcmp(*Position, osi_maxFileServer) >= 0) {
	    errorCode = EFBIG;
	    goto Bad_FetchData4;
	}
	temp = osi_maxFileServer;
	AFS_hsub(temp, *Position);
#ifdef SGIMIPS
/* Turn off "pointless comparison of unsigned integer with zero" error." */
#pragma set woff 1183
#endif
	if (AFS_hcmp64(temp, 0, Length) < 0)
	    Length = AFS_hgetlo(temp);
#ifdef SGIMIPS
#pragma reset woff 1183
#endif
    }

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_FetchData4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * the request
     */
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_FetchData4;

    px_AdjustCell(Fidp);
    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_FetchData3;

    /*
     * Get status and data read tokens for the associated file
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_READ | TKN_DATA_READ);
	ltoken.beginRange = *Position;
	ltoken.endRange = *Position;
	AFS_hadd32(ltoken.endRange, Length - 1);
	errorCode = tkset_AddTokenSet(tset, Fidp, &ltoken, NULL,
				      fshs_GetPrincHost(princp), 0, &dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_FetchData1;	/* go delete the tset */
    }
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with Fidp
     */
    errorCode = pxreg_Lookup(Fidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &vp);
    if (errorCode)
	goto Bad_FetchData1;

    /* before doing the fetching, make sure that we don't do more token
     * work in the fault routines called should px_rdwr actually use the VM
     * system to do the work.
     */
    /*
     * Do the actual fetching. On success return the updated file's attributes
     * (in OutStatusp) along with a token promise (if a r/w file)
     */
    errorCode =	px_rdwr(vp, UIO_READ, Position, Length, credp,
			streamPtr, /* flags */ 0, &pipeDrained,
			volp, &volpDeactivated, &dmptr);
    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
	    px_SetSync(volp, Syncp, 0, credp);
	    if (Flags & AFS_FLAG_RETURNTOKEN)
		errorCode = px_SetTokenStruct(tset, Fidp, OutTokenp, princp);
	    else
		px_ClearTokenStruct(OutTokenp);
	}
    }
    OSI_VN_RELE(vp);
    if (!volpDeactivated)
	vol_EndVnodeOp(volp, &dmptr);
    VOL_RELE(volp);
Bad_FetchData1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
	if (errorCode == 0)
	   errorCode = code;	/* To preserve incoming errorCode */
    }
    fshs_PutPrincipal(princp);
Bad_FetchData3:
    icl_Trace1(px_iclSetp, PX_TRACE_FETCHDATAEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_FetchData4:
    if (errorCode && !pipeDrained) {
      unsigned char pbuf[1];
      osi_RestorePreemption(preempting);
      pbuf[0] = 0;
      TRY {  /* Try to catch an exception from a broken pipe, in case */

	  /*
	   * Make sure the pipe is drained. Otherwise, CM will be confused.
	   */
	  (*(streamPtr->push))(streamPtr->state, pbuf, 0);
      } CATCH_ALL {
	  pbuf[0] = 1;
      } ENDTRY
      preempting = osi_PreemptionOff();
      if (pbuf[0]) {
	  uprintf("fx: unexpected pipe exception while pushing to clean up!\n");
	  icl_Trace0(px_iclSetp, PX_TRACE_PUSHEXCEPT);
      }
    }
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}
/*
 * FetchACL RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_FetchACL (
     handle_t h,
     afsFid *Fidp,		        /* Fid of target dir */
     unsigned32 aclType,		/* ACL Types */
     afs_hyper_t *minVVp,		/* min acceptable volume version num */
     unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
     afsACL *AccessListp,		/* Returned contents of dir's ACL */
     afsFetchStatus *OutStatusp,        /* Returned status for the dir */
     afsVolSync *Syncp)		        /* volume synchronization info */
#else
unsigned32
SAFS_FetchACL (h, Fidp, aclType, minVVp, Flags, AccessListp,
			  OutStatusp, Syncp)
     handle_t h;
     afsFid *Fidp;		        /* Fid of target dir */
     unsigned long aclType;		/* ACL Types */
     afs_hyper_t *minVVp;		/* min acceptable volume version num */
     unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
     afsACL *AccessListp;		/* Returned contents of dir's ACL */
     afsFetchStatus *OutStatusp;        /* Returned status for the dir */
     afsVolSync *Syncp;		        /* volume synchronization info */
#endif /* SGIMIPS */
{
    int	code, errorCode = 0;		/* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with Fid */
    struct vnode *vp;			/* Vnode associated with Fid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset=0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;		        /* Token for this Fid */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace2(px_iclSetp, PX_TRACE_FETCHACL, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, aclType);
    afsStats.FetchACL++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
        errorCode = EINVAL;
        goto Bad_FetchACL4;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_FetchACL4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * the request
     */
    px_AdjustCell(Fidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_FetchACL4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_FetchACL3;
    /*
     * Get read status tokens for the associated file
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_READ);
	errorCode = tkset_AddTokenSet(tset, Fidp, &ltoken, NULL,
	    fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_FetchACL1;	/* go delete the tset */
    }
    /*
     * Let VOPX_GETACL allocate the proper amount; just initialize it here
     * to make xdr stubs happy.
     */
    bzero((char *) AccessListp, sizeof(afsACL));

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with Fidp
     */
    errorCode = pxreg_Lookup(Fidp, VNOP_TYPE_READONLY, &dmptr, &volp, &vp);
    if (errorCode)
	goto Bad_FetchACL1;

    /* Older clients don't understand the new DCE1.1 ACL entry types */
    if (princp->hostp->flags & FSHS_HOST_NEW_ACL_IF) 
	aclType |= VNX_ACL_DELEGATE_ENTRY_OK;
    else
	icl_Trace3(px_iclSetp, PX_TRACE_PRE_11_CLIENT,
		   ICL_TYPE_FID, Fidp,
		   ICL_TYPE_POINTER, vp,
		   ICL_TYPE_LONG, aclType);
		   
    /*
     * Finally get the object's access list and its updated attributes
     */
#ifdef SGIMIPS
    VOPX_GETACL(vp, ((struct dfs_acl*)AccessListp), (int)aclType, 
						credp, errorCode);
    if (!errorCode) {
	VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETACL(vp, ((struct dfs_acl*)AccessListp), aclType, credp);
    if (!errorCode) {
	errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
	    px_SetSync(volp, Syncp, 0, credp);
	}
    }
    OSI_VN_RELE(vp);
    vol_EndVnodeOp(volp, &dmptr);
    VOL_RELE(volp);

Bad_FetchACL1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
      }
    fshs_PutPrincipal(princp);
Bad_FetchACL3:
    icl_Trace1(px_iclSetp, PX_TRACE_FETCHACLEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_FetchACL4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * FetchStatus RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_FetchStatus (
    handle_t h,
    afsFid *Fidp,		        /* Fid of target file */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp,	        /* Returned status for the fid */
    afs_token_t *OutTokenp,		/* if r/w, Token promise for Fid */
    afsVolSync *Syncp)		        /* volume synchronization parm */
#else
unsigned32
SAFS_FetchStatus (h, Fidp, minVVp, Flags, OutStatusp,
			     OutTokenp, Syncp)
    handle_t h;
    afsFid *Fidp;		        /* Fid of target file */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp;	        /* Returned status for the fid */
    afs_token_t *OutTokenp;		/* if r/w, Token promise for Fid */
    afsVolSync *Syncp;		        /* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with Fid */
    struct vnode *vp;			/* Vnode associated with Fid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;		        /* Token for this Fid */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    icl_Trace2(px_iclSetp, PX_TRACE_FETCHSTATUS, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, Flags);
    afsStats.FetchStatus++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
        errorCode = EINVAL;
        goto Bad_FetchStatus4;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_FetchStatus4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(Fidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_FetchStatus4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_FetchStatus3;
    /*
     * Get read status tokens for the associated file
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_READ);
	errorCode = tkset_AddTokenSet(tset, Fidp, &ltoken, NULL,
	    fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_FetchStatus1;	/* go delete the tset */
    }
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with Fidp
     */
    errorCode = pxreg_Lookup(Fidp, VNOP_TYPE_READONLY, &dmptr, &volp, &vp);
    if (errorCode)
	goto Bad_FetchStatus1;

    /*
     * Get the object's attributes and a token promise (if a writeable file)
     */
#ifdef SGIMIPS
    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
    if (!errorCode) {
	px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
	errorCode = DoLargeFileOldClient(OutStatusp, princp);
    }
    if (!errorCode) {
	px_SetSync(volp, Syncp, 0, credp);
	if (Flags & AFS_FLAG_RETURNTOKEN)
	    errorCode = px_SetTokenStruct(tset, Fidp, OutTokenp, princp);
	else
	    px_ClearTokenStruct(OutTokenp);
    }
    OSI_VN_RELE(vp);
    vol_EndVnodeOp(volp, &dmptr);
    VOL_RELE(volp);
Bad_FetchStatus1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
    fshs_PutPrincipal(princp);
Bad_FetchStatus3:
    icl_Trace1(px_iclSetp, PX_TRACE_FETCHSTATUSEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_FetchStatus4:
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * StoreData RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_StoreData (
    handle_t h,
    afsFid *Fidp,			/* Fid of taret file */
    afsStoreStatus *InStatusp,		/* Input Status for Fid */
    afs_hyper_t *Position,		/* Offset in the file */
    signed32 Length,			/* Length of data to store */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    pipe_t *streamPtr,			/* Ptr to the data stream pipe */
    afsFetchStatus *OutStatusp,		/* Returned status for target fid */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32
SAFS_StoreData (h, Fidp, InStatusp, Position, Length,
		minVVp, Flags, streamPtr, OutStatusp, Syncp)
    handle_t h;
    afsFid *Fidp;			/* Fid of taret file */
    afsStoreStatus *InStatusp;		/* Input Status for Fid */
    afs_hyper_t *Position;		/* Offset in the file */
    long Length;			/* Length of data to store */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    pipe_t *streamPtr;			/* Ptr to the data stream pipe */
    afsFetchStatus *OutStatusp;		/* Returned status for target fid */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    volatile int code, errorCode = 0;	/* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with Fid */
    struct vnode *vp;			/* Vnode associated with Fid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    struct vattr vattr;                 /* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token for this Fid */
    int pipeDrained = 0;		/* whether the pipe is drained */
    int volpDeactivated = 0;		/* whether the activeVnop count is down already */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace4(px_iclSetp, PX_TRACE_STOREDATA, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, InStatusp->mask,
	       ICL_TYPE_LONG, AFS_hgetlo(*Position),
	       ICL_TYPE_LONG, Length);
    afsStats.StoreData++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
	errorCode = EINVAL;
	goto Bad_StoreData4;
    }

    {   /* Bounds check *Position and Length against max supported file size */
	afs_hyper_t temp = *Position;

	AFS_hadd32(temp, Length);
	if (AFS_hcmp(*Position, osi_maxFileServer) > 0 ||
	    AFS_hcmp(temp, osi_maxFileServer) > 0) {
	    errorCode = EFBIG;
	    goto Bad_StoreData4;
	}
    }

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_StoreData4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(Fidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_StoreData4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_StoreData3;

    /*
     * Get status and data write tokens for the associated file
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
	errorCode = tkset_AddTokenSet(tset, Fidp, &ltoken, NULL,
				      fshs_GetPrincHost(princp), 0, &dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_StoreData1;	/* go delete the tset */
    }
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with Fidp
     */
    errorCode = pxreg_Lookup(Fidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &vp);
    if (errorCode)
	goto Bad_StoreData1;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_StoreData0;
    }


    /* Length==0 happens if the CM wants to take advantage of side effects */
    if ((px_checkStoreTokens != PX_NO_TOKEN_CHECK) && (Length > 0)) {
	afs_hyper_t endPos;
	afs_hyper_t rights;

	/*
	 * Make sure that the CM issuing the call is playing by the rules.
	 * Note that in the default case we will store the data even
	 * if we find that the host making the RPC has no tokens
	 */
	endPos = *Position;
	AFS_hadd32(endPos, (Length -1));
#ifdef SGIMIPS
	errorCode = (int)tkm_GetRightsHeld(fshs_GetPrincHost(princp), Fidp, 
#else
	errorCode = tkm_GetRightsHeld(fshs_GetPrincHost(princp), Fidp, 
#endif
				      (afs_hyper_t *) Position,
				      (afs_hyper_t *) &endPos, 
				      &rights);
	
	if (errorCode || ((AFS_hgetlo(rights) & TKN_DATA_WRITE) == 0)) {
	    icl_Trace4(px_iclSetp, PX_TRACE_STORE_WITHOUT_TOKENS, 
		       ICL_TYPE_FID, Fidp,
		       ICL_TYPE_LONG, AFS_hgetlo(*Position),
		       ICL_TYPE_LONG, Length,
		       ICL_TYPE_LONG, errorCode);
	    if (px_checkStoreTokens == PX_CHECK_AND_REJECT) {
		errorCode = ECONNREFUSED;
		goto Bad_StoreData0;
	    } else {
		printf("Storing without tokens for %x.%x.%x.%x (%d -> %d)\n",
		       AFS_HGETBOTH(Fidp->Volume), Fidp->Vnode, 
		       Fidp->Unique,
		       AFS_hgetlo(*Position), AFS_hgetlo(endPos));
	    }
	}
    }

    /*
     * Do the actual store. Also set the new object's status and return the new
     * attributes back to the caller.
     */
#ifdef SGIMIPS
    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
    if (errorCode)
	goto Bad_StoreData0;


    /* Check whether we're going to store past what the client wants as EOF. */
    if (InStatusp->mask & AFS_SETLENGTH) {
	afs_hyper_t endPos;

	endPos = *Position;
	AFS_hadd32(endPos, Length);
	if (AFS_hcmp(endPos, InStatusp->length) > 0) {
	    icl_Trace4(px_iclSetp, PX_TRACE_STORE_OVERRUN,
		       ICL_TYPE_POINTER, vp,
		       ICL_TYPE_HYPER, Position,
		       ICL_TYPE_LONG, Length,
		       ICL_TYPE_HYPER, &InStatusp->length);
	    errorCode = EINVAL;
	    goto Bad_StoreData0;
	}
    }

    /* takes xvattr representing current file status, and InStatusp,
     * representing changes to make, and produces vattr, new attributes
     * to set to get to this state.  We do this before rdwr since the
     * new attrs are sometimes a function of the original file state,
     * e.g. mtime, since it is used as a version number, and shouldn't
     * decrease, normally.
     * No setattr is actually done here, except when pretruncating a
     * file.  The setattr will be done below by px_PostSetExistingStatus.
     */
    errorCode = px_PreSetExistingStatus(vp, InStatusp, &vattr, &xvattr, credp);

    /* copy the data */
    if (!errorCode)
	errorCode = px_rdwr(vp, UIO_WRITE, Position, Length, credp, streamPtr,
#ifdef SGIMIPS
			    (int)Flags & AFS_FLAG_ZERO, &pipeDrained,
#else
			    Flags & AFS_FLAG_ZERO, &pipeDrained,
#endif
			    volp, &volpDeactivated, &dmptr);

    /* finally, set the goal state, including file size.  May do
     * 2 setattrs, once for normal states, and one for a/m times.
     * InStatusp->mask still describes what we want to change, vattr
     * contains the values we want to set things to (computed above),
     * and xvattr contains some extended status info we need for
     * checking access rights.
     */
    if (!errorCode)
	errorCode = px_PostSetExistingStatus(vp, InStatusp, &vattr,
					     &xvattr, credp);

    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
	}
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC)
#ifdef SGIMIPS
	   {
           off_t endPos = (off_t) *Position;
	   endPos += (off_t)Length;
           AFS_hadd32(endPos, Length);
	    VOPX_FSYNC(vp, 0, credp, (off_t)*Position, endPos, errorCode);
	    }
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(vp, credp);
#endif /* SGIMIPS */
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

Bad_StoreData0:
    OSI_VN_RELE(vp);
    if (!volpDeactivated)
	vol_EndVnodeOp(volp, &dmptr);
    VOL_RELE(volp);
Bad_StoreData1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
    fshs_PutPrincipal(princp);
Bad_StoreData3:
    icl_Trace1(px_iclSetp, PX_TRACE_STOREDATAEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_StoreData4:
    if (errorCode && !pipeDrained) {
	unsigned char * volatile pbuf = (unsigned char *)osi_AllocBufferSpace();
#ifdef SGIMIPS
	idl_ulong_int numByte = 0;
#else
	unsigned long numByte = 0;
#endif /* SGIMIPS */
	volatile unsigned char flagger = 0;
	osi_RestorePreemption(preempting);
	TRY {
	    /*
	    * Make sure the pipe is drained. Otherwise, CM will be confused.
	    */
	    do {
		(*(streamPtr->pull))(streamPtr->state, 
			(unsigned char *)pbuf, osi_BUFFERSIZE, &numByte);
	    } while (numByte > 0);
	} CATCH_ALL {
	    flagger = 1;
	} ENDTRY
	preempting = osi_PreemptionOff();
	osi_FreeBufferSpace((struct osi_buffer *)pbuf);
	if (flagger) {
	    icl_Trace0(px_iclSetp, PX_TRACE_PULLEXCEPT);
	    uprintf("fx: unexpected pipe exception while pulling to clear pipe!\n");
	}
    }
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * StoreACL RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_StoreACL (
    handle_t h,
    afsFid *Fidp,			/* Target dir's fid */
    afsACL *AccessListp,		/* Access List's contents */
    unsigned32 aclType,			/* ACL types (composite) */
    afsFid *aclFidp,			/* Fid for that ACL */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp,		/* Returned status of fid */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32
SAFS_StoreACL (h, Fidp, AccessListp, aclType, aclFidp, minVVp,
			  Flags, OutStatusp, Syncp)
    handle_t h;
    afsFid *Fidp;			/* Target dir's fid */
    afsACL *AccessListp;		/* Access List's contents */
    unsigned long aclType;		/* ACL types (composite) */
    afsFid *aclFidp;			/* Fid for that ACL */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp;		/* Returned status of fid */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with Fid */
    struct vnode *vp;			/* Vnode associated with Fid */
    struct vnode *svp;			/* Vnode associated with aclFidp */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token for this Fid */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    int aclSetFlags;			/* flags from aclType parameter */
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace3(px_iclSetp, PX_TRACE_STOREACL, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, aclType, ICL_TYPE_FID, aclFidp);
    afsStats.StoreACL++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags) || (AccessListp == NULL && aclFidp == NULL)) {
        errorCode = EINVAL;
        goto Bad_StoreACL4;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_StoreACL4;
	}
    }
    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(Fidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_StoreACL4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_StoreACL3;
    /*
     * Get write status tokens for the associated file
     * IF the caller has tokens already, we will NOT request them.
     */
    svp = (struct vnode *) 0;
#ifdef SGIMIPS
    aclSetFlags = (int)(aclType >> 16) & 0xffff;
#else
    aclSetFlags = (aclType >> 16) & 0xffff;
#endif
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_WRITE);
	errorCode = tkset_AddTokenSet(tset, Fidp, &ltoken, NULL,
	    fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_StoreACL1;	/* go delete the tset */
    }

    if (!(aclSetFlags & AFS_ACLFLAG_COPY)) {
	/* we're setting ACL directly from incoming parameter */
	aclFidp = NULL;
    } else {
	/* we're copying ACL from another file's ACL */
	AccessListp = 0;	/* for VOPX_SETACL below */
	px_AdjustCell(aclFidp);
	/*
	 * Get write status tokens for the associated file
	 * IF the caller has tokens already, we will NOT request them.
	 */
	if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	    InitToken(&ltoken, TKN_STATUS_READ);
	    errorCode = tkset_AddTokenSet(tset, aclFidp, &ltoken, NULL,
	        fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
	    if (errorCode && errorCode != TKSET_RELOCK)
		goto Bad_StoreACL1;	/* go delete the tset */
	}
    }

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with Fidp
     */
    errorCode = pxreg_Lookup(Fidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &vp);
    if (errorCode)
	goto Bad_StoreACL1;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_StoreACL0;
    }

    if (aclSetFlags & AFS_ACLFLAG_COPY) {
	/*
	 * Get volume and vnode pointers (both held) associated with aclFidp
	 */
	errorCode = VOL_VGET(volp, aclFidp, &svp);
	if (errorCode)
	    goto Bad_StoreACL0;

    }

    /* Finally set the object's access list and return its updated attributes
     * back.
     * Only one of AccessListp or svp will be non-null
     */
#ifdef SGIMIPS
    VOPX_SETACL(vp, ((struct dfs_acl *)AccessListp), svp, 
	(int)((aclType >> 8) & 0xff), (int)(aclType & 0xff), credp, errorCode);
#else
    errorCode = VOPX_SETACL(vp, ((struct dfs_acl *)AccessListp), svp,
			    ((aclType >> 8) & 0xff), (aclType & 0xff), credp);
#endif
    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
	}
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC)
#ifdef SGIMIPS
	    VOPX_FSYNC(vp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(vp, credp);
#endif /* SGIMIPS */
    }

    if (svp) OSI_VN_RELE(svp);
Bad_StoreACL0:
    OSI_VN_RELE(vp);
    vol_EndVnodeOp(volp, &dmptr);
    VOL_RELE(volp);
Bad_StoreACL1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
      }
    fshs_PutPrincipal(princp);
Bad_StoreACL3:
    icl_Trace1(px_iclSetp, PX_TRACE_STOREACLEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_StoreACL4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * StoreStatus RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_StoreStatus (
    handle_t h,
    afsFid *Fidp,			/* Target file's fid */
    afsStoreStatus *InStatusp,		/* Input status for Fid */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp,		/* Output status for fid */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32
SAFS_StoreStatus (h, Fidp, InStatusp, minVVp, Flags,
				 OutStatusp, Syncp)
    handle_t h;
    afsFid *Fidp;			/* Target file's fid */
    afsStoreStatus *InStatusp;		/* Input status for Fid */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutStatusp;		/* Output status for fid */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with Fid */
    struct vnode *vp;			/* Vnode associated with Fid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    struct vattr *vap;			/* pointer to incoming vattr, if any */
    struct vattr vattr;                 /* Vnode attribute structure */
    struct vattr tvattr;                /* Temp vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token for this Fid */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace2(px_iclSetp, PX_TRACE_STORESTATUS, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, InStatusp->mask);
    afsStats.StoreStatus++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
        errorCode = EINVAL;
        goto Bad_StoreStatus4;
    }

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_StoreStatus4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(Fidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_StoreStatus4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_StoreStatus3;

   /*
     * Get write status tokens for the associated file
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_WRITE);
	errorCode = tkset_AddTokenSet(tset, Fidp, &ltoken, NULL,
					  fshs_GetPrincHost(princp), 0, &dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_StoreStatus1;	/* go delete the tset */
    }
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with Fidp
     */
    errorCode = pxreg_Lookup(Fidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &vp);
    if (errorCode)
	goto Bad_StoreStatus1;

    if (volp->v_states & VOL_READONLY) {
	/* Handle the slightly-bogus attempt to set the access time. */
	if ((InStatusp->mask & ~AFS_SETACCESSTIME) == 0) {
	    /* At most, trying to set the access time. */
	    /* Get the status and return setting nothing. */
#ifdef SGIMIPS
	    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	    if (!errorCode) {
		px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
		px_SetSync(volp, Syncp, 0, credp);
	    }
	} else {
	    errorCode = EROFS;
	}
	goto Bad_StoreStatus0;
    }

    /*
     * Set the object's new attributes and, on success, return them back.
     * If we're setting the modtime to a client specified value, but the
     * client doesn't insist that this exact value be used, we ensure
     * that the modtime properly increases.  If modtime not set, we
     * must preserve old value (but bump by 1), and if set by caller
     * but unexactly, we ensure it increases, but otherwise try to use
     * the value passed in.  In all cases, we now need old value to compare.
     */
#ifdef SGIMIPS
    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
    if (errorCode) {
	goto Bad_StoreStatus0;
    }

    /* check for a symbolic link; we can't chmod these (otherwise could change
     * a symlink to a mt pt or vice versa).  In other words, if we're
     * changing the mode bits in a symlink, make sure we're *not* changing
     * the 0111 bits.
     */
    if (vp->v_type == VLNK && (InStatusp->mask & AFS_SETMODE)
	&& ((xvattr.vattr.va_mode & 0111) != (InStatusp->mode & 0111))) {
	errorCode = EINVAL;
	goto Bad_StoreStatus0;
    }

    /* compute what values we want to use in vattr, then call in
     * to set them in postsetexistingstatus.  More details in
     * corresponding comment in AFS_StoreData.  Function is split
     * in two for AFS_StoreData.
     */
    errorCode = px_PreSetExistingStatus(vp, InStatusp, &vattr, &xvattr, credp);
    if (!errorCode)
	errorCode = px_PostSetExistingStatus(vp, InStatusp, &vattr,
					     &xvattr, credp);

    /* stat the file to return updated status */
    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
	}
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC)
#ifdef SGIMIPS
	    VOPX_FSYNC(vp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(vp, credp);
#endif /* SGIMIPS */
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

  Bad_StoreStatus0:
    OSI_VN_RELE(vp);
    vol_EndVnodeOp(volp, &dmptr);
    VOL_RELE(volp);
  Bad_StoreStatus1:
   if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
    fshs_PutPrincipal(princp);
  Bad_StoreStatus3:
    icl_Trace1(px_iclSetp, PX_TRACE_STORESTATUSEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
  Bad_StoreStatus4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * RemoveFile RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_RemoveFile (
    handle_t h,
    afsFid *DirFidp,			/* Dir fid for file to remove */
    afsFidTaggedName *Namep,		/* File name to remove */
    afs_hyper_t *retTokenIDp,		/* Token ID to piggyback return */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutDirStatusp,	/* Output status for dir fid's */
    afsFetchStatus *OutFileStatusp,	/* Deleted file's status */
    afsFid *OutFileFidp,		/* Deleted file's Fid */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32
SAFS_RemoveFile (h, DirFidp, Namep, retTokenIDp, minVVp, Flags,
			OutDirStatusp, OutFileStatusp, OutFileFidp, Syncp)
    handle_t h;
    afsFid *DirFidp;			/* Dir fid for file to remove */
    afsFidTaggedName *Namep;		/* File name to remove */
    afs_hyper_t *retTokenIDp;		/* Token ID to piggyback return */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutDirStatusp;	/* Output status for dir fid's */
    afsFetchStatus *OutFileStatusp;	/* Deleted file's status */
    afsFid *OutFileFidp;		/* Deleted file's Fid */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with DirFid */
    struct vnode *dvp;			/* Vnode associated with  DirFid Dir */
    struct vnode *vp;			/* Vnode associated with  Name file */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token for this DirFid */
    int preempting;                     /* kernel preemption lock */
    long retryCount = 0;		/* relock count for tkset_Create */
    struct context *cookie;
    afs_hyper_t tempType;		/* type used for token return */
    afsFid NameFid;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace3(px_iclSetp, PX_TRACE_REMOVEFILE, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_STRING, Namep->name.tn_chars, ICL_TYPE_HYPER, retTokenIDp);
    afsStats.RemoveFile++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
        errorCode = EINVAL;
        goto Bad_RemoveFile4;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_RemoveFile4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_RemoveFile4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_RemoveFile3;

RemoveFile_Retry:
    NameFid = Namep->fid;
    /*
     * Get status and data write tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(retryCount);
    else
        tset = (struct tkset_set *) 0;
    vp = (struct vnode *) 0;	/* init here for any loop iteration's use of Bad_RemoveFile1 */
    dvp = (struct vnode *) 0;
    volp = (struct volume *) 0;
    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
    errorCode = tkset_AddTokenSet(tset, DirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_RemoveFile1;	/* go delete the tset */

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with directory
     * DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &dvp);
    if (errorCode)
	goto Bad_RemoveFile1;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_RemoveFile;
    }

#ifdef SGIMIPS
    VOPX_LOOKUP(dvp, (char *)&Namep->name.tn_chars[0], &vp,
				credp, errorCode);
    if (errorCode)
#else
    if (errorCode = VOPX_LOOKUP(dvp, (char *)&Namep->name.tn_chars[0], &vp,
				credp))
#endif
	goto Bad_RemoveFile;
    xvfs_ConvertDev(&vp);

    /*
     * Get Fid associated with file to be removed; this fid will be used for
     * the token locking below and also to return the deleted file's fid back
     */
    FidCopy(OutFileFidp, DirFidp);
#ifdef SGIMIPS
    VOPX_AFSFID(vp, OutFileFidp, 0, errorCode);
    if (errorCode) 
#else
    if (errorCode = VOPX_AFSFID(vp, OutFileFidp, 0)) 
#endif
    {
	OSI_VN_RELE(vp);	/* don't do ZLC on the target file */
	vp = (struct vnode *) 0;
	goto Bad_RemoveFile;
    }
    /* now do the token return, if specified */
    if (!AFS_hiszero(*retTokenIDp)) {
	/* return all token types for this token, even though
	 * we may not have them all; makes RPC interface simpler.
	 */
	AFS_hset64(tempType, ~0, PX_OPEN_TOKENS);
	vol_EndVnodeOp(volp, &dmptr);
	tkm_ReturnToken(OutFileFidp, (tkm_tokenID_t *) retTokenIDp,
			(tkm_tokenSet_t *) &tempType,
			TKM_FLAGS_PENDINGRESPONSE);
	errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READWRITE, VOL_XCODE_NOWAIT, &dmptr);
	if (errorCode) {
	    /* volume went busy in the middle! */
	    VOL_RELE(volp);
	    volp = (struct volume *) 0;
	    goto Bad_RemoveFile;
	}
    }

    /*
     * We should try to do the TKN_AddTokenSet for &NameFid here
     */
    if (!AFS_hiszero(NameFid.Volume)) {
	px_AdjustCell(&NameFid);
	if (FidCmp(&NameFid, OutFileFidp) != 0) {
	    OSI_VN_RELE(vp);	/* don't do ZLC on the target file */
	    vp = (struct vnode *) 0;
	    errorCode = EINVAL;
	    goto Bad_RemoveFile;
	}
    } else {
	NameFid = *OutFileFidp;
    }

    /*
     * Get write status tokens for the file to be removed
     */
    InitToken(&ltoken, TKN_STATUS_WRITE);
    tkset_SetVolSync(tset, volp, VNOP_TYPE_READWRITE);
    if (errorCode = tkset_AddTokenSet(tset, &NameFid, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr)) {
	if (errorCode == TKSET_RELOCK) {
	    OSI_VN_RELE(vp);
	    OSI_VN_RELE(dvp);
	    vol_EndVnodeOp(volp, &dmptr);
	    VOL_RELE(volp);
	    if (errorCode = tkset_Delete(tset))
		goto Bad_RemoveFile2;			/* XXX */
	    retryCount++;
	    goto RemoveFile_Retry;	/* pointers re-initialized there */
	}
	volp = (struct volume *) 0;
	goto Bad_RemoveFile;
    }

    /*
     * Do the actual remove, guarding against other clients having
     * the file open.  Note that vp is currently held, so the data won't be
     * free until the OSI_VN_RELE far below.  This is crucial, since we must
     * let px_TryRemove get a shot at keeping the file around before we
     * really lose the data.
     */
#ifdef SGIMIPS
    VOPX_REMOVE(dvp, (char *)&Namep->name.tn_chars[0], credp, errorCode);
#else
    errorCode =	VOPX_REMOVE(dvp, (char *)&Namep->name.tn_chars[0], credp);
#endif
    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
	    /*
	     * Also return the deleted file's attributes
	     */
#ifdef SGIMIPS
	    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	    if (!errorCode)
	        px_xvattr_to_afsstatus(&xvattr, OutFileStatusp, princp);
	}
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC) {
#ifdef SGIMIPS
	VOPX_FSYNC(dvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(dvp, credp);
#endif /* SGIMIPS */
	    if (!errorCode)
#ifdef SGIMIPS
	    VOPX_FSYNC(vp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
		errorCode = VOPX_FSYNC(vp, credp);
#endif /* SGIMIPS */
	}
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

Bad_RemoveFile:
    OSI_VN_RELE(dvp);
Bad_RemoveFile1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
    if (vp) {
        /* Must follow the return of the token set */
	if (volp)
	    px_TryRemove(vp, OutFileFidp, volp);
	OSI_VN_RELE(vp);	/* Release reference to deleted file */
    }
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_RemoveFile2:
    fshs_PutPrincipal(princp);
Bad_RemoveFile3:
    icl_Trace1(px_iclSetp, PX_TRACE_REMOVEFILEEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_RemoveFile4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * CreateFile RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_CreateFile (
    handle_t h,
    afsFid *DirFidp,			/* Parent Dir fid */
    afsTaggedName *Namep,		/* File name to be created */
    afsStoreStatus *InStatusp,		/* Input status for new file */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp,			/* Fid for newly created file */
    afsFetchStatus *OutFidStatusp,	/* Output status for new file */
    afsFetchStatus *OutDirStatusp,	/* Ouput status for the parent dir */
    afs_token_t *OutTokenp,		/* Return Token promise for new file */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32 SAFS_CreateFile (h, DirFidp, Namep, InStatusp, minVVp, Flags,
                    OutFidp, OutFidStatusp, OutDirStatusp, OutTokenp, Syncp)
    handle_t h;
    afsFid *DirFidp;			/* Parent Dir fid */
    afsTaggedName *Namep;		/* File name to be created */
    afsStoreStatus *InStatusp;		/* Input status for new file */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp;			/* Fid for newly created file */
    afsFetchStatus *OutFidStatusp;	/* Output status for new file */
    afsFetchStatus *OutDirStatusp;	/* Ouput status for the parent dir */
    afs_token_t *OutTokenp;		/* Return Token promise for new file */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with DirFid */
    struct vnode *dvp;			/* Vnode associated with DirFid dir */
    struct vnode *vp=0;			/* Vnode associated with new OutFid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    struct vattr vattr;                 /* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    u_long tkmFlags;			/* space for tkm_GetToken flags */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace3(px_iclSetp, PX_TRACE_CREATEFILE, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_STRING, Namep->tn_chars, ICL_TYPE_LONG, InStatusp->mask);
    afsStats.CreateFile++, afsStats.TotalCalls++;
    /*
     * Set the cmask value to zero in this server process
     */
    osi_SetCMASK(InStatusp->cmask);

    if (InvalidTokenFlags(Flags) || !px_FileNameOK((char *)&Namep->tn_chars[0])) {
        errorCode = EINVAL;
        goto Bad_CreateFile4;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_CreateFile4;
	}
    }
    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_CreateFile4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_CreateFile3;

    /*
     * Get status and data write tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(0);

    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
    errorCode = tkset_AddTokenSet(tset, DirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_CreateFile1;	/* go delete the tset */
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with directory
     * DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &dvp);
    if (errorCode)
	goto Bad_CreateFile1;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_CreateFile0;
    }

    /*
     * Create the new object. On success we also return attributes for new
     * file and its parent along with the appropriate tokens for created file
     */
    px_afsstatus_to_vattr(InStatusp, &vattr, (struct vattr *) 0);

    /* above call doesn't handle device type or # work, so we decode this
     * ourselves.
     */
    if (InStatusp->mask & AFS_SETDEVNUM) {
	if (InStatusp->deviceType == CharDev)
	    vattr.va_type = VCHR;
	else if (InStatusp->deviceType == BlockDev)
	    vattr.va_type = VBLK;
	else if (InStatusp->deviceType == FIFO) {
	    vattr.va_type = VFIFO;
	}
	if (!osi_DecodeDeviceNumber(InStatusp, &vattr.va_rdev)) {
	    /* 
	     * cannot create a device with this major/minor number on this 
	     * architecture.
	     */
	    errorCode = EINVAL;
	    goto Bad_CreateFile0;
	}
    }
    else
	vattr.va_type = VREG;	/* otherwise we're creating a normal file */

#ifdef SGIMIPS
    /*
     * IRIX expects the umask bits to have been applied above the VOP interface
     * to the va_mode field.  Make it so.
     */
    vattr.va_mode = (InStatusp->mode & MODEMASK) & ~InStatusp->cmask;
#endif /* SGIMIPS */

#ifdef SGIMIPS
    VOPX_CREATE(dvp, (char *)&Namep->tn_chars[0], &vattr, 0,
			    VWRITE, &vp, credp, errorCode);
#else
    errorCode = VOPX_CREATE(dvp, (char *)&Namep->tn_chars[0], &vattr, NONEXCL,
			    VWRITE, &vp, credp);
#endif
    if (!errorCode) {
	xvfs_ConvertDev(&vp);
	FidCopy(OutFidp, DirFidp);	/* Copy Cell & Volume info */
#ifdef SGIMIPS
	VOPX_AFSFID(vp, OutFidp, 0, errorCode);
#else
	errorCode = VOPX_AFSFID(vp, OutFidp, 0);
#endif
	if (!errorCode) {
	    /*
	     * Get appropriate tokens for newly created file too; since it's a
	     * new file there is no danger of a conflict in ordering
	     */
	    /* Now, we don't really want to drop the volume lock here (by
	     * calling vol_EndVnodeOp, or by letting tkset_AddTokenSet do that
	     * for us), since we've done the VOPX_CREATE call and have thus
	     * made a non-idempotent change to the disk.  If we were to fail
	     * to re-obtain the vol_StartVnodeOp lock, then we would have to
	     * pass back the failure status from that call.  Chances are that
	     * our caller will retry the RPC, but at that point, the object
	     * will have been created, and the permissions that allowed the
	     * caller to create the new object can be different from the
	     * permissions necessary to overwrite the object with a new
	     * VOPX_CREATE call (if it's even allowed to overwrite).  What we
	     * need to do is keep from introducing new methods of getting a
	     * retry-able failure from this call, which isn't idempotent and
	     * has already had its principal side-effect.
	     * 
	     * However, there's a system-wide locking hierarchy between token
	     * management and the activeVnops lock-out, which is that you have
	     * to get tokens first, then negotiate the activeVnops stuff.
	     * (Thus, the tkset module, in its inmost parts, knows how to drop
	     * an activeVnops lock, calling vol_EndVnodeOp, before attempting
	     * to obtain a token.)  We want to get a token now, on the
	     * newly-created object, but we don't want to drop the activeVnops
	     * lock and possibly fail to re-acquire it.  Instead, we ask for
	     * the token in no-conflict mode, so that the token manager will
	     * not issue any token-revocation calls, but will fail our request
	     * if the token we ask for isn't immediately available.  This then
	     * functions like a conditional-lock call, wherein we seek a lock
	     * but don't block if it's not available.
	     *
	     * Now, we mostly expect to get the token we want, since it's a
	     * newly-created object with a newly-created file ID.  But if we
	     * somehow don't get this token (due either to a random race or to
	     * a conflict with some whole-volume token), that's OK: we can
	     * tell the client that there's no token being returned by zeroing
	     * the type bits.  We go ahead and return some version of the new
	     * object's status (that we obtain by calling VOPX_GETATTR) even
	     * without the token, just so the client will have some snapshot,
	     * even if outdated, of the object status and will know (for
	     * example) what the real file type of the object is.
	     *
	     * One other possibility is that the TKM is out of tokens and
	     * the call to tkm_GetToken() will return TKM_ERROR_NOMEM.
	     * Treat this case just like the TKM_ERROR_TOKENCONFLICT case.
	     *
	     * There is comparable logic for this call (SAFS_CreateFile) and
	     * for the other two object-creating calls (SAFS_MakeDir,
	     * SAFS_Symlink).
	     */

	    InitToken(&ltoken,
		      TKN_STATUS_WRITE|TKN_DATA_WRITE|TKN_OPEN_PRESERVE);
	    tkmFlags = (TKM_FLAGS_EXPECTNOCONFL | TKM_FLAGS_DONTSLEEP);
	    if (!(Flags & AFS_FLAG_NOOPTIMISM))
		tkmFlags |= TKM_FLAGS_OPTIMISTICGRANT;
	    errorCode = tkm_GetToken(OutFidp, tkmFlags,  &ltoken,
				     fshs_GetPrincHost(princp), 0, NULL);
	    /*
	     * We'll get TKM_ERROR_TOKENCONFLICT if our EXPECTNOCONFL
	     * unexpectedly found a conflict.  We'll get TKM_ERROR_NOMEM
	     * if there are no tokens left.
	     */
	    if (!errorCode && (Flags & AFS_FLAG_RETURNTOKEN)) {
		*OutTokenp = ltoken;
		/* cvt to relative time */
		FX_FIX_EXPIRATION(OutTokenp->expirationTime);
	    } else {
		px_ClearTokenStruct(OutTokenp);
		icl_Trace2(px_iclSetp, PX_TRACE_CREATEFILENOTOKEN,
			   ICL_TYPE_FID, OutFidp,
			   ICL_TYPE_LONG, errorCode);
		/* but now proceed as if errorCode==0 */
	    }
	    if (!errorCode ||
		errorCode == TKM_ERROR_TOKENCONFLICT ||
		errorCode == TKM_ERROR_NOMEM) {
		/* set extra creation parameters */
		errorCode = px_SetCreationParms(vp, InStatusp, &vattr);
		if (errorCode) {
		    goto Bad_CreateFile;
		}
#ifdef SGIMIPS
		VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
		errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
		if (!errorCode) {
		    /* don't have to worry about px_PreTruncate since
		     * file is already 0-length when created.
		     */
		    px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
#ifdef SGIMIPS
		    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
		    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
		    if (!errorCode) {
			px_xvattr_to_afsstatus(&xvattr, OutFidStatusp, princp);
		    }
		}
	    }	/* errorCode */
	}
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC) {
#ifdef SGIMIPS
	    VOPX_FSYNC(dvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(dvp, credp);
#endif /* SGIMIPS */
	    if (!errorCode)
#ifdef SGIMIPS
		VOPX_FSYNC(vp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
		errorCode = VOPX_FSYNC(vp, credp);
#endif /* SGIMIPS */
	}
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

Bad_CreateFile:
    if (vp)
	OSI_VN_RELE(vp);
Bad_CreateFile0:
    OSI_VN_RELE(dvp);
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_CreateFile1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
            errorCode = code;    /* To preserve incoming errorCode */
      }
    fshs_PutPrincipal(princp);
Bad_CreateFile3:
    icl_Trace3(px_iclSetp, PX_TRACE_CREATEFILEEND, ICL_TYPE_FID, OutFidp,
	       ICL_TYPE_HYPER, &OutTokenp->tokenID, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_CreateFile4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * Rename RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_Rename (
    handle_t h,
    afsFid *OldDirFidp,			/* From parent dir's fid */
    afsFidTaggedName *OldNamep,		/* From file name */
    afsFid *NewDirFidp,			/* To parent dir's fid */
    afsFidTaggedName *NewNamep,		/* To new file name */
    afs_hyper_t *retTokenIDp,		/* token to return */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutOldDirStatusp,	/* Output status for From parent dir */
    afsFetchStatus *OutNewDirStatusp,	/* Output status for To parent dir */
    afsFid *OutOldFileFidp,		/* Old file's fid */
    afsFetchStatus *OutOldFileStatusp,	/* Old file's status */
    afsFid *OutNewFileFidp,		/* New file's fid  */
    afsFetchStatus *OutNewFileStatusp,	/* New file's status */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32 
SAFS_Rename (h, OldDirFidp, OldNamep, NewDirFidp, NewNamep,
	    retTokenIDp, minVVp, Flags, OutOldDirStatusp,
	    OutNewDirStatusp,  OutOldFileFidp,
            OutOldFileStatusp, OutNewFileFidp, OutNewFileStatusp, Syncp)
    handle_t h;
    afsFid *OldDirFidp;			/* From parent dir's fid */
    afsFidTaggedName *OldNamep;		/* From file name */
    afsFid *NewDirFidp;			/* To parent dir's fid */
    afsFidTaggedName *NewNamep;		/* To new file name */
    afs_hyper_t *retTokenIDp;		/* token to return */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutOldDirStatusp;	/* Output status for From parent dir */
    afsFetchStatus *OutNewDirStatusp;	/* Output status for To parent dir */
    afsFid *OutOldFileFidp;		/* Old file's fid */
    afsFetchStatus *OutOldFileStatusp;	/* Old file's status */
    afsFid *OutNewFileFidp;		/* New file's fid  */
    afsFetchStatus *OutNewFileStatusp;	/* New file's status */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with OldDirFid */
    struct vnode *olddvp;		/* Vnode associated with OldDirFid */
    struct vnode *newdvp;		/* Vnode associated with NewDirFid */
    struct vnode *oldvp;		/* Vnode associated with OldName */
    struct vnode *newvp;		/* Vnode associated with NewName */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    long retryCount = 0;		/* relock count for tkset_Create */
    struct tkset_set *tset;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    afs_hyper_t tempType;			/* temp used by return token */
    int preempting;                     /* kernel preemption lock */
    int crossDirectory;		        /* rename is across directories */
    struct context *cookie;
    afsFid *OldNameFidp = &OldNamep->fid;
    afsFid *NewNameFidp = &NewNamep->fid;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace4(px_iclSetp, PX_TRACE_RENAME, ICL_TYPE_FID, OldDirFidp,
	       ICL_TYPE_STRING, OldNamep->name.tn_chars, ICL_TYPE_FID,
	       NewDirFidp, ICL_TYPE_STRING, NewNamep->name.tn_chars);
    afsStats.Rename++, afsStats.TotalCalls++;

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_Rename4;
	}
    }

    px_AdjustCell(OldDirFidp);
    px_AdjustCell(NewDirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_Rename4;
    /*
     * Do some initial testing
     */
    if (!px_FileNameOK((char *)&NewNamep->name.tn_chars[0])) {
	errorCode = EINVAL;
	goto Bad_Rename3;
    }
    if (!AFS_hsame(OldDirFidp->Volume, NewDirFidp->Volume)) {
	/*
	 * Can only rename within a single volume
	 */
	errorCode = EXDEV;
	goto Bad_Rename3;
    }
    if ((strcmp((char *)OldNamep->name.tn_chars,".")==0)
	|| (strcmp((char *)OldNamep->name.tn_chars,"..")==0)
	|| (strcmp((char *)NewNamep->name.tn_chars,".")== 0)
	|| (strcmp((char *)NewNamep->name.tn_chars,"..")==0)
	|| (NewNamep->name.tn_chars[0] == '\0')
	|| (OldNamep->name.tn_chars[0] == '\0')) {
	errorCode = EINVAL;
	goto Bad_Rename3;
    }
    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_Rename3;

Rename_Retry:
    /*
     * Get status and data write tokens for the "old" parent directory,
     * OldDirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(retryCount);
    else
	tset = (struct tkset_set *) 0;
    olddvp = (struct vnode *) 0;	/* clear these for Bad_Rename1 use on any iteration */
    newdvp = (struct vnode *) 0;
    oldvp = (struct vnode *) 0;
    newvp = (struct vnode *) 0;
    volp = (struct volume *) 0;

    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
    errorCode = tkset_AddTokenSet(tset, OldDirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_Rename1;	/* go delete the tset */
    /*
     * Also get status/data write tokens for the "new" parent directory,
     * NewDirFidp, only IF "new" and "old" are NOT the same dir.
     * (We already know that OldDrFidp and NewDirFidp have the same Volume.)
     */
    if ((OldDirFidp->Unique != NewDirFidp->Unique)	||
	(OldDirFidp->Vnode != NewDirFidp->Vnode)) {

	 crossDirectory = 1;	/* move is across directories */
	 InitToken(&ltoken, TKN_STATUS_WRITE|TKN_DATA_WRITE);
	 if (errorCode = tkset_AddTokenSet(tset, NewDirFidp, &ltoken, NULL,
					   fshs_GetPrincHost(princp),
					   (Flags&AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr)){
	     if (errorCode == TKSET_RELOCK) {
		 if (errorCode = tkset_Delete(tset))
		     goto Bad_Rename2;
		 retryCount++;
		 goto Rename_Retry;	/* pointers re-initialized there */
	     }
	     goto Bad_Rename1;	/* go delete the tset */
	 }
     } else
	 crossDirectory = 0;
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with directory
     * OldDirFidp
     */
    errorCode = pxreg_Lookup(OldDirFidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &olddvp);
    if (errorCode)
	goto Bad_Rename1;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_Rename1;
    }

    if (errorCode = VOL_VGET(volp, NewDirFidp, &newdvp)) {   /* XXXX *** */
	goto Bad_Rename1;
    }
    /*
     * Ideally should try adding token locking for NewNameFidp somewhere here
     */
#ifdef SGIMIPS
    VOPX_LOOKUP(olddvp, (char *)&OldNamep->name.tn_chars[0], 
					&oldvp,credp, errorCode);
    if (errorCode)
#else
    if (errorCode = VOPX_LOOKUP(olddvp, (char *)&OldNamep->name.tn_chars[0], &oldvp,credp))
#endif
	goto Bad_Rename1;
    xvfs_ConvertDev(&oldvp);
    /*
     * Get the Fid associated with the file(dir) to be moved; this fid will be
     * used for the token locking below & also to return the deleted fids back
     */
    FidCopy(OutOldFileFidp, OldDirFidp);
#ifdef SGIMIPS
    VOPX_AFSFID(oldvp, OutOldFileFidp, 0, errorCode);
    if (errorCode)
#else
    if (errorCode = VOPX_AFSFID(oldvp, OutOldFileFidp, 0))
#endif
	 goto Bad_Rename1;

    if (!AFS_hiszero(OldNameFidp->Volume)) {
	if (FidCmp(OldNameFidp, OutOldFileFidp) != 0) {
	    /*
	     * Name doesn't match its assumed Fid
	     */
	    errorCode = EINVAL;
	    goto Bad_Rename1;
	}
    } else {
	*OldNameFidp = *OutOldFileFidp;
    }
    px_AdjustCell(OldNameFidp);

    /*
     * If OldNameFidp points to a directory, we must get a DATA_WRITE
     * token on it as well if the source and target directories are
     * different, since '..' needs to be updated.
     */
    if ((osi_vType(oldvp) == VDIR) && crossDirectory)
	InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
    else
	InitToken(&ltoken, TKN_STATUS_WRITE);
    tkset_SetVolSync(tset, volp, VNOP_TYPE_READWRITE);
    if (errorCode = tkset_AddTokenSet(tset, OldNameFidp, &ltoken, NULL,
				      fshs_GetPrincHost(princp),
				      (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr)) {
	if (errorCode == TKSET_RELOCK) {
	    OSI_VN_RELE(oldvp);
	    OSI_VN_RELE(newdvp);
	    OSI_VN_RELE(olddvp);
	    vol_EndVnodeOp(volp, &dmptr);
	    VOL_RELE(volp);
	    if (errorCode = tkset_Delete(tset))
		goto Bad_Rename2;
	    retryCount++;
	    goto Rename_Retry;	/* pointers re-initialized there */
	}
	volp = (struct volume *) 0;
	goto Bad_Rename1;
    }

    /*
     * Note it's OK if the following call fails, it is the target of
     * the rename call, which may not exist.
     */
#ifdef SGIMIPS
    VOPX_LOOKUP(newdvp, (char *)&NewNamep->name.tn_chars[0], &newvp, credp, errorCode);
#else
    errorCode = VOPX_LOOKUP(newdvp, (char *)&NewNamep->name.tn_chars[0], &newvp, credp);
#endif
    if (!errorCode) {
	xvfs_ConvertDev(&newvp);
	FidCopy(OutNewFileFidp, OldDirFidp);
#ifdef SGIMIPS
	VOPX_AFSFID(newvp, OutNewFileFidp, 0, errorCode);
	if (errorCode) 
#else
	if (errorCode = VOPX_AFSFID(newvp, OutNewFileFidp, 0)) 
#endif
	{
	    OSI_VN_RELE(newvp);	/* don't use OutNewFileFidp */
	    newvp = (struct vnode *) 0;
	    goto Bad_Rename1;
	}

	/*
	 * Only if target exists
	 */
	if (!AFS_hiszero(NewNameFidp->Volume)) {
	    if (FidCmp(NewNameFidp, OutNewFileFidp) != 0) {
		/*
		 * Name doesn't match its assumed Fid
		 */
		OSI_VN_RELE(newvp);	/* don't use OutNewFileFidp */
		newvp = (struct vnode *) 0;
		errorCode = EINVAL;
		goto Bad_Rename1;
	    }
	} else {
	    *NewNameFidp = *OutNewFileFidp;
	}
	px_AdjustCell(NewNameFidp);

	/*
	 * Get write status tokens for the for the file to be renamed,
	 * NewNameFidp.
	 */
	InitToken(&ltoken, TKN_STATUS_WRITE);
	/* tkset's syncvol still set */
	if (errorCode = tkset_AddTokenSet(tset, NewNameFidp, &ltoken, NULL,
	    fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr)) {
	    if (errorCode == TKSET_RELOCK) {
		OSI_VN_RELE(newvp);
		OSI_VN_RELE(oldvp);
		OSI_VN_RELE(newdvp);
		OSI_VN_RELE(olddvp);
		vol_EndVnodeOp(volp, &dmptr);
		VOL_RELE(volp);
		if (errorCode = tkset_Delete(tset))
		    goto Bad_Rename2;
		retryCount++;
		goto Rename_Retry;	/* pointers re-initialized there */
	    }
	    volp = (struct volume *) 0;
	    goto Bad_Rename1;
	}
    } else {/* the target of rename (ie., new name) does not exist */
        /* tell the caller that the target file does not exit */
        bzero((char *)OutNewFileFidp, sizeof (afsFid));
    }

    /* now do the token return, if specified */
    if (!AFS_hiszero(*retTokenIDp)) {
	/* return all token types for this token, even though
	 * we may not have them all; makes RPC interface simpler.
	 */
	AFS_hset64(tempType, ~0, PX_OPEN_TOKENS);
	vol_EndVnodeOp(volp, &dmptr);
	tkm_ReturnToken(NewNameFidp, (tkm_tokenID_t *) retTokenIDp,
			(tkm_tokenSet_t *) &tempType,
			TKM_FLAGS_PENDINGRESPONSE);
	errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READWRITE,
				     VOL_XCODE_NOWAIT, &dmptr);
	if (errorCode) {
	    VOL_RELE(volp);
	    volp = (struct volume *) 0;
	    goto Bad_Rename1;
	}
    }

    /*
     * Do the actual rename and get updated attributes for both source and
     * target directories
     */
#ifdef SGIMIPS
    VOPX_RENAME(olddvp, (char *)&OldNamep->name.tn_chars[0], newdvp,
		    (char *)&NewNamep->name.tn_chars[0], credp, errorCode);
#else
    errorCode = VOPX_RENAME(olddvp, (char *)&OldNamep->name.tn_chars[0], newdvp,
			    (char *)&NewNamep->name.tn_chars[0], credp);
#endif
    if (errorCode)
        goto Bad_Rename1;

#ifdef SGIMIPS
    VOPX_GETATTR(olddvp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(olddvp, &xvattr, 1, credp);
#endif
    if (!errorCode) {
        px_xvattr_to_afsstatus(&xvattr, OutOldDirStatusp, princp);
#ifdef SGIMIPS
        VOPX_GETATTR(newdvp, &xvattr, 1, credp, errorCode);
#else
        errorCode = VOPX_GETATTR(newdvp, &xvattr, 1, credp);
#endif
        if (!errorCode) {
            px_xvattr_to_afsstatus(&xvattr, OutNewDirStatusp, princp);
        }
    }
    if (errorCode) goto Bad_Rename1;

    /*
     * Also return attributes for the deleted source and (possibly) deleted
     * target objects
     */
#ifdef SGIMIPS
    VOPX_GETATTR(oldvp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(oldvp, &xvattr, 1, credp);
#endif
    if (!errorCode)
	px_xvattr_to_afsstatus(&xvattr, OutOldFileStatusp, princp);
    if (newvp) {
#ifdef SGIMIPS
	VOPX_GETATTR(newvp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(newvp, &xvattr, 1, credp);
#endif
	if (!errorCode)
	    px_xvattr_to_afsstatus(&xvattr, OutNewFileStatusp, princp);
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC) {
	    /* sync old dir */
#ifdef SGIMIPS
	    VOPX_FSYNC(olddvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(olddvp, credp);
#endif /* SGIMIPS */
	    /* sync new dir, if different */
	    if (!errorCode && (olddvp != newdvp))
#ifdef SGIMIPS
		VOPX_FSYNC(newdvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
		errorCode = VOPX_FSYNC(newdvp, credp);
#endif /* SGIMIPS */
	    /* sync moving object, in case it is a dir */
	    if (!errorCode)
#ifdef SGIMIPS
		VOPX_FSYNC(oldvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
		errorCode = VOPX_FSYNC(oldvp, credp);
#endif /* SGIMIPS */
	    /* sync target, in case it was unlinked but not deleted,
	     * if it exists.
	     */
	    if (!errorCode && newvp)
#ifdef SGIMIPS
		VOPX_FSYNC(newvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
		errorCode = VOPX_FSYNC(newvp, credp);
#endif /* SGIMIPS */
	}
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

Bad_Rename1:
    if (oldvp) {
	/*
	 * XXX More complicated if oldvp and newvp the same XXX
	 */
	OSI_VN_RELE(oldvp);
    }
    if (olddvp)
	OSI_VN_RELE(olddvp);
    if (newdvp)
	OSI_VN_RELE(newdvp);
    if (tset && (code = tkset_Delete(tset))) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
    if (newvp) {
	if (volp)
	    px_TryRemove(newvp, OutNewFileFidp, volp);
	OSI_VN_RELE(newvp);
    }
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_Rename2:
    fshs_PutPrincipal(princp);
Bad_Rename3:
    icl_Trace1(px_iclSetp, PX_TRACE_RENAMEEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_Rename4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * Symlink RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_Symlink(
    handle_t h,
    afsFid *DirFidp,			/* Parent dir's fid */
    afsTaggedName *Namep,		/* File name to create */
    afsTaggedPath *LinkContentsp,	/* Contents of the new created file */
    afsStoreStatus *InStatusp,		/* Input status for new symbolic lnk */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp,			/* Fid for new symbolic link */
    afsFetchStatus *OutFidStatusp,	/* Output status for new sym link */
    afsFetchStatus *OutDirStatusp,	/* Output status for parent dir */
    afs_token_t *OutTokenp,		/* Return Token promise for sym file */
    afsVolSync *Syncp)			/* vol synchronization information */
#else
unsigned32
SAFS_Symlink(h, DirFidp, Namep, LinkContentsp, InStatusp, minVVp, Flags,
	     OutFidp, OutFidStatusp, OutDirStatusp, OutTokenp, Syncp)
    handle_t h;
    afsFid *DirFidp;			/* Parent dir's fid */
    afsTaggedName *Namep;		/* File name to create */
    afsTaggedPath *LinkContentsp;	/* Contents of the new created file */
    afsStoreStatus *InStatusp;		/* Input status for new symbolic lnk */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp;			/* Fid for new symbolic link */
    afsFetchStatus *OutFidStatusp;	/* Output status for new sym link */
    afsFetchStatus *OutDirStatusp;	/* Output status for parent dir */
    afs_token_t *OutTokenp;		/* Return Token promise for sym file */
    afsVolSync *Syncp;			/* vol synchronization information */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp=0;		/* Volume associated with DirFid */
    struct vnode *dvp;			/* Vnode associated with DirFid dir */
    struct vnode *vp=0;			/* Vnode ass with new OutFid symlink */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    struct vattr vattr;                 /* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    int preempting;                     /* kernel preemption lock */
    u_long tkmFlags;			/* space for tkm_GetToken flags */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace4(px_iclSetp, PX_TRACE_SYMLINK, ICL_TYPE_FID, DirFidp,
		ICL_TYPE_STRING, Namep->tn_chars,
		ICL_TYPE_STRING, LinkContentsp->tp_chars,
		ICL_TYPE_LONG, InStatusp->mask);
    /*
     * Set the cmask value to zero in this server process
     */
    osi_SetCMASK(InStatusp->cmask);

    if (InvalidTokenFlags(Flags) || !px_FileNameOK((char *)&Namep->tn_chars[0]) ) {
        errorCode = EINVAL;
        goto Bad_SymLink4;
    }
    afsStats.Symlink++, afsStats.TotalCalls++;

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_SymLink4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_SymLink4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_SymLink3;
    /*
     * Get status and data write tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(0);
    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
    errorCode = tkset_AddTokenSet(tset, DirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_SymLink1;	/* go delete the tset */

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with directory
     * DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &dvp);
    if (errorCode)
	goto Bad_SymLink1;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_SymLink;
    }

    /* if creating a mount point, make sure that we have C rights, since
     * mount points are incredibly confusing entities to users, and I
     * rights alone shouldn't suffice to allow these things to be created.
     */
    if ((InStatusp->mask & AFS_SETMODE) && (InStatusp->mode & 0111) == 0) {
	/* we *are* creating a mount point, use extended getattr to
	 * check if we have C rights.
	 */
#ifdef SGIMIPS
	VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
	if (errorCode) goto Bad_SymLink;
	if (!(xvattr.xvattr.callerAccess & dacl_perm_control)) {
	    errorCode = EACCES;
	    goto Bad_SymLink;
	}
    }


    /*
     * Create the symbolic link; on success return the symlink's and its
     * parent's attributes along with a read token for the created file.
     */
    px_afsstatus_to_vattr(InStatusp, &vattr, (struct vattr *) 0);
    vattr.va_type = VLNK;
#ifdef SGIMIPS
    VOPX_SYMLINK(dvp, (char *)&Namep->tn_chars[0], &vattr,
		      (char *)&LinkContentsp->tp_chars[0], credp, errorCode);
#else
    errorCode = VOPX_SYMLINK(dvp, (char *)&Namep->tn_chars[0], &vattr,
			      (char *)&LinkContentsp->tp_chars[0], credp);
#endif
    if (!errorCode) {
	/*
	 * Since the VOPX_SYMLINK doesn't return the newly create vnode back
	 * we need to do the lookup again below.
	 */
#ifdef SGIMIPS
	VOPX_LOOKUP(dvp, (char *)&Namep->tn_chars[0], &vp, 
				credp, errorCode);
	if (errorCode)
#else
	if (errorCode = VOPX_LOOKUP(dvp, (char *)&Namep->tn_chars[0], &vp, credp))
#endif
	    goto Bad_SymLink;
	xvfs_ConvertDev(&vp);
	FidCopy(OutFidp, DirFidp);
#ifdef SGIMIPS
	VOPX_AFSFID(vp, OutFidp, 0, errorCode);
	if (errorCode)
#else
	if (errorCode = VOPX_AFSFID(vp, OutFidp, 0))
#endif
	    goto Bad_SymLink;
	/*
	 * Get read status tokens for the newly created symlink, OutFidp.
	 */
	/* Check the massive comment in SAFS_CreateFile for details. */
	/* Basically, we hold the activeVnops lock while conditionally
	 * getting a token. */
	InitToken(&ltoken, TKN_STATUS_READ|TKN_OPEN_PRESERVE);
	tkmFlags = (TKM_FLAGS_EXPECTNOCONFL | TKM_FLAGS_DONTSLEEP);
	if (!(Flags & AFS_FLAG_NOOPTIMISM))
	    tkmFlags |= TKM_FLAGS_OPTIMISTICGRANT;
	errorCode = tkm_GetToken(OutFidp, tkmFlags,  &ltoken,
				 fshs_GetPrincHost(princp), 0, NULL);
	/*
	 * We'll get TKM_ERROR_TOKENCONFLICT if our EXPECTNOCONFL
	 * in fact found a conflict.  We'll get TKM_ERROR_NOMEM if tkm
	 * is out of tokens.
	 */
	if (!errorCode && (Flags & AFS_FLAG_RETURNTOKEN)) {
	    *OutTokenp = ltoken;
	    /* cvt to relative time */
	    FX_FIX_EXPIRATION(OutTokenp->expirationTime);
	} else {
	    px_ClearTokenStruct(OutTokenp);
	}
	if (errorCode) {
	    icl_Trace2(px_iclSetp, PX_TRACE_SYMLINKNOTOKEN,
		       ICL_TYPE_FID, OutFidp,
		       ICL_TYPE_LONG, errorCode);
	    if (errorCode != TKM_ERROR_TOKENCONFLICT &&
		errorCode != TKM_ERROR_NOMEM) {
		goto Bad_SymLink;
	    }
	    /* but now proceed as if errorCode==0 */
	}
	/* set other creation parameters */
	errorCode = px_SetCreationParms(vp, InStatusp, &vattr);
	if (errorCode) {
	    goto Bad_SymLink;
	}
#ifdef SGIMIPS
	VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
#ifdef SGIMIPS
	    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	    if (!errorCode) {
		px_xvattr_to_afsstatus(&xvattr, OutFidStatusp, princp);
	    }
	}
    }

    if (!errorCode && (Flags & AFS_FLAG_SYNC)) {
#ifdef SGIMIPS
	VOPX_FSYNC(dvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	errorCode = VOPX_FSYNC(dvp, credp);
#endif /* SGIMIPS */
	if (!errorCode)
#ifdef SGIMIPS
	    VOPX_FSYNC(vp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(vp, credp);
#endif
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }


Bad_SymLink:
    if (vp)
	OSI_VN_RELE(vp);
    OSI_VN_RELE(dvp);
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_SymLink1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
      }
    fshs_PutPrincipal(princp);
Bad_SymLink3:
    icl_Trace2(px_iclSetp, PX_TRACE_SYMLINKEND, ICL_TYPE_FID, OutFidp,
	       ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_SymLink4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * HardLink RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_HardLink (
    handle_t h,
    afsFid *DirFidp,			/* Parent dir's fid */
    afsTaggedName *Namep,		/* File name to create */
    afsFid *ExistingFidp,		/* Fid of existing fid we'll make lnk*/
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutFidStatusp,	/* Output status for new file */
    afsFetchStatus *OutDirStatusp,	/* Outpout status for parent dir */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else 
unsigned32 
SAFS_HardLink (h, DirFidp, Namep, ExistingFidp, minVVp,
			      Flags, OutFidStatusp, OutDirStatusp, Syncp)
    handle_t h;
    afsFid *DirFidp;			/* Parent dir's fid */
    afsTaggedName *Namep;		/* File name to create */
    afsFid *ExistingFidp;		/* Fid of existing fid we'll make lnk*/
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutFidStatusp;	/* Output status for new file */
    afsFetchStatus *OutDirStatusp;	/* Outpout status for parent dir */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with DirFid */
    struct vnode *dvp;			/* Vnode associated with DirFid dir */
    struct vnode *vp=0;			/* Vnode associated with ExistingFid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace3(px_iclSetp, PX_TRACE_LINK, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_STRING, Namep->tn_chars, ICL_TYPE_FID, ExistingFidp);
    afsStats.Link++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags) || !px_FileNameOK((char*)&Namep->tn_chars[0])) {
        errorCode = EINVAL;
        goto Bad_HardLink4;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_HardLink4;
	}
    }

    /*
     * Do some initial testing
     */
    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_HardLink4;

    if (!AFS_hsame(DirFidp->Volume, ExistingFidp->Volume)) {
        /*
	 * Hard links are restricted within the same volume
	 */
	errorCode = EXDEV;
	goto Bad_HardLink3;
    }
    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_HardLink3;

    /*
     * Get status and data write tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(0);
    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
    errorCode = tkset_AddTokenSet(tset, DirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_HardLink1;	/* go delete the tset */
    /*
     * Also get write status tokens for the file we hard link to, ExistingFidp.
     */
    InitToken(&ltoken, TKN_STATUS_WRITE);
    errorCode = tkset_AddTokenSet(tset,ExistingFidp,&ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_HardLink1;

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with directory
     * DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &dvp);
    if (errorCode)
	goto Bad_HardLink1;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_HardLink;
    }

    if (errorCode = VOL_VGET(volp, ExistingFidp, &vp))	   /* XXXX */
	goto Bad_HardLink;

    /*
     * Do the actual hard link; on success return the attributes for the
     * newly created file and its parent
     */
#ifdef SGIMIPS
    VOPX_LINK(vp, dvp, (char *)&Namep->tn_chars[0], credp, errorCode);
#else
    errorCode = VOPX_LINK(vp, dvp, (char *)&Namep->tn_chars[0], credp);
#endif
    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
#ifdef SGIMIPS
	    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	    if (!errorCode) {
		px_xvattr_to_afsstatus(&xvattr, OutFidStatusp, princp);
	    }
	}
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC) {
#ifdef SGIMIPS
	VOPX_FSYNC(dvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(dvp, credp);
#endif /* SGIMIPS */
	    if (!errorCode)
#ifdef SGIMIPS
	    VOPX_FSYNC(vp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
		errorCode = VOPX_FSYNC(vp, credp);
#endif /* SGIMIPS */
	}
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

    if (vp)
	OSI_VN_RELE(vp);
Bad_HardLink:
    OSI_VN_RELE(dvp);
    vol_EndVnodeOp(volp, &dmptr);
    VOL_RELE(volp);
Bad_HardLink1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
      }
    fshs_PutPrincipal(princp);
Bad_HardLink3:
    icl_Trace1(px_iclSetp, PX_TRACE_LINKEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_HardLink4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * MakeDir RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_MakeDir (
    handle_t h,
    afsFid *DirFidp,			/* Parent dir's Fidp */
    afsTaggedName *Namep,		/* Name of dir to be created */
    afsStoreStatus *InStatusp,		/* Input status for new dir */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp,			/* Fidp of new dir */
    afsFetchStatus *OutFidStatusp,	/* Output status for new directory */
    afsFetchStatus *OutDirStatusp,	/* Output status for parent dir */
    afs_token_t *OutTokenp,		/* Returned Token promise for new dir*/
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32 
SAFS_MakeDir (h, DirFidp, Namep, InStatusp, minVVp,  Flags,
		      OutFidp, OutFidStatusp, OutDirStatusp, OutTokenp, Syncp)
    handle_t h;
    afsFid *DirFidp;			/* Parent dir's Fidp */
    afsTaggedName *Namep;		/* Name of dir to be created */
    afsStoreStatus *InStatusp;		/* Input status for new dir */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp;			/* Fidp of new dir */
    afsFetchStatus *OutFidStatusp;	/* Output status for new directory */
    afsFetchStatus *OutDirStatusp;	/* Output status for parent dir */
    afs_token_t *OutTokenp;		/* Returned Token promise for new dir*/
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with DirFid */
    struct vnode *dvp;			/* Vnode associated with DirFid dir */
    struct vnode *ndvp=0;		/* Vnode associated with newOutFiddir*/
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    struct vattr vattr;                 /* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    int preempting;                     /* kernel preemption lock */
    u_long tkmFlags;			/* space for tkm_GetToken flags */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace3(px_iclSetp, PX_TRACE_MAKEDIR, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_STRING, Namep->tn_chars, ICL_TYPE_LONG, InStatusp->mask);
    afsStats.MakeDir++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags) || !px_FileNameOK((char *)&Namep->tn_chars[0])) {
        errorCode = EINVAL;
        goto Bad_MakeDir4;
    }
    /*
     * Set the cmask value to zero in this server process
     */
    osi_SetCMASK(InStatusp->cmask);

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_MakeDir4;
	}
    }

    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_MakeDir4;

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_MakeDir3;

    /*
     * Get status and data write tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(0);
    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);
    errorCode = tkset_AddTokenSet(tset, DirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_MakeDir1;	/* we're off to delete the tset */
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with directory
     * DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &dvp);
    if (errorCode)
	goto Bad_MakeDir1;

    if (volp->v_states & VOL_READONLY) {
	code = EROFS;
	goto Bad_MakeDir;
    }

    /*
     * Create the new directory; on success return new attributes for the
     * newly created dir and its parent along with a write status token for
     * the new directory
     */
    px_afsstatus_to_vattr(InStatusp, &vattr, (struct vattr *) 0);
    vattr.va_type = VDIR;
#ifdef SGIMIPS
    VOPX_MKDIR(dvp, (char *)&Namep->tn_chars[0], &vattr, &ndvp, credp, 
							errorCode);
#else
    errorCode = VOPX_MKDIR(dvp, (char *)&Namep->tn_chars[0], &vattr, &ndvp, credp);
#endif
    if (!errorCode) {
	FidCopy(OutFidp, DirFidp);
#ifdef SGIMIPS
	VOPX_AFSFID(ndvp, OutFidp, 0, errorCode); /* Must be of type 'fid' */
#else
	errorCode = VOPX_AFSFID(ndvp, OutFidp, 0); /* Must be of type 'fid' */
#endif
	if (!errorCode) {
	    /*
	     * Get write status tokens for the newly created dir, OutFidp.
	     */
	    /* Check the massive comment in SAFS_CreateFile for details. */
	    /* Basically, we hold the activeVnops lock while conditionally
	     * getting a token. */
	    InitToken(&ltoken, TKN_STATUS_WRITE|TKN_OPEN_PRESERVE);
	    tkmFlags = (TKM_FLAGS_EXPECTNOCONFL | TKM_FLAGS_DONTSLEEP);
	    if (!(Flags & AFS_FLAG_NOOPTIMISM))
		tkmFlags |= TKM_FLAGS_OPTIMISTICGRANT;
	    errorCode = tkm_GetToken(OutFidp, tkmFlags,  &ltoken,
				     fshs_GetPrincHost(princp), 0, NULL);
	    /*
	     * We'll get TKM_ERROR_TOKENCONFLICT if our EXPECTNOCONFL
	     * in fact found a conflict.  We'll get TKM_ERROR_NOMEM if tkm
	     * is out of tokens.
	     */
	    if (!errorCode && (Flags & AFS_FLAG_RETURNTOKEN)) {
		*OutTokenp = ltoken;
		/* cvt to relative time */
		FX_FIX_EXPIRATION(OutTokenp->expirationTime);
	    } else {
		px_ClearTokenStruct(OutTokenp);
	    }
	    if (errorCode) {
		icl_Trace2(px_iclSetp, PX_TRACE_MAKEDIRNOTOKEN,
			   ICL_TYPE_FID, OutFidp,
			   ICL_TYPE_LONG, errorCode);
		if (errorCode != TKM_ERROR_TOKENCONFLICT &&
		    errorCode != TKM_ERROR_NOMEM) {
		    goto Bad_MakeDir0;
		}
		/* proceed as if errorCode==0 */
	    }
	    /* update the status if more changed */
	    errorCode = px_SetCreationParms(ndvp, InStatusp, &vattr);
	    if (errorCode) {
		goto Bad_MakeDir0;
	    }
#ifdef SGIMIPS
	    VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
	    errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
	    if (!errorCode) {
		px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
#ifdef SGIMIPS
		VOPX_GETATTR(ndvp, &xvattr, 1, credp, errorCode);
#else
		errorCode = VOPX_GETATTR(ndvp, &xvattr, 1, credp);
#endif
		if (!errorCode) {
		    px_xvattr_to_afsstatus(&xvattr, OutFidStatusp, princp);
		}
	    }
	}
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC) {
#ifdef SGIMIPS
	    VOPX_FSYNC(dvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(dvp, credp);
#endif /* SGIMIPS */
	}
	    if (!errorCode) {
#ifdef SGIMIPS
	        VOPX_FSYNC(ndvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
		errorCode = VOPX_FSYNC(ndvp, credp);
#endif /* SGIMIPS */
	    }
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

Bad_MakeDir0:
    if (ndvp)
	OSI_VN_RELE(ndvp);
Bad_MakeDir:
    OSI_VN_RELE(dvp);
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_MakeDir1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
    fshs_PutPrincipal(princp);
Bad_MakeDir3:
    icl_Trace3(px_iclSetp, PX_TRACE_MAKEDIREND, ICL_TYPE_FID, OutFidp,
	       ICL_TYPE_HYPER, &OutTokenp->tokenID, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_MakeDir4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * RemoveDir RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_RemoveDir (
    handle_t h,
    afsFid *DirFidp,			/* Parent dir's fid */
    afsFidTaggedName *Namep,		/* (Empty) dir's name to be removed */
    afs_hyper_t *retTokenIDp,		/* token ID to return */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutDirStatusp,	/* Output status for the parent dir */
    afsFid *OutFidp,			/* removed dir's fid */
    afsFetchStatus *OutDelStatusp,	/* Output status for the deleted dir */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32 
SAFS_RemoveDir (h, DirFidp, Namep, retTokenIDp, minVVp, Flags,
			       OutDirStatusp, OutFidp, OutDelStatusp, Syncp)
    handle_t h;
    afsFid *DirFidp;			/* Parent dir's fid */
    afsFidTaggedName *Namep;		/* (Empty) dir's name to be removed */
    afs_hyper_t *retTokenIDp;		/* token ID to return */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFetchStatus *OutDirStatusp;	/* Output status for the parent dir */
    afsFid *OutFidp;			/* removed dir's fid */
    afsFetchStatus *OutDelStatusp;	/* Output status for the deleted dir */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with DirFid */
    struct vnode *dvp;			/* Vnode associated with DirFid dir */
    struct vnode *rdvp;		/* Vnode associated with  Name dir */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    long retryCount = 0;
    afs_hyper_t tempType;			/* type for token return */
    afsFid NameFid;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace3(px_iclSetp, PX_TRACE_REMOVEDIR, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_STRING, Namep->name.tn_chars, ICL_TYPE_HYPER, retTokenIDp);
    afsStats.RemoveDir++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
        errorCode = EINVAL;
        goto Bad_RemoveDir4;
    }

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_RemoveDir4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_RemoveDir4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_RemoveDir3;

RemoveDir_Retry:

    NameFid = Namep->fid;

    /*
     * Get status and data write tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(retryCount);
    else
	tset = (struct tkset_set *) 0;
    volp = (struct volume *) 0;	/* init these here so they're OK for any iteration */
    dvp = (struct vnode *) 0;
    rdvp = (struct vnode *) 0;


    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_WRITE);

    errorCode = tkset_AddTokenSet(tset, DirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);

    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_RemoveDir;	/* need to delete the tset */

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with directory
     * DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READWRITE, &dmptr, &volp, &dvp);
    if (errorCode)
	goto Bad_RemoveDir;

    if (volp->v_states & VOL_READONLY) {
	errorCode = EROFS;
	goto Bad_RemoveDir;
    }

#ifdef SGIMIPS
    VOPX_LOOKUP(dvp, (char *)&Namep->name.tn_chars[0], &rdvp, credp, 
			errorCode);
    if (errorCode)
#else
    if (errorCode = VOPX_LOOKUP(dvp, (char *)&Namep->name.tn_chars[0], &rdvp, credp))   /* XXX */
#endif
	goto Bad_RemoveDir;

    xvfs_ConvertDev(&rdvp);

    FidCopy(OutFidp, DirFidp);
#ifdef SGIMIPS
    VOPX_AFSFID(rdvp, OutFidp, 0, errorCode);
    if (errorCode)
#else
    if (errorCode = VOPX_AFSFID(rdvp, OutFidp, 0)) 
#endif
    {
	OSI_VN_RELE(rdvp);	/* don't use OutFidp */
	rdvp = (struct vnode *) 0;
	goto Bad_RemoveDir;
    }

    /* now do the token return, if specified */
    if (!AFS_hiszero(*retTokenIDp)) {
	/* return all token types for this token, even though
	 * we may not have them all; makes RPC interface simpler.
	 */
	AFS_hset64(tempType, ~0, PX_OPEN_TOKENS);
	vol_EndVnodeOp(volp, &dmptr);
	tkm_ReturnToken(OutFidp, (tkm_tokenID_t *) retTokenIDp,
			(tkm_tokenSet_t *) &tempType,
			TKM_FLAGS_PENDINGRESPONSE);
	errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READWRITE, VOL_XCODE_NOWAIT, &dmptr);
	if (errorCode) {
	    VOL_RELE(volp);
	    volp = (struct volume *) 0;
	    goto Bad_RemoveDir;
	}
    }

    /*
     * We should try to do the tkm_AddTokenSet for NameFid up here
     */
    if (!AFS_hiszero(NameFid.Volume)) {
	if (!FidCmp(&NameFid, OutFidp)) {
	    /*
	     * Name doesn't match its assumed Fid
	     */
	    errorCode = EINVAL;
	    OSI_VN_RELE(rdvp);	/* don't use OutFidp */
	    rdvp = (struct vnode *) 0;
	    goto Bad_RemoveDir;
	}
    } else {
	NameFid = *OutFidp;
    }
    px_AdjustCell(&NameFid);

    /*
     * Get write status tokens for the directory to be removed, NameFid.
     */
    InitToken(&ltoken, TKN_STATUS_WRITE | TKN_DATA_READ);
    tkset_SetVolSync(tset, volp, VNOP_TYPE_READWRITE);
    if (errorCode = tkset_AddTokenSet(tset, &NameFid, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr)) {
	if (errorCode == TKSET_RELOCK) {
	    OSI_VN_RELE(rdvp);
	    OSI_VN_RELE(dvp);
	    vol_EndVnodeOp(volp, &dmptr);
	    VOL_RELE(volp);
	    if (errorCode = tkset_Delete(tset))
		goto Bad_RemoveDir2;		/* XXX */
	    retryCount++;
	    goto RemoveDir_Retry;	/* this will clear all the pointers */
	}
	OSI_VN_RELE(rdvp);	/* don't bother calling TryRemove */
	rdvp = (struct vnode *) 0;
	volp = (struct volume *) 0;
	goto Bad_RemoveDir;
    }

    /*
     * Remove the (empty) directory and return its parent's status back.
     * 'rdvp' needs to stay held until TryRemove gets a shot at it.
     */
#ifdef SGIMIPS
    VOPX_RMDIR(dvp, (char*)&Namep->name.tn_chars[0],
			   (struct vnode *) 0, credp, errorCode);
#else
    errorCode = VOPX_RMDIR(dvp, (char*)&Namep->name.tn_chars[0],
			   (struct vnode *) 0, credp);
#endif
    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
	if (!errorCode) {
	    px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
	}
    }

    /*
     * XXX We could do without the deleted (empty) dir's status! XXX
     */
    if (!errorCode) {
#ifdef SGIMIPS
	VOPX_GETATTR(rdvp, &xvattr, 1, credp, errorCode);
#else
	errorCode = VOPX_GETATTR(rdvp, &xvattr, 1, credp);
#endif
	if (!errorCode)
	    px_xvattr_to_afsstatus(&xvattr, OutDelStatusp, princp);
    }

    if (!errorCode) {
	if (Flags & AFS_FLAG_SYNC) {
#ifdef SGIMIPS
	VOPX_FSYNC(dvp, 0, credp, (off_t)0, (off_t)-1, errorCode);
#else  /* SGIMIPS */
	    errorCode = VOPX_FSYNC(dvp, credp);
#endif /* SGIMIPS */
	    /* since dirs can't have hard links, we don't have to worry
	     * about fsync'ing the nuked directory, since it is gone now,
	     * and a re-execution of this call won't find the dir anyway.
	     */
	}
	/* Get the fileset version only after all stores are done. */
	px_SetSync(volp, Syncp, 0, credp);
    }

Bad_RemoveDir:
    if (dvp)
	OSI_VN_RELE(dvp);
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
      }
    if (rdvp)  {
	/* Must follow the return of the token set */
	if (volp)
	    px_TryRemove(rdvp, OutFidp, volp);
	OSI_VN_RELE(rdvp);	/* Release reference to deleted dir */
    }
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_RemoveDir2:
    fshs_PutPrincipal(princp);
Bad_RemoveDir3:
    icl_Trace1(px_iclSetp, PX_TRACE_REMOVEDIREND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_RemoveDir4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * Readdir RPC call.
 *
 * This is tricky stuff.  The Readdir RPC call is defined to return a new-style
 * directory listing of the specified directory, no matter what machine it is
 * executed on. This means that there should be an offset field in the returned
 * data, even though it turns out it isn't set (it pads things out and so
 * reduces the work required by some Unix clients, who don't have to worry
 * about the dir's * data expanding).
 *
 * It further returns data packed into 512 byte (DIR_BLKSIZE) pieces, so that
 * repacking at the client won't expand the data beyond what the client is
 * expecting.  In other words, the server ensures that no dir records span
 * 512 byte boundaries.
 *
 * Finally, a further constraint is that the dir code can't stop at just
 * any dir offset, but can only stop at a multiple of 512 bytes, since
 * that's all that a later lseek is allowed to go back to, at the vnode layer.
 */
#ifdef SGIMIPS
error_status_t
SAFS_Readdir (
    handle_t h,
    afsFid *DirFidp,			/* Parent dir's fid */
    afs_hyper_t *Offsetp,		/* Offset in dir */
    unsigned32 Size,			/* # of bytes to read */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afs_hyper_t * volatile NextOffsetp,	/* Offset to next entry */
    afsFetchStatus *OutDirStatusp,	/* Output status for parent dir */
    afs_token_t *OutTokenp,		/* Token promise for dir */
    afsVolSync *Syncp,			/* volume synchronization parm */
    pipe_t *streamPtr)                  /* Ptr to the stream Pipe */
#else
unsigned32
SAFS_Readdir (h, DirFidp, Offsetp, Size, minVVp, Flags,
	      NextOffsetp, OutDirStatusp, OutTokenp, Syncp, streamPtr)
    handle_t h;
    afsFid *DirFidp;			/* Parent dir's fid */
    afs_hyper_t *Offsetp;		/* Offset in dir */
    unsigned long Size;			/* # of bytes to read */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afs_hyper_t * volatile NextOffsetp;	/* Offset to next entry */
    afsFetchStatus *OutDirStatusp;	/* Output status for parent dir */
    afs_token_t *OutTokenp;		/* Token promise for dir */
    afsVolSync *Syncp;			/* volume synchronization parm */
    pipe_t *streamPtr;                  /* Ptr to the stream Pipe */
#endif /* SGIMIPS */
{
    volatile int	errorCode = 0;		/* error code to return */
    struct fshs_principal * volatile princp;	/* ptr to conn's principal */
    struct volume * volatile volp;		/* Volume for DirFid */
    struct vnode * volatile dvp;		/* Vnode for DirFid dir */
    struct xvfs_attr xvattr;			/* Vnode attribute structure */
    osi_cred_t * volatile credp;	/* Unix credentials for this request */
    volatile unsigned long currOffset;	/* 32-bit version of Offset */
    char * volatile bufferp;
    unsigned char * volatile bufferpx;
    register struct dir_minxentry *olddp;
    register struct dir_minwireentry *newdp;
    register struct dir_minwireentry *lastdp;
    long toffset, incount, outcount, orecCount, irecCount;
    int junk;
    long tlen, extraCount, pushCount;
    u_long BaseOffset;			/* base offset of input dir record */
    struct uio tuio;
    volatile int eof;
    struct iovec iov;
    int code = 0;			/* Temp error code */
    struct tkset_set * volatile tset = 0; /* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    volatile int preempting;		/* kernel preemption lock */
    volatile int pipeDrained = 0;	/* whether the pipe is drained */
    struct context * volatile cookie;
    opaque volatile dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    currOffset = AFS_hgetlo(*Offsetp);
    icl_Trace3(px_iclSetp, PX_TRACE_READDIR, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_LONG, currOffset, ICL_TYPE_LONG, Size);
    afsStats.Readdir++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags)) {
	errorCode = EINVAL;
	goto Bad_Readdir4;
    }

    {   /* Bounds check *Offsetp and Size against max supported file size;
         * truncate requested Size if necessary. */

	afs_hyper_t temp;

	if (AFS_hcmp(*Offsetp, osi_maxFileServer) >= 0) {
	    errorCode = EFBIG;
	    goto Bad_Readdir4;
	}
	temp = osi_maxFileServer;
	AFS_hsub(temp, *Offsetp);
#ifdef SGIMIPS
/* Turn off "pointless comparison of unsigned integer with zero" error." */
#pragma set woff 1183
#endif
	if (AFS_hcmp64(temp, 0, Size) < 0)
	    Size = AFS_hgetlo(temp);
#ifdef SGIMIPS
#pragma reset woff 1183
#endif
    }

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_Readdir4;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, (struct context **)&cookie);
    if (errorCode)
        goto Bad_Readdir4;

    if (errorCode = fshs_GetPrincipal((struct context *)cookie,
	(struct fshs_principal **)&princp, 1))
        goto Bad_Readdir3;
    /*
     * Get status and data read tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_READ | TKN_DATA_READ);
	errorCode = tkset_AddTokenSet((struct tkset_set *)tset, DirFidp, 
	    &ltoken, NULL, fshs_GetPrincHost(princp), 
	    (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, (opaque*)&dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_Readdir1;	/* we're off to delete tset now */
    }
    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);
    /*
     * Get volume and vnode pointers (both held) associated with dir DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READONLY, (opaque*)&dmptr,
			     (struct volume **)&volp, (struct vnode **)&dvp);
    if (errorCode)
	goto Bad_Readdir1;
    /*
     * We should we do an VOPX_ACCESS call since VOPX_READDIR doesn't check it
     */
    /* Check for search and read permissions */
#ifdef SGIMIPS
    VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
    if (!errorCode)
    {
	u_int32 perms = dacl_perm_read;	/* don't need X rights to list a dir */
	if ((xvattr.xvattr.callerAccess & perms) != perms)
	{
	    errorCode = EACCES;
	}
    }
    if (errorCode)
	goto Bad_Readdir;

    /*
     * Force offset to be valid (multiple of DIR_BLKSIZE)
     * Make sure that even reading 1 byte named entries can't expand us so
     * far that the output buffer will overflow.  It turns out that the
     * maximum expansion ratio is when 1 byte entries really requiring
     * 9 bytes expand into 1 byte entries requiring 13 byte in the new
     * format.  9/13 is about .69, but to be conservative, we'll use .6
     * as the ratio.
     */
    if (Size < 2*DIR_BLKSIZE) {
	/* too small to guarantee that reading one 512 byte record won't
	 * overflow output buffer.
	 */
	errorCode = EINVAL;
	goto Bad_Readdir;
    }
    Size = Size & ~(DIR_BLKSIZE - 1);
    bufferp = osi_AllocBufferSpace();
    bufferpx = (unsigned char *)osi_AllocBufferSpace();
    eof = 0;

    TRY {  /* Try to catch an exception in case of a broken pipe. */

    outcount = 0;	/* total bytes written */
    orecCount = 0;	/* bytes written in last DIR_BLKSIZE record */
    lastdp = (struct dir_minwireentry *) 0;	/* last guy, for repairs */
    pushCount = 0;	/* bytes since last push */
    newdp = (struct dir_minwireentry *) bufferpx;
    while (1) {
	/*
	 * before doing a read, make sure that we have at least 2 * DIR_BLKSIZE
	 * bytes left; if not, we can't read more, since we might not be able
	 * to return everything we've read.  Also, make sure there's sufficient
	 * room in the buffer, otherwise we will push it now.
	 */
	if (Size - outcount < 2*DIR_BLKSIZE)
	    break;
	if (osi_BUFFERSIZE - pushCount < 2*DIR_BLKSIZE) {
	    /* make each push end on a 512 byte boundary */
	    if (lastdp) {
		/* there's leftover stuff in the buffer */
		extraCount = DIR_BLKSIZE - orecCount;
		code = ntohs(lastdp->recordlen) + extraCount;
		lastdp->recordlen = htons(code);
		outcount += extraCount;
		pushCount += extraCount;
		orecCount = 0;
		lastdp = (struct dir_minwireentry *) 0;
	    }

	    /* Using the stream pipe feature to push the data to client
	     */
	    if (pushCount > 0) {
		osi_RestorePreemption(preempting);
		(*(streamPtr->push))(streamPtr->state,
				(unsigned char *)bufferpx, pushCount);
		preempting = osi_PreemptionOff();
	    }
	    pushCount = 0;
	    newdp = (struct dir_minwireentry *) bufferpx;
	}

	/*
	 * Set up uio vector to read one DIR_BLKSIZE (= 512) byte record.
	 */
	osi_InitUIO(&tuio);
	tuio.osi_uio_iov = &iov;
	tuio.osi_uio_iovcnt = 1;
	BaseOffset = tuio.osi_uio_offset = currOffset;
	tuio.osi_uio_resid = DIR_BLKSIZE;
	tuio.osi_uio_seg = OSI_UIOSYS;
	iov.iov_base = (char *) bufferp;
	iov.iov_len = DIR_BLKSIZE;

#ifdef SGIMIPS
	VOPX_READDIR(dvp, &tuio, credp, &junk, 1, errorCode);
	if (errorCode)
#else
	if (errorCode = VOPX_READDIR(dvp, &tuio, credp, &junk, 1))
#endif
	{
	    break;  /* goto drain the pipe */
	}
	/* number of bytes actually read */
	irecCount = DIR_BLKSIZE - tuio.osi_uio_resid;
	currOffset = tuio.osi_uio_offset;	/* continue where we left off */
	if (irecCount == 0) {
	    eof = 1;
	    break;	/* real EOF */
	}

	/* Handle old style dir format, too: convert it to new dir format.
	 * Don't have to worry about overflowing the size, since we've ensured
	 * above we can process a whole 512 byte blocks' worth of entries.
	 * We check to make sure that we haven't crossed an output 512 byte
	 * boundary, on every record.
	 */
	olddp = (struct dir_minxentry *) bufferp;
	for(incount = 0; incount<irecCount;) {
	    if (olddp->inode) {	/* Non-empty entry */
	        /* bytes in new-style dir entry for this dude */
		tlen = WLDIRSIZE(olddp);
		/*
		 * check to see if we need to force alignment in output by
		 * seeing if we fit in this 512 byte output record.
		 */
		if (tlen + orecCount > DIR_BLKSIZE) {
		    /* pad out the last guy; must exist since a 256 byte entry
		     * can't fill a dir all by itself, and that's as big as
		     * they come.
		     */
		    if (!lastdp)
		        panic("readdir bad dir");
		    /* compute extra bytes we're adding to the last record */
		    extraCount = DIR_BLKSIZE - orecCount;
		    code = ntohs(lastdp->recordlen) + extraCount;
		    lastdp->recordlen = htons(code);
#ifdef SGIMIPS
		    newdp = (struct dir_minwireentry *)((long)newdp + extraCount);
#else
		    newdp = (struct dir_minwireentry *)((int)newdp + extraCount);
#endif
		    orecCount = 0;
		    lastdp = (struct dir_minwireentry *) 0;
		    outcount += extraCount;
		    pushCount += extraCount;
		}
		if ((pushCount + WLDIRSIZE(olddp)) > osi_BUFFERSIZE)
		    panic("server readdir logic");
		/*
		 * At this point, it's safe to add new record to the buffer.
		 * We know we have enough room in the buffer and in the
		 * DIR_BLKSIZE(512)  byte record to fit this guy.
		 */
		newdp->inode = htonl(olddp->inode);
		/* Fill in the extended wire fields, too. */
		/* AFS_TAG_ORIGASCII for backward compatibility */
		newdp->codesetTag = htonl(AFS_TAG_ORIGASCII);
		/* 0 in high word of offset, for now */
		newdp->highOffset = htonl(0);
		/* copy local string to wire string */
		strcpy(((char *)newdp)+SIZEOF_DIR_MINWIREENTRY,
		       ((char *)olddp)+SIZEOF_DIR_MINXENTRY);
		/* copy in first, put in network order after WDIRSIZE call */
#if defined(AFS_SUNOS5_ENV) || defined(SGIMIPS) 
		/* no namelen field is available */
		newdp->namelen = strlen(((char *)olddp)+SIZEOF_DIR_MINXENTRY);
#else /* not AFS_SUNOS5_ENV || SGIMIPS*/
		newdp->namelen = olddp->namelen;
#endif /* not AFS_SUNOS5_ENV || SGIMIPS */
		code = WDIRSIZE(newdp);
		newdp->recordlen = htons(code);
		/* finally put in network byte order after WDIRSIZE call */
		newdp->namelen = htons(newdp->namelen);
		/* we need server offset of next record, in both cases */
		newdp->offset = htonl(olddp->offset);
		lastdp = newdp;
		outcount += code;
		orecCount += code;
		pushCount += code;
#ifdef AFS_IRIX_ENV
		newdp = (struct dir_minwireentry *)((caddr_t)newdp + code);
#else
		newdp = (struct dir_minwireentry *)((int)newdp + code);
#endif /* AFS_IRIX_ENV */		
	    }	/* if inode non-zero */
	    incount += olddp->recordlen;
#ifdef AFS_IRIX_ENV	    
	    olddp = (struct dir_minxentry *)((caddr_t)olddp + olddp->recordlen);
#else	    
	    olddp = (struct dir_minxentry *)((int)olddp + olddp->recordlen);
#endif /* AFS_IRIX_ENV */	    
	}	/* for loop over input record */
    }	/* while until EOF hit */

    /* Before doing push, make sure that we're returning a multiple of
     * DIR_BLKSIZE bytes, which should help keep things aligned on both
     * sides, and seems like a reasonable protocol to follow.
     */
    if (lastdp) {
	/* there's leftover stuff in the buffer */
	extraCount = DIR_BLKSIZE - orecCount;
	code = ntohs(lastdp->recordlen) + extraCount;
	lastdp->recordlen = htons(code);
	outcount += extraCount;
	pushCount += extraCount;
    }

    /* Using the stream pipe feature to push the data to client
     */
    osi_RestorePreemption(preempting);
    if (pushCount > 0)
	(*(streamPtr->push))(streamPtr->state,
				(unsigned char *)bufferpx, pushCount);
    /* Push EOF indicator */
    (*(streamPtr->push))(streamPtr->state, (unsigned char *)bufferpx, 0);
    preempting = osi_PreemptionOff();

    } CATCH_ALL {
        preempting = osi_PreemptionOff();	/* exception came while we were in push routine */
        uprintf("fx: unexpected exception while pushing !\n");
        errorCode = EIO;  /* What error ? ???? */
    } ENDTRY
    pipeDrained = 1;
    osi_FreeBufferSpace((struct osi_buffer *)bufferp);
    osi_FreeBufferSpace((struct osi_buffer *)bufferpx);

    if (errorCode)
        goto Bad_Readdir;
    /*
     * Indicate EOF to the reader.  0 means eof for chunk, but 0x1
     * means end of whole file.
     */
    if (eof)
	AFS_hzero(*NextOffsetp);
    else {
	AFS_hset64(*NextOffsetp, 0, currOffset);
        /*
         * 32/64-bit support: If the client is old, it can't handle an offset
         * beyond 2^31-1, so just return EOVERFLOW.
         *
         * XXX -- this is a place holder now, as currOffset is only 32bits.
         */
        if (!princp->hostp->supports64bit &&
	    AFS_hcmp(*NextOffsetp, osi_hMaxint32) >= 0) {
            errorCode = EOVERFLOW;
            goto Bad_Readdir;
        }
    }

#ifdef SGIMIPS
    VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
    if (!errorCode) {
	px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
	px_SetSync(volp, Syncp, 0, credp);
	if ((Flags & AFS_FLAG_RETURNTOKEN) &&
	    !DoLargeFileOldClient(OutDirStatusp, princp))
	    errorCode = px_SetTokenStruct(tset, DirFidp, OutTokenp, princp);
	else
	    px_ClearTokenStruct(OutTokenp);
    }
Bad_Readdir:
    OSI_VN_RELE(dvp);
    vol_EndVnodeOp((struct volume *)volp, (opaque*)&dmptr);
    VOL_RELE(volp);
Bad_Readdir1:
    if (code = tkset_Delete((struct tkset_set *)tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
    fshs_PutPrincipal((struct fshs_principal *)princp);
Bad_Readdir3:
    icl_Trace2(px_iclSetp, PX_TRACE_READDIREND,
	       ICL_TYPE_LONG, AFS_hgetlo(*NextOffsetp),
	       ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_Readdir4:
    if (errorCode && !pipeDrained) {
	unsigned char pbuf[1];
	osi_RestorePreemption(preempting);
	pbuf[0] = 0;
	TRY {  /* Try to catch an exception from a broken pipe, in case */

	    /*
	     * Make sure the pipe is drained. Otherwise, CM will be confused.
	     */
	    (*(streamPtr->push))(streamPtr->state, pbuf, 0);
	} CATCH_ALL {
	    pbuf[0] = 1;
	} ENDTRY
	preempting = osi_PreemptionOff();
	if (pbuf[0]) {
	    uprintf("fx: unexpected pipe exception while pushing to clean up!\n");
	    icl_Trace0(px_iclSetp, PX_TRACE_PUSHEXCEPT);
	}
    }
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * Lookup RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_Lookup (
    handle_t h,
    afsFid *DirFidp,			/* Parent dir's fid */
    afsTaggedName *Namep,		/* Name of file/dir to look up */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp,			/* Fid for name */
    afsFetchStatus *OutFidStatusp,	/* Output status for name */
    afsFetchStatus *OutDirStatusp,	/* Output status for parent dir */
    afs_token_t *OutTokenp,		/* Return Token promise */
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32
SAFS_Lookup (h, DirFidp, Namep, minVVp, Flags, OutFidp,
	     OutFidStatusp, OutDirStatusp, OutTokenp, Syncp)
    handle_t h;
    afsFid *DirFidp;			/* Parent dir's fid */
    afsTaggedName *Namep;		/* Name of file/dir to look up */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp;			/* Fid for name */
    afsFetchStatus *OutFidStatusp;	/* Output status for name */
    afsFetchStatus *OutDirStatusp;	/* Output status for parent dir */
    afs_token_t *OutTokenp;		/* Return Token promise */
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp;		/* Volume associated with DirFid */
    struct vnode *dvp;			/* Vnode associated with DirFid dir */
    struct vnode *vp;			/* Vnode associated with new OutFid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    struct tkset_set *tset;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken;			/* Token struct used by local Fids */
    int preempting;                     /* kernel preemption lock */
    int alreadyHasTokens;		/* do we already have tokens? */
    int retryCount = 0;			/* relock count for tkset_Create */
    struct context *cookie;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    gluevn_SetThreadFlags(1);
    icl_Trace3(px_iclSetp, PX_TRACE_LOOKUP, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_STRING, Namep->tn_chars, ICL_TYPE_LONG, Flags);
    afsStats.Lookup++, afsStats.TotalCalls++;

    if (InvalidTokenFlags(Flags) || !px_FileNameOK((char*)&Namep->tn_chars[0])) {
        errorCode = EINVAL;
        goto Bad_Lookup4;
    }
    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_Lookup4;
	}
    }
    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(DirFidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_Lookup4;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_Lookup3;

Lookup_Retry:
    /*
     * Get status and data read tokens for the parent directory, DirFidp.
     * IF the caller has tokens already, we will NOT request them.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(retryCount);
    else
	tset = (struct tkset_set *) 0;
    dvp = (struct vnode *) 0;
    vp = (struct vnode *) 0;
    volp = (struct volume *) 0;
    InitToken(&ltoken, TKN_STATUS_READ | TKN_DATA_READ);
    errorCode = tkset_AddTokenSet(tset, DirFidp, &ltoken, NULL,
	fshs_GetPrincHost(princp), (Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
    if (errorCode && errorCode != TKSET_RELOCK)
	goto Bad_Lookup1;	/* now go delete the tset */

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Get volume and vnode pointers (both held) associated with directory
     * DirFidp
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READONLY, &dmptr, &volp, &dvp);
    if (errorCode)
	goto Bad_Lookup1;

#ifdef SGIMIPS
    VOPX_LOOKUP(dvp, (char *)&Namep->tn_chars[0], &vp, credp, errorCode);
#else
    errorCode = VOPX_LOOKUP(dvp, (char *)&Namep->tn_chars[0], &vp, credp);
#endif
    if (!errorCode) {
	xvfs_ConvertDev(&vp);
	FidCopy(OutFidp, DirFidp);
#ifdef SGIMIPS
	VOPX_AFSFID(vp, OutFidp, 0, errorCode); /* Must be of type 'fid' */
#else
	errorCode = VOPX_AFSFID(vp, OutFidp, 0); /* Must be of type 'fid' */
#endif
	if (!errorCode) {
	    /* Get read status tokens for the desired file, OutFidp.
	     * Do token check before adding token for this file.
	     */
	    alreadyHasTokens = px_ClientHasTokens(fshs_GetPrincHost(princp),
						  OutFidp);
	    InitToken(&ltoken, TKN_STATUS_READ|TKN_OPEN_PRESERVE);
	    tkset_SetVolSync(tset, volp, VNOP_TYPE_READONLY);
	    errorCode = tkset_AddTokenSet(tset, OutFidp, &ltoken, NULL,
		fshs_GetPrincHost(princp),(Flags & AFS_FLAG_NOOPTIMISM) ? 0:1, &dmptr);
	    if (errorCode) {
		if (errorCode == TKSET_RELOCK) {
		    OSI_VN_RELE(dvp);
		    OSI_VN_RELE(vp);
		    vol_EndVnodeOp(volp, &dmptr);
		    VOL_RELE(volp);
		    if (errorCode = tkset_Delete(tset))
			goto Bad_Lookup2;
		    retryCount++;
		    goto Lookup_Retry;
		}
		volp = (struct volume *) 0;
		goto Bad_Lookup;
	    }

#ifdef SGIMIPS
	    VOPX_GETATTR(dvp, &xvattr, 1, credp, errorCode);
#else
	    errorCode = VOPX_GETATTR(dvp, &xvattr, 1, credp);
#endif
	    if (!errorCode) {
		px_xvattr_to_afsstatus(&xvattr, OutDirStatusp, princp);
#ifdef SGIMIPS
		VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
		errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
		if (!errorCode) {
		    px_xvattr_to_afsstatus(&xvattr, OutFidStatusp, princp);
		    px_SetSync(volp, Syncp, 0, credp);
		    if ((Flags & AFS_FLAG_RETURNTOKEN) && !alreadyHasTokens &&
			!DoLargeFileOldClient(OutFidStatusp, princp))
			errorCode =
			    px_SetTokenStruct(tset,OutFidp, OutTokenp, princp);
		    else
			px_ClearTokenStruct(OutTokenp);
		}
	    }
	}
    } else {/* Lookup failed */
        bzero((char *)OutFidp, sizeof (afsFid));
    }

Bad_Lookup:
    if (vp)
	OSI_VN_RELE(vp);
    OSI_VN_RELE(dvp);
    if (volp) {
	vol_EndVnodeOp(volp, &dmptr);
	VOL_RELE(volp);
    }
Bad_Lookup1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
      }
Bad_Lookup2:
    fshs_PutPrincipal(princp);
Bad_Lookup3:
    icl_Trace3(px_iclSetp, PX_TRACE_LOOKUPEND, ICL_TYPE_FID, OutFidp,
	       ICL_TYPE_HYPER, &OutTokenp->tokenID, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_Lookup4:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

#ifdef SGIMIPS
error_status_t
SAFS_GetStatistics (
    handle_t h,
    afsStatistics *Statisticsp)		/* Place holder for returned AFS stat*/
#else
unsigned32
SAFS_GetStatistics (h, Statisticsp)
    handle_t h;
    afsStatistics *Statisticsp;		/* Place holder for returned AFS stat*/
#endif /* SGIMIPS */
{
    int	errorCode = 0;			/* error code to return */
    struct fshs_principal *princp;	/* pointer to conn's principal */
    osi_cred_t *credp;			/* Unix credentials for this request */
    int preempting;                     /* kernel preemption lock */

    preempting = osi_PreemptionOff();
    icl_Trace0(px_iclSetp, PX_TRACE_GETSTATISTICS);
    afsStats.GetStatistics++, afsStats.TotalCalls++;

    /*
     * The caller of this procedure is un-authenticated, therefore no principal
     * is needed.
     */
    bzero((char *)Statisticsp, sizeof(*Statisticsp));

    /* At present these statistics not gathered */
#ifndef SGIMIPS
    GetFileServerStats(Statisticsp);
    GetVolumeStats(Statisticsp);
    GetSystemStats(Statisticsp);
#endif

    Statisticsp->CurrentConnections = fshs_stats.usedPrincipals;
    Statisticsp->WorkStations = fshs_stats.usedHosts;
    Statisticsp->ActiveWorkStations = fshs_stats.activeHosts;

    Statisticsp->TotalAfsCalls = afsStats.TotalCalls;
    Statisticsp->TotalFetchs = afsStats.FetchSize1;
    Statisticsp->FetchedBytes = afsStats.TotalFetchedBytes;
    Statisticsp->TotalStores = afsStats.StoreSize1;
    Statisticsp->StoredBytes = afsStats.TotalStoredBytes;

    icl_Trace1(px_iclSetp, PX_TRACE_GETSTATISTICSEND, ICL_TYPE_LONG, errorCode);
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * ReleaseTokens RPC call:
 */

#ifdef SGIMIPS
error_status_t
SAFS_ReleaseTokens (
    handle_t h,
    afsReturns *TokenArrayp,		/* array of Tokens */
    unsigned32 Flags)			/* Skip Tokens, SYNC or ZERO, etc.. */
#else
unsigned32
SAFS_ReleaseTokens (h, TokenArrayp, Flags)
    handle_t h;
    afsReturns *TokenArrayp;		/* array of Tokens */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
#endif /* SGIMIPS */
{
    int	errorCode = 0, code = 0;	/* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    int i;
    afsReturnDesc *rp;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    icl_Trace1(px_iclSetp, PX_TRACE_RELEASETOKENS,
	       ICL_TYPE_LONG, TokenArrayp->afsReturns_len);
    afsStats.ReleaseTokens++, afsStats.TotalCalls++;

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_ReleaseTokens1;
	}
    }

    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request.
     */
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_ReleaseTokens1;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 0))
	goto Bad_ReleaseTokens;

    /*
     * Handle the case where the client does not register the token.
     */
    if (!fshs_HostHasTKNSvc(princp) ) {
        errorCode =  EINVAL;   /* need a better error code ??? */
	goto Bad_ReleaseTokens;
    }

    if (TokenArrayp->afsReturns_len > (AFS_BULKMAX - 1))
	TokenArrayp->afsReturns_len = AFS_BULKMAX - 1; /* Don't do the rest! */

    rp = &TokenArrayp->afsReturns_val[0];
    for (i = 0; i < TokenArrayp->afsReturns_len; i++, rp++) {
	px_AdjustCell(&rp->fid);
	code = tkm_ReturnToken(&rp->fid, (tkm_tokenID_t *) &rp->tokenID,
		    (tkm_tokenSet_t *) &rp->type, TKM_FLAGS_PENDINGRESPONSE);
	if (code && !errorCode) {
	   errorCode = code;   /* report the first error */
	}
    }
    fshs_PutPrincipal(princp);
Bad_ReleaseTokens:
    icl_Trace1(px_iclSetp, PX_TRACE_RELEASETOKENSEND, ICL_TYPE_LONG, errorCode);
    osi_Free((char *)cookie, sizeof (struct context));
Bad_ReleaseTokens1:
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * GetToken RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_GetToken (
    handle_t h,
    afsFid *Fidp,			/* Target file's fid */
    afs_token_t *MinTokenp,		/* desired token */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afs_token_t *OutTokenp,		/* Output token */
    afs_recordLock_t *OutBlockerp,		/* record lock's info */
    afsFetchStatus *OutStatusp,		/* Output status for fid */
    afsVolSync *Syncp)		        /* volume synchronization parm */
#else
unsigned32 
SAFS_GetToken (h, Fidp, MinTokenp, minVVp, Flags, OutTokenp,
			  OutBlockerp, OutStatusp, Syncp)
    handle_t h;
    afsFid *Fidp;			/* Target file's fid */
    afs_token_t *MinTokenp;		/* desired token */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afs_token_t *OutTokenp;		/* Output token */
    afs_recordLock_t *OutBlockerp;		/* record lock's info */
    afsFetchStatus *OutStatusp;		/* Output status for fid */
    afsVolSync *Syncp;		        /* volume synchronization parm */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    struct volume *volp = 0;		/* Volume associated with Fid */
    struct vnode *vp = 0;		/* Vnode associated with Fid */
    struct xvfs_attr xvattr;		/* Vnode attribute structure */
    osi_cred_t *credp;			/* Unix credentials for this request */
    int isWvt;
    struct tkset_set *tset = 0;		/* Lock args for tkm during vfs calls*/
    afs_token_t ltoken, xtoken;		/* Token for this Fid */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    afs_recordLock_t *blockerp;
    unsigned long tksetFlags = 0;	/* flags to pass to tkset routines */
    int xcodeFlags;
    opaque dmptr = NULL;

    preempting = osi_PreemptionOff();
    icl_Trace4(px_iclSetp, PX_TRACE_GETTOKEN, ICL_TYPE_FID, Fidp,
	       ICL_TYPE_LONG, AFS_hgetlo(MinTokenp->type),
	       ICL_TYPE_HYPER, &MinTokenp->tokenID,
	       ICL_TYPE_LONG, Flags);
    if (AFS_hgetlo(MinTokenp->type) &
	(TKN_LOCK_READ | TKN_LOCK_WRITE | TKN_DATA_READ | TKN_DATA_WRITE)) {
	icl_Trace2(px_iclSetp, PX_TRACE_GETTOKEN2,
		   ICL_TYPE_HYPER, &MinTokenp->beginRange,
		   ICL_TYPE_HYPER, &MinTokenp->endRange);
    }
    afsStats.GetToken++, afsStats.TotalCalls++;

    if ((Flags & (AFS_FLAG_ASYNCGRANT | AFS_FLAG_MOVE_REESTABLISH))
	 == (AFS_FLAG_ASYNCGRANT | AFS_FLAG_MOVE_REESTABLISH)) {
	/* Incompatible flags! */
	errorCode = EINVAL;
	goto Bad_GetToken5;
    }

    /*
     * if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH
     * unless AFS_FLAG_SERVER_REESTABLISH set in Flags.  If
     * not in recovery and flag is set, return FSHS_ERR_SERVERUP.
     */
    if (fshs_postRecovery) {
	if (!(Flags & AFS_FLAG_SERVER_REESTABLISH)) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_GetToken5;
	}
	px_ComputeTokenRecoveryTime(1);
	/* Ensure that we get no more than what we asked for, without revokes */
	tksetFlags |= TKSET_ATS_NOREVOKE;
	Flags |= AFS_FLAG_NOOPTIMISM;
    } else {
	if (Flags & AFS_FLAG_SERVER_REESTABLISH) {
	    errorCode = FSHS_ERR_SERVER_UP;
	    goto Bad_GetToken5;
	}
    }
    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    px_AdjustCell(Fidp);
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_GetToken5;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 1))
	goto Bad_GetToken4;
    /*
     * Handle the case where the client does not register the token.
     */
    if (!fshs_HostHasTKNSvc(princp) ) {
        errorCode = EINVAL;
	goto Bad_GetToken3;
    }
#ifdef AFS_IRIX_ENV
    isWvt = (Fidp->Vnode == (unsigned32)(~(1L))) && (Fidp->Unique == 0);
#else
    isWvt = (Fidp->Vnode == (~(1L))) && (Fidp->Unique == 0);
#endif	/* AFS_IRIX_ENV */

    /*
     * Get volume pointer associated with Fidp.  Ask for vnode later
     * when we're done with token management.
     */

    /* WVT is acquired when getting TKN_SPOT_THERE token to initiate
     * token movement; VOL_MOVE_SOURCE is set at this point in case fts
     * drops offline (^C, for example).
     *
     * FLDB has been already updated, so getting persistent error
     * will send client back to fldb and to new location, if a normal
     * call.
     */
    xcodeFlags = VOL_XCODE_NOWAIT;
    if (Flags & AFS_FLAG_MOVE_SOURCE_OK)
	xcodeFlags |= VOL_XCODE_SOURCEOK;

    errorCode = pxreg_Lookup2(Fidp, &volp);
    if (errorCode)
	goto Bad_GetToken2;

    /*
     * check TSR fileset move recovery flags:
     */
    /* attempt to reestablish after move has completed */
    lock_ObtainRead(&volp->v_lock);
    if ((Flags & AFS_FLAG_MOVE_REESTABLISH) &&
	!(volp->v_states & VOL_MOVE_TARGET)) {
	lock_ReleaseRead(&volp->v_lock);
	errorCode = FSHS_ERR_MOVE_FINISHED;
	goto Bad_GetToken2;
    }
    /* fileset is being moved, allow only reestablishment */
    if (!(Flags & AFS_FLAG_MOVE_REESTABLISH) &&
	(volp->v_states & VOL_MOVE_TARGET))  {
	lock_ReleaseRead(&volp->v_lock);
	errorCode = FSHS_ERR_MOVE_REESTABLISH;
	goto Bad_GetToken2;
    }
    /* reestablishing tokens to moved fileset -- just don't try to revoke */
    if ((Flags & AFS_FLAG_MOVE_REESTABLISH) &&
	 (volp->v_states & VOL_MOVE_TARGET)) {
	xcodeFlags |= VOL_XCODE_TARGETOK;
	/* Ensure that we get no more than what we asked for, without revokes */
	tksetFlags |= TKSET_ATS_NOREVOKE;
	Flags |= AFS_FLAG_NOOPTIMISM;
    }
    lock_ReleaseRead(&volp->v_lock);

    /*
     * Get the appropriate (i.e. MinTokenp->type) tokens for file, Fidp.
     */
    if ((Flags & AFS_FLAG_SKIPTOKEN) == 0)
	tset = tkset_Create(0);
    ltoken = *MinTokenp;
    bzero((char *)&xtoken, sizeof(afs_token_t));

    /* 64-bit support: Recognize "whole file" tokens from old servers
     * and convert them into the local representation (0..2^63-1). */

    if (!princp->hostp->supports64bit) {
	if (AFS_hcmp(ltoken.beginRange, osi_hMaxint32) > 0) {
	    /* These clients should never have ranges beyond 2^31-1, so add
             * this bug-trap. */
	    errorCode = TKM_ERROR_64_BITS_NOT_IMPLEMENTED;
	    goto Bad_GetToken1;
	}
	if (AFS_hcmp(ltoken.endRange, osi_hMaxint32) >= 0)
	    ltoken.endRange = osi_hMaxint64;
    }

    if (AFS_hgetlo(MinTokenp->type) & (TKN_LOCK_READ | TKN_LOCK_WRITE))
        blockerp = OutBlockerp;
    else
        blockerp = NULL;

    /*
     * Get Unix ucred struct to pass to the vnode layer
     */
    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    if (Flags & AFS_FLAG_ASYNCGRANT) {
	/* get the token directly, so that we don't have to deal with
	 * teaching the tkset package how to handle only a subspace of
	 * the TKM's request IDs.  Since we don't have the tkset's
	 * revoke procedure blocking out revokes, we don't stat the file,
	 * since revoke would go straight to the client instead of
	 * synchronizing our GETATTR below.
	 */
	tksetFlags = TKM_FLAGS_QUEUEREQUEST;
	if (Flags & AFS_FLAG_TOKENJUMPQUEUE)
	    tksetFlags |= TKM_FLAGS_JUMPQUEUEDTOKENS;
	errorCode = tkm_GetToken(Fidp, tksetFlags,  &ltoken,
				 fshs_GetPrincHost(princp), 0, NULL);
	/* we're not returning status, so warn the caller */
	OutStatusp->fileType = Invalid;
	if (errorCode == 0) {
	    errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READONLY, xcodeFlags, &dmptr);
	    px_SetSync(volp, Syncp, 0, credp);		/* return this, too */
	    if (errorCode == 0) {
		vol_EndVnodeOp(volp, &dmptr);	/* do px_SetSync first */
	    }
	} else
	    px_SetSync(volp, Syncp, 0, credp);		/* return this, too */

	/* note: need to return token ID even if request queued */
	*OutTokenp = ltoken;
	/* cvt to relative time */
	FX_FIX_EXPIRATION(OutTokenp->expirationTime);
    }
    else {	/* not async grant */
	if (isWvt || (Flags & AFS_FLAG_NOOPTIMISM))
	    tksetFlags |= TKSET_ATS_NOOPTIMISM;
	else
	    tksetFlags |= TKSET_ATS_WANTOPTIMISM;

	if (Flags & AFS_FLAG_TOKENJUMPQUEUE)
	    tksetFlags |= TKSET_ATS_WANTJUMPQUEUE;
	if (Flags & AFS_FLAG_NO_NEW_EPOCH)
	    tksetFlags |= TKSET_ATS_NONEWEPOCH;
	if (Flags & AFS_FLAG_FORCEREVOCATIONS)
	    tksetFlags |= TKSET_ATS_FORCEREVOCATION;
	if (Flags & AFS_FLAG_FORCEVOLQUIESCE)
	    tksetFlags |= TKSET_ATS_FORCEVOLQUIESCE;
	if (Flags & AFS_FLAG_NOREVOKE)
	    tksetFlags |= TKSET_ATS_NOREVOKE;

	if (Flags & AFS_FLAG_TOKENID) {
	    tksetFlags |= TKSET_ATS_SAMETOKENID;
	}
	errorCode = tkset_AddTokenSet(tset, Fidp, &ltoken, blockerp,
				      fshs_GetPrincHost(princp), tksetFlags, &dmptr);
	if (errorCode && errorCode != TKSET_RELOCK)
	    goto Bad_GetToken1;

	/*
	 * Return the token promise and the attributes for the file
	 */
	if (!isWvt) {
	    errorCode = vol_StartVnodeOp(volp, VNOP_TYPE_READONLY, xcodeFlags, &dmptr);
	    if (errorCode) {
		goto Bad_GetToken1;
	    }
	    errorCode = VOL_VGET(volp, Fidp, &vp);
	    if (errorCode) {
		vol_EndVnodeOp(volp, &dmptr);
		goto Bad_GetToken1;
	    }
#ifdef SGIMIPS
	    VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
	    errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
	    if (!errorCode) {
		px_xvattr_to_afsstatus(&xvattr, OutStatusp, princp);
		errorCode = DoLargeFileOldClient(OutStatusp, princp);
	    }
	    if (!errorCode) {
		px_SetSync(volp, Syncp, 0, credp); /* do before EndVnodeOp */
	    }
	    OSI_VN_RELE(vp);
	    vol_EndVnodeOp(volp, &dmptr);
	} else {
	    errorCode = 0;	/* as opposed to TKSET_RELOCK */
	    px_SetSync(volp, Syncp, 0, credp);
	}
	if (!errorCode) {
	    /* don't bother checking for (Flags & 1) on this call, it doesn't
             * do anything at all if you don't return the resulting token
             * structure.
             */
	    errorCode = px_SetTokenStruct(tset, Fidp, OutTokenp, princp);
	}

	if (!errorCode) {
	    /* if we didn't have status/read token over the VOPX_GETATTR
	     * above, then the getattr might have run in the middle of
	     * another server thread changing this same file, in
	     * px_write, or some such, and thus might be returning
	     * very bogus information.  So, don't claim to know status
	     * if we're not returning a status/read token.
	     *
	     * Note that we do return the status anyway, only with the
	     * fileType set to Invalid.  This is important to the TSR
	     * code in the cache manager, which needs to probe the file's
	     * ctime and mtime, even if it changes in the meantime.
	     * The cache manager's TSR code contains the explanation as
	     * to how it uses this information safely.
	     */
	    if (!((AFS_hgetlo(OutTokenp->type) & TKN_STATUS_READ) ||
		  (AFS_hgetlo(xtoken.type) & TKN_STATUS_READ)))
		OutStatusp->fileType = Invalid;
	}
    }	/* end of not async grant case */

Bad_GetToken1:
    if (code = tkset_Delete(tset)) {
        uprintf("fx: failed on tkset_Delete (code %d)\n", code);
        if (errorCode == 0)
           errorCode = code;    /* To preserve incoming errorCode */
    }
Bad_GetToken2:
    if (volp)
	VOL_RELE(volp);
Bad_GetToken3:
    fshs_PutPrincipal(princp);
Bad_GetToken4:
    icl_Trace3(px_iclSetp, PX_TRACE_GETTOKENEND,
	       ICL_TYPE_HYPER, &OutTokenp->tokenID,
	       ICL_TYPE_HYPER, &OutTokenp->type,
	       ICL_TYPE_LONG, errorCode);
    if (errorCode == 0 && (AFS_hgetlo(OutTokenp->type) &
	(TKN_LOCK_READ | TKN_LOCK_WRITE | TKN_DATA_READ | TKN_DATA_WRITE))) {
	icl_Trace2(px_iclSetp, PX_TRACE_GETTOKENEND2,
		   ICL_TYPE_HYPER, &OutTokenp->beginRange,
		   ICL_TYPE_HYPER, &OutTokenp->endRange);
    }
    osi_Free((char *)cookie, sizeof (struct context));
Bad_GetToken5:
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * GetTime RPC call:
 */
#ifdef SGIMIPS
error_status_t SAFS_GetTime (
    handle_t h,
    unsigned32 *Secondsp,		/* Returned time in seconds */
    unsigned32 *USecondsp,		/* Returned leftovers in useconds */
    unsigned32 *SyncDistancep,		/* Synchronization distance */
    unsigned32 *SyncDispp)		/* and, no doubt, dispersion */
#else
unsigned32 SAFS_GetTime (h, Secondsp, USecondsp, SyncDistancep, SyncDispp)
    handle_t h;
    unsigned long *Secondsp;		/* Returned time in seconds */
    unsigned long *USecondsp;		/* Returned leftovers in useconds */
    unsigned long *SyncDistancep;	/* Synchronization distance */
    unsigned long *SyncDispp;		/* and, no doubt, dispersion */
#endif /* SGIMIPS */
{
    int code, errorCode = 0;            /* error code to return */
    struct fshs_principal *princp;	/* represent the remote caller */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    struct timeval tv;

    preempting = osi_PreemptionOff();
    icl_Trace0(px_iclSetp, PX_TRACE_GETTIME);
    afsStats.GetTime++, afsStats.TotalCalls++;

    if (fshs_postRecovery) {
	px_ComputeTokenRecoveryTime(0);
	/* Don't return error FSHS_ERR_SERVER_REESTABLISH in this request */
    }
    /*
     * Get hold of the fshs_host/fshs_principal information associated with
     * this request
     */
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
       goto Bad_GetTime1;

    if (errorCode = fshs_GetPrincipal(cookie, &princp, 0))
	goto Bad_GetTime;

    osi_GetTime(&tv);
    *Secondsp = tv.tv_sec;
    *USecondsp = tv.tv_usec;
    *SyncDistancep = afscall_timeSynchDistance;
    *SyncDispp = afscall_timeSynchDispersion;

    fshs_PutPrincipal(princp);
Bad_GetTime:
    osi_Free((char *)cookie, sizeof (struct context));
Bad_GetTime1:
    icl_Trace1(px_iclSetp, PX_TRACE_GETTIMEEND, ICL_TYPE_LONG, errorCode);
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}

/*
 * SAFS_MakeMountPoint() RPC call
 */
#ifdef SGIMIPS
error_status_t
SAFS_MakeMountPoint(
    handle_t h,
    afsFid *DirFidp,
    afsTaggedName *Namep,
    afsTaggedName *cellNamep,
    afsFStype Type,
    afsTaggedName *volumeNamep,
    afsStoreStatus *InStatusp,
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp,
    afsFetchStatus *OutFidStatusp,
    afsFetchStatus *OutDirStatusp,
    afsVolSync *Syncp)			/* volume synchronization parm */
#else
unsigned32 
SAFS_MakeMountPoint(h, DirFidp, Namep, cellNamep, Type,
			  volumeNamep, InStatusp, minVVp, Flags, OutFidp,
			   OutFidStatusp, OutDirStatusp, Syncp)
    handle_t h;
    afsFid *DirFidp;
    afsTaggedName *Namep;
    afsTaggedName *cellNamep;
    afsFStype Type;
    afsTaggedName *volumeNamep;
    afsStoreStatus *InStatusp;
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsFid *OutFidp;
    afsFetchStatus *OutFidStatusp;
    afsFetchStatus *OutDirStatusp;
    afsVolSync *Syncp;			/* volume synchronization parm */
#endif /* SGIMIPS */
{
    return osi_errEncode(ESRCH);
}


/*
 * SAFS_BulkFetchVV() RPC call
 */
#ifdef SGIMIPS
error_status_t
SAFS_BulkFetchVV(
    handle_t h,
    afs_hyper_t *cellIdp,		/* cell ID in which these volumes are */
    afsBulkVolIDs *VolIDsp,		/* volume IDs whose VVs are wanted */
    unsigned32 NumVols,			/* number of IDs wished */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    unsigned32 spare1,
    unsigned32 spare2,
    afsBulkVVs *VolVVsp,		/* the desired VV set */
    unsigned32 *spare4)
#else
unsigned32
SAFS_BulkFetchVV(h, cellIdp, VolIDsp, NumVols, Flags,
				spare1, spare2, VolVVsp, spare4)
    handle_t h;
    afs_hyper_t *cellIdp;		/* cell ID in which these volumes are */
    afsBulkVolIDs *VolIDsp;		/* volume IDs whose VVs are wanted */
    unsigned long NumVols;		/* number of IDs wished */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    unsigned long spare1;
    unsigned long spare2;
    afsBulkVVs *VolVVsp;		/* the desired VV set */
    unsigned long *spare4;
#endif /* SGIMIPS */
{
    int preempting;			/* kernel preemption lock */
    int	errorCode = 0;			/* error code to return */
    struct volume *volp;
    unsigned long Ix;
    afsFid fidDum;
    afs_hyper_t *vidp;

    preempting = osi_PreemptionOff();
    icl_Trace3(px_iclSetp, PX_TRACE_BULKFETCHVV, ICL_TYPE_HYPER, cellIdp,
	       ICL_TYPE_LONG, NumVols, ICL_TYPE_LONG, Flags);
    afsStats.BulkFetchVV++, afsStats.TotalCalls++;
    VolVVsp->afsBulkVVs_len = 0;	/* in case of any error return */

    /* if in post-recovery period, return FSHS_ERR_SERVER_REESTABLISH */
    if (fshs_postRecovery) {
	px_CheckTSREndTime();
	if (fshs_postRecovery) {
	    errorCode = FSHS_ERR_SERVER_REESTABLISH;
	    goto Bad_BulkFetchVV;
	}
    }

    /* does cellIdp have to be set to 0,,1?  Probably. */
    bzero((char *) &fidDum, sizeof(fidDum));
    fidDum.Cell = *cellIdp;
    if (NumVols > VolIDsp->afsBulkVolIDs_len)
	NumVols = VolIDsp->afsBulkVolIDs_len;
    if (NumVols == 0) {
	errorCode = EINVAL;
	goto Bad_BulkFetchVV;
    }

    for (Ix = 0; Ix < NumVols; ++Ix) {/* iterate through all given volume IDs*/
	fidDum.Volume = VolIDsp->afsBulkVolIDs_val[Ix];
	errorCode = pxreg_Lookup2(&fidDum, &volp);
	if (errorCode)
	    break;
	/* get the afsVolSync info from *volp */
	px_SetSync(volp, &VolVVsp->afsBulkVVs_val[Ix], 1/*for repserver*/, 0);
	VOL_RELE(volp);
    }

    VolVVsp->afsBulkVVs_len = Ix;

    *spare4 = 0;
Bad_BulkFetchVV:
    icl_Trace1(px_iclSetp, PX_TRACE_BULKFETCHVVEND, ICL_TYPE_LONG, errorCode);
    osi_RestorePreemption(preempting);
    return osi_errEncode(errorCode);
}


/*
 * SAFS_BulkKeepAlive() RPC call
 */
#ifdef SGIMIPS
error_status_t SAFS_BulkKeepAlive(
    handle_t h,
    struct afsBulkFEX *KAFEXp,		/* Fids of files to be kept alive */
    unsigned32 numExecFids,		/* num of Fids are open-for-execute */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    unsigned32 spare1,
    unsigned32 spare2,
    unsigned32 *spare4)
#else
unsigned32 SAFS_BulkKeepAlive(h, KAFEXp,  numExecFids, Flags,
				  spare1, spare2, spare4)
    handle_t h;
    struct afsBulkFEX *KAFEXp;		/* Fids of files to be kept alive */
    unsigned long numExecFids;		/* num of Fids are open-for-execute */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    unsigned long spare1;
    unsigned long spare2;
    unsigned long *spare4;
#endif /* SGIMIPS */
{
    long errorCode;
    int preempting;
    /* Always allow this RPC even if recovering */

    preempting = osi_PreemptionOff();
    /* Call out to the px_repops.c module. */
    icl_Trace2(px_iclSetp, PX_TRACE_BULKKEEPALIVE, ICL_TYPE_LONG, numExecFids,
	       ICL_TYPE_LONG, Flags);
    errorCode = osi_errEncode(pxint_BulkKeepAlive(h, KAFEXp, numExecFids, Flags,
					     spare1, spare2, spare4));
    icl_Trace1(px_iclSetp, PX_TRACE_BULKKEEPALIVEEND, ICL_TYPE_LONG, errorCode);
    osi_RestorePreemption(preempting);
    return errorCode;
}


#ifdef SGIMIPS
error_status_t
SAFS_ProcessQuota (
    handle_t h,
    afsFid *Fidp,		        /* Fid of target dir */
    afs_hyper_t *minVVp,		/* min acceptable volume version num */
    unsigned32 Flags,			/* Skip Tokens, SYNC or ZERO, etc.. */
    afsQuota *quotaListp,		/* Returned contents of dir's ACL */
    afsFetchStatus *OutStatusp,	        /* Returned status for the dir */
    afsVolSync *Syncp)		        /* volume synchronization info */
#else
unsigned32 
SAFS_ProcessQuota (h, Fidp, minVVp, Flags, quotaListp,
				  OutStatusp, Syncp)
    handle_t h;
    afsFid *Fidp;		        /* Fid of target dir */
    afs_hyper_t *minVVp;		/* min acceptable volume version num */
    unsigned long Flags;		/* Skip Tokens, SYNC or ZERO, etc.. */
    afsQuota *quotaListp;		/* Returned contents of dir's ACL */
    afsFetchStatus *OutStatusp;	        /* Returned status for the dir */
    afsVolSync *Syncp;		        /* volume synchronization info */
#endif /* SGIMIPS */

{
    /* function is now obsolete */
    return EINVAL;
}


#ifdef SGIMIPS
error_status_t
SAFS_GetServerInterfaces (
    handle_t h,
    struct dfs_interfaceList *serverInterfacesP)
#else
unsigned32
SAFS_GetServerInterfaces (h, serverInterfacesP)
    handle_t h;
    struct dfs_interfaceList *serverInterfacesP;
#endif /* SGIMIPS */
{
    register struct dfs_interfaceDescription *idp;
    rpc_if_id_t if_id;
    error_status_t st;

    icl_Trace0(px_iclSetp, PX_TRACE_GETSERVERINTERFACES);
    afsStats.TotalCalls++;

    /* Always allow this RPC even if recovering */

    /* only one interface defined so far */
    /* New interfaces or extensions will add to this list (not replacing the original). */
    serverInterfacesP->dfs_interfaceList_len = 0;
    rpc_if_inq_id (AFS4Int_v4_0_s_ifspec, &if_id, &st);
    if (st != error_status_ok)
	return st;
    serverInterfacesP->dfs_interfaceList_len = 1;
    idp = &serverInterfacesP->dfs_interfaceList_val[0]; /* fill this in */

    bzero((char *) idp, sizeof(struct dfs_interfaceDescription));
    idp->interface_uuid = if_id.uuid;
    idp->vers_major = if_id.vers_major;
    idp->vers_minor = if_id.vers_minor;
    /* Bulkstat RPC raises provider_version from 1 to 2. */
    idp->vers_provider = 2;
    strncpy((char *) &idp->spareText[0], "Transarc DFS file service",
	    MAXSPARETEXT);
    return error_status_ok;
}

/*
 * Set connection and other parameters.  Currently this only deals with
 * recovery time and timeout parameters.
 */
#ifdef SGIMIPS
error_status_t
SAFS_SetParams (
  handle_t h,
  unsigned32 Flags,
  afsConnParams *paramsP)
#else
unsigned32
SAFS_SetParams (
  handle_t h,
  unsigned long Flags,
  afsConnParams *paramsP)
#endif /* SGIMIPS */
{
    int	errorCode;			/* error code to return */
    struct fshs_host *hostp;		/* represent the remote host */
    int preempting;                     /* kernel preemption lock */
    struct context *cookie;
    unsigned long value;
    int i;

    preempting = osi_PreemptionOff();
    icl_Trace0(px_iclSetp, PX_TRACE_SETPARAMS);
    afsStats.SetParams++, afsStats.TotalCalls++;

    /* Always allow this RPC even if recovering */

    /*
     * Get hold of the fshs_host information associated with
     * this request.
     */
    errorCode = fshs_InqContext(h, &cookie);
    if (errorCode)
	goto Bad_SetParams1;

    if (!(hostp = fshs_GetHost(cookie))) {
	/* couldn't get host */
	if (cookie->initContext)
	    errorCode = FSHS_ERR_FATALCONN;
	else
	    errorCode = FSHS_ERR_STALEHOST;
	goto Bad_SetParams1;
    }

    /*
     * Added by the 64/32-bit compatibility effort.
     */
    if (AFS_CONN_PARAM_ISVALID(paramsP, AFS_CONN_PARAM_MAXFILE_CLIENT)) {
	SetClientSize(hostp,
		      AFS_CONN_PARAM_GET(paramsP,
					 AFS_CONN_PARAM_MAXFILE_CLIENT));
    }

    switch (Flags) {
    case AFS_PARAM_RESET_CONN:
    case AFS_PARAM_SET_SIZE:
      /* Allow this SET_SIZE value as another way to alter the parameters. */
      /* Change with new protocol: always return current values, not simply
       * the ones that changed. */

      /* don't allow changing dead server timeout */

      /*
       * Allow changes to host and RPC lifetimes but RPC >= host
       * maxima must also be checked
       */
      if (AFS_CONN_PARAM_ISVALID(paramsP, AFS_CONN_PARAM_HOSTLIFE)) {
	  value = AFS_CONN_PARAM_GET(paramsP, AFS_CONN_PARAM_HOSTLIFE);
	  /* check maximum */
	  if (value > fshs_tsrParams.maxLife) {
	      value = fshs_tsrParams.maxLife;
	  }
	  hostp->hostLifeGuarantee = value;
      }

      if (AFS_CONN_PARAM_ISVALID(paramsP, AFS_CONN_PARAM_HOSTRPC))  {
	  value = AFS_CONN_PARAM_GET(paramsP, AFS_CONN_PARAM_HOSTRPC);
	  /* check maximum */
	  if (value > fshs_tsrParams.maxRPC)	{
	      value = fshs_tsrParams.maxRPC;
	  }
	  hostp->hostRPCGuarantee = value;
      }
      if (hostp->hostLifeGuarantee > hostp->hostRPCGuarantee)  {
	  hostp->hostRPCGuarantee = hostp->hostLifeGuarantee;
      }
      break;

    case AFS_PARAM_TSR_COMPLETE:
      /*
       * This is actually an NOOP in the current implementation.
       * In the future, when the FX server decides to perform the callback
       * (which triggers client's TSR procedure) based on the info kept in
       * the "stable storage", this Flag will be used a  signal to indicate
       * that a client has completed its TSR work. The FX server then
       * perform necessary clean-up work in fshs_GetHost.
       */
      break;

    default:
      uprintf("fx: Unreconized flag (%d) from AFS_SetParams\n", Flags);

    } /* switch */

    /* Having dealt with the input parameters, make the return structure
     * represent what the connection parameters are currently set to.  In
     * addition, make its Mask field describe exactly what field values are
     * valid.
     */
    bzero((char *)paramsP, sizeof(*paramsP));
    AFS_CONN_PARAM_SET(paramsP, AFS_CONN_PARAM_HOSTLIFE,
		       hostp->hostLifeGuarantee);
    AFS_CONN_PARAM_SET(paramsP, AFS_CONN_PARAM_HOSTRPC,
		       hostp->hostRPCGuarantee);
    AFS_CONN_PARAM_SET(paramsP, AFS_CONN_PARAM_DEADSERVER,
		       hostp->deadServerTimeout);
    AFS_CONN_PARAM_SET(paramsP, AFS_CONN_PARAM_EPOCH,
		       fshs_tsrParams.serverRestart);
    AFS_CONN_PARAM_SET(paramsP, AFS_CONN_PARAM_MAXFILE_SERVER,
		       OSI_MAX_FILE_PARM_SERVER);
    if (hostp->maxFileParm)
	AFS_CONN_PARAM_SET(paramsP, AFS_CONN_PARAM_MAXFILE_CLIENT,
			   hostp->maxFileParm);
    fshs_PutHost(hostp);

Bad_SetParams1:
    icl_Trace1(px_iclSetp, PX_TRACE_SETPARAMSEND, ICL_TYPE_LONG, errorCode);
    osi_RestorePreemption(preempting);
    return errorCode;
}

/*
 * Return the status of a window of files in directory. The window is
 * set at AFS_BULKMAX files or osi_BUFFERSIZE of complete dir records,
 * whichever comes first. We transfer the file names in directory record
 * wire format.
 */
int max_files_bulkstat = AFS_BULKMAX;

/*
 * Allow server to limit number of tokens returned via bulkstat.
 */
#define DEFAULT_MAXTOK	100
int bulkstat_maxtok = DEFAULT_MAXTOK;

/*
 *Allow this RPC to be disabled at the server.
 */
int bulkstat_server_enable = 1;

/*
 * Bulk Fetch Status RPC call:
 */
#ifdef SGIMIPS
error_status_t
SAFS_BulkFetchStatus (
  handle_t h,
  afsFid *DirFidp,
  afs_hyper_t *Offsetp,
  unsigned32 Size,
  afs_hyper_t *minVVp,
  unsigned32 Flags,
  BulkStat *bulk_stats,
  afs_hyper_t *NextOffsetp,
  afsFetchStatus *OutDirStatusp,
  afs_token_t *OutTokenp,
  afsVolSync *Syncp,
  pipe_t *dirStream)
#else
unsigned32
SAFS_BulkFetchStatus (h, DirFidp, Offsetp, Size, minVVp,
				 Flags, bulk_stats, NextOffsetp, OutDirStatusp,
				 OutTokenp, Syncp, dirStream)
  handle_t h;
  afsFid *DirFidp;
  afs_hyper_t *Offsetp;
  unsigned32 Size;
  afs_hyper_t *minVVp;
  unsigned32 Flags;
  BulkStat *bulk_stats;
  afs_hyper_t *NextOffsetp;
  afsFetchStatus *OutDirStatusp;
  afs_token_t *OutTokenp;
  afsVolSync *Syncp;
  pipe_t *dirStream;
#endif /* SGIMIPS */
{
    long errorCode = 0;
    volatile int preempting;
    int j, i = 0;
    struct fshs_principal *volatile princp;
    struct volume *volatile volp;
    struct vnode *volatile dvp;
    osi_cred_t *volatile credp;
    unsigned long currOffset;
    char * volatile bufferp;
    struct dir_minxentry *dp;
    struct dir_minwireentry *wiredp;
    char *namep;
    int namelen;
    long incount,irecCount;
    int junk;
    struct uio tuio;
    int eof = 0;
    struct iovec iov;
    struct context *volatile cookie;
    struct vnode *vp;
    afs_token_t ltoken;
    afs_token_t ftoken;
    struct tkset_set *volatile tset = 0;
    afsFid OutFid, *outfidp1, *outfidp2;
    afsFetchStatus OutFidStatus;
    struct xvfs_attr xvattr;
    struct xvfs_attr dirxvattr;
    unsigned char *volatile pipe_buf;	/* the pipe buffer for RPC */
    volatile int pipeDrained = 0;	/* whether the pipe is drained */
    volatile int count_pipe_buf = 0;	/* how many pipe pushed bytes */
    unsigned long directory_offset = 0;
    int token_count = 0;
    int looping = 1;
    int wdirsize;
    unsigned32 tksetflags;
    int max_files = MIN(max_files_bulkstat, AFS_BULKMAX);
    opaque volatile dmptr = NULL;

    icl_Trace3(px_iclSetp, PX_TRACE_BULKSTAT, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_LONG, AFS_hgetlo(*Offsetp), ICL_TYPE_LONG, Size);

    px_ClearTokenStruct(OutTokenp);
    OutDirStatusp->fileType = (unsigned32) Invalid;
    bulk_stats->BulkStat_len = 0;
    if (!bulkstat_server_enable) {
	errorCode = rpc_s_op_rng_error;
    }

    if (!errorCode) {
	afs_hyper_t temp;

	/* Bounds check *Offsetp and Size against max supported file size;
         * truncate requested Size if necessary. */

	if (AFS_hcmp(*Offsetp, osi_maxFileServer) >= 0) {
	    errorCode = EFBIG;
	    goto errorexit;
	}
	temp = osi_maxFileServer;
	AFS_hsub(temp, *Offsetp);
#ifdef SGIMIPS
/* Turn off "pointless comparison of unsigned integer with zero" error." */
#pragma set woff 1183
#endif
	if (AFS_hcmp64(temp, 0, Size) < 0)
	    Size = AFS_hgetlo(temp);
#ifdef SGIMIPS
#pragma reset woff 1183
#endif
    }


    /* check for valid flags */
    if (!errorCode && InvalidTokenFlags(Flags)) {
	icl_Trace1(px_iclSetp, PX_TRACE_BULKSTAT_FLAG, ICL_TYPE_LONG, Flags);
	errorCode = EINVAL;
    }
    currOffset = AFS_hgetlo(*Offsetp);

    preempting = osi_PreemptionOff();
    afsStats.BulkFetchStatus++;
    afsStats.TotalCalls++;
    gluevn_SetThreadFlags(1);

    /* During post-recovery, return FSHS_ERR_SERVER_REESTABLISH. */
    if (!errorCode && fshs_postRecovery) {
	icl_Trace0(px_iclSetp, PX_TRACE_BULKSTAT_RECOVER);
	errorCode = FSHS_ERR_SERVER_REESTABLISH;
    }
    if (errorCode) {
	goto errorexit;
    }

    /*
     * Alloc up pipe buffer space
     * From here on, if error must at least go through freepipe.
     */
    pipe_buf = osi_AllocBufferSpace();
    /* bzero((caddr_t)pipe_buf, sizeof(pipe_buf)); */
    wiredp = (struct dir_minwireentry *)pipe_buf;

    px_AdjustCell(DirFidp);

    /*
     * Get a context from the binding to determine the caller's context.
     * From here on, if errror must at least go through freecookie.
     */
    errorCode = fshs_InqContext(h, (struct context **)&cookie);
    if (errorCode) {
	icl_Trace0(px_iclSetp, PX_TRACE_BULKSTAT_COOKIE);
	goto freecookie;
    }

    /*
     * Get our principal for this RPC.
     */
    if (errorCode = fshs_GetPrincipal((struct context *)cookie,
	(struct fshs_principal **)&princp, 1)) {
	icl_Trace0(px_iclSetp, PX_TRACE_BULKSTAT_PRINCIP);
	goto freecookie;
    }

    /*
     * We now have a principal that is 'held' and must be 'Put' on exit.
     * From here on, if errror go through deltset to clean up.
     */
    /* use this for files */
    InitToken(&ftoken, TKN_STATUS_READ);

    if (Flags & AFS_FLAG_NOOPTIMISM) {
    	tksetflags = TKSET_ATS_NOOPTIMISM;
    } else {
	tksetflags = TKSET_ATS_WANTOPTIMISM;
    }

    if (Flags & AFS_FLAG_NOREVOKE) {
	tksetflags |= TKSET_ATS_NOREVOKE;
    }

    if (!(Flags & AFS_FLAG_SKIPTOKEN)) {
	tset = tkset_Create(0);
	InitToken(&ltoken, TKN_STATUS_READ | TKN_DATA_READ);
	errorCode = tkset_AddTokenSet((struct tkset_set *)tset, DirFidp,
				      (afs_token_t *)&ltoken, NULL,
				      fshs_GetPrincHost(princp), tksetflags,
				      (opaque*)&dmptr);
	if (errorCode)
	    goto deltset;
    }

    credp = xcred_XCredToUCred(fshs_GetPrincAuth(princp));
    osi_assert(credp);

    /*
     * Do a lookup.  If thise succeeds we have a volp and dvp that are
     * 'held' and must be 'released' on the way out.
     */
    errorCode = pxreg_Lookup(DirFidp, VNOP_TYPE_READONLY, (opaque*)&dmptr,
			     (struct volume **)&volp, (struct vnode **)&dvp);
    if (errorCode) {
	icl_Trace1(px_iclSetp, PX_TRACE_BULKSTAT_LOOKUP_DIR,
		   ICL_TYPE_FID, DirFidp);
	goto deltset;
    }
    /*
     * From here on, if error, must go past baddir to rele volp, and dvp
     */

    if (tset)
	tkset_SetVolSync(tset, volp, VNOP_TYPE_READONLY);

    /*
     * Check for search and read permissions.
     */
#ifdef SGIMIPS
    VOPX_GETATTR(dvp, &dirxvattr, 1, credp, errorCode);
#else
    errorCode = VOPX_GETATTR(dvp, &dirxvattr, 1, credp);
#endif
    if (!errorCode) {
	u_int32 perms = dacl_perm_read;

	if ((dirxvattr.xvattr.callerAccess & perms) != perms)
	    errorCode = EACCES;
    }
    if (errorCode) {
	icl_Trace1(px_iclSetp, PX_TRACE_BULKSTAT_ATTR, ICL_TYPE_FID, DirFidp);
	goto baddir;
    }

    /*
     * Alloc our buffer space for reading directories.
     * Must free this on the way out.
     */
    bufferp = osi_AllocBufferSpace();

    while (looping) {
	/* check for pipe buf overflow */
	if (count_pipe_buf >= Size) {
	    break;
	}

	osi_InitUIO(&tuio);
	tuio.osi_uio_iov = &iov;
	tuio.osi_uio_iovcnt = 1;

	/*
	 * We track two offsets into the directory: where we
	 * are (currOffset) and where we were (directory_offset).
	 * We use currOffset to read the next dir record but may
	 * have to discard it for want of room in the buffer. We
	 * ultimately return directory_offset so the client knows
	 * where to start next call.
	 */
	if (i < max_files) {
	    tuio.osi_uio_offset = currOffset;
	} else {
	    tuio.osi_uio_offset = directory_offset;
	}
	tuio.osi_uio_resid = DIR_BLKSIZE;
	tuio.osi_uio_seg = OSI_UIOSYS;
	iov.iov_base = (char *) bufferp;
	iov.iov_len = DIR_BLKSIZE;

#ifdef SGIMIPS
	VOPX_READDIR(dvp, &tuio, credp, &junk, 1, errorCode);
	if (errorCode)
#else
	if (errorCode = VOPX_READDIR(dvp, &tuio, credp, &junk, 1))
#endif
	{
	    break;
	}

	/* bytes actually read */
	irecCount = DIR_BLKSIZE - tuio.osi_uio_resid;

	/* done? */
	if (irecCount <= 0) {
	    eof = 1;
	    break;
	}

	if (i >= max_files) {
	    break;
	}

	directory_offset = currOffset;

	/* where we left off */
	currOffset = tuio.osi_uio_offset;

	/*
	 * See direntry.h for
	 * platform dependency ifdefs.
	 */
	dp = (struct dir_minxentry *)bufferp;

	for (incount = 0; incount < irecCount; /*nothing*/) {
	    vp = (struct vnode *) 0;

	    if (i >= max_files) {
		looping = 0;
		break;
	    }

	    /* got a file? */
	    if (dp->inode) {

		/* stay within client byte limit */
		namep = (char *)dp + SIZEOF_DIR_MINXENTRY;

#if defined(AFS_SUNOS5_ENV) || defined(SGIMIPS)
		/* no namelen field is available */
		namelen = strlen(namep);
#else	/* not AFS_SUNOS5_ENV || SGIMIPS */
		namelen = dp->namelen;
#endif	/* not AFS_SUNOS5_ENV || SGIMIPS */

		/*
		 * Check to see if there is room enough in pipe buffer
		 */
		if ((count_pipe_buf + SIZEOF_DIR_MINWIREENTRY + namelen) >
		    MIN(Size, osi_BUFFERSIZE)) {
		    looping = 0;
		    break;
		}

		/* yep */
		wiredp->inode = htonl(dp->inode);

		/*
		 * AFS_TAG_ORIGASCII for backward compatibility.
		 */
		wiredp->codesetTag = htonl(AFS_TAG_ORIGASCII);

		/* 0 in high word of offset */
		wiredp->highOffset = htonl(0);

		/* copy local string and length to wire string */
		/* copy in first, put in network order after WDIRSIZE call */
		strcpy(((char *)wiredp) + SIZEOF_DIR_MINWIREENTRY, namep);
		wiredp->namelen = namelen;

		wdirsize = WDIRSIZE(wiredp);

		/* finally put in network byte order after WDIRSIZE call */
		wiredp->namelen = htons(namelen);
		wiredp->recordlen = htons(wdirsize);

		/* server offset of next record */
		wiredp->offset = htonl(dp->offset);

		count_pipe_buf += wdirsize;
		directory_offset = dp->offset;
		wiredp = (struct dir_minwireentry *)((char *)wiredp+wdirsize);

		/*
		 * In case there is an error while stat-ing this file
		 * we return an error in the stat buffer so as to keep the
		 * dir info and status info in sync at the client.
		 */
		bulk_stats->BulkStat_val[i].error = (error_status_t)0;
		AFS_hzero(bulk_stats->BulkStat_val[i].token.tokenID);
		px_ClearTokenStruct(&(bulk_stats->BulkStat_val[i].token));
		bulk_stats->BulkStat_val[i].stat.fileType =
		    (unsigned32) Invalid;
		bzero((char *)&(bulk_stats->BulkStat_val[i].fid),
		      sizeof(afsFid));

		/* get vnode */
#ifdef SGIMIPS
		VOPX_LOOKUP(dvp, namep, &vp, credp, errorCode);
#else
		errorCode = VOPX_LOOKUP(dvp, namep, &vp, credp);
#endif

		if (errorCode) {
		    icl_Trace1(px_iclSetp, PX_TRACE_BULKSTAT_LOOKUP_FILE,
			       ICL_TYPE_STRING, namep);

		    bulk_stats->BulkStat_val[i].error =
			(error_status_t)errorCode;

		    goto keeplooking;
		}
		/*
		 * Now have a held vp,  must be sure to rele if we exit
		 */

		/* get fid */
		xvfs_ConvertDev((struct vnode **) &vp);
		outfidp1 = &OutFid;
		FidCopy(outfidp1, DirFidp);
#ifdef SGIMIPS
		VOPX_AFSFID(vp, &OutFid, 0, errorCode);
#else
		errorCode = VOPX_AFSFID(vp, &OutFid, 0);
#endif
		if (errorCode) {
		    icl_Trace1(px_iclSetp, PX_TRACE_BULKSTAT_FID,
			       ICL_TYPE_POINTER, vp);

		    bulk_stats->BulkStat_val[i].error =
			(error_status_t)errorCode;

		    goto keeplooking;
		}

		/*
		 * Status token for file.
		 */
		if (!(Flags & AFS_FLAG_SKIPTOKEN)) {
		    if (!px_ClientHasTokens(fshs_GetPrincHost(princp), &OutFid)){
			errorCode=tkset_AddTokenSet(tset, &OutFid, &ftoken,
						    NULL,
						    fshs_GetPrincHost(princp),
						    tksetflags, (opaque*)&dmptr);
		    }

		    /* bail out on error */
		    if (errorCode) {
			/*
			 * The RELOCK error actually means success,
			 * but some token had to be dropped during
			 * the call. This effectively invalidates the
			 * status info that we have collected up to now.
			 * If some other error is returned, it is also
			 * possible that some token was dropped during the
			 * call, so to be safe we have to assume the status
			 * info is bad on any error. We also treat the token
			 * info as unsafe. For the RELOCK case remap the
			 * error to something reasonable to send over the
			 * network as tkset errors should not leave the
			 * server.
			 */
			if (errorCode == TKSET_RELOCK) {
			    icl_Trace0(px_iclSetp, PX_TRACE_BULKSTAT_RELOCK);
			    errorCode = TKM_ERROR_NOENTRY;
			} else {
			    volp = (struct volume *) 0;
			}

			/* maybe lost token, so all stat invalid */
			for (j = 0; j <= i; ++j) {
			    AFS_hzero(bulk_stats->BulkStat_val[j].token.tokenID);
			    px_ClearTokenStruct(&(bulk_stats->BulkStat_val[j].token));
			    bulk_stats->BulkStat_val[j].stat.fileType = (unsigned32) Invalid;
			    bulk_stats->BulkStat_val[j].error = (error_status_t) errorCode;
			}

			if (vp) {
			    OSI_VN_RELE(vp);
			    vp = NULL;
			}

			/* don't return dir info either */
			if (dvp) {
			    OSI_VN_RELE(dvp);
			    dvp = NULL;
			}

			/* will return errorCode from here */
			looping = 0;
			break;
		    }
		}
		/*
		 * We will clear the token if SKIPTOKEN set.
		 */

		/* get status */
#ifdef SGIMIPS
		VOPX_GETATTR(vp, &xvattr, 1, credp, errorCode);
#else
		errorCode = VOPX_GETATTR(vp, &xvattr, 1, credp);
#endif
		if (!errorCode) {
		    px_xvattr_to_afsstatus(&xvattr,
					   &(bulk_stats->BulkStat_val[i].stat),
					   princp);
		    errorCode = DoLargeFileOldClient(&(bulk_stats->
						       BulkStat_val[i].stat),
						     princp);
		}
		if (errorCode) {
		    icl_Trace1(px_iclSetp, PX_TRACE_BULKSTAT_STAT,
			       ICL_TYPE_POINTER, vp);

		    /* mark status as invalid, and don't return status token */
		    bulk_stats->BulkStat_val[i].stat.fileType = Invalid;
		    AFS_hzero(bulk_stats->BulkStat_val[i].token.tokenID);
		    px_ClearTokenStruct(&(bulk_stats->BulkStat_val[i].token));

		    goto keeplooking;
		}

		/*
		 * At last, we have:
		 * 1. filename
		 * 2. fid
		 * 3. tokens
		 * 4. status (already set)
		 */
		outfidp1 = &(bulk_stats->BulkStat_val[i].fid);
		outfidp2 = &OutFid;
		FidCopy(outfidp1, outfidp2);
	    }

keeplooking:
	    if (vp) {
		OSI_VN_RELE(vp);
		vp = NULL;
	    }

	    ++i;

	    incount += dp->recordlen;
	    dp = (struct dir_minxentry *)((char *)dp + dp->recordlen);
	} /* for (incount = 0;... */
    } /* while looping */

    bulk_stats->BulkStat_len = i;

    /*
     * Is it safe to push?
     * If we push at at all, even if an exception occurs, then we indicate
     * that we tried, by setting pipeDrained.  This prevents pushing on
     * the pipe again in the clean up code at the end.
     */
    if (i > 0 && count_pipe_buf > 0) {
	osi_RestorePreemption(preempting);
	TRY {
	    (*(dirStream->push))(dirStream->state,
				 (unsigned char *)pipe_buf, count_pipe_buf);

	    /* Push EOF indicator */
	    (*(dirStream->push))(dirStream->state,
				 (unsigned char *)pipe_buf, 0);
	} CATCH_ALL {
	    preempting = osi_PreemptionOff();
	    icl_Trace0(px_iclSetp, PX_TRACE_BULKSTAT_PIPE);
	    uprintf("fx: BulkFetchStatus unexpected exception while pushing.\n");
	    errorCode = EIO;
	    osi_RestorePreemption(preempting);
	} ENDTRY
	preempting = osi_PreemptionOff();
	pipeDrained = 1;
    }

    if (errorCode) {
	goto freebuffer;
    }

    /* setup next offset */
    if (eof) {
	AFS_hzero(*NextOffsetp);
	directory_offset = (long) 0;
    } else {
	AFS_hset64(*NextOffsetp, 0, directory_offset);
        /*
         * 32/64-bit support: If the client is old, it can't handle an offset
         * beyond 2^31-1, so just return EOVERFLOW.
	 *
	 * XXX -- this is a place holder, as directory_offset is only 32bits.
         */
        if (!princp->hostp->supports64bit &&
	    AFS_hcmp(*NextOffsetp, osi_hMaxint32) >= 0) {
            errorCode = EOVERFLOW;
            goto freebuffer;
        }
    }

    if (dvp) {
    	if (!errorCode) {
	    if (volp) {
	    	px_SetSync(volp, Syncp, 0, credp);
	    }
	    px_xvattr_to_afsstatus(&dirxvattr, OutDirStatusp, princp);
	    if ((Flags & AFS_FLAG_RETURNTOKEN) &&
		!DoLargeFileOldClient(OutDirStatusp, princp)) {
		errorCode =
		    px_SetTokenStruct(tset, DirFidp, OutTokenp, princp);
	    }
    	}
    }

freebuffer:
    osi_FreeBufferSpace((struct osi_buffer *)bufferp);

baddir:
    if (dvp) {
	OSI_VN_RELE(dvp);
    }
    if (volp) {
	vol_EndVnodeOp((struct volume *)volp, ((opaque*)&dmptr));
	VOL_RELE(volp);
    }

    /* return tokens for files in the window */
    if (!errorCode && tset && (Flags & AFS_FLAG_RETURNTOKEN)) {
	for (j = 0; j < i; ++j) {
	    if (token_count < bulkstat_maxtok) {
		/* valid token? */
		if (!bulk_stats->BulkStat_val[j].error &&
		    bulk_stats->BulkStat_val[j].stat.fileType !=
		    (unsigned32)Invalid) {
		    px_SetTokenStruct(tset,
				      &(bulk_stats->BulkStat_val[j].fid),
				      &(bulk_stats->BulkStat_val[j].token),
				      princp);
		}
		++token_count;
	    }
	}
    }

deltset:
    if (tset) {
	tkset_Delete((struct tkset_set *)tset);
    }

    fshs_PutPrincipal((struct fshs_principal *)princp);

freecookie:
    osi_Free((char *)cookie, sizeof(struct context));

freepipe:
    osi_FreeBufferSpace((struct osi_buffer *)pipe_buf);

drainpipe:
errorexit:
    gluevn_Done();
    osi_RestorePreemption(preempting);
    /*
     * If when we arrive here, and we never pushed anything on the pipe
     * we must push at least once to keep CM from getting confused
     */
    if (!pipeDrained) {
	unsigned char pbuf[1];

	pbuf[0] = 0;
	TRY {			/* Try to catch any pipe exceptions */
	    /*
	     * Make sure to send CM at least 1 empty buffer.
	     */
	    (*(dirStream->push))(dirStream->state, pbuf, 0);
	} CATCH_ALL {
	    pbuf[0] = 1;
	} ENDTRY

	if (pbuf[0]) {
	    uprintf("fx: BulkFetchStatus pipe exception while pushing to clean up !\n");
	    icl_Trace0(px_iclSetp, PX_TRACE_BULKSTATDRAIN);
	}
    }
    icl_Trace3(px_iclSetp, PX_TRACE_BULKSTAT_END, ICL_TYPE_FID, DirFidp,
	       ICL_TYPE_LONG, directory_offset, ICL_TYPE_LONG, errorCode);

    return osi_errEncode(errorCode);
}

#ifdef notdef   /* For Future features ???????????  */
/*
 * SAFS_Spare1() RPC call
 */
SAFS_Spare1(h, InFidNamep, InStatusp, AccessListp, InTokenp, Flags,
	    spare1, spare2, spare3, OutFidNamep, OutStatusp, OutTokenp,
	    spare4, spare5, spare6, Syncp)
    handle_t h;		/* RPC-Call handle */
    afsFidName *InFidNamep;
    afsStoreStatus *InStatusp;
    afsACL *AccessListp;
    afs_token_t *InTokenp;
    unsigned long Flags;
    afs_hyper_t *spare1;
    afs_hyper_t *spare2;
    afs_hyper_t *spare3;
    afsFidName *OutFidNamep;
    afsFetchStatus *OutStatusp;
    afs_token_t *OutTokenp;
    afs_hyper_t *spare4;
    afs_hyper_t *spare5;
    afs_hyper_t *spare6;
    afsVolSync *Syncp;		/* volume synchronization parm */
{
    return osi_errEncode(ESRCH);
}


/*
 * SAFS_Spare2() RPC call
 */
SAFS_Spare2(h, InFidNamep, InStatusp, AccessListp, InTokenp, Flags,
	    spare1, spare2, spare3, OutFidNamep, OutStatusp, OutTokenp,
	    spare4, spare5, spare6, Syncp)
    handle_t h;		/* RPC-Call handle */
    afsFidName *InFidNamep;
    afsStoreStatus *InStatusp;
    afsACL *AccessListp;
    afs_token_t *InTokenp;
    unsigned long Flags;
    afs_hyper_t *spare1;
    afs_hyper_t *spare2;
    afs_hyper_t *spare3;
    afsFidName *OutFidNamep;
    afsFetchStatus *OutStatusp;
    afs_token_t *OutTokenp;
    afs_hyper_t *spare4;
    afs_hyper_t *spare5;
    afs_hyper_t *spare6;
    afsVolSync *Syncp;		/* volume synchronization parm */
{
     return osi_errEncode(ESRCH);
}


/*
 * SAFS_Spare3() RPC call
 */
SAFS_Spare3(h, InFidNamep, InStatusp, AccessListp, InTokenp, Flags,
	    spare1, spare2, spare3, OutFidNamep, OutStatusp, OutTokenp,
	    spare4, spare5, spare6, Syncp)
    handle_t h;		/* RPC-Call handle */
    afsFidName *InFidNamep;
    afsStoreStatus *InStatusp;
    afsACL *AccessListp;
    afs_token_t *InTokenp;
    unsigned long Flags;
    afs_hyper_t *spare1;
    afs_hyper_t *spare2;
    afs_hyper_t *spare3;
    afsFidName *OutFidNamep;
    afsFetchStatus *OutStatusp;
    afs_token_t *OutTokenp;
    afs_hyper_t *spare4;
    afs_hyper_t *spare5;
    afs_hyper_t *spare6;
    afsVolSync *Syncp;		/* volume synchronization parm */
{
     return osi_errEncode(ESRCH);
}


/*
 * SAFS_Spare4() RPC call
 */
SAFS_Spare4(h, InFidNamep, InStatusp, AccessListp, InTokenp, Flags,
	    spare1, spare2, spare3, OutFidNamep, OutStatusp, OutTokenp,
	    spare4, spare5, spare6, Syncp)
    handle_t h;		/* RPC-Call handle */
    afsFidName *InFidNamep;
    afsStoreStatus *InStatusp;
    afsACL *AccessListp;
    afs_token_t *InTokenp;
    unsigned long Flags;
    afs_hyper_t *spare1;
    afs_hyper_t *spare2;
    afs_hyper_t *spare3;
    afsFidName *OutFidNamep;
    afsFetchStatus *OutStatusp;
    afs_token_t *OutTokenp;
    afs_hyper_t *spare4;
    afs_hyper_t *spare5;
    afs_hyper_t *spare6;
    afsVolSync *Syncp;		/* volume synchronization parm */
{
     return osi_errEncode(ESRCH);
}


/*
 * SAFS_Spare5() RPC call
 */
SAFS_Spare5(h, InFidNamep, InStatusp, AccessListp, InTokenp, Flags,
	    spare1, spare2, spare3, OutFidNamep, OutStatusp, OutTokenp,
	    spare4, spare5, spare6, Syncp)
    handle_t h;		/* RPC-Call handle */
    afsFidName *InFidNamep;
    afsStoreStatus *InStatusp;
    afsACL *AccessListp;
    afs_token_t *InTokenp;
    unsigned long Flags;
    afs_hyper_t *spare1;
    afs_hyper_t *spare2;
    afs_hyper_t *spare3;
    afsFidName *OutFidNamep;
    afsFetchStatus *OutStatusp;
    afs_token_t *OutTokenp;
    afs_hyper_t *spare4;
    afs_hyper_t *spare5;
    afs_hyper_t *spare6;
    afsVolSync *Syncp;		/* volume synchronization parm */
{
     return osi_errEncode(ESRCH);
}


/*
 * SAFS_Spare6() RPC call
 */
SAFS_Spare6(h, InFidNamep, InStatusp, AccessListp, InTokenp, Flags,
	    spare1, spare2, spare3, OutFidNamep, OutStatusp, OutTokenp,
	    spare4, spare5, spare6, Syncp)
    handle_t h;			/* RPC-Call handle */
    afsFidName *InFidNamep;
    afsStoreStatus *InStatusp;
    afsACL *AccessListp;
    afs_token_t *InTokenp;
    unsigned long Flags;
    afs_hyper_t *spare1;
    afs_hyper_t *spare2;
    afs_hyper_t *spare3;
    afsFidName *OutFidNamep;
    afsFetchStatus *OutStatusp;
    afs_token_t *OutTokenp;
    afs_hyper_t *spare4;
    afs_hyper_t *spare5;
    afs_hyper_t *spare6;
    afsVolSync *Syncp;		/* volume synchronization parm */
{
     return osi_errEncode(ESRCH);
}


#endif /* notdef For future use */



/*
 *
 * To define the exported interface
 *
 */
    globaldef AFS4Int_v4_0_epv_t AFS4Int_v4_0_manager_epv = {
	SAFS_SetContext,
	SAFS_LookupRoot,
	SAFS_FetchData,
	SAFS_FetchACL,
	SAFS_FetchStatus,
	SAFS_StoreData,
	SAFS_StoreACL,
	SAFS_StoreStatus,
	SAFS_RemoveFile,
	SAFS_CreateFile,
	SAFS_Rename,
	SAFS_Symlink,
	SAFS_HardLink,
	SAFS_MakeDir,
	SAFS_RemoveDir,
	SAFS_Readdir,
	SAFS_Lookup,
	SAFS_GetToken,
	SAFS_ReleaseTokens,
	SAFS_GetTime,
	SAFS_MakeMountPoint,
	SAFS_GetStatistics,
	SAFS_BulkFetchVV,
	SAFS_BulkKeepAlive,
	SAFS_ProcessQuota,
	SAFS_GetServerInterfaces,
	SAFS_SetParams,
	SAFS_BulkFetchStatus
    };
