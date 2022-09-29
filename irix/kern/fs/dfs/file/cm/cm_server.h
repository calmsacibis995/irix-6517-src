/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_server.h,v $
 * Revision 65.2  1998/03/19 23:47:24  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.1  1997/10/20  19:17:25  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.73.1  1996/10/02  17:12:40  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:57  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_server.h,v 65.2 1998/03/19 23:47:24 bdr Exp $ */

#ifndef	TRANSARC_CM_SERVERH_
#define	TRANSARC_CM_SERVERH_

#include <dcedfs/osi_net.h>
#include <dcedfs/flserver.h>

/*
 * Server module related constants
 */
#define	SR_NSERVERS	17 	/* hash table size for server table */
#define	SR_NTOKENS	16	/* max tokens per server to queue */
#ifndef	SR_MINCHANGE		/* (So that some can inc it in param.h) */
#define	SR_MINCHANGE 	2	/* min change we'll bother with */
#endif
#ifndef SR_MAXCHANGEBCK
#define SR_MAXCHANGEBCK	10	/* max secs we'll set a clock back at once */
#endif

/*
 * One such structure for each file server that the cm has contacted
 */
struct cm_server {
    struct cm_server *next;		/* next server in UUID hash chain */
    struct cm_server *nextPPL;          /* next server in PPL hash chain */
    struct cm_cell *cellp;		/* cell in which this host resides */
    struct cm_conn *connsp;		/* all user conns to this server */
    struct cm_site *sitep;              /* server's ntwk connection state */
    struct cm_asyncGrant *asyncGrantsp;	/* list of pending async grants */
    afs_hyper_t maxFileSize;		/* max file size supported by server */
					/* NOTE -- should be 8-byte aligned */
    struct cm_tokenList *returnList;	/* List to add to server.tokens */
    struct lock_data lock;		/* lock for each server entry */
    afsUUID objId;			/* object-ID for ifspec being used */
    afsUUID serverUUID;			/* uuid to identify the FX server */
    u_long  avgRtt;			/* smoothed rtt for this server */
    u_long  lastCall;			/* last successful RPC to server */
    u_long  lastAuthenticatedCall;	/* last successful auth'd RPC to server */
    u_long  downTime;			/* server down time */
    u_long  serverEpoch;		/* server epoch time */
    u_long  setCtxProcID;		/* thread ID who is DOINGSETCONTEXT */
    u_long  failoverCount;              /* addr fails since successful RPC */
    unsigned32 tokenCount;		/* count of list */
    u_short hostLifeTime;		/* poll interval to FX server (secs) */
    u_short origHostLifeTime;		/* unwashed hostLifeTime value */
    u_short deadServerTime;		/* poll interval to dead server  */
    u_short refCount;			/* not used. We don't do GC for now */
    u_short states;			/* server's state */
    u_short sType;              	/* type of server; RPC ifspec */
    u_char minAuthn, maxAuthn;	/* track what we learn of authn bounds */
    u_char maxSuppAuthn;		/* track hard-max of whats supported */
    char principal[MAXKPRINCIPALLEN];	/* princ name for the given file srv */
    unsigned32 maxFileParm;		/* value provided to set max file */
    unsigned supports64bit:1;		/* server supports file size >= 2^31 */
};


/* 
 * Return true if we have any cached RW vnodes from this server 
 * Must be called with, at least, a read "servep->lock"  held. 
 */
#define cm_HaveTokensFrom(serverp) ((serverp)->states & SR_HASTOKENS)

/*
 * Server's states
 */
#define SR_DOWN		     0x001	/* server is down */
#define SR_TSR		     0x002	/* doing server Token State Recovery */
#define SR_HASTOKENS	     0x004	/* have tokens from this server */
#define SR_NEWSTYLESERVER    0x008	/* set if is a new-interface server */
#define SR_DOINGSETCONTEXT   0x010	/* set if AFS_SetContext in progress */
#define SR_GOTAUXADDRS       0x020      /* got aux addrs from FLDB (FX/REP) */

/*    note: for each new RPC, we introduce the capability of bypassing calls
 *    to servers that do not support the RPC.
 */
#define SR_RPC_BULKSTAT	     0x100      /* doesn't support bulkstat */


/*
 * Types of Server, one for each RPC Ifspec
 */
#define	SRT_FL		0x1	/* flserver */
#define	SRT_FX		0x2	/* file exporter */
#define	SRT_REP		0x3	/* replication server */

/*
 * Macro to determine if server types share a rank.
 */
#define SRT_SHARE_RANK(svc1, svc2)  \
    (((svc1) == SRT_FL || (svc2) == SRT_FL) ? ((svc1) == (svc2)) : 1)

/*
 * Server attribute bounds
 */
/*    Max RTT of 500 sec (in microsec); greater value implies undefined */
#define SR_AVGRTT_MAX  (500 * 1000000)

/*
 * Server-address manipulation macros
 */
/*    test for address equality */
#define SR_ADDR_EQ(sockaddrp1, sockaddrp2) \
    ((sockaddrp1)->sin_addr.s_addr == (sockaddrp2)->sin_addr.s_addr)

/*    scalar address value for hashing/tracing */
#define SR_ADDR_VAL(sockaddrp)  ((sockaddrp)->sin_addr.s_addr)

/*    maximum number of address fail-overs before server is declared down */
#define SR_ADDR_FAILOVER_MAX    4

/*
 * Service level, ie., main or secondary,  for each Server type
 */
#define MAIN_SERVICE(srvType) ( (srvType) << 16 )
#define SEC_SERVICE(srvType)  ( (srvType) << 16 | 0x1 )

/*
 * Server type from this service 
 */
#define DFS_SRVTYPE(service)  ( (service) >> 16 )

/* 
 * Exported by cm_server.c 
 */
extern struct cm_server *cm_servers[SR_NSERVERS];
extern struct lock_data cm_serverlock;
extern int cm_needServerFlush;

extern struct cm_server *cm_GetServer _TAKES((struct cm_cell *,
					      char *,
					      int,
					      afsUUID *,
					      struct sockaddr_in *,
					      int));

extern struct cm_server *cm_FindServer _TAKES((afsUUID *));

extern struct cm_server *cm_FindNextServer _TAKES((struct cm_server *,
						   u_short));
extern int cm_SetServerRank _TAKES((struct sockaddr_in *, u_short, u_short));
extern void cm_SortServers _TAKES((struct cm_server **, int));
extern int cm_ServerDown _TAKES((struct cm_server *, struct cm_conn *, u_long));
extern u_long cm_CheckDownServers _TAKES((struct cm_cell *, u_long));
#ifdef SGIMIPS
extern unsigned32 cm_PingServers _TAKES((struct cm_cell *, u_long));
#else  /* SGIMIPS */
extern u_long cm_PingServers _TAKES((struct cm_cell *, u_long));
#endif /* SGIMIPS */
extern void cm_AdjustIncomingFid _TAKES((struct cm_server *, afsFid *));
extern int cm_SameServers _TAKES((struct cm_server **,
				  struct cm_server **,
				  int maxservers));
extern void cm_PrintServerText _TAKES((char *preamb,
				       struct cm_server *srvp,
				       char *postamb));
extern void cm_FxRepAddrFetch _TAKES((struct cm_cell *,
				      char *,
				      afsNetAddr *));
extern void cm_ReviveAddrsForServers _TAKES((void));
extern void cm_ResetAuthnForServers _TAKES((void));
#endif /* TRANSARC_CM_SERVERH_ */
