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
/*
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/fshost/RCS/fshs_host.h,v 65.4 1998/04/01 14:15:54 gwehrman Exp $
 */



#ifndef	_FSHS_HOSTH_
#define	_FSHS_HOSTH_

#include <dcedfs/osi_net.h>


#define INITIAL_GRACE_PERIOD 20         /* start with 20 sec grace period */
/*
 * fshs_host's general macros 
 */
#define	FSHS_PROBETIME	15*60	/* Probe a client after 15m of inactivity */
#define	FSHS_HOSTGCTIME	60*60	/* GC host entry after 1 hr of inactivity */
#define	FSHS_HOSTSPERBLOCK 64	/* Group of hosts to allocate at an instance */
#define	FSHS_MAXHOSTBLOCKS 16	/* Maximum # of hosts: 16 * 64 */
#define	FSHS_MINFIFOHOSTS  20	/* Min of FIFO hosts to free */

/* 
 * fshs_host's flags bits 
 */
#define	FSHS_HOST_STALE		0x1  /* Host needs to be Garbage Collected */
#define	FSHS_HOST_STALEPRINC	0x2  /* A host's principal is stale */
#define FSHS_HOST_INSEC		0x4 /* in secondary hash table, too */
#define FSHS_HOST_INCALL	0x8 /* doing init or revoke call */
#define FSHS_HOST_WAITCALL	0x10 /* waiting for init or revoke to finish */
#define FSHS_HOST_INPRI		0x20/* in primary hash table, too */
#define FSHS_HOST_NEW_IF	0x40/* new-style client */
#define FSHS_HOST_NEW_ACL_IF	0x80/* new-style acl_edit client */
#define FSHS_HOST_LOCALCELL	0x100/* client in local cell */

/* 
 * The Host structure describes the state of a client (or an instance of the 
 * client), usually a cache manager or Rep, who is making a rpc call to the FX
 * server. Each host structure is chained by its IP address and also by its
 * client's UUID. 
 * For a given client type, say the cache manager, the remote call might be 
 * either for a primary service or for a secondary service. In this case, 
 * the FX server allocates two Host structures for a given client; one is for 
 * the primary service and the other one for the secondary service. However,
 * only the prime host structure contains all the info that FX server needs to
 * keep track of the desired client. 
 * IF the host structure is allocated for a secondary service, ie., 
 * IsPrimeHost == FALSE, it is merely used by the FX server to locate its 
 * associated prime host.
 *
 * lifeGuarantee <= RPCGuarantee.
 * If life guarantee is exceeded, if we get a token revoke, we return
 * success and mark the host down, but only after trying an RPC to
 * revoke the token.  If rpc guarantee exceeded, we do the same, but
 * don't even *bother* to try to revoke the token.  The idea is that
 * the client need only poll at the RPC frequency, and while we'll grab
 * the tokens back earlier, we'll at least notify the client.  However,
 * the client will see inconsistencies from life guarantee until rpc
 * guarantee.  Before life guarantee is exceeded, we return failure,
 * and token isn't even revoked.
 */

struct fshs_host {
    struct hs_host h;			/* Generic host object */
#ifdef SGIMIPS
    signed32 flags;				/* state flags */
#else  /* SGIMIPS */
    long flags;				/* state flags */
#endif /* SGIMIPS */
    unsigned32 maxFileParm;		/* value provided to set max file */
    afs_hyper_t maxFileSize;		/* max file size supported on host */
					/* NOTE -- should be 8-byte aligned */
    struct fshs_host *nextPriID;	/* Next pointer in uuid hash chain */
    struct fshs_host *nextSecID;	/* Next pointer in uuid hash chain */
    afsUUID clientMainID;		/* client's primary service ID */
    afsUUID clientSecID;		/* client's secondary service ID */
    int priIndex;			/* hash index on client uuid */
    int secIndex;			/* hash index on client uuid */
    struct sockaddr_in clientAddr;	/* callback ip address to client */
#ifdef SGIMIPS
    unsigned32 lastCall;		/* TimeStamp of last OK call */
    unsigned32 clientEpoch;		/* TimeStamp of the RPC connection */
    unsigned32 hostLifeGuarantee;	/* host timeout parameter (TSR) */
    unsigned32 hostRPCGuarantee;	/* host RPC timeout parameter (TSR) */
    unsigned32 deadServerTimeout;	/* CM poll interval on recovery */
    unsigned32 lastRevoke;		/* TimeStamp of last OK revocation */
#else  /* SGIMIPS */
    unsigned long lastCall;		/* TimeStamp of last OK call */
    unsigned long clientEpoch;		/* TimeStamp of the RPC connection */
    unsigned long hostLifeGuarantee;	/* host timeout parameter (TSR) */
    unsigned long hostRPCGuarantee;	/* host RPC timeout parameter (TSR) */
    unsigned long deadServerTimeout;	/* CM poll interval on recovery */
    unsigned long lastRevoke;		/* TimeStamp of last OK revocation */
#endif /* SGIMIPS */
    int rvkErrorCount;		/* Count of successive revocation errors */
    handle_t cbBinding;			/* call back to client's token server*/
    struct fshs_principal *princListp; 	/* List of host's principals */
    unsigned registerTokens:1;		/* the remote host exports TKN svc */
    unsigned supports64bit:1;		/* host supports file size >= 2^31 */
};

/*
 * hash bucket for two hash tables; fshs_priHostID and fshs_secHostID
 */
struct fshs_bucket { 
    struct fshs_host *next;
    /* struct lock_data lock; */
};

/*
 * Define macro to find out if a host is down.
 */
#define FSHS_ISHOSTDOWN(fshostp)	(HS_ISHOSTDOWN(&((fshostp)->h)))

/* 
 * Host hash table 
 */
#define	FSHS_NHOSTS		257	/* a prime number */

/* 
 * Host queue implementation routines
 */
#define FSHS_QTOH(q)		((struct fshs_host *)((char *)(q)))

/*
 * Exported by fshs_host.c
 */
extern struct fshs_bucket fshs_priHostID[];
extern struct fshs_bucket fshs_secHostID[];
extern struct lock_data fshs_hostlock;
extern struct squeue fshs_fifo;
extern struct fshs_host *fshs_HostFreeListp;

extern void fshs_InitHostMod _TAKES((void));
extern struct fshs_host *fshs_AllocHost _TAKES((void));
extern void fshs_FreeHost _TAKES((struct fshs_host *));
extern void fshs_UpdateHostList _TAKES((int));
extern struct fshs_host *fshs_GetHost _TAKES((struct context *));
extern void fshs_PutHost _TAKES((struct fshs_host *));
extern void fshs_GCHost _TAKES((struct fshs_host *));
extern struct fshs_host *fshs_GetSimpleHost _TAKES((void));
extern long fshs_AssignHost _TAKES((struct fshs_host *, struct context *,
				    int, struct fshs_host *));
extern long fshs_CreateHost _TAKES(
   (struct context *, struct sockaddr_in *, uuid_t *,
    handle_t, u_char *, unsigned32, struct fshs_host **hostP));
extern long fshs_GetTSRCode _TAKES((struct fshs_host *));

/* exported by fshs_subr.c */
extern int fshs_InitHostModule _TAKES((char *, struct fshs_security_advice *));
#ifdef SGIMIPS
extern int fshs_StartCall _TAKES((struct fshs_host *));
extern int fshs_EndCall _TAKES((struct fshs_host *));
#endif


#endif	/* _FSHS_HOSTH_ */
