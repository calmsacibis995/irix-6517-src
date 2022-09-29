/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: afs4int.h,v $
 * Revision 65.2  1999/02/04 19:19:39  mek
 * Provide C style include file for IRIX kernel integration.
 *
 * Revision 65.1  1997/10/20 19:20:27  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.76.1  1996/10/02  17:47:00  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:37:27  damon]
 *
 * Revision 1.1.71.3  1994/08/23  18:58:42  rsarbo
 * 	add AFS_FLAG_CONTEXT_NEW_ACL_IF flag
 * 	[1994/08/23  16:41:01  rsarbo]
 * 
 * Revision 1.1.71.2  1994/06/09  14:07:51  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:22:48  annie]
 * 
 * Revision 1.1.71.1  1994/02/04  20:18:44  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:13:04  devsrc]
 * 
 * Revision 1.1.69.1  1993/12/07  17:24:17  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:22:58  jaffe]
 * 
 * Revision 1.1.5.10  1993/01/21  19:35:49  zeliff
 * 	Embedding copyright notices
 * 	[1993/01/19  19:49:31  zeliff]
 * 
 * Revision 1.1.5.9  1993/01/13  17:49:29  shl
 * 	Transarc delta: cfe-ot4029-catch-more-unportable-types 1.1
 * 	  Selected comments:
 * 	    Convert even the debugging interface to more obviously portable types.
 * 	    Change decls of ``const long''s also.
 * 	Transarc delta: cfe-ot6051-four-bits-o-spares 1.1
 * 	  Selected comments:
 * 	    Create spares for various non-Unix clients (that will probably never be
 * 	    servers).
 * 	    Define fetchStatus and storeStatus slots for these attributes.
 * 	Transarc delta: cfe-ot6052-tkm-forced-revoke-protocol 1.1
 * 	  Selected comments:
 * 	    Define the network protocol piece of a forced-revocation feature.
 * 	    Define an additional flags bit.
 * 	Transarc delta: cfe-ot6054-genl-and-i18n-spares 1.1
 * 	  Selected comments:
 * 	    Add spares to several RPC structures, with two goals:
 * 	    - general future usage
 * 	    - allow for at least some internationalization designs
 * 	    The only tricky part of this work is that the on-the-wire representation for
 * 	    directory contents is expanded.
 * 	Transarc delta: tu-ot5849a-ot5849b-ot6048-super-delta 1.2
 * 	  Selected comments:
 * 	    This super-delta was created from configuration dfs-102, revision 2.2,
 * 	    by importing the following deltas:
 * 	    tu-ot5849-add-icl-fshost 1.2
 * 	    tu-ot5849-resolve-merge-conflict 1.1
 * 	    tu-ot6048-deadlock-primary-sec-service 1.3
 * 	    From tu-ot6048-deadlock-primary-sec-service 1.3:
 * 	    In servicing incoming CM requests, the krpc thread pool manager usually
 * 	    selects a free thread from either the default pool or the secondary pool
 * 	    depending on the type of an object id used in the rpc binding. If the CM
 * 	    binds its rpc call with the secondary object id, the call will be serviced
 * 	    by a thread from the secondary pool.
 * 	    The main concern is that CM's very first secondary SetContext call (which
 * 	    is to establish the secondary host context) is actually serviced by a
 * 	    free thread in the default thread pool, despite the call is binding with a
 * 	    secondary object id. This could cause a deadlock, however, if there is no
 * 	    available thread at the time, and all threads in use are waiting for modified
 * 	    data to be delivered once the secondary host context is established.
 * 	    Change the signature in AFS_SetContext by adding a new argument,
 * 	    afsUUID *secObject. Add two new flags AFS_FLAG_NOREVOKE and
 * 	    AFS_FLAG_SEC_SERVICE.
 * 	    more fixes
 * 	    fix some merge problems
 * 	Transarc delta: tu-ot5968-remove-obsolete-api 1.1
 * 	  Selected comments:
 * 	    Remove the obsolete rpc call, AFS_RenewAllToken and others.
 * 	[1993/01/12  20:58:21  shl]
 * 
 * Revision 1.1.5.8  1992/12/09  20:07:06  jaffe
 * 	Transarc delta: kazar-ot6315-return-blocks-allocated 1.1
 * 	  Selected comments:
 * 	    ls -ls should show the # of blocks really allocated.
 * 	    pass blocks used over the wire
 * 	[1992/12/09  18:40:13  jaffe]
 * 
 * Revision 1.1.5.7  1992/11/24  17:03:40  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  17:59:14  bolinger]
 * 
 * Revision 1.1.5.6  1992/11/18  20:32:34  jaffe
 * 	Transarc delta: kazar-ot5680-fix-copy-acl 1.1
 * 	  Selected comments:
 * 	    copy acl was assuming that null pointers could be passed across
 * 	    the wire
 * 	    add flag for describing what type of acl settign to do
 * 	[1992/11/17  21:38:01  jaffe]
 * 
 * Revision 1.1.5.5  1992/11/04  18:31:48  jaffe
 * 	Transarc delta: cburnett-ot5797-add-afsskipstatus 1.1
 * 	  Selected comments:
 * 	    Add new flag AFS_FLAG_SKIPSTATUS for future use
 * 	    Added flag.
 * 	[1992/11/04  17:08:44  jaffe]
 * 
 * Revision 1.1.5.4  1992/10/27  21:04:06  jaffe
 * 	Transarc delta: comer-ot5610-enable-fileset-move-tsr 1.3
 * 	  Selected comments:
 * 	    The code to enable the file exporter to go into move token state recovery
 * 	    was disabled by #ifdefs.  This delta removes them.
 * 	    Extend this delta to debug a snafu in the original TSR spec: even if
 * 	    a fileset is marked with VOL_MOVE_SOURCE, the fts command itself wants to
 * 	    obtain tokens on it.
 * 	    Add yet another AFS_GetToken flag saying that it's OK to get a token on the
 * 	    fileset even if it's marked VOL_MOVE_SOURCE.
 * 	    Need to turn off the VOL_MOVE_SOURCE and VOL_MOVE_TARGET flags.
 * 	Transarc delta: kazar-ot5556-pass-umask-to-create-ops 1.2
 * 	  Selected comments:
 * 	    pass umask to create ops for default acl handling
 * 	    new IDL for creating objs
 * 	    Fix unresolved symbol during link.
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
 * 	    Add two new flags, AFS_PARAM_RESET_CONN and  AFS_PARAM_TSR_COMPLETE for
 * 	    AFS_SetParams as part of TSR work.
 * 	    tyops
 * 	    From tu-ot4713-add-one-more-tokenflag 1.2:
 * 	    Our DFS should allow its client (ie., CM) to have an option of asking the
 * 	    server not to return the "optimistic" tokens if the CM  only wants
 * 	    a particular token.
 * 	    Added a new token flag, AFS_FLAG_NOOPTIMISM.
 * 	    same
 * 	[1992/10/27  14:25:44  jaffe]
 * 
 * Revision 1.1.5.3  1992/09/25  18:13:52  jaffe
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
 * 	    Several case-(a) errors fixed here: afsBulkFEX_val, afsACL_val, afsQuota_val,
 * 	    afsBulkVVs_val, afsBulkVolIDs_val, afsBulkStats_val.
 * 	Transarc delta: kazar-ot4474-sys-v-locking-async-grant 1.2
 * 	  Selected comments:
 * 	    start work on async grant
 * 	    add new flag to server API
 * 	    finish work on async grant
 * 	[1992/09/23  19:10:17  jaffe]
 * 
 * Revision 1.1.5.2  1992/08/31  19:45:31  jaffe
 * 	Transarc delta: cfe-ot4029-portable-rpc-data-types 1.2
 * 	  Selected comments:
 * 	    If ``long'' could possibly mean ``64 bits'' any time soon, we need to keep
 * 	    our RPC interfaces from breaking.
 * 	    see above
 * 	    More of the same.  Forgot a couple of .idl files, and needed to change
 * 	    a couple of procedure signatures to match.
 * 	Transarc delta: kazar-ot4376-truncpos-idl-change 1.1
 * 	  Selected comments:
 * 	    allow the normal storestatus call to send truncpos to the server
 * 	    fixes bug wherein a file truncate could go too far sometimes
 * 	    change .IDL definitions
 * 	[1992/08/30  02:26:22  jaffe]
 * 
 * Revision 1.1.3.4  1992/07/03  01:53:51  mason
 * 	Transarc delta: kazar-ot4358-token-return-etc-idl-change 1.1
 * 	  Selected comments:
 * 	    New IDL for ACL cache entry expiration, and for piggybacked token return.
 * 	    update IDL description
 * 	[1992/07/02  19:02:03  mason]
 * 
 * Revision 1.1.3.3  1992/07/02  21:52:02  mason
 * 	Transarc delta: kazar-ot4358-token-return-etc-idl-change 1.1
 * 	  Selected comments:
 * 	    New IDL for ACL cache entry expiration, and for piggybacked token return.
 * 	    update IDL description
 * 	[1992/07/02  19:02:03  mason]
 * 
 * Revision 1.1.3.2  1992/05/20  19:52:53  mason
 * 	Transarc delta: cfe-ot2605-tsr-i-f-changes 1.5
 * 	  Files modified:
 * 	    cm: cm_scache.h
 * 	    config: common_data.acf, common_data.idl, common_def.h
 * 	    fshost: fshs_errs.et
 * 	    fsint: afs4int.acf, afs4int.idl, tkn4int.acf, tkn4int.idl
 * 	    host: hs_errs.et; px: px_repops.c; rep: rep_main.c, repser.h
 * 	    userInt/fts: volc_tokens.c; xvolume: volume.h
 * 	  Selected comments:
 * 	    This delta should encompass the interface changes associated with token
 * 	    state recovery (over server crashes, network partitions, fileset moves,
 * 	    and other things).
 * 	    This delta now captures the interface changes for real.  It includes RPC interface
 * 	    changes, new error codes, and new status bits.  It also includes incomplete changes
 * 	    to fts to support the new interface.
 * 	    Specify the new RPC procedure and some AFS_GetToken flags.
 * 	    More TSR interface fallout, mostly to the new model.
 * 	    Fix a merge error.
 * 	    Fixing more merge and syntax errors
 * 	Transarc delta: kazar-ot3034-bad-time-mgmt 1.1
 * 	  Files modified:
 * 	    cm: cm_dcache.c, cm_scache.c, cm_scache.h, cm_subr.c
 * 	    cm: cm_vnodeops.c; fsint: afs4int.idl
 * 	    px: px.h, px_intops.c, px_subr.c
 * 	  Selected comments:
 * 	    mtime and ctime are version numbers as well as timestamps,
 * 	    and must be managed carefully (only increase, except during
 * 	    utimes call)
 * 	    ctime and mtime
 * 	[1992/05/20  11:31:29  mason]
 * 
 * Revision 1.1  1992/01/19  02:51:22  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef AFS4Int_v4_0_included
