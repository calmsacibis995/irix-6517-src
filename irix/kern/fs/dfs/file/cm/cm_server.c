/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_server.c,v 65.9 1999/08/24 20:05:41 gwehrman Exp $";
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
 *      All rights reserved.
 */

#include <dcedfs/param.h>			/* Should be always first */
#include <dcedfs/sysincludes.h>		/* Standard vendor system headers */
#include <dcedfs/osi_cred.h>
#include <dcedfs/lock.h>
#include <dcedfs/tpq.h>			/* thread pool package */
#include <dcedfs/syscall.h>
#include <dcedfs/nubik.h>
#include <dcedfs/afsvl_proc.h>
#include <dcedfs/rep_proc.h>
#include <dcedfs/scx_errs.h>
#include <dce/uuid.h>
#include "cm.h"				/* Cm-based standard headers */
#include "cm_server.h"
#include "cm_cell.h"
#include "cm_conn.h"
#include "cm_volume.h"
#include "cm_serverpref.h"
#include "cm_site.h"

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_server.c,v 65.9 1999/08/24 20:05:41 gwehrman Exp $")

struct lock_data cm_serverlock;		/* allocation lock for servers */

static afsUUID cm_setTimeHostUUID;      /* host we use for time */
struct afsNetAddr cm_setTimeHostAddr;   /* an addr for cm_setTimeHostUUID */
static int cm_setTimeHostValid = 0;     /* cm_setTimeHostUUID valid ? */

int cm_needServerFlush = 0;
struct afsTimeval cm_timeWhenSet;
u_long cm_timeResidue = 0;

/* Max number of consecutive address fail-overs before server is declared down;
 * patchable value for multi-homed server testing.
 */
static int cm_addrFailoverMax = 4;

long cm_random _TAKES((void));
#if	defined(KERNEL)
static void SyncLocalTime _TAKES((struct cm_conn *, struct timeval *, long, 
				 u_long, u_long));
static int SyncFxServer _TAKES((struct cm_conn *));
static void ReviveAddrsForServer _TAKES((void *));
static void FxRepAddrFetch _TAKES((void *, void *));
#endif /* KERNEL */

/* Server structure hash tables and hash functions.
 *
 * Server structures (cm_server) are chained in two hash tables, one
 * hashed by UUID (cm_servers[]), and the other hashed by principal
 * name (cm_serversPPL[]).  Currently, server UUID is assigned in
 * cm_GetServer() and is thus CM-centric.
 *
 * Hopefully, one day DFS servers will have a "real" UUID, obviating
 * the need for the cm_serversPPL[] hash table.
 */

struct cm_server *cm_servers[SR_NSERVERS];
static struct cm_server *cm_serversPPL[SR_NSERVERS];

#define SR_HASH(suuidp, stp) ((u_long)uuid_hash((suuidp), (stp)) % SR_NSERVERS)
#define SR_HASHPPL(namep)    (strToInt((namep)) % SR_NSERVERS)


/* Declare some local functions */

static struct cm_server*
FindServerByPPL(struct cm_cell *cellp, char *namep, u_short svc);

static int strToInt(char *strp);



/* Define exported/local functions */

/* 
 * Find an already-existing server based on the server UUID. 
 */
#ifdef SGIMIPS
struct cm_server *cm_FindServer (afsUUID *serverID)
#else  /* SGIMIPS */
struct cm_server *cm_FindServer (serverID)
    afsUUID *serverID;
#endif /* SGIMIPS */
{
    register struct cm_server *serverp;
    int i;
    unsigned32 st;

#ifdef SGIMIPS
    i = (int)SR_HASH(serverID, &st);
#else  /* SGIMIPS */
    i = SR_HASH(serverID, &st);
#endif /* SGIMIPS */
    
    lock_ObtainRead(&cm_serverlock);
    for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	if (uuid_equal(serverID, &serverp->serverUUID, &st)) { 
	    lock_ReleaseRead(&cm_serverlock);
	    /* should increment the refCount, if we GC servers */
	    return serverp;
	}
    }
    lock_ReleaseRead(&cm_serverlock);
    return (struct cm_server *) 0;
}



/***********************************************************************
 *
 * cm_FindNextServer(lastHad, type):	Find the next server entry in 
 *			the (UUID) hash table which has the specified type.
 *
 * OUTPUT:	Returns an unlocked server pointer.
 *
 * LOCKING: 	Called with no locks.  Takes and releases the cm_serverlock
 *		while searching the hash table.
 ***********************************************************************/

struct cm_server *cm_FindNextServer(struct cm_server *lastHad, u_short type) 
{
    struct cm_server	*serverp = lastHad;
    int		i=0;
#ifdef SGIMIPS
    unsigned32 st;
#else  /* SGIMIPS */
    u_long st;
#endif /* SGIMIPS */

    lock_ObtainRead(&cm_serverlock);
    if (serverp != NULL) {
    	while ((serverp->next != NULL) && (serverp->next->sType != type)) {
	    serverp = serverp->next;
	}
	/*
	 * Either about to fall off the end, or next one is a candidate
	 */
        if (serverp->next != NULL) {
            serverp = serverp->next;
            lock_ReleaseRead(&cm_serverlock);
            return(serverp);
        } else {		/* Get the bucket number where we left off */
#ifdef SGIMIPS
	    i = (int)SR_HASH(&serverp->serverUUID, &st) + 1;
#else  /* SGIMIPS */
	    i = SR_HASH(&serverp->serverUUID, &st) + 1;
#endif /* SGIMIPS */
        }
    }
    /*
     * common case for either first call (null arg) 
     * or fell off the end of a bucket.
     * Find first non-empty bucket after i and continue search.
     */
    for (; i<SR_NSERVERS; i++) {
        if (cm_servers[i] != NULL) {
            serverp = cm_servers[i];
	    if (serverp->sType == type) {
		lock_ReleaseRead(&cm_serverlock);
		return(serverp);
	    } else {
		while ((serverp->next != NULL) && 
		       (serverp->next->sType != type)) {
		    serverp = serverp->next;
		}
		/*
		 * Either about to fall off the end, or next one is a candidate
		 */
		if (serverp->next != NULL) {
		    serverp = serverp->next;
		    lock_ReleaseRead(&cm_serverlock);
		    return(serverp);
		} else {
		    /* EMPTY */
		    /*
		     * Fell off the end of another bucket,
		     * keep looking...
		     */
		    /* continue; */
		}
	    }
        }
    }
    lock_ReleaseRead(&cm_serverlock);
    return(NULL);
}



/* 
 * Given a server & a fid, fill in the cell name of the fid appropriately.
 */
#ifdef SGIMIPS
void cm_AdjustIncomingFid(
register struct cm_server *aserverp,
register afsFid *afidp) 
#else  /* SGIMIPS */
void cm_AdjustIncomingFid(aserverp, afidp)
register struct cm_server *aserverp;
register afsFid *afidp; 
#endif /* SGIMIPS */
{
    register struct cm_cell *tcellp;

    tcellp = aserverp->cellp;
    afidp->Cell = tcellp->cellId;
}



/*
 * cm_GetServer() -- locate/allocate a server structure for the
 *     triple (cellp, princp, sType).
 *
 *     Note that if the server entry already exists, its object-ID is updated
 *     if objIdp is defined; however, its address list is NOT changed.
 */

