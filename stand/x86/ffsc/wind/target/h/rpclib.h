/* rpcLib.h - Remote Procedure Call library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,07sep92,smb  added include of rpc/clnt.h to remove warnings.
02a,04jul92,jcf  cleaned up.
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01b,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01a,16nov88,llk  written.
*/

#ifndef __INCrpcLibh
#define __INCrpcLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "rpc/clnt.h"

/* rpcLib status codes derived from Sun's RPC specification (see rpc/clnt.h) */

#define S_rpcLib_RPC_SUCCESS		(M_rpcClntStat | (int) RPC_SUCCESS)
/*
 * local errors
 */
#define S_rpcLib_RPC_CANTENCODEARGS	(M_rpcClntStat | (int) RPC_CANTENCODEARGS)
#define S_rpcLib_RPC_CANTDECODERES	(M_rpcClntStat | (int) RPC_CANTDECODERES)
#define S_rpcLib_RPC_CANTSEND		(M_rpcClntStat | (int) RPC_CANTSEND)
#define S_rpcLib_RPC_CANTRECV		(M_rpcClntStat | (int) RPC_CANTRECV)
#define S_rpcLib_RPC_TIMEDOUT		(M_rpcClntStat | (int) RPC_TIMEDOUT)
#define S_rpcLib_RPC_VERSMISMATCH	(M_rpcClntStat | (int) RPC_VERSMISMATCH)
#define S_rpcLib_RPC_AUTHERROR		(M_rpcClntStat | (int) RPC_AUTHERROR)
#define S_rpcLib_RPC_PROGUNAVAIL	(M_rpcClntStat | (int) RPC_PROGUNAVAIL)
#define S_rpcLib_RPC_PROGVERSMISMATCH	(M_rpcClntStat | (int) RPC_PROGVERSMISMATCH)
#define S_rpcLib_RPC_PROCUNAVAIL	(M_rpcClntStat | (int) RPC_PROCUNAVAIL)
#define S_rpcLib_RPC_CANTDECODEARGS	(M_rpcClntStat | (int) RPC_CANTDECODEARGS)
#define S_rpcLib_RPC_SYSTEMERROR	(M_rpcClntStat | (int) RPC_SYSTEMERROR)
#define S_rpcLib_RPC_UNKNOWNHOST	(M_rpcClntStat | (int) RPC_UNKNOWNHOST)
#define S_rpcLib_RPC_PMAPFAILURE	(M_rpcClntStat | (int) RPC_PMAPFAILURE)
#define S_rpcLib_RPC_PROGNOTREGISTERED	(M_rpcClntStat | (int) RPC_PROGNOTREGISTERED)
#define S_rpcLib_RPC_FAILED		(M_rpcClntStat | (int) RPC_FAILED)

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	rpcInit (void);
extern STATUS 	rpcTaskInit (void);
extern int 	rpcErrnoGet (void);
extern void 	rpcClntErrnoSet (enum clnt_stat status);

#else	/* __STDC__ */

extern STATUS 	rpcInit ();
extern STATUS 	rpcTaskInit ();
extern int 	rpcErrnoGet ();
extern void 	rpcClntErrnoSet ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCrpcLibh */
