/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: rep_data.h,v $
 * Revision 65.2  1999/02/04 19:19:39  mek
 * Provide C style include file for IRIX kernel integration.
 *
 * Revision 65.1  1997/10/20 19:20:27  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.9.1  1996/10/02  17:47:04  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:37:33  damon]
 *
 * Revision 1.1.4.1  1994/06/09  14:07:57  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:22:53  annie]
 * 
 * Revision 1.1.2.6  1993/01/21  19:36:06  zeliff
 * 	Embedding copyright notices
 * 	[1993/01/19  19:49:46  zeliff]
 * 
 * Revision 1.1.2.5  1993/01/13  17:49:42  shl
 * 	Transarc delta: cfe-ot6054-genl-and-i18n-spares 1.1
 * 	  Selected comments:
 * 	    Add spares to several RPC structures, with two goals:
 * 	    - general future usage
 * 	    - allow for at least some internationalization designs
 * 	    The only tricky part of this work is that the on-the-wire representation for
 * 	    directory contents is expanded.
 * 	[1993/01/12  20:58:46  shl]
 * 
 * Revision 1.1.2.4  1992/11/24  17:03:50  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  17:59:25  bolinger]
 * 
 * Revision 1.1.2.3  1992/09/25  18:13:57  jaffe
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
 * 	    Case-(a) errors: fidsInVol_val, repStatuses_val, afsNetAddrs_val.
 * 	[1992/09/23  19:10:37  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  19:45:49  jaffe
 * 	Transarc delta: cfe-ot4029-portable-rpc-data-types 1.2
 * 	  Selected comments:
 * 	    If ``long'' could possibly mean ``64 bits'' any time soon, we need to keep
 * 	    our RPC interfaces from breaking.
 * 	    see above
 * 	    More of the same.  Forgot a couple of .idl files, and needed to change
 * 	    a couple of procedure signatures to match.
 * 	[1992/08/30  02:26:42  jaffe]
 * 
 * Revision 1.1  1992/01/19  02:51:10  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/* Generated by IDL compiler version OSF DCE T1.2.0-09 */
#ifndef rep_data_v0_0_included
#define rep_data_v0_0_included
#ifndef IDLBASE_H
#include <dce/idlbase.h>
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifndef nbase_v0_0_included
#include <dce/nbase.h>
#endif
#ifndef common_data_v0_0_included
#include <dcedfs/common_data.h>
#endif
#define REP_FIDBULKMAX (50)
#define REP_STATBULKMAX (10)
#define REP_ADDRBULKMAX (30)
#define REPMSG_SIZE (400)
typedef struct fidInVol {
unsigned32 Vnode;
unsigned32 Unique;
} fidInVol;
typedef struct fidsInVol {
unsigned32 fidsInVol_len;
fidInVol fidsInVol_val[50];
} fidsInVol;
typedef struct repNumTrack {
unsigned32 Count;
unsigned32 OverCount;
unsigned32 SizeOverCount;
/* Type must appear in user header or IDL */ afs_hyper_t SizeOverCountSq;
} repNumTrack;
typedef struct repserverStatus {
repNumTrack Attns;
repNumTrack KAs;
unsigned32 nextForceKA;
unsigned32 numReplicas;
unsigned32 numHosts;
unsigned32 numAllocVIDs;
unsigned32 numUsedVIDs;
unsigned32 numReusedVIDs;
unsigned32 numWillCalls;
unsigned32 willCallState;
unsigned32 willCallError;
unsigned32 nextWillCallTime;
unsigned32 spare1;
unsigned32 spare2;
unsigned32 spare3;
unsigned32 spare4;
unsigned32 spare5;
unsigned32 spare6;
unsigned32 spare7;
unsigned32 spare8;
unsigned32 spare9;
} repserverStatus;
typedef struct repStatus {
/* Type must appear in user header or IDL */ afs_hyper_t volId;
/* Type must appear in user header or IDL */ afs_hyper_t srcVolId;
/* Type must appear in user header or IDL */ afs_hyper_t curVV;
/* Type must appear in user header or IDL */ afs_hyper_t srcVV;
afsNetAddr srcAddr;
/* Type must appear in user header or IDL */ afs_hyper_t CellId;
/* Type must appear in user header or IDL */ afs_hyper_t WVT_ID;
/* Type must appear in user header or IDL */ afs_hyper_t spareh1;
/* Type must appear in user header or IDL */ afs_hyper_t spareh2;
/* Type must appear in user header or IDL */ afs_hyper_t spareh3;
afsTimeval tokenLossTime;
afsTimeval tokenExpireTime;
afsTimeval lastReplicaPublish;
afsTimeval vvCurr;
afsTimeval vvPingCurr;
afsTimeval whenTried;
afsTimeval whenToTryAgain;
afsTimeval lastKASweep;
afsTimeval sparet1;
afsTimeval sparet2;
unsigned32 flags;
unsigned32 volStates;
unsigned32 curAggr;
unsigned32 srcAggr;
unsigned32 numKAs;
unsigned32 numVolChanged;
unsigned32 volChangedOldestTimeUsed;
unsigned32 nextVolChangedTime;
unsigned32 sparel1;
unsigned32 sparel2;
unsigned32 sparel3;
codesetTag msgTag;
unsigned32 msgLength;
idl_byte volName[112];
idl_byte RepMsg[400];
} repStatus;
typedef struct repStatuses {
unsigned32 repStatuses_len;
repStatus repStatuses_val[10];
} repStatuses;
typedef struct afsNetAddrs {
unsigned32 afsNetAddrs_len;
afsNetAddr afsNetAddrs_val[30];
} afsNetAddrs;

#ifdef __cplusplus
    }

#endif
#endif