struct cm_server *cm_GetServer(
  struct cm_cell *cellp,           /* server's cell */
  char *princp,                    /* server's principal name */
  int sType,                       /* server's type (SRT_FX/REP/FL) */
  afsUUID *objIdp,                 /* object ID (optional) */
  struct sockaddr_in *addrvp,      /* server-address vector */
  int addrvcnt)                    /* server-address vector size */
{
    register struct cm_server *serverp;
    register int i;
    unsigned32 st;


    osi_assert(cellp && princp);
    osi_assert(sType == SRT_FX || sType == SRT_REP || sType == SRT_FL);
    osi_assert(addrvp && addrvcnt > 0);

    lock_ObtainShared(&cm_serverlock);

    if (serverp = FindServerByPPL(cellp, princp, sType)) {
	/* server entry already exists */
	lock_ReleaseShared(&cm_serverlock);

	if (objIdp) {
	    /* update server's objId */
	    lock_ObtainWrite(&serverp->lock);
	    serverp->objId = *objIdp;
	    lock_ReleaseWrite(&serverp->lock);
	}
	return serverp;
    }

    serverp = (struct cm_server *) osi_Alloc(sizeof(struct cm_server));
    bzero((char *)serverp, sizeof(struct cm_server));

    serverp->cellp = cellp;
    serverp->sType = sType;
    strncpy(&serverp->principal[0], princp, MAXKPRINCIPALLEN);

    /*
     * lastCall and lastAuthenticatedCall will just start out zero. This server
     * has granted us no tokens, so there's nothing lost in allowing it to be
     * declared ``down,'' especially if it really is down now.
     */
#if 0
    serverp->lastCall = osi_Time();
    serverp->lastAuthenticatedCall = serverp->lastCall;
#endif /* 0 */

    lock_Init(&serverp->lock);

    if (objIdp) {
	serverp->objId = *objIdp;
    }

    /* Set initial lifetime values */
    serverp->deadServerTime = 180;		/* Start with 3 mins */
    serverp->hostLifeTime = 90;		/* Start with 90 seconds */
    serverp->origHostLifeTime = 120;		/* Start with 2 mins */

    /* Track what the server tells us are auth values that are too high/low */
    serverp->minAuthn = CM_MIN_SECURITY_LEVEL;
    serverp->maxAuthn = CM_MAX_SECURITY_LEVEL;
    serverp->maxSuppAuthn = CM_MAX_SECURITY_LEVEL;

    /* Create the UUID for this server */
    dfsuuid_Create(&serverp->serverUUID, 0);	/* create even UUID */

    /* Set RTT to max + (1..64); indicates undefined while providing
     * randomness for cm_ConnByMHosts() server selection
     */
    serverp->avgRtt = SR_AVGRTT_MAX + ((cm_random() % 64) + 1);
 
    /* init maxFileSize -- part of 32/64-bit interoperability changes */
    serverp->maxFileSize = osi_maxFileSizeDefault;
    serverp->supports64bit = 0;

    /* Put server struct into both hash tables and alloc site info */
    lock_UpgradeSToW(&cm_serverlock);

#ifdef SGIMIPS
    i = (int)SR_HASH(&serverp->serverUUID, &st);
#else  /* SGIMIPS */
    i = SR_HASH(&serverp->serverUUID, &st);
#endif /* SGIMIPS */

    serverp->next = cm_servers[i];
    cm_servers[i] = serverp;

    i = SR_HASHPPL(princp);

    serverp->nextPPL = cm_serversPPL[i];
    cm_serversPPL[i] = serverp;

    serverp->sitep = cm_SiteAlloc((u_short)sType, addrvp, addrvcnt);

    lock_ReleaseWrite(&cm_serverlock);
    return serverp;
}



/*
 * Determine if server-sets are the same; assumes sorted by same key
 */
#ifdef SGIMIPS
int cm_SameServers(
  register struct cm_server *serverpp1[], 
  register struct cm_server *serverpp2[],
  int maxservers)
#else  /* SGIMIPS */
int cm_SameServers(serverpp1, serverpp2, maxservers)
  register struct cm_server *serverpp1[]; 
  register struct cm_server *serverpp2[];
  int maxservers;
#endif /* SGIMIPS */
{
    int i;

    if (serverpp1 == NULL || serverpp2 == NULL) {
        return 0;
    }
    for (i = 0; i < maxservers; i++) {
        if (serverpp1[i] != serverpp2[i]) {
	    return 0;
	} else if (!serverpp1[i]) {
	    return 1;
	}
    }
    return 1;
}



/* 
 * Sorts server-set by principal
 */
#ifdef SGIMIPS
void cm_SortServers(
  register struct cm_server **serverpp,
  int maxservers)
#else  /* SGIMIPS */
void cm_SortServers(serverpp, maxservers)
  register struct cm_server **serverpp;
  int maxservers;
#endif /* SGIMIPS */
{
    struct cm_server *tsp;
    int i, j, count;

    for (i = 0; i < maxservers; i++) {
	if (!serverpp[i]) {
	    break;
	}
    }
    count = i;

    for (i = 0; i < count - 1; i++) {
	for (j = i + 1; j < count; j++) {
	    if (strcmp(serverpp[i]->principal, serverpp[j]->principal) > 0) {
		/* servers out of order */
		tsp         = serverpp[i];
		serverpp[i] = serverpp[j];
		serverpp[j] = tsp;
	    }
	}
    }
}



#if	defined(KERNEL)
/* 
 * This function performs a keep-alive for a single server for which the
 * CM has valid tokens.
 */

static void PingServer(void *arg)
{
    struct cm_server *serverp = (struct cm_server *)arg;
    register struct cm_conn *connp;
    struct cm_rrequest rreq;
    register long code;
#ifdef SGIMIPS
    struct timeval start, end;
#else /* SGIMIPS */
    struct afsTimeval start, end;
#endif /* SGIMIPS */
    struct timeval tv;
#ifdef SGIMIPS
    unsigned32 synchDistance, synchDispersion;
#else  /* SGIMIPS */
    u_long synchDistance, synchDispersion;
#endif /* SGIMIPS */
    u_long thisRTT;

    icl_Trace1(cm_iclSetp, CM_TRACE_PINGSERVER, ICL_TYPE_POINTER, serverp);
    if (cm_setTime)
	cm_InitUnauthReq(&rreq);	 /* fall back to .../self as needed */
    else
	cm_InitNoauthReq(&rreq);	/* no authentication at all */
    cm_SetConnFlags(&rreq, CM_RREQFLAG_NOVOLWAIT);
    do {
        if (connp = cm_ConnByHost(serverp, MAIN_SERVICE(SRT_FX),
				  &serverp->cellp->cellId, &rreq, 0)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_GETTIMESTART);
	    osi_GetTime((struct timeval *)&start); /* time the gettimeofday call */
	    code = BOMB_EXEC("COMM_FAILURE",
			     (osi_RestorePreemption(0),
			      code = AFS_GetTime(connp->connp,
#ifdef SGIMIPS
					 (unsigned32 *)&tv.tv_sec,
					 (unsigned32 *) &tv.tv_usec,
#else  /* SGIMIPS */
					 (u_long *)&tv.tv_sec,
					 (u_long *) &tv.tv_usec,
#endif /* SGIMIPS */
					 &synchDistance, &synchDispersion),
			      osi_PreemptionOff(),
			      code));
	    osi_GetTime((struct timeval *)&end);
	    code = osi_errDecode(code);
	    icl_Trace1(cm_iclSetp, CM_TRACE_GETTIMEEND, ICL_TYPE_LONG, code);

	} else 
	    code = -1;
#ifdef SGIMIPS
    } while (cm_Analyze(connp, code, (afsFid *)0, &rreq, NULL, -1, 
		(u_long)start.tv_sec));
#else /* SGIMIPS */
    } while (cm_Analyze(connp, code, (afsFid *)0, &rreq, NULL, -1, start.sec));
#endif /* SGIMIPS */
    /*
     * Set the local time only if the cm_setTime flag is on and that rpc call
     * worked quickly (same second response).
     */
#ifdef SGIMIPS
    if (cm_setTime && (code == 0) && (start.tv_sec == end.tv_sec))
        SyncLocalTime(connp, &tv, end.tv_sec, synchDistance, synchDispersion);
#else /* SGIMIPS */
    if (cm_setTime && (code == 0) && (start.sec == end.sec))
        SyncLocalTime(connp, &tv, end.sec, synchDistance, synchDispersion);
#endif /* SGIMIPS */

    /* Now, track the average RTT for this server. */
    if (code == 0) {
#ifdef SGIMIPS
	thisRTT = end.tv_usec - start.tv_usec + 
			1000000*(end.tv_sec - start.tv_sec);
#else /* SGIMIPS */
	thisRTT = end.usec - start.usec + 1000000*(end.sec - start.sec);
#endif /* SGIMIPS */
	if (thisRTT < (30 * 1000000)) {
	    /* if less than 30 seconds (in microseconds) */
	    lock_ObtainWrite(&serverp->lock);
	    if (serverp->avgRtt > SR_AVGRTT_MAX) {
		/* RTT undefined; assign the initial RTT */
		serverp->avgRtt = thisRTT;
	    } else {
		/* smooth the RTT estimate */
		serverp->avgRtt = ((7 * serverp->avgRtt) + thisRTT) >> 3;
	    }
	    /* cap the RTT estimate at the max value */
	    if (serverp->avgRtt > SR_AVGRTT_MAX) {
#ifdef SGIMIPS
		serverp->avgRtt = SR_AVGRTT_MAX + 1;
					/*  ">" is used to check for
					the max value, so set it to
					the max + 1 so the test is true. 
					Otherwise, it will use this value
					to calculate the first Rtt */
#else /* SGIMIPS */
		serverp->avgRtt = SR_AVGRTT_MAX;
#endif /* SGIMIPS */
	    }
	    lock_ReleaseWrite(&serverp->lock);
	}
    }
}