#define AFS4Int_v4_0_included
#ifndef IDLBASE_H
#include <dce/idlbase.h>
#endif
#include <dce/rpc.h>

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef nbase_v0_0_included
#include <dce/nbase.h>
#endif
#ifndef common_data_v0_0_included
#include <dcedfs/common_data.h>
#endif
#define AFS_SETMODTIME (1)
#define AFS_SETOWNER (2)
#define AFS_SETGROUP (4)
#define AFS_SETMODE (8)
#define AFS_SETACCESSTIME (16)
#define AFS_SETCHANGETIME (32)
#define AFS_SETLENGTH (64)
#define AFS_SETTYPEUUID (128)
#define AFS_SETDEVNUM (256)
#define AFS_SETMODEXACT (512)
#define AFS_SETTRUNCLENGTH (1024)
#define AFS_SETCLIENTSPARE (2048)
#define AFS_SYNCUNSYNC (2147483632)
#define AFS_SYNCINITIAL (2147483633)
#define AFS_ACLFLAG_COPY (1)
#define AFS_FLAG_RETURNTOKEN (1)
#define AFS_FLAG_TOKENJUMPQUEUE (2)
#define AFS_FLAG_SKIPTOKEN (4)
#define AFS_FLAG_NOOPTIMISM (8)
#define AFS_FLAG_TOKENID (16)
#define AFS_FLAG_RETURNBLOCKER (32)
#define AFS_FLAG_ASYNCGRANT (64)
#define AFS_FLAG_NOREVOKE (128)
#define AFS_FLAG_MOVE_REESTABLISH (256)
#define AFS_FLAG_SERVER_REESTABLISH (512)
#define AFS_FLAG_NO_NEW_EPOCH (1024)
#define AFS_FLAG_MOVE_SOURCE_OK (2048)
#define AFS_FLAG_SYNC (4096)
#define AFS_FLAG_ZERO (8192)
#define AFS_FLAG_SKIPSTATUS (16384)
#define AFS_FLAG_FORCEREVOCATIONS (32768)
#define AFS_FLAG_FORCEVOLQUIESCE (65536)
#define AFS_PARAM_RESET_CONN (1)
#define AFS_PARAM_TSR_COMPLETE (2)
#define AFS_PARAM_SET_SIZE (3)
#define AFS_FLAG_SEC_SERVICE (1)
#define AFS_FLAG_CONTEXT_NEW_IF (2)
#define AFS_FLAG_CONTEXT_DO_RESET (4)
#define AFS_FLAG_CONTEXT_NEW_ACL_IF (8)
#define AFS_CLIENTATTR_HIDDEN (1)
#define AFS_CLIENTATTR_SYSTEM (2)
#define AFS_CLIENTATTR_ARCHIVE (4)
#define AFS_CLIENTATTR_READONLY (8)
typedef struct afsNetData {
afsNetAddr sockAddr;
NameString_t principalName;
} afsNetData;
typedef struct afsVolSync {
/* Type must appear in user header or IDL */ afs_hyper_t VolID;
/* Type must appear in user header or IDL */ afs_hyper_t VV;
unsigned32 VVAge;
unsigned32 VVPingAge;
unsigned32 vv_spare1;
unsigned32 vv_spare2;
} afsVolSync;
typedef struct afsFetchStatus {
unsigned32 interfaceVersion;
unsigned32 fileType;
unsigned32 linkCount;
/* Type must appear in user header or IDL */ afs_hyper_t length;
/* Type must appear in user header or IDL */ afs_hyper_t dataVersion;
unsigned32 author;
unsigned32 owner;
unsigned32 group;
unsigned32 callerAccess;
unsigned32 anonymousAccess;
unsigned32 aclExpirationTime;
unsigned32 mode;
unsigned32 parentVnode;
unsigned32 parentUnique;
afsTimeval modTime;
afsTimeval changeTime;
afsTimeval accessTime;
afsTimeval serverModTime;
afsUUID typeUUID;
afsUUID objectUUID;
unsigned32 deviceNumber;
unsigned32 blocksUsed;
unsigned32 clientSpare1;
unsigned32 deviceNumberHighBits;
unsigned32 spare0;
unsigned32 spare1;
unsigned32 spare2;
unsigned32 spare3;
unsigned32 spare4;
unsigned32 spare5;
unsigned32 spare6;
} afsFetchStatus;
typedef struct afsStoreStatus {
unsigned32 mask;
afsTimeval modTime;
afsTimeval accessTime;
afsTimeval changeTime;
unsigned32 owner;
unsigned32 group;
unsigned32 mode;
/* Type must appear in user header or IDL */ afs_hyper_t truncLength;
/* Type must appear in user header or IDL */ afs_hyper_t length;
afsUUID typeUUID;
unsigned32 deviceType;
unsigned32 deviceNumber;
unsigned32 cmask;
unsigned32 clientSpare1;
unsigned32 deviceNumberHighBits;
unsigned32 spare1;
unsigned32 spare2;
unsigned32 spare3;
unsigned32 spare4;
unsigned32 spare5;
unsigned32 spare6;
} afsStoreStatus;
typedef struct afsDisk {
unsigned32 BlocksAvailable;
unsigned32 TotalBlocks;
unsigned32 spare1;
unsigned32 spare2;
unsigned32 spare3;
afsDiskName Name;
} afsDisk;
typedef struct afsStatistics {
unsigned32 CurrentMsgNumber;
unsigned32 OldestMsgNumber;
unsigned32 CurrentTime;
unsigned32 BootTime;
unsigned32 StartTime;
unsigned32 CurrentConnections;
unsigned32 TotalAfsCalls;
unsigned32 TotalFetchs;
unsigned32 FetchDatas;
unsigned32 FetchedBytes;
unsigned32 HighFetchedBytes;
unsigned32 FetchDataRate;
unsigned32 TotalStores;
unsigned32 StoreDatas;
unsigned32 StoredBytes;
unsigned32 HighStoredBytes;
unsigned32 StoreDataRate;
unsigned32 TotalRPCBytesSent;
unsigned32 HighTotalRPCBytesSent;
unsigned32 TotalRPCBytesReceived;
unsigned32 HighTotalRPCBytesReceived;
unsigned32 TotalRPCPacketsSent;
unsigned32 TotalRPCPacketsReceived;
unsigned32 TotalRPCPacketsLost;
unsigned32 TotalRPCBogusPackets;
unsigned32 SystemCPU;
unsigned32 UserCPU;
unsigned32 NiceCPU;
unsigned32 IdleCPU;
unsigned32 TotalIO;
unsigned32 ActiveVM;
unsigned32 TotalVM;
unsigned32 EtherNetTotalErrors;
unsigned32 EtherNetTotalWrites;
unsigned32 EtherNetTotalInterupts;
unsigned32 EtherNetGoodReads;
unsigned32 EtherNetTotalBytesWritten;
unsigned32 EtherNetTotalBytesRead;
unsigned32 ProcessSize;
unsigned32 WorkStations;
unsigned32 ActiveWorkStations;
unsigned32 Spare1;
unsigned32 Spare2;
unsigned32 Spare3;
unsigned32 Spare4;
unsigned32 Spare5;
unsigned32 Spare6;
unsigned32 Spare7;
unsigned32 Spare8;
afsDisk Disk1;
afsDisk Disk2;
afsDisk Disk3;
afsDisk Disk4;
afsDisk Disk5;
afsDisk Disk6;
afsDisk Disk7;
afsDisk Disk8;
afsDisk Disk9;
afsDisk Disk10;
afsDisk Disk11;
afsDisk Disk12;
afsDisk Disk13;
afsDisk Disk14;
afsDisk Disk15;
afsDisk Disk16;
} afsStatistics;
typedef struct afsFidExp {
afsFid fid;
unsigned32 keepAliveTime;
} afsFidExp;
#define AFS_KA_TIME_RECHECK (1)
typedef struct afsBulkFEX {
unsigned32 afsBulkFEX_len;
afsFidExp afsBulkFEX_val[32];
} afsBulkFEX;
typedef struct afsACL {
unsigned32 afsACL_len;
idl_byte afsACL_val[8188];
} afsACL;
typedef struct afsQuota {
unsigned32 afsQuotaType;
unsigned32 afsQuotaOp;
unsigned32 afsQuota_len;
unsigned32 afsQuota_val[32];
} afsQuota;
typedef struct afsBulkVVs {
unsigned32 afsBulkVVs_len;
afsVolSync afsBulkVVs_val[32];
} afsBulkVVs;
typedef struct afsBulkVolIDs {
unsigned32 afsBulkVolIDs_len;
/* Type must appear in user header or IDL */ afs_hyper_t afsBulkVolIDs_val[32];
} afsBulkVolIDs;
typedef struct afsBulkStats {
unsigned32 afsBulkStats_len;
afsFetchStatus afsBulkStats_val[32];
} afsBulkStats;
typedef struct bundled_stat {
afsFid fid;
afsFetchStatus stat;
/* Type must appear in user header or IDL */ afs_token_t token;
error_status_t error;
} bundled_stat;
typedef struct BulkStat {
unsigned32 BulkStat_len;
bundled_stat BulkStat_val[32];
} BulkStat;
extern error_status_t AFS_SetContext(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ unsigned32 epochTime,
    /* [in] */ afsNetData *callbackAddr,
    /* [in] */ unsigned32 Flags,
    /* [in] */ afsUUID *secObjectID,
    /* [in] */ unsigned32 clientSizesAttrs,
    /* [in] */ unsigned32 parm7
#endif
);
extern error_status_t AFS_LookupRoot(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *InFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_FetchData(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Position,
    /* [in] */ signed32 Length,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp,
    /* [out] */ pipe_t *fetchStream
#endif
);
extern error_status_t AFS_FetchACL(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ unsigned32 aclType,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsACL *AccessListp,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_FetchStatus(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_StoreData(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Position,
    /* [in] */ signed32 Length,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [in] */ pipe_t *storeStream,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_StoreACL(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ afsACL *AccessListp,
    /* [in] */ unsigned32 aclType,
    /* [in] */ afsFid *aclFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_StoreStatus(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_RemoveFile(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsFidTaggedName *Namep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *returnTokenIDp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsFetchStatus *OutFileStatusp,
    /* [out] */ afsFid *OutFileFidp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_CreateFile(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_Rename(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *OldDirFidp,
    /* [in] */ afsFidTaggedName *OldNamep,
    /* [in] */ afsFid *NewDirFidp,
    /* [in] */ afsFidTaggedName *NewNamep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *returnTokenIDp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutOldDirStatusp,
    /* [out] */ afsFetchStatus *OutNewDirStatusp,
    /* [out] */ afsFid *OutOldFileFidp,
    /* [out] */ afsFetchStatus *OutOldFileStatusp,
    /* [out] */ afsFid *OutNewFileFidp,
    /* [out] */ afsFetchStatus *OutNewFileStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_Symlink(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsTaggedPath *LinkContentsp,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_HardLink(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsFid *ExistingFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_MakeDir(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_RemoveDir(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsFidTaggedName *Namep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *returnTokenIDp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutDelStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_Readdir(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Offsetp,
    /* [in] */ unsigned32 Size,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ /* Type must appear in user header or IDL */ afs_hyper_t *NextOffsetp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp,
    /* [out] */ pipe_t *dirStream
#endif
);
extern error_status_t AFS_Lookup(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_GetToken(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_token_t *MinTokenp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_recordLock_t *OutBlockerp,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_ReleaseTokens(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsReturns *Tokens_Arrayp,
    /* [in] */ unsigned32 Flags
#endif
);
extern error_status_t AFS_GetTime(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [out] */ unsigned32 *Secondsp,
    /* [out] */ unsigned32 *USecondsp,
    /* [out] */ unsigned32 *SyncDistance,
    /* [out] */ unsigned32 *SyncDispersion
#endif
);
extern error_status_t AFS_MakeMountPoint(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsTaggedName *cellNamep,
    /* [in] */ afsFStype Type,
    /* [in] */ afsTaggedName *volumeNamep,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_GetStatistics(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [out] */ afsStatistics *Statisticsp
#endif
);
extern error_status_t AFS_BulkFetchVV(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *cellIdp,
    /* [in] */ afsBulkVolIDs *VolIDsp,
    /* [in] */ unsigned32 NumVols,
    /* [in] */ unsigned32 Flags,
    /* [in] */ unsigned32 spare1,
    /* [in] */ unsigned32 spare2,
    /* [out] */ afsBulkVVs *VolVVsp,
    /* [out] */ unsigned32 *spare4
#endif
);
extern error_status_t AFS_BulkKeepAlive(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsBulkFEX *KAFEXp,
    /* [in] */ unsigned32 numExecFids,
    /* [in] */ unsigned32 Flags,
    /* [in] */ unsigned32 spare1,
    /* [in] */ unsigned32 spare2,
    /* [out] */ unsigned32 *spare4
#endif
);
extern error_status_t AFS_ProcessQuota(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [in, out] */ afsQuota *quotaListp,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
extern error_status_t AFS_GetServerInterfaces(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in, out] */ dfs_interfaceList *serverInterfacesP
#endif
);
extern error_status_t AFS_SetParams(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ unsigned32 Flags,
    /* [in, out] */ afsConnParams *paramsP
#endif
);
extern error_status_t AFS_BulkFetchStatus(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Offsetp,
    /* [in] */ unsigned32 Size,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ BulkStat *bulkstats,
    /* [out] */ /* Type must appear in user header or IDL */ afs_hyper_t *NextOffsetp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp,
    /* [out] */ pipe_t *dirStream
#endif
);
typedef struct AFS4Int_v4_0_epv_t {
error_status_t (*AFS_SetContext)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ unsigned32 epochTime,
    /* [in] */ afsNetData *callbackAddr,
    /* [in] */ unsigned32 Flags,
    /* [in] */ afsUUID *secObjectID,
    /* [in] */ unsigned32 clientSizesAttrs,
    /* [in] */ unsigned32 parm7
#endif
);
error_status_t (*AFS_LookupRoot)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *InFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_FetchData)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Position,
    /* [in] */ signed32 Length,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp,
    /* [out] */ pipe_t *fetchStream
#endif
);
error_status_t (*AFS_FetchACL)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ unsigned32 aclType,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsACL *AccessListp,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_FetchStatus)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_StoreData)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Position,
    /* [in] */ signed32 Length,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [in] */ pipe_t *storeStream,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_StoreACL)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ afsACL *AccessListp,
    /* [in] */ unsigned32 aclType,
    /* [in] */ afsFid *aclFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_StoreStatus)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_RemoveFile)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsFidTaggedName *Namep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *returnTokenIDp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsFetchStatus *OutFileStatusp,
    /* [out] */ afsFid *OutFileFidp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_CreateFile)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_Rename)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *OldDirFidp,
    /* [in] */ afsFidTaggedName *OldNamep,
    /* [in] */ afsFid *NewDirFidp,
    /* [in] */ afsFidTaggedName *NewNamep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *returnTokenIDp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutOldDirStatusp,
    /* [out] */ afsFetchStatus *OutNewDirStatusp,
    /* [out] */ afsFid *OutOldFileFidp,
    /* [out] */ afsFetchStatus *OutOldFileStatusp,
    /* [out] */ afsFid *OutNewFileFidp,
    /* [out] */ afsFetchStatus *OutNewFileStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_Symlink)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsTaggedPath *LinkContentsp,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_HardLink)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsFid *ExistingFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_MakeDir)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_RemoveDir)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsFidTaggedName *Namep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *returnTokenIDp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutDelStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_Readdir)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Offsetp,
    /* [in] */ unsigned32 Size,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ /* Type must appear in user header or IDL */ afs_hyper_t *NextOffsetp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp,
    /* [out] */ pipe_t *dirStream
#endif
);
error_status_t (*AFS_Lookup)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_GetToken)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_token_t *MinTokenp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_recordLock_t *OutBlockerp,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_ReleaseTokens)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsReturns *Tokens_Arrayp,
    /* [in] */ unsigned32 Flags
