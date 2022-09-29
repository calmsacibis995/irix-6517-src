/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */


#ifndef	_FSHS_H_
#define	_FSHS_H_

#include <dcedfs/common_data.h>
#include <dcedfs/icl.h>
#include <dcedfs/lock.h>
#include <dcedfs/hs_host.h>
#include <dcedfs/hs_errs.h>
#include <dce/id_base.h>   /* dce pac's header */
#include <dce/rpcbase.h>   /* dce auth object rpc_authz_cred_handle_t */

extern struct icl_set *fshs_iclSetp;

/*
 * The 'context' structure contains each RPC call's authn/authz information 
 * representing the identity of the remote caller. The FX server quickly
 * composes (via fshs_InqContext()) this info upon receiving the rpc call and
 * then uses this 'cookie' to locate (via fshs_GetPrincipal) structure 
 * 'fshs_host and fshs_principal' that maintain remote caller's current state. 
 */
struct context {
    rpc_authz_cred_handle_t     cred_ptr;   /* cred handle */
    handle_t   callbackBinding;	/* call back binding to TokenRevo srv. */
    unsigned32 epochTime;	/* the time that client was born */
    afsUUID    clientID;	/* the objectID of the client */
    int        uuidIndex;	/* hash index on client's uuid */
    int		authnLevel;	/* incoming authn level--authz_dce or authz_name */
    /* some flags */
    unsigned   isAuthn:1;		/* Is authenticated ? */
    unsigned   needSecondarySvc:1;	/* Need secondary service ? */
    unsigned   registerTokens:1;	/* Is Token Revocation registered ? */
    unsigned   initContext:1;		/* initiate a context ? */
};

/* 
 * Contains general statistics regarding the file server host module 
 */
struct fshs_stats {
    long    usedHosts;		    /* # of hosts in hash table */
    long    allocHostBlocks;	    /* # of allocated blocks of hosts */
    long    usedPrincipals;         /* # of principals not in free list */
    long    allocPrincipalBlocks;   /* # of allocated blocks of princ */
    /* 
     * The following are up to date stats regarding the state of the various 
     * host entries; these number are re-evaluated periodically (every 5 mins)
     * by a daemon that traverses the host linked list 
     */
    long    goodHosts;              /* # of valid host structures */
    long    activeHosts;            /* # of "active" hosts */
    long    downHosts;              /* # of hosts whose clients are "down"*/
    /* 
     * The following are actually timers that determine when certain events
     * should happen; they may be adjusted based on the system load
     */
    u_long   hostProbeTime;         /* Host idle period before probing it */
    u_long   hostGCTime;            /* Host inactivity period before GC it */
    u_long   principalGCTime;       /* Principal conn inactivity before GCed */
};

/*
 * The following define the recovery characteristics of the file
 * exporter as part of the improved token state recovery.
 */
struct fshs_tsrParams {
    u_long   lifeGuarantee;	    /* default host timeout */
    u_long   RPCGuarantee;    	    /* default RPC timeout */
    u_long   deadServer;	    /* dead server timeout */ 
    u_long   maxLife;		    /* max value for lifeGuarantee */
    u_long   maxRPC;		    /* max value for RPC Guarantee */
    u_long   serverRestart;	    /* restart epoch */
    u_long   endRecoveryTime;	    /* end of post-recovery period */
};

/*
 * Argument structure for security advice
 */
struct fshs_security_bounds {
    unsigned32	minProtectLevel;
    unsigned32	maxProtectLevel;
};
/* Define the current shape of a syscall argument */
#define	FSHS_SEC_ADVICE_FORMAT_1	1
struct fshs_security_advice {
    unsigned32	formatTag;
    struct fshs_security_bounds	local;
    struct fshs_security_bounds	nonLocal;
};

/*
 * HS related headers
 */
#include <dcedfs/fshs_errs.h>
#include <dcedfs/fshs_host.h>
#include <dcedfs/fshs_princ.h>

/*
 * Exported by hs_init.c
 */
extern int fshs_postRecovery;
extern struct fshs_stats fshs_stats;
extern struct fshs_tsrParams fshs_tsrParams;
extern int fshs_HostCheckInterval;
extern struct sockaddr_in *localHost;
extern char *fshs_cellNamep;
extern int fshs_cellNameLen;
extern osi_dlock_t fshost_SecurityLock;
extern struct fshs_security_advice fshost_Security;
#ifdef SGIMIPS
int fshs_Enumerate(int (*proc)(), char *param);
int fshs_CheckHost(struct fshs_host *hostp);
#else
extern int fshs_Enumerate(), fshs_CheckHost();
#endif

/*
 * Exported by fshs_hostops.c
 */
extern struct hostops fshs_ops;
#ifdef SGIMIPS
int tokenint_InitTokenState(struct fshs_host *hostp, handle_t hdl);

/*
 * Exported by fshs_princ.c
 */
extern int fshs_GCPrinc(register struct fshs_host *hostp, int *nump);

/*
 * Exported by fshs_prutils.c
 */
extern int fshs_InitUnAuthPA(void);
extern int fshs_GetPAC(struct context *cookie, struct fshs_principal *princp);

/*
 * Exported by fshs_subr.c
 */
extern u_long fshs_HostCheckDaemon(void);
extern fshs_SetRecoveryParameters(u_long lifeGuarantee, u_long RPCGuarantee,
				  u_long deadServer, u_long postRecovery,
				  u_long maxLife, u_long maxRPC);
#endif

#endif	/* _FSHS_H_ */