/* 
 * This is to perform the "keep-alive" work with each FX server from which
 * the CM has valid tokens.  Each individual server will be polled every 
 * "hostLifeTime" time interval. The "hostLifeTime" time value is per server's 
 * property. It is provided by the fx server when the CM first contacts the 
 * server. (refer to TKN_InitTokenState())
 */

#ifdef SGIMIPS
unsigned32 cm_PingServers(
    struct cm_cell *cellp,
    u_long  leastTimeInterval)
#else  /* SGIMIPS */
u_long cm_PingServers(cellp, leastTimeInterval)
    struct cm_cell *cellp;
    u_long  leastTimeInterval;
#endif /* SGIMIPS */
{
    register struct cm_server *serverp;
#ifdef SGIMIPS
    unsigned32 nextPollCycle, now, timeToPoll;
#else  /* SGIMIPS */
    u_long nextPollCycle, now, timeToPoll;
#endif /* SGIMIPS */
    register long i;
    void *parsetp;

    nextPollCycle = 0x7fffffff;

    /* create a tpq parallel set */
    parsetp = tpq_CreateParSet(cm_auxThreadPoolHandle);
    osi_assert(parsetp != NULL);

    lock_ObtainRead(&cm_serverlock);

    now = osi_Time();
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    if (serverp->sType == SRT_FX &&
		(!cellp || cellp == serverp->cellp)) {

		/* Go ahead, we don't GC servers. */
		lock_ReleaseRead(&cm_serverlock);
		lock_ObtainRead(&serverp->lock);
		nextPollCycle = MIN(nextPollCycle, serverp->hostLifeTime);
#ifdef SGIMIPS 
		timeToPoll = (unsigned32) 
				(serverp->lastCall + serverp->hostLifeTime);
#else  /* SGIMIPS */
		timeToPoll = serverp->lastCall + serverp->hostLifeTime;
#endif /* SGIMIPS */
		/* 
		 * We don't ping the server, If
		 * 1) we don't have any tokens from this server OR,
		 * 2) the "timeToPoll" is still in the future. 
		 */
		if (!cm_HaveTokensFrom(serverp) ||
		    (timeToPoll > now && (timeToPoll - now) > leastTimeInterval)) {
		    lock_ReleaseRead(&serverp->lock);
		}
		else {
		    /* If the server is still down, just skip this one. */
		    if (serverp->states & (SR_DOWN | SR_TSR)) {
			lock_ReleaseRead(&serverp->lock);
		    }
		    else {
			lock_ReleaseRead(&serverp->lock);
			if (cm_auxThreadPoolHandle) {
			    /* 
			     * we have a thread pool set up queue up operation to 
			     * run 
			     */
			    (void) tpq_AddParSet(parsetp, PingServer, 
					(void *)serverp, TPQ_HIGHPRI, 30);
			}  
			else  {
			    /* no thread pool running just run it now */
			    PingServer(serverp);
			}
		    }
		}
		lock_ObtainRead(&cm_serverlock);
	    }
	}
    }
    lock_ReleaseRead(&cm_serverlock);

    /* wait for each individual operation to complete */
    icl_Trace1(cm_iclSetp, CM_TRACE_PINGSERVER1, ICL_TYPE_LONG, osi_Time());
    tpq_WaitParSet(parsetp);
    icl_Trace1(cm_iclSetp, CM_TRACE_PINGSERVER2, ICL_TYPE_LONG, osi_Time());

    /* calculate the next poll interval */
    now = osi_Time();
    lock_ObtainRead(&cm_serverlock);
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    if (serverp->sType == SRT_FX) {
		lock_ReleaseRead(&cm_serverlock);
		lock_ObtainRead(&serverp->lock);
		nextPollCycle = MIN(nextPollCycle, serverp->hostLifeTime);
		if (!(serverp->states & (SR_DOWN | SR_TSR)) &&
		    cm_HaveTokensFrom(serverp))  {
#ifdef SGIMIPS
		    timeToPoll = (unsigned32) 
				 (serverp->lastCall + serverp->hostLifeTime);
#else  /* SGIMIPS */
		    timeToPoll = serverp->lastCall + serverp->hostLifeTime;
#endif /* SGIMIPS */
		    nextPollCycle = MIN(nextPollCycle, timeToPoll - now);
		}
		lock_ReleaseRead(&serverp->lock);
		lock_ObtainRead(&cm_serverlock);
	    }
	}
    }
    lock_ReleaseRead(&cm_serverlock);

    return nextPollCycle; 
}



/*
 * check specified down server:
 */
static void CheckDownServer(void *arg)
{
    struct cm_server *serverp = (struct cm_server *)arg;
    struct cm_rrequest rreq;
    register struct cm_conn *connp;
    u_long code, oldstates;
    char msgbuf[90], *serverName;
    error_status_t st;
    struct sockaddr_in connAddr;
    int connAddrWasUp;

#ifdef SGIMIPS
    extern error_status_t REP_Probe(handle_t);
#endif /* SGIMIPS */

    icl_Trace1(cm_iclSetp, CM_TRACE_CHECKDOWNINT,
	       ICL_TYPE_POINTER, serverp);

    /* Specify the use of an un-authenticated binding */
    cm_InitNoauthReq(&rreq);

    /* Update server downTime whether the RPC goes through or not.
     *
     * Also, if server's addresses are all marked down then mark them all up.
     * In this way, CheckDownServer() will keep rotating through a server's
     * addresses (one per invocation) until a successful RPC is completed.
     *
     * Another periodic background thread is responsible for probing
     * down-addresses for any server marked up.
     */
    lock_ObtainWrite(&serverp->lock);
    serverp->downTime = osi_Time();
    lock_ReleaseWrite(&serverp->lock);

    cm_SiteAddrMarkAllUp(serverp->sitep, 1 /* only if all marked down */);

    /* Probe "current" server address */
    lock_ObtainRead(&cm_siteLock);
    connAddr = serverp->sitep->addrCurp->addr;
    lock_ReleaseRead(&cm_siteLock);

    /* Try to force a connection to the server */
    connp = cm_ConnByAddr(&connAddr,
			  serverp,  MAIN_SERVICE(serverp->sType),
			  &serverp->cellp->cellId,
			  &rreq, NULL, 1 /* force */, NULL);

    if (!connp && serverp->sType == SRT_FX) {
	/* No conn (failed AFS_SetContext); do addr fail-over even though
	 * not sure if due to comm failure (since might be).
	 */
	(void)cm_SiteAddrDown(serverp->sitep, &connAddr, &connAddrWasUp);
    }

    if (connp) {
	/* Got conn; attempt RPC */
	osi_RestorePreemption(0);
	/* Set conn timeout to minimum + 2 */
	rpc_mgmt_set_com_timeout(connp->connp,
				 rpc_c_binding_min_timeout + 2,
				 &st);

	switch (serverp->sType) {
	  case SRT_FL:
	    code = VL_Probe(connp->connp);
	    code = osi_errDecode(code);
	    serverName = "fileset location server ";
	    break;
	    
	  case SRT_REP:
	    code = REP_Probe(connp->connp);
	    code = osi_errDecode(code);
	    serverName = "replication server ";
	    break;

	  case SRT_FX:
	    /* Performed a successful AFS_SetContext(); server back up */
	    code = 0;
	    serverName = "fx server ";

	    /* Sync local time with FX server's, if specified */
	    if (cm_setTime) {
		code = SyncFxServer(connp);
	    }
	    break;

	  default:
	    /* Invalid server type */
	    panic("CM CheckDownServer(): invalid server type");
	    break;
	}

	/* Set conn timeout to default + 2 (about 2 minutes) */
	rpc_mgmt_set_com_timeout(connp->connp,
				 rpc_c_binding_default_timeout + 2,
				 &st);
	osi_PreemptionOff();

	/* React to a change in binding addr; syncs conn and binding addrs */
	cm_ReactToBindAddrChange(connp, 0 /* doOverride */);

	/* Deal with RPC result */
	if (code == 0) {
	    /* RPC successful; mark server back up */
	    lock_ObtainWrite(&serverp->lock);
	    oldstates = serverp->states;
	    serverp->states &= ~SR_DOWN;
	    serverp->downTime = 0;
	    serverp->failoverCount = 0;
	    lock_ReleaseWrite(&serverp->lock);

	    /* Print a message if server state changed */
	    if (oldstates & SR_DOWN) {
		strcpy(msgbuf, "dfs: ");
		strcat(msgbuf, serverName);
		cm_PrintServerText(msgbuf, serverp, " back up!\n");
	    }
	} else if (code == rpc_s_comm_failure || code == rpc_s_call_timeout) {
	    /* RPC comm failure; try to fail-over to another address */
	    (void)cm_SiteAddrDown(serverp->sitep, &connAddr, &connAddrWasUp);
	} else {
	    /* RPC non-comm failure; take action if auth problem */
	    (void) cm_ReactToAuthCodes(connp, (struct cm_volume *)NULL,
#ifdef SGIMIPS
				       &rreq, (long)code);
#else  /* SGIMIPS */
				       &rreq, code);
#endif /* SGIMIPS */
	}

	/* Rele conn */
	cm_PutConn(connp);
    }
}