#endif
);
error_status_t (*AFS_GetTime)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [out] */ unsigned32 *Secondsp,
    /* [out] */ unsigned32 *USecondsp,
    /* [out] */ unsigned32 *SyncDistance,
    /* [out] */ unsigned32 *SyncDispersion
#endif
);
error_status_t (*AFS_MakeMountPoint)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ afsTaggedName *Namep,
    /* [in] */ afsTaggedName *cellNamep,
    /* [in] */ afsFStype Type,
    /* [in] */ afsTaggedName *volumeNamep,
    /* [in] */ afsStoreStatus *InStatusp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ afsFid *OutFidp,
    /* [out] */ afsFetchStatus *OutFidStatusp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_GetStatistics)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [out] */ afsStatistics *Statisticsp
#endif
);
error_status_t (*AFS_BulkFetchVV)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *cellIdp,
    /* [in] */ afsBulkVolIDs *VolIDsp,
    /* [in] */ unsigned32 NumVols,
    /* [in] */ unsigned32 Flags,
    /* [in] */ unsigned32 spare1,
    /* [in] */ unsigned32 spare2,
    /* [out] */ afsBulkVVs *VolVVsp,
    /* [out] */ unsigned32 *spare4
#endif
);
error_status_t (*AFS_BulkKeepAlive)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsBulkFEX *KAFEXp,
    /* [in] */ unsigned32 numExecFids,
    /* [in] */ unsigned32 Flags,
    /* [in] */ unsigned32 spare1,
    /* [in] */ unsigned32 spare2,
    /* [out] */ unsigned32 *spare4
#endif
);
error_status_t (*AFS_ProcessQuota)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *Fidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [in, out] */ afsQuota *quotaListp,
    /* [out] */ afsFetchStatus *OutStatusp,
    /* [out] */ afsVolSync *Syncp
#endif
);
error_status_t (*AFS_GetServerInterfaces)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in, out] */ dfs_interfaceList *serverInterfacesP
#endif
);
error_status_t (*AFS_SetParams)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ unsigned32 Flags,
    /* [in, out] */ afsConnParams *paramsP
#endif
);
error_status_t (*AFS_BulkFetchStatus)(
#ifdef IDL_PROTOTYPES
    /* [in] */ handle_t h,
    /* [in] */ afsFid *DirFidp,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *Offsetp,
    /* [in] */ unsigned32 Size,
    /* [in] */ /* Type must appear in user header or IDL */ afs_hyper_t *minVVp,
    /* [in] */ unsigned32 Flags,
    /* [out] */ BulkStat *bulkstats,
    /* [out] */ /* Type must appear in user header or IDL */ afs_hyper_t *NextOffsetp,
    /* [out] */ afsFetchStatus *OutDirStatusp,
    /* [out] */ /* Type must appear in user header or IDL */ afs_token_t *OutTokenp,
    /* [out] */ afsVolSync *Syncp,
    /* [out] */ pipe_t *dirStream
#endif
);
} AFS4Int_v4_0_epv_t;
extern rpc_if_handle_t AFS4Int_v4_0_c_ifspec;
extern rpc_if_handle_t AFS4Int_v4_0_s_ifspec;

#ifdef __cplusplus
    }

#else
#endif
#endif