/* 
 * Check down servers to make sure the servers has come back. 
 * This function is called when the time "nextPollCycle" is due. Note that each
 * server machine may request a different time interval (ie, deadServerTime) 
 * for polling. The time "nextPollCycle" is calculated based on the lowest 
 * "deadServerTime" values among all servers, but "nextPollCycle" can not be 
 * less than the given "leastTimeInterval".
 *
 * All down servers will be polled, if no particular cell is given.
 * "nextPollCycle" is returned to the caller.
 */
#ifdef SGIMIPS
u_long cm_CheckDownServers(
    struct cm_cell *cellp,
    u_long leastTimeInterval)
#else  /* SGIMIPS */
u_long cm_CheckDownServers(cellp, leastTimeInterval)
    struct cm_cell *cellp;
    u_long leastTimeInterval;
#endif /* SGIMIPS */
{
    register struct cm_server *serverp;
    register long i;
    u_long  nextPollCycle, now, timeToPoll;
    void *parsetp;

    /* create a tpq parallel set */
    parsetp = tpq_CreateParSet(cm_auxThreadPoolHandle);
    osi_assert(parsetp != NULL);

    nextPollCycle = 0x7fffffff;

    lock_ObtainRead(&cm_serverlock);
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    /* Go ahead, we don't GC servers. */
	    lock_ReleaseRead(&cm_serverlock);
	    lock_ObtainRead(&serverp->lock);
	    if ((serverp->states & SR_DOWN) &&
		(!cellp || cellp == serverp->cellp)) {
		now = osi_Time();
		timeToPoll = serverp->downTime + serverp->deadServerTime;
		if ((timeToPoll <= now) || 
		    (timeToPoll - now) <= leastTimeInterval) {
		    if (cm_auxThreadPoolHandle) {
			/* we have a thread pool set up */
			/* queue up operation to run */
			(void) tpq_AddParSet(parsetp, CheckDownServer, 
					     (void *)serverp, TPQ_MEDPRI, 60);
		    }
		    else {
			/* no thread pool running */
			/* just run it now */
			lock_ReleaseRead(&serverp->lock);
			CheckDownServer(serverp);
			lock_ObtainRead(&serverp->lock);
		    }
		}
	    }
	    lock_ReleaseRead(&serverp->lock);
	    lock_ObtainRead(&cm_serverlock);
	}  /* for loop */
    } /* for i loop */
    lock_ReleaseRead(&cm_serverlock);

    /* let the queued CheckDownServer()s run */
    tpq_WaitParSet(parsetp);

    /* calculate nextPollCycle */
    lock_ObtainRead(&cm_serverlock);
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    lock_ReleaseRead(&cm_serverlock);
	    lock_ObtainRead(&serverp->lock);
	    nextPollCycle = MIN(nextPollCycle, serverp->deadServerTime);
	    if ((serverp->states & SR_DOWN) &&
		(!cellp || cellp == serverp->cellp)) {
		now = osi_Time();
		timeToPoll = serverp->downTime + serverp->deadServerTime;
		nextPollCycle = MIN(nextPollCycle, timeToPoll - now);
	    } 
	    lock_ReleaseRead(&serverp->lock);
	    lock_ObtainRead(&cm_serverlock);
	}  /* for loop */
    } /* for i loop */

    lock_ReleaseRead(&cm_serverlock);
    return nextPollCycle;
}



/*
 * ReviveAddrsForServer() -- pings up server on addresses marked down
 *     to determine if the connection has been restored.
 * 
 * LOCKS USED: locks serverp->lock and cm_siteLock; released on exit.
 *
 * RETURN CODES: none
 */

static void
ReviveAddrsForServer(void *arg)  /* server */
{
    register struct cm_server *serverp;
    register struct cm_siteAddr *listp;
    struct cm_rrequest rreq;
    struct cm_conn *connp;
#ifdef SGIMIPS
    u_long code;
    unsigned32 dummyArg;
#else  /* SGIMIPS */
    u_long code, dummyArg;
#endif /* SGIMIPS */
    char premsgbuf[50], postmsgbuf[60], *serverName;
    error_status_t st;
    struct sockaddr_in connAddr;
    int connAddrWasDown;

    serverp = (struct cm_server *)arg;

    icl_Trace1(cm_iclSetp, CM_TRACE_REVIVEADDRS_SERVER_ENTER,
	       ICL_TYPE_POINTER, serverp);

    /* Determine if another thread is reviving addrs for this server */

    lock_ObtainRead(&serverp->lock);
    lock_ObtainWrite(&cm_siteLock);

    if (serverp->sitep->state & SITE_PINGING) {
	lock_ReleaseWrite(&cm_siteLock);
	lock_ReleaseRead(&serverp->lock);

	icl_Trace1(cm_iclSetp, CM_TRACE_REVIVEADDRS_SERVER_CONFLICT,
		   ICL_TYPE_POINTER, serverp);
	return;
    }

    serverp->sitep->state |= SITE_PINGING;

    /* Scan server's address-list looking for down-addresses to ping */

    while (!(serverp->states & SR_DOWN) &&
	   (serverp->sitep->state & SITE_DOWNADDR)) {
	/* get next address to ping */
	for (listp = serverp->sitep->addrListp; listp; listp = listp->next_sa){
	    if ((listp->state & SITEADDR_DOWN) &&
		!(listp->state & SITEADDR_PINGED)) {
		/* found down address that have not yet pinged */
		break;
	    }
	}

	if (!listp) {
	    /* no more addresses to ping */
	    break;
	}

	connAddr = listp->addr;
	listp->state |= SITEADDR_PINGED;

	lock_ReleaseWrite(&cm_siteLock);
	lock_ReleaseRead(&serverp->lock);

	/* specify the use of an un-authenticated binding */
	cm_InitNoauthReq(&rreq);

	/* get connection to server */
	connp = cm_ConnByAddr(&connAddr,
			      serverp,
			      MAIN_SERVICE(serverp->sType),
			      &serverp->cellp->cellId,
			      &rreq,
			      NULL, 0 /* NO force */, NULL);

	/* ping server on address */
	if (!connp) {
	    /* Did not get conn */
	    icl_Trace2(cm_iclSetp, CM_TRACE_REVIVEADDRS_SERVER_NOCONN,
		       ICL_TYPE_POINTER, serverp,
		       ICL_TYPE_LONG, SR_ADDR_VAL(&connAddr));
	} else {
	    /* Got conn; attempt RPC */
	    osi_RestorePreemption(0);

	    /* Set conn timeout to minimum + 2 */
	    rpc_mgmt_set_com_timeout(connp->connp,
				     rpc_c_binding_min_timeout + 2,
				     &st);

	    switch (serverp->sType) {
	      case SRT_FL:
		code = VL_Probe(connp->connp);
		code = osi_errDecode(code);
		serverName = "fileset location server ";
		break;
	    
	      case SRT_REP:
		code = REP_Probe(connp->connp);
		code = osi_errDecode(code);
		serverName = "replication server ";
		break;
	    
	      case SRT_FX:
		code = AFS_GetTime(connp->connp,
				   &dummyArg, &dummyArg, &dummyArg, &dummyArg);
		code = osi_errDecode(code);
		serverName = "fx server ";
		break;

	      default:
		/* Invalid server type */
		panic("CM ReviveAddrsForServer(): invalid server type");
		break;
	    }

	    /* Set conn timeout to default + 2 (about 2 minutes) */
	    rpc_mgmt_set_com_timeout(connp->connp,
				     rpc_c_binding_default_timeout + 2,
				     &st);
	    osi_PreemptionOff();

	    icl_Trace3(cm_iclSetp, CM_TRACE_REVIVEADDRS_SERVER_CONN,
		       ICL_TYPE_POINTER, serverp,
		       ICL_TYPE_LONG, SR_ADDR_VAL(&connAddr),
		       ICL_TYPE_LONG, code);

	    /* React to a change in binding addr; syncs conn and bind addrs */
	    cm_ReactToBindAddrChange(connp, 0 /* doOverride */);

	    if (code == 0) {
		/* RPC was successful; mark address back up */
		cm_SiteAddrUp(serverp->sitep, &connAddr, &connAddrWasDown);

		/* print a message if address state changed */
		if (connAddrWasDown) {
		    strcpy(premsgbuf, "dfs: ");
		    strcat(premsgbuf, serverName);
		    strcpy(postmsgbuf, " again reachable at ");
		    osi_inet_ntoa_buf(connAddr.sin_addr,
				      &postmsgbuf[strlen(postmsgbuf)]);
		    strcat(postmsgbuf, "\n");

		    cm_PrintServerText(premsgbuf, serverp, postmsgbuf);
		}
	    } else {
		/* RPC failure; take action if auth problem */
		(void) cm_ReactToAuthCodes(connp, (struct cm_volume *)NULL,
#ifdef SGIMIPS
					   &rreq, (long)code);
#else  /* SGIMIPS */
					   &rreq, code);
#endif /* SGIMIPS */
	    }

	    /* Rele conn */
	    cm_PutConn(connp);
	} /* connp */

	lock_ObtainRead(&serverp->lock);
	lock_ObtainWrite(&cm_siteLock);
    } /* while */

    /* All down addresses have been pinged; reset site state */
    for (listp = serverp->sitep->addrListp; listp; listp = listp->next_sa) {
	listp->state &= ~(SITEADDR_PINGED);
    }

    serverp->sitep->state &= ~(SITE_PINGING);

    lock_ReleaseWrite(&cm_siteLock);
    lock_ReleaseRead(&serverp->lock);

    icl_Trace1(cm_iclSetp, CM_TRACE_REVIVEADDRS_SERVER_EXIT,
	       ICL_TYPE_POINTER, serverp);
}



/*
 * cm_ReviveAddrsForServers() -- pings all up servers on addresses marked
 *     down to determine if the connection has been restored.
 * 
 * LOCKS USED: locks cm_serverlock; released on exit
 *
 * RETURN CODES: none
 */

void
cm_ReviveAddrsForServers(void)
{
    register struct cm_server *serverp;
    register int i;
    void *parsetp;

    icl_Trace0(cm_iclSetp, CM_TRACE_REVIVEADDRS_ENTER);

    /* Create a tpq parallel set for pinging servers' down addresses */
    parsetp = tpq_CreateParSet(cm_auxThreadPoolHandle);

    /* Scan server list looking for up servers with down addresses */
    lock_ObtainRead(&cm_serverlock);

    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    /* no harm in making check without locks; at worst will add
	     * an extraneous thread to the set that will immediately exit.
	     */
	    if (!(serverp->states & SR_DOWN) &&
		(serverp->sitep->state & SITE_DOWNADDR)) {
		/* found up server with one or more down addresses */

		if (cm_auxThreadPoolHandle) {
		    /* a thread pool has been set-up; queue revive operation */
		    (void)tpq_AddParSet(parsetp,
					ReviveAddrsForServer,
					(void *)serverp,
					TPQ_MEDPRI,
					60);
		} else {
		    /* no thread pool running; perform revive operation */
		    lock_ReleaseRead(&cm_serverlock);

		    ReviveAddrsForServer((void *)serverp);

		    lock_ObtainRead(&cm_serverlock);
		}
	    }
	}
    }
    lock_ReleaseRead(&cm_serverlock);

    /* Wait for any queued ReviveAddrsForSever() to complete */
    tpq_WaitParSet(parsetp);

    icl_Trace0(cm_iclSetp, CM_TRACE_REVIVEADDRS_EXIT);
}



/*
 * cm_FxRepAddrFetch() -- schedule a thread to update the specified
 *     FX/REP-server address lists using complete site information
 *     fetched from the FLDB.
 *
 *     Note that this function's raison d'etre is to compensate for
 *     the case where incomplete server information is returned in a
 *     vldbentry structure.
 * 
 * LOCKS USED: cm_serverlock and FX/REP-server locks; released on exit.
 *
 * CAUTIONS: There is a potential race between {cm_}FxRepAddrFetch()
 *     and the function cm_CheckVolumeNames() involving the SR_GOTAUXADDRS
 *     flag.  Essentially, cm_CheckVolumeNames() can clear the flag before
 *     the indicated update has actually taken place.  Note that this is NOT
 *     a correctness problem; at worst an extraneous update can be scheduled.
 *
 * RETURN CODES: none
 */

struct auxAddrUpdateArgs {
    struct cm_cell *cellp;            /* servers' cell */
    struct cm_server *fxserverp;      /* FX-server reference */
    struct cm_server *repserverp;     /* REP-server reference */
    afsNetAddr addr;                  /* known addr, net-style */
};

void
cm_FxRepAddrFetch(struct cm_cell *cellp,   /* servers' cell */
		  char *princp,            /* servers' principal name */
		  afsNetAddr *addrp)       /* known addr, net-style */
{
    struct cm_server *fxserverp, *repserverp;
    struct auxAddrUpdateArgs *argsp;

    icl_Trace0(cm_iclSetp, CM_TRACE_FXREPFETCH_SCHED_ENTER);

    /* Determine if FX/REP addrs already updated via this function */
    lock_ObtainRead(&cm_serverlock);

    if (fxserverp = FindServerByPPL(cellp, princp, SRT_FX)) {
	lock_ReleaseRead(&cm_serverlock);
	lock_ObtainWrite(&fxserverp->lock);

	if (!(fxserverp->states & SR_GOTAUXADDRS)) {
	    /* FX addrs not updated; indicate that will do so */
	    fxserverp->states |= SR_GOTAUXADDRS;
	    lock_ReleaseWrite(&fxserverp->lock);
	} else {
	    /* FX addrs already updated */
	    lock_ReleaseWrite(&fxserverp->lock);
	    fxserverp = NULL;
	}

	lock_ObtainRead(&cm_serverlock);
    }

    if (repserverp = FindServerByPPL(cellp, princp, SRT_REP)) {
	lock_ReleaseRead(&cm_serverlock);
	lock_ObtainWrite(&repserverp->lock);

	if (!(repserverp->states & SR_GOTAUXADDRS)) {
	    /* REP addrs not updated; indicate that will do so */
	    repserverp->states |= SR_GOTAUXADDRS;
	    lock_ReleaseWrite(&repserverp->lock);
	} else {
	    /* REP addrs already updated */
	    lock_ReleaseWrite(&repserverp->lock);
	    repserverp = NULL;
	}
    } else {
	lock_ReleaseRead(&cm_serverlock);
    }

    /* Allocate args and schedule an update thread, if necessary */
    if (fxserverp || repserverp) {
	argsp = (struct auxAddrUpdateArgs *)osi_Alloc(sizeof(*argsp));

	argsp->cellp      = cellp;
	argsp->fxserverp  = fxserverp;
	argsp->repserverp = repserverp;
	argsp->addr       = *addrp;

	if (!(tpq_QueueRequest(cm_threadPoolHandle,
			       FxRepAddrFetch,
			       (void *)argsp,
			       TPQ_MEDPRI, 5 * 60, 0, 0))) {
	    /* failed to sched an update thread; dealloc args, reset flags */
	    icl_Trace0(cm_iclSetp, CM_TRACE_FXREPFETCH_SCHED_FAILED);

	    osi_Free((opaque)argsp, sizeof(*argsp));

	    if (fxserverp) {
		lock_ObtainWrite(&fxserverp->lock);
		fxserverp->states &= ~(SR_GOTAUXADDRS);
		lock_ReleaseWrite(&fxserverp->lock);
	    }

	    if (repserverp) {
		lock_ObtainWrite(&repserverp->lock);
		repserverp->states &= ~(SR_GOTAUXADDRS);
		lock_ReleaseWrite(&repserverp->lock);
	    }
	}
    }

    icl_Trace0(cm_iclSetp, CM_TRACE_FXREPFETCH_SCHED_EXIT);
}



/*
 * FxRepAddrFetch() -- update the specified FX/REP-server address lists
 *     using complete site information fetched from the FLDB.
 * 
 * LOCKS USED: locks individual FX/REP-server locks; released on exit.
 *
 * RETURN CODES: none
 */

static void
FxRepAddrFetch(void *opArgp, void *opHandlep)
{
    struct cm_cell *cellp;
    struct cm_server *fxserverp, *repserverp;
    struct cm_conn *connp;
    struct siteDesc *fullInfop;
    struct sockaddr_in *addrlist;
    afsNetAddr addr;
    struct cm_rrequest rreq;
    u_long startTime;
    long code, n;

    /* Retrieve arguments */

    cellp      = ((struct auxAddrUpdateArgs *)opArgp)->cellp;
    fxserverp  = ((struct auxAddrUpdateArgs *)opArgp)->fxserverp;
    repserverp = ((struct auxAddrUpdateArgs *)opArgp)->repserverp;
    addr       = ((struct auxAddrUpdateArgs *)opArgp)->addr;

    osi_Free((opaque)opArgp, sizeof(struct auxAddrUpdateArgs));

    icl_Trace2(cm_iclSetp, CM_TRACE_FXREPFETCH_ENTER,
	       ICL_TYPE_POINTER, fxserverp,
	       ICL_TYPE_POINTER, repserverp);

    /* Get full FX/REP addr list and perform required updates */

    if (fxserverp || repserverp) {
	/* specify use of an un-authenticated binding */
	cm_InitUnauthReq(&rreq);

	/* attempt to get all addrs in FLDB */
	fullInfop = (struct siteDesc *)osi_Alloc(sizeof(*fullInfop));

	do {
	    if (connp = cm_ConnByMHosts(cellp->cellHosts,
					MAIN_SERVICE(SRT_FL),
					&cellp->cellId,
					&rreq,
					NULL,
					cellp,
					NULL)) {
		/* got connection; perform RPC */
		icl_Trace0(cm_iclSetp, CM_TRACE_FXREPFETCH_STARTGETINFO);

		startTime = osi_Time();
		osi_RestorePreemption(0);

		code = VL_GetSiteInfo(connp->connp,
				      &addr,
				      fullInfop);

		osi_PreemptionOff();

		icl_Trace1(cm_iclSetp, CM_TRACE_FXREPFETCH_ENDGETINFO,
			   ICL_TYPE_LONG, code);
	    } else {
		/* unable to obtain connection */
		icl_Trace0(cm_iclSetp, CM_TRACE_FXREPFETCH_NOCONN);
		code = -1;
	    }
	} while (cm_Analyze(connp, code, NULL, &rreq, NULL, -1, startTime));

	if (code == 0) {
	    /* RPC was successful */
	    /* determine number of site addrs; convert to kernel-usable form */
	    addrlist = (struct sockaddr_in *)&fullInfop->Addr[0];

	    for (n = 0; n < ADDRSINSITE; n++) {
		if (fullInfop->Addr[n].type != (u_short) -1) {
		    /* valid addr; convert (in-place) to kernel-usable form */
		    osi_KernelSockaddr(&addrlist[n]);
		    addrlist[n].sin_port = 0; /* port is of no use; use rpcd */
		} else {
		    /* no more valid addresses */
		    break;
		}
	    }

	    /* update server address lists, as required */
	    if (n > 0) {
		if (fxserverp) {
#ifdef SGIMIPS
		    cm_SiteAddrReplace(fxserverp->sitep, addrlist, (int)n);
#else  /* SGIMIPS */
		    cm_SiteAddrReplace(fxserverp->sitep, addrlist, n);
#endif /* SGIMIPS */
		}

		if (repserverp) {
#ifdef SGIMIPS
		    cm_SiteAddrReplace(repserverp->sitep, addrlist, (int)n);
#else  /* SGIMIPS */
		    cm_SiteAddrReplace(repserverp->sitep, addrlist, n);
#endif /* SGIMIPS */
		}
	    }

	} else {
	    /* RPC was NOT successful; reset server flags */
	    if (fxserverp) {
		lock_ObtainWrite(&fxserverp->lock);
		fxserverp->states &= ~(SR_GOTAUXADDRS);
		lock_ReleaseWrite(&fxserverp->lock);
	    }

	    if (repserverp) {
		lock_ObtainWrite(&repserverp->lock);
		repserverp->states &= ~(SR_GOTAUXADDRS);
		lock_ReleaseWrite(&repserverp->lock);
	    }
	}

	osi_Free((opaque)fullInfop, sizeof(*fullInfop));
    }

    icl_Trace2(cm_iclSetp, CM_TRACE_FXREPFETCH_EXIT,
	       ICL_TYPE_POINTER, fxserverp,
	       ICL_TYPE_POINTER, repserverp);
}
#endif	/* KERNEL */



/*
 * cm_ResetAuthnForServers() -- for all file-servers, reset cached
 *     authentication bounds to their initial values.
 * 
 * LOCKS USED: locks cm_serverlock and FX-server locks; released on exit
 *
 * RETURN CODES: none
 */

void
cm_ResetAuthnForServers(void)
{
    struct cm_server *serverp;
    int i;

    icl_Trace0(cm_iclSetp, CM_TRACE_RESETAUTHN_ENTER);

    /* Scan server list, resetting cached file-server authn bounds */
    lock_ObtainRead(&cm_serverlock);

    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    if (serverp->sType == SRT_FX) {
		/* just drop cm_serverlock; don't GC server structs */
		lock_ReleaseRead(&cm_serverlock);
		lock_ObtainWrite(&serverp->lock);

		serverp->minAuthn = CM_MIN_SECURITY_LEVEL;
		serverp->maxAuthn = CM_MAX_SECURITY_LEVEL;
		serverp->maxSuppAuthn = CM_MAX_SECURITY_LEVEL;

		lock_ReleaseWrite(&serverp->lock);
		lock_ObtainRead(&cm_serverlock);
	    }
	}
    }

    lock_ReleaseRead(&cm_serverlock);
}



/* 
 * Report that a file/volume server is down. This must be called with no 
 * server lock held.
 * If the server has been active recently enough, just sleep and return 0
 * rather than 1.
 */
#ifdef SGIMIPS
int cm_ServerDown(
    register struct cm_server *serverp,
    register struct cm_conn *connp,
    u_long errcode)
#else  /* SGIMIPS */
int cm_ServerDown(serverp, connp, errcode)
    register struct cm_server *serverp;
    register struct cm_conn *connp;
    u_long errcode;
#endif /* SGIMIPS */
{
#ifdef SGIMIPS
    char premsgbuf[128], postmsgbuf[60], *serverName;
#else /* SGIMIPS */
    char premsgbuf[90], postmsgbuf[60], *serverName;
#endif /* SGIMIPS */
    int reallyDown = 1, callIsAuthenticated, authOnlyProblem = 0;
    int addrFailover, connAddrWasUp;
    struct sockaddr_in connAddr;

    /* Determine if conn authenticated; retrieve conn address */
    if (connp) {
	lock_ObtainRead(&connp->lock);

	callIsAuthenticated = (connp->currAuthn != rpc_c_protect_level_none);
	connAddr = connp->addr;

	lock_ReleaseRead(&connp->lock);
    } else {
	callIsAuthenticated = 0;
    }

    /* Check if server already marked down; if so, nothing to do */
    lock_ObtainWrite(&serverp->lock);

    if (serverp->states & SR_DOWN) {
	lock_ReleaseWrite(&serverp->lock);
	return 1;
    }

    /* If communication failure, try to fail-over to another address */
    addrFailover = 0;

    if ((connp) &&
	(errcode == rpc_s_comm_failure || errcode == rpc_s_call_timeout)) {
	addrFailover = !cm_SiteAddrDown(serverp->sitep,
					&connAddr, &connAddrWasUp);
	if (connAddrWasUp) {
	    /* log server address failure */
	    serverp->failoverCount++;
	}
    }

    /* Increase server's RTT by a factor of 5/4, plus one microsecond */
    if (serverp->avgRtt < SR_AVGRTT_MAX) {
	/* RTT is defined; adjust it upward */
	serverp->avgRtt += ((serverp->avgRtt >> 2) + 1);

	/* cap the RTT estimate at the max value */
	if (serverp->avgRtt > SR_AVGRTT_MAX) {
	    serverp->avgRtt = SR_AVGRTT_MAX;
	}
    }

    /* Determine if server is "really down" */
    if (errcode == TKN_ERR_NEW_NEEDS_RESET || errcode == UNOQUORUM) {
	reallyDown = 1;
    } else if (errcode == rpc_s_unsupported_protect_level) {
	reallyDown = 0;
    } else if (addrFailover && (serverp->failoverCount < cm_addrFailoverMax)) {
	reallyDown = 0;
    } else if ((serverp->lastCall + serverp->origHostLifeTime) > osi_Time()) {
	/* The server can not think that we are down, since it has had
	 * good communication with us within the last host lifetime interval.
	 * Ignore whatever RPC-ish failure this might be and retry.
	 */
	reallyDown = 0;

	if (callIsAuthenticated) {
	    /* This is an authenticated call.  Go down anyway if we can't be
	     * sure that authenticated calls have been working recently.
	     */
	    authOnlyProblem = 1;

	    if ((serverp->lastAuthenticatedCall +
		 serverp->origHostLifeTime) < osi_Time()) {
		/* No recent successful authenticated call.  Go down. */
		reallyDown = 1;
	    }
	}
    } else {
	reallyDown = 1;
    }

    /* Set server/connection state if server is to be marked down */
    if (reallyDown) {
	serverp->states |= SR_DOWN;
	serverp->downTime = osi_Time();

	/* Reset the tracking of authn values that are too high or low */
	serverp->minAuthn = CM_MIN_SECURITY_LEVEL;
	serverp->maxAuthn = CM_MAX_SECURITY_LEVEL;
	serverp->maxSuppAuthn = CM_MAX_SECURITY_LEVEL;
    }
    lock_ReleaseWrite(&serverp->lock);

    if (reallyDown) {
	icl_Trace1(cm_iclSetp, CM_TRACE_SERVERDOWN,
		   ICL_TYPE_POINTER, serverp);
	/* mark as bad all conns to this server */
	cm_MarkBadConns(serverp, CM_BADCONN_ALLCONNS);
    } else {
	icl_Trace4(cm_iclSetp, CM_TRACE_NOTREALLYDOWN,
		   ICL_TYPE_POINTER, serverp,
		   ICL_TYPE_LONG, serverp->lastCall,
		   ICL_TYPE_LONG, serverp->origHostLifeTime,
		   ICL_TYPE_LONG, (long) osi_Time());
    }

    /* Construct server connection-problem message */
    if (errcode == rpc_s_comm_failure) {
#ifdef SGIMIPS
	if (authOnlyProblem) {
	    sprintf(premsgbuf, "dfs: pag 0x%x, uid %d authenticated comm failure with ",
		    connp->authIdentity[0], connp->authIdentity[1]);
	}
	else {
	    strcpy(premsgbuf, "dfs: communications failure with ");
	}
#else /* SGIMIPS */
	strcpy(premsgbuf,
	       (authOnlyProblem ?
		"dfs: authenticated comm failure with " :
		"dfs: communications failure with "));
#endif /* SGIMIPS */
    } else if (errcode == rpc_s_call_timeout) {
	strcpy(premsgbuf,
	       (authOnlyProblem ?
		"dfs: timeout on authenticated RPC to " :
		"dfs: timeout on RPC to "));
    } else if (errcode == UNOQUORUM) {
	/* this is never an authentication issue */
	strcpy(premsgbuf, "dfs: no quorum from ");
    } else if (errcode == rpc_s_who_are_you_failed) {
	/* this is always an authenticated issue */
#ifdef SGIMIPS
	sprintf(premsgbuf, "dfs: pag 0x%x, uid %d RPC 'who are you' failure at ",
		connp->authIdentity[0], connp->authIdentity[1]);
#else /* SGIMIPS */
	strcpy(premsgbuf, "dfs: RPC 'who are you' failure at ");
#endif /* SGIMIPS */
    } else {
#ifdef SGIMIPS
	if (authOnlyProblem) {
	    sprintf(premsgbuf, "dfs: pag 0x%x, uid %d authenticated ",
		    connp->authIdentity[0], connp->authIdentity[1]);
	}
	else {
	    strcpy(premsgbuf, "dfs: ");
	}
#else /* SGIMIPS */
	strcpy(premsgbuf,
		   (authOnlyProblem ?
		    "dfs: authenticated " :
		    "dfs: "));
#endif /* SGIMIPS */
	strcat(premsgbuf,
	       (cm_RPCError(errcode) ?
		"rpc" :
		((errcode >= 0x20000000 && errcode < 0x30000000) ?
		 "dfs" : "dce")));
	strcat(premsgbuf, " errors (code ");
	osi_cv2string(errcode, premsgbuf + strlen(premsgbuf));
	strcat(premsgbuf,") from ");
    }

    switch (serverp->sType) {
	case SRT_FX:
	    serverName = "the fx";
	    break;
	case SRT_FL:
	    serverName = "the fileset location";
	    break;
	case SRT_REP:
	    serverName = "the replication";
	    break;
	default:
	    serverName = "an unknown";
	    break;
    }

    strcat(premsgbuf, serverName);
    strcat(premsgbuf, " server ");

    if (reallyDown) {
	strcpy(postmsgbuf, "--contact lost\n");
    } else if (addrFailover) {
	strcpy(postmsgbuf, "--performed fail-over from ");
	osi_inet_ntoa_buf(connAddr.sin_addr, &postmsgbuf[strlen(postmsgbuf)]);
	strcat(postmsgbuf, "\n");
    } else {
	strcpy(postmsgbuf, "\n");
    }

    cm_PrintServerText(premsgbuf, serverp, postmsgbuf);

    if (!reallyDown) {
	CM_SLEEP(5);
    }
    return reallyDown;
}



/* Random number generator and constants from KnuthV2 2d ed, p170
 * lws&ota 92.08.13
 * Updated berman 94.02.25
 * Rules:
 *  X = (aX + c) % m
 *  m is a power of two and may be "2^32" so storage class does the mod.
 *  a % 8 is 5
 *  a is 0.73m  should be 0.01m .. 0.99m
 *  c is more or less immaterial.  1 or a is suggested.
 *
 * NB: LOW ORDER BITS are not very random.  To get small random numbers,
 *     treat result as <1, with implied binary point, and multiply by 
 *     desired modulus.
 * NB: Has to be unsigned, since shifts on signed quantities may preserve
 *     the sign bit.
 */
/* Try to get as much initial randomness as 
 * possible, since at least one customer reboots ALL their clients 
 * simultaneously -- so osi_GetTime is bound to be the same on some of the
 * clients.  But all of our DFS platforms get microsecond or better
 * resolution, so we are okay with just osi_Time().
 */
/* 
 * 3141592621 meets above requirements.
 */
#define	ranstage(x)	(x)= (u_int32) (3141592621*((u_int32)x)+1)
#ifdef SGIMIPS
long cm_random(void) 
#else  /* SGIMIPS */
long cm_random() 
#endif /* SGIMIPS */
{
    static u_int32 state = 0;
    int i;

    if (!state) {
        struct timeval t;
	osi_GetTime(&t);
#ifdef SGIMIPS
	state = (u_int32)((t.tv_usec & 0xfffffff0 ) + (t.tv_sec & 0xff));
#else  /* SGIMIPS */
	state = (t.tv_usec & 0xfffffff0 ) + (t.tv_sec & 0xff);
#endif /* SGIMIPS */
	for (i = 0; i < 30; i++) {
	    ranstage(state);
	}
    }

    ranstage(state);
    return state;
}



#if	defined(KERNEL)
/*
 * This internal function is called to synchronize the local system time
 * with the server serving in the same cell. 
 */
#ifdef SGIMIPS
static void SyncLocalTime(
    struct cm_conn *connp,
    struct timeval *serverTimep,
    long now, 
    u_long synchDistance,
    u_long synchDispersion)
#else  /* SGIMIPS */
static void SyncLocalTime(connp, serverTimep, now, synchDistance, synchDispersion) 
    struct cm_conn *connp;
    struct timeval *serverTimep;
    long now; 
    u_long synchDistance, synchDispersion;
#endif /* SGIMIPS */
{
    long delta, residue;
    char msgbuf[90];
#ifdef SGIMIPS
    unsigned32 st;
#else  /* SGIMIPS */
    u_long st;
#endif /* SGIMIPS */

    /* 
     * If we're supposed to set the time, and this is the host we use 
     * for the time (must be a server serving the local cell) and 
     * the time is really different, then really set the time.
     */
    if ((cm_setTimeHostValid &&
	 uuid_equal(&connp->serverp->serverUUID, &cm_setTimeHostUUID, &st)) ||

	(!cm_setTimeHostValid &&
	 (connp->serverp->cellp->lclStates & CLL_LCLCELL))) {
	delta = now - serverTimep->tv_sec;	/* how many secs fast we are */
	residue = 0;	/* how much we're not setting */
	/* 
	 * See if clock has changed enough to make it worthwhile 
	 */
	if (delta >= SR_MINCHANGE || delta <= -SR_MINCHANGE) {
	    if (delta > SR_MAXCHANGEBCK) {
		/* 
		 * Setting clock too far back; just do it a little 
		 */
		residue = delta - SR_MAXCHANGEBCK;
#ifdef SGIMIPS
		serverTimep->tv_sec = (time_t)(now - SR_MAXCHANGEBCK);
#else  /* SGIMIPS */
		serverTimep->tv_sec = now - SR_MAXCHANGEBCK;
#endif /* SGIMIPS */
	    }
	    osi_SetTime(serverTimep);	/* set the time */
	    if (delta > 0) {
		strcpy(msgbuf, "dfs: setting clock back ");
		if (delta > SR_MAXCHANGEBCK) {
		    osi_cv2string(SR_MAXCHANGEBCK,
			&msgbuf[sizeof "dfs: setting clock back " - 1]);
		    strcat(msgbuf, " secs (from ");
		    cm_PrintServerText(msgbuf, connp->serverp,
				       "); it's fast!\n");
		} else {
		    osi_cv2string(delta,
			&msgbuf[sizeof "dfs: setting clock back " - 1]);
		    strcat(msgbuf, " seconds (from ");
		    cm_PrintServerText(msgbuf, connp->serverp, ").\n");
		}
	    } else {
		/*
		 * Used to ifdef this out on ATRs, but they're 
		 * totally worthless machines anyway 
		 */
		strcpy(msgbuf, "dfs: setting clock ahead ");
		osi_cv2string(-delta,
			      &msgbuf[sizeof "dfs: setting clock ahead " - 1]);
		strcat(msgbuf, " seconds (from ");
		cm_PrintServerText(msgbuf, connp->serverp, ").\n");
	    }
	}

	cm_setTimeHostUUID  = connp->serverp->serverUUID;
	cm_setTimeHostValid = 1;

	/* Remember what we did for the pioctl. */
	cm_setTimeHostAddr = *(afsNetAddr *)&connp->addr;
	cm_timeResidue = residue;
#ifdef SGIMIPS
	cm_timeWhenSet.sec = (unsigned32)serverTimep->tv_sec; /* what we set it to */
	cm_timeWhenSet.usec = (unsigned32)serverTimep->tv_usec;
	afscall_timeSynchDistance = (unsigned int)synchDistance;
	afscall_timeSynchDispersion = (unsigned int)synchDispersion;
#else  /* SGIMIPS */
	cm_timeWhenSet.sec = serverTimep->tv_sec;	/* what we set it to */
	cm_timeWhenSet.usec = serverTimep->tv_usec;
	afscall_timeSynchDistance = synchDistance;
	afscall_timeSynchDispersion = synchDispersion;
#endif /* SGIMIPS */
    }
}



/* 
 * This function is to synchronize the local time with fx server's after a 
 * period of timeout. The flag cm_setTime must be set, otherwise this function 
 * should not be called. 
 * This is called with preemption ON.
 */
#ifdef SGIMIPS
static int SyncFxServer(struct cm_conn *connp)
#else  /* SGIMIPS */
static int SyncFxServer(connp)
    struct cm_conn *connp;
#endif /* SGIMIPS */
{
    long code, start, end;
    struct timeval tv;
#ifdef SGIMIPS
    unsigned32 synchDistance, synchDispersion;
#else  /* SGIMIPS */
    u_long synchDistance, synchDispersion;
#endif /* SGIMIPS */
    
    osi_PreemptionOff();
    start = osi_Time();	/* time the gettimeofday call */
    icl_Trace0(cm_iclSetp, CM_TRACE_GETTIMESTART);
    code = BOMB_EXEC("COMM_FAILURE",
		      (osi_RestorePreemption(0),
#ifdef SGIMIPS
		       code = AFS_GetTime(connp->connp, 
					  (unsigned32 *)&tv.tv_sec, 
				 	  (unsigned32  *) &tv.tv_usec,
#else  /* SGIMIPS */
		       code = AFS_GetTime(connp->connp, (u_long *)&tv.tv_sec, 
				 (u_long *) &tv.tv_usec,
#endif /* SGIMIPS */
				 &synchDistance, &synchDispersion),
		       osi_PreemptionOff(),
		       code));
    end = osi_Time();
    code = osi_errDecode(code);
    icl_Trace1(cm_iclSetp, CM_TRACE_GETTIMEEND, ICL_TYPE_LONG, code);

    /* Sync conn and binding addrs */
    cm_ReactToBindAddrChange(connp, 0 /* doOverride */);

    /* Set the local time only if the cm_setTime flag is on and that rpc call
     * worked quickly (same second response).
     */
    if ((code == 0) && (start == end)) {
        SyncLocalTime(connp, &tv, end, synchDistance, synchDispersion);
    }
    osi_RestorePreemption(0);
#ifdef SGIMIPS
    return (int)code;
#else  /* SGIMIPS */
    return code;
#endif /* SGIMIPS */
}
#endif /* KERNEL */


/*
 * cm_PrintServerText() -- print out address and cell for a server along
 *     with text; format is preamble, server info, postamble
 *
 * LOCKS USED: locks cm_siteLock; released on exit
 */

#ifdef SGIMIPS
void cm_PrintServerText(
  char *preamb,
  register struct cm_server *srvp,
  char *postamb)
#else  /* SGIMIPS */
void cm_PrintServerText(preamb, srvp, postamb)
  register struct cm_server *srvp;
  char *preamb, *postamb;
#endif /* SGIMIPS */
{
    char realpostamb[200];
    struct sockaddr_in srvaddr;

    /* Get "current" server address */
    lock_ObtainRead(&cm_siteLock);
    srvaddr = srvp->sitep->addrCurp->addr;
    lock_ReleaseRead(&cm_siteLock);

    /* Print preamble, server info, postamble */
    if (srvp->cellp->cellNamep &&
	(strlen(postamb) + strlen(srvp->cellp->cellNamep) + 15) < sizeof(realpostamb)) {
	strcpy(realpostamb, " in cell ");
	strcat(realpostamb, srvp->cellp->cellNamep);
	strcat(realpostamb, postamb);
	osi_printIPaddr(preamb, SR_ADDR_VAL(&srvaddr), realpostamb);
    } else {
	osi_printIPaddr(preamb, SR_ADDR_VAL(&srvaddr), postamb);
    }
}



/*
 * FindServerByPPL() -- find server identified by (cell, principal, svc)
 * 
 * ASSUMPTIONS: caller holds a lock on cm_serverlock
 *
 * RETURN CODES: reference to server, or NULL
 */

static struct cm_server*
FindServerByPPL(struct cm_cell *cellp,    /* server's cell */
		char *namep,              /* server's principal name */
		u_short svc)              /* server's type (SRT_FX/REP/FL) */
{
    int i;
    struct cm_server *serverp;

    i = SR_HASHPPL(namep);

    for (serverp = cm_serversPPL[i]; serverp; serverp = serverp->nextPPL) {
	if (serverp->cellp == cellp &&
	    serverp->sType == svc &&
	    !strcmp(serverp->principal, namep)) {
	    /* found (cell, principal, svc) match */
	    break;
	}
    }

    return serverp;
}



/*
 * strToInt() -- convert a string to an integer for hashing.
 *
 *     Algorithm cribbed from the Dragon book (Aho, Sethi, Ullman) pg 436.
 * 
 * ASSUMPTIONS: at least a 32-bit architecture.
 */

static int strToInt(char *strp)
{
    unsigned h, g;

    h = 0;

    for (; *strp; strp++) {
	h = (h << 4) + *strp;
	if (g = h & 0xf0000000) {
	    h ^= (g >> 24);
	    h ^= g;
	}
    }

    return h;
}
