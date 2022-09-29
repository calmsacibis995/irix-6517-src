/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: fshs_host.c,v 65.6 1998/04/01 14:15:53 gwehrman Exp $";
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
#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/lock.h>
#include <dcedfs/queue.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/tkn4int.h>
#include <dcedfs/scx_errs.h>
#include <dcedfs/bomb.h>
#include <fshs.h>
#include <fshs_trace.h>
#ifdef SGIMIPS
#include <dcedfs/krpc_misc.h>
#include <dcedfs/uuidutils.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/fshost/RCS/fshs_host.c,v 65.6 1998/04/01 14:15:53 gwehrman Exp $")

/* 
 * Declaration of all host-related (static?) variables.
 */
struct fshs_bucket fshs_priHostID[FSHS_NHOSTS];	/* Host UUID hash table */
struct fshs_bucket fshs_secHostID[FSHS_NHOSTS];	/* Host UUID hash table */
struct lock_data fshs_hostlock;			/* FIFO lock for hosts */
struct squeue fshs_fifo;		      	/* FIFO queue for hosts */
struct fshs_host *fshs_HostFreeListp = 0;	/* free host entries  */
static int fshs_toomanyhosts=0;

#ifdef SGIMIPS
extern void rpc_mgmt_set_call_timeout (rpc_binding_handle_t, unsigned32,
		unsigned32 *);
extern tkm_ResetTokensFromHost(struct hs_host *);
struct fshs_host *fshs_FindHost(struct context *);
void fshs_SimulateTSRCrash(void);
int fshs_HashOut(struct fshs_host *hostp);
#endif /* SGIMIPS */


/* 
 * Locking Hierachy in the fshost package. 
 *
 * There are four types of locks used in the fshost package to synchronize
 * operations on HOST and PRINCIPAL data structures. These locks must be
 * acquired in the following order to avoid the deadlock:
 *
 * 1)  fshs_hostlock: host FIFO queue, the free host list, and hash chain
 *
 * 2)  host.h.lock:  all fields of a host data structure. 
 *                   primary host first and then secondary
 *     NB: Incrementing host's refCount also takes a READ fshs_hostlock first
 *         in addition to a WRITE host lock. 
 *
 * 3)  fshs_princlock: principal's FIFO queue and the free principal list. 
 *
 * 4)  princ.h.lock: all fields of a principal data structure. 
 *
 */

extern char protSeq[];

/* 
 * fshs_InitHostMod -- initialize the fshs_host file structures. 
 */
#ifdef SGIMIPS
void fshs_InitHostMod(void)
#else
void fshs_InitHostMod()
#endif
{
    lock_Init(&fshs_hostlock);
}
/*
 * Get the next host entry from the Host free linked list; if none available
 * allocate a bunch (FSHS_HOSTSPERBLOCK many) and insert them into the free
 * linked list. If the max host allocation quota has reached, try first
 * freeing stale hosts (by calling fshs_CheckHost on all hosts) and if that
 * fails allocate an additional bunch.
 * Note: It must be called with WRITE fshs_hostlock. 
 */
#ifdef SGIMIPS
struct fshs_host *fshs_AllocHost(void)
#else
struct fshs_host *fshs_AllocHost()
#endif
{
    register struct fshs_host *hostp;

    if (fshs_HostFreeListp == 0) {
	/* 
	 * Get a new block of Host entries & chain them on fshs_HostFreeListp 
	 */
	register int i;

	if (fshs_stats.allocHostBlocks >= FSHS_MAXHOSTBLOCKS) {
	    lock_ReleaseWrite(&fshs_hostlock);
	    /* 
	     * fshs_CheckHost will GC any stale host and/or principal entries,
	     * so we might get some free host entries 
	     */
	    i = fshs_Enumerate(fshs_CheckHost, (char *)0);
	    lock_ObtainWrite(&fshs_hostlock);
	}
	if (!fshs_HostFreeListp) {
	    hostp = (struct fshs_host *) osi_Alloc(FSHS_HOSTSPERBLOCK * sizeof(*hostp));
	    fshs_HostFreeListp = (struct fshs_host *) &(hostp[0]);
	    for (i = 0; i < (FSHS_HOSTSPERBLOCK -1); i++)
		hostp[i].h.lruq.next = (struct squeue *) &(hostp[i+1]);
	    hostp[FSHS_HOSTSPERBLOCK-1].h.lruq.next = (struct squeue *)0;
	    fshs_stats.allocHostBlocks++;
	}
    }
    hostp = fshs_HostFreeListp;
    fshs_HostFreeListp = (struct fshs_host *) (hostp->h.lruq.next);
    fshs_stats.usedHosts++;
    bzero((char *)hostp, sizeof(struct fshs_host));
    return hostp;
}
/* 
 * Free the host entry and put it back in the fshs_HostFreeListp 
 *
 * Called with WRITE fshs_hostlock and WRITE host lock.
 */
#ifdef SGIMIPS
void fshs_FreeHost( register struct fshs_host *hostp)
#else
void fshs_FreeHost(hostp)
    register struct fshs_host *hostp;
#endif
{
    icl_Trace1(fshs_iclSetp, FSHS_FREEHOST, ICL_TYPE_POINTER, hostp);
    QRemove(&hostp->h.lruq);
    osi_assert(lock_WriteLocked(&hostp->h.lock));
    lock_ReleaseWrite(&hostp->h.lock);
    lock_Destroy(&hostp->h.lock);
    hostp->h.lruq.next = (struct squeue *) fshs_HostFreeListp;
    fshs_HostFreeListp = hostp;
    fshs_stats.usedHosts--;
}


/*
 * Locate the desired host based on a client's uuid. 
 * If the request is from a secondary service call, locate the prime
 * host structure after finding its secondary service host structure. 
 *
 * Locks: called with at least READ hostlock held.
 */
#ifdef SGIMIPS
struct fshs_host *fshs_FindHost(
     struct context *cookie)
#else
struct fshs_host *fshs_FindHost(cookie)
     struct context *cookie;
#endif
{
    struct fshs_host *hostp;
    struct fshs_bucket *hashp;
    unsigned32 st;
    int isPrimary;

    icl_Trace1(fshs_iclSetp, FSHS_FINDHOST, ICL_TYPE_POINTER, cookie);

    isPrimary = (dfsuuid_GetParity(&cookie->clientID) == 0);
    hostp = NULL;

    if (isPrimary) {
	hashp = &fshs_priHostID[cookie->uuidIndex];
	for (hostp = hashp->next;  hostp;  hostp = hostp->nextPriID) {
	    if (uuid_equal(&hostp->clientMainID, &cookie->clientID, &st))  {
		lock_ObtainWrite(&hostp->h.lock);
		if (hostp->flags & FSHS_HOST_STALE) {
		    /* not this one */
		    lock_ReleaseWrite(&hostp->h.lock);
		    continue;
		}
		/* bump the refCount and then we're done */
		hostp->h.refCount++;
		icl_Trace1(fshs_iclSetp, FSHS_FINDPRIMEHOST, ICL_TYPE_POINTER, 
			   hostp);
		lock_ReleaseWrite(&hostp->h.lock);
		break;
	    }
	}
    }
    else {
	hashp = &fshs_secHostID[cookie->uuidIndex];
	for (hostp = hashp->next;  hostp;  hostp = hostp->nextSecID) {
	    if (uuid_equal(&hostp->clientSecID, &cookie->clientID, &st))  {
		lock_ObtainWrite(&hostp->h.lock);
		if (hostp->flags & FSHS_HOST_STALE) {
		    /* not this one */
		    lock_ReleaseWrite(&hostp->h.lock);
		    continue;
		}
		/* bump the refCount and then we're done */
		hostp->h.refCount++;
		icl_Trace1(fshs_iclSetp, FSHS_FINDPRIMEHOST, ICL_TYPE_POINTER, 
			   hostp);
		lock_ReleaseWrite(&hostp->h.lock);
		break;
	    }
	}
    }
    return hostp;
}


/* get a host suitable for normal or simple operation.
 * Must be called with write-locked fshs_hostlock.
 */
#ifdef SGIMIPS
struct fshs_host *fshs_GetSimpleHost(void)
#else
struct fshs_host *fshs_GetSimpleHost()
#endif
{
    register struct fshs_host *hp;

    hp = fshs_AllocHost();
    lock_Init(&hp->h.lock);
    hp->h.refCount = 1;
    hp->h.hstopsp = &fshs_ops;
    hp->lastCall = osi_Time();	/* avoid quick GC */
    hp->lastRevoke = hp->lastCall;	/* ditto */
    QAdd(&fshs_fifo, &hp->h.lruq);

    /* set TSR timeout parameters to defaults */
#ifdef SGIMIPS
    hp->hostLifeGuarantee = (unsigned int)fshs_tsrParams.lifeGuarantee;
    hp->hostRPCGuarantee = (unsigned int)fshs_tsrParams.RPCGuarantee;
    hp->deadServerTimeout = (unsigned int)fshs_tsrParams.deadServer;
#else
    hp->hostLifeGuarantee = fshs_tsrParams.lifeGuarantee;
    hp->hostRPCGuarantee = fshs_tsrParams.RPCGuarantee;
    hp->deadServerTimeout = fshs_tsrParams.deadServer;
#endif
    return hp;
}


/* Get the TSR descriptor bits for this host. */
/* Called with the hostp at least read-locked. */
#ifdef SGIMIPS
long
fshs_GetTSRCode(
  struct fshs_host *hp)
#else
long fshs_GetTSRCode(hp)
  struct fshs_host *hp;
#endif /* SGIMIPS */
{
    long Flags = 0;

    /*
     * Currently, the server does not keep track of the 
     * clients across crashes and it doesn't keep track of epochs.
     */

    /* If we're in the post-crash recovery phase, say so to allow re-connection. */
    if (fshs_postRecovery) 
	Flags |= TKN_FLAG_CRASHED;

    /* If the host object never had a token, there's no point in getting old ones by ID */
    if (!(hp->h.states & HS_HOST_HADTOKENS))
	Flags |= TKN_FLAG_DISALLOW_SAME;

    /* In the new I/F, this call is made to set the timer values; the tokens are OK */
    if (hp->flags & FSHS_HOST_NEW_IF)
	Flags |= TKN_FLAG_NEW_IF;

    icl_Trace2(fshs_iclSetp, FSHS_GETTSRCODE,
		ICL_TYPE_POINTER, hp,
		ICL_TYPE_LONG, Flags);

    return Flags;
}


/* Create a host object for the incoming call.  If our client
 * intends to make calls on both a primary and secondary interface,
 * then cookie->needsSecondarySvc is set and we should setup the
 * other interface here, too.  Otherwise, all calls are primary interface
 * calls, and we set that interface alone.  We setup the callback binding
 * if we were passed a non-zero callback address.
 *
 * Note that one host object handles both interfaces, and has two
 * associated UUIDs.
 *
 * This function is called rarely (setcontext calls) so it should be
 * OK to get a write lock on the fshs_hostlock.
 */

/* The general idea of the state transitions in the new world is like this.
  A freshly-created host object, created via the primary interface, starts out
  DOWN, but if the TKN_InitTokenState call is successful, it's marked UP.
  A freshly-created host object, created via the secondary interface, starts
  out in NEEDINIT state.  Subsequent calls to the object created via the
  primary interface will simply use the host object.  Subsequent secondary
  calls to the object created via the secondary interface will use the host
  object normally, but subsequent primary calls will require a non-resetting
  AFS_SetContext call to be used (so that the TKN_InitTokenState call-back
  can be made).
*/

/*
 * It's better to take the actual handle for the incoming RPC here, converting
 * to a string binding and so forth when we need to, than to do that conversion
 * on every call in InqContext and store the answer in the context cookie.
 */
/* PARAMETERS -- hostP is returned even when the returned code is non-zero so
 *     caller must call fshs_PutHost unconditionally. */

long
fshs_CreateHost(
  struct context *cookiep,
  struct sockaddr_in *ipAddrp,
  uuid_t *otherUUIDp,
  handle_t hdl,				/* the incoming RPC handle */
  u_char *princNameP,			/* from the incoming afsNetData */
  unsigned32 Flags,			/* from the AFS_SetContext call */
  struct fshs_host **hostP)
{
    struct fshs_host *hp;	/* the host for this incoming call */
    struct fshs_bucket *hashp;	/* hash bucket */
    long code;			/* error code */
    int isPrimary;		/* if this is a primary int. call */
    int canDoResetOrCallback;	/* if we can even try to reset the DOWN or NEEDINIT flags */
#ifdef SGIMIPS
    unsigned32 st;
#else
    unsigned long st;
#endif
    handle_t reverseClientBinding, tmpBinding;
    struct sockaddr_in reverseSockAddr;
    unsigned32 now;
    /* local ..../self identity: */
    static unsigned32 selfIdentity[2] = { ~0u, 0 };

    isPrimary = (dfsuuid_GetParity(&cookiep->clientID) == 0);
    lock_ObtainWrite(&fshs_hostlock);
    hp = fshs_FindHost(cookiep);
    if (hp) {
	lock_ReleaseWrite(&fshs_hostlock);
	if (ipAddrp && hp->clientAddr.sin_port != ipAddrp->sin_port) {
	    /* The client's local port number changed.  Re-bind, always. */
	    icl_Trace4(fshs_iclSetp, FSHS_TRACE_NEWPORT,
		       ICL_TYPE_POINTER, hp,
		       ICL_TYPE_LONG, ipAddrp->sin_addr.s_addr,
		       ICL_TYPE_LONG, hp->clientAddr.sin_port,
		       ICL_TYPE_LONG, ipAddrp->sin_port);
	    fshs_StartCall(hp);
	    code = krpc_CopyBindingWithNewPort("fx", hp->cbBinding,
					       &reverseClientBinding,
					       ntohs(ipAddrp->sin_port));
	    if (code == 0) {
		/* Always bind back to the ``main ID'', never the secondary ID. */
		rpc_binding_set_object(reverseClientBinding, &hp->clientMainID, &st);
		rpc_mgmt_set_call_timeout(reverseClientBinding, 150 /* sec */, &st);
		if (st != error_status_ok) {
		    uprintf("fx: can't set the timeout value (code %d)\n", st);
		    code = st;
		}
		if (code == 0 && princNameP) {
		    osi_RestorePreemption(0);
		    rpc_binding_set_auth_info(reverseClientBinding, princNameP,
					      rpc_c_protect_level_pkt_integ,
					      rpc_c_authn_dce_secret,
					      (rpc_auth_identity_handle_t)selfIdentity,
					      rpc_c_authz_dce, &st);
		    (void)osi_PreemptionOff();
		    icl_Trace3(fshs_iclSetp, FSHS_TRACE_NEWPORTAUTH,
			       ICL_TYPE_POINTER, hp,
			       ICL_TYPE_STRING, princNameP,
			       ICL_TYPE_LONG, st);
		    code = (st == sec_rgy_object_not_found) ? 0 : st;
		}
	    }
	    lock_ObtainWrite(&hp->h.lock);
	    if (code == 0) {
		/* Make the change permanent. */
		tmpBinding = hp->cbBinding;
		hp->cbBinding = reverseClientBinding;
		rpc_binding_free(&tmpBinding, &st);
		hp->clientAddr.sin_port = ipAddrp->sin_port;
		hp->clientEpoch = cookiep->epochTime;
	    }
	    fshs_EndCall(hp);
	    lock_ReleaseWrite(&hp->h.lock);
	    if (code)
		goto out;
	}
    } else {
	/* no host; create the dude */
	hp = fshs_GetSimpleHost();
	if (isPrimary) {
	    hp->clientMainID = cookiep->clientID;
	    hp->clientSecID = *otherUUIDp;
	}
	else {
	    hp->clientMainID = *otherUUIDp;
	    hp->clientSecID = cookiep->clientID;
	}
	if (Flags & AFS_FLAG_CONTEXT_DO_RESET) {
	    /* New interface, asked for a reset. */
	    /* Make this DOWN, while we wait for initialization. */
	    hp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
	} else if (Flags & AFS_FLAG_CONTEXT_NEW_IF) {
	    /* New interface, but didn't ask for a reset. */
	    /* Make this DOWN, while we wait for initialization. */
	    hp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
	} else {
	    /* Old interface.  Distinguish based on primary/secondary. */
	    if (isPrimary) {
		/* Make this DOWN, while we wait for initialization. */
		hp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
	    } else {
		/*
		 * Start with the host UP, not DOWN (so token revocations will be
		 * processed), but in NEEDINIT state so that the next primary call
		 * will do an init-token-state call.
		 */
		hp->h.states |= HS_HOST_NEEDINIT;
	    }
	}
	/* prevent early call uses: mimic fshs_StartCall(). */
	hp->flags |= FSHS_HOST_INCALL;
	hp->priIndex = uuid_hash((uuid_t *) &hp->clientMainID, &st)
	    % FSHS_NHOSTS;
	hashp = &fshs_priHostID[hp->priIndex];
	hp->nextPriID = hashp->next;
	hashp->next = hp;
	hp->flags |= FSHS_HOST_INPRI;
	hp->maxFileSize = osi_maxFileSizeDefault;
	hp->supports64bit = 0;
	if (cookiep->needSecondarySvc) {
	    hp->secIndex = uuid_hash((uuid_t *)&hp->clientSecID, &st)
		% FSHS_NHOSTS;
	    hashp = &fshs_secHostID[hp->secIndex];
	    hp->nextSecID = hashp->next;
	    hashp->next = hp;
	    hp->flags |= FSHS_HOST_INSEC;
	}
	hp->clientEpoch = cookiep->epochTime;
	/* Set the new-style interface flag as necessary */
	if (Flags & (AFS_FLAG_CONTEXT_NEW_IF |
		     AFS_FLAG_CONTEXT_DO_RESET)) {
	    hp->flags |= FSHS_HOST_NEW_IF;
	}

	/* set the new delegation acl types flag */
	if (Flags & AFS_FLAG_CONTEXT_NEW_ACL_IF)
	    hp->flags |= FSHS_HOST_NEW_ACL_IF;

	icl_Trace4(fshs_iclSetp, FSHS_CREATEHOST1,
		   ICL_TYPE_POINTER, hp,
		   ICL_TYPE_LONG, isPrimary,
		   ICL_TYPE_LONG, Flags,
		   ICL_TYPE_LONG, hp->h.states);
	icl_Trace2(fshs_iclSetp, FSHS_CREATEHOST2,
		   ICL_TYPE_POINTER, hp,
		   ICL_TYPE_LONG, hp->flags);

	/* this next lock call won't wait, since we've just created
	 * the lock here.
	 */
	lock_ObtainWrite(&hp->h.lock);
	lock_ReleaseWrite(&fshs_hostlock);
	
	/* now do the time-consuming initialization, if any.  For example,
	 * the binding handle creation and callback work gets done here.
	 */
	if (ipAddrp) {
	    /* only have to set port and host for krpc_BindingFromSockaddr */
	    hp->clientAddr.sin_addr.s_addr = ipAddrp->sin_addr.s_addr;
	    hp->clientAddr.sin_port = ipAddrp->sin_port;

#ifdef SGIMIPS
	    /* is a struct sockaddr_in interchangeable with an afsNetAddr ??? */
	    code = krpc_BindingFromSockaddr("fx", protSeq,
					    (afsNetAddr *)ipAddrp,
					    &hp->cbBinding);
#else
	    code = krpc_BindingFromSockaddr("fx", protSeq,
					    ipAddrp,
					    &hp->cbBinding);
#endif
	    if (code) {
		fshs_EndCall(hp);
		lock_ReleaseWrite(&hp->h.lock);
		goto out;
	    }
	    /* Always bind back to the ``main ID'', never the secondary ID. */
	    rpc_binding_set_object(hp->cbBinding, &hp->clientMainID, &st);
	    rpc_mgmt_set_call_timeout(hp->cbBinding, 150 /* sec */, &st);
	    if (st != error_status_ok) {
		uprintf("fx: can't set the timeout value (code %d)\n", st);
	    }
	    hp->registerTokens = 1;
	}
	fshs_EndCall(hp);	/* allowed to use this binding now */
	lock_ReleaseWrite(&hp->h.lock);
    }

    /* if this is a primary call, do a callback, too, if host was down */
    code = 0;	/* default */
    canDoResetOrCallback = 0;
    if (hp->flags & FSHS_HOST_NEW_IF) {
	/* New-style usage.  Consider the reset if the caller agreed. */
	if (Flags & AFS_FLAG_CONTEXT_DO_RESET) {
	    canDoResetOrCallback = 1;
	} else {
	    /* Do just a callback if it's primary and NEEDINIT (not DOWN). */
	    if (isPrimary
		&& (Flags & AFS_FLAG_CONTEXT_NEW_IF)
		&& ((hp->h.states & (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT))
		    == HS_HOST_NEEDINIT))
		canDoResetOrCallback = 1;
	}
    } else {
	/* Old-style usage.  Consider the reset only if we're a primary. */
	if (isPrimary)
	    canDoResetOrCallback = 1;
    }
    if (hp->registerTokens && canDoResetOrCallback) {
	icl_Trace1(fshs_iclSetp, FSHS_CREATEHOST3,
		   ICL_TYPE_POINTER, hp);
	fshs_StartCall(hp);
	/* 
	 * when we get here, it is safe to start a call if we need
	 * to in order to clear the down flag.  We're worried about
	 * two different users doing primary SetContexts, user A
	 * marking the host up, user B getting his setcontext call through,
	 * a revoke to this host failing, and then user B marking the
	 * host up incorrectly.  Remember, setcontexts are not serialized
	 * from a given client, only from a given *user*.
	 */
	lock_ObtainWrite(&hp->h.lock);
	if (isPrimary
	    && (hp->h.states & (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT))) {
	    code = krpc_BindingFromInBinding("fx", hdl, &reverseClientBinding,
					     &hp->clientAddr, &reverseSockAddr);
	    if (code) {
		icl_Trace2(fshs_iclSetp, FSHS_CREATEHOSTSUB1,
			   ICL_TYPE_POINTER, hp,
			   ICL_TYPE_LONG, code);
	    } else {
		rpc_binding_set_object(reverseClientBinding, &hp->clientMainID, &st);
		rpc_mgmt_set_call_timeout(reverseClientBinding, 150 /* sec */, &st);
		if (st != error_status_ok) {
		    uprintf("fx: can't set the call timeout value (code %d)\n", st);
		    icl_Trace2(fshs_iclSetp, FSHS_CREATEHOSTSUB2,
			       ICL_TYPE_POINTER, hp,
			       ICL_TYPE_LONG, st);
		}
		lock_ReleaseWrite(&hp->h.lock);
		if (princNameP) {
		    osi_RestorePreemption(0);
		    rpc_binding_set_auth_info(reverseClientBinding, princNameP,
					      rpc_c_protect_level_pkt_integ,
					      rpc_c_authn_dce_secret,
					      (rpc_auth_identity_handle_t)selfIdentity,
					      rpc_c_authz_dce, &st);
		    (void)osi_PreemptionOff();
		    icl_Trace3(fshs_iclSetp, FSHS_CREATEHOSTSUB2_1,
			       ICL_TYPE_POINTER, hp,
			       ICL_TYPE_STRING, princNameP,
			       ICL_TYPE_LONG, st);
		    code = (st == sec_rgy_object_not_found) ? 0 : st;
		}
		if (code == 0) {
		    icl_Trace4(fshs_iclSetp, FSHS_CREATEHOSTSUB3,
			       ICL_TYPE_LONG, reverseSockAddr.sin_addr.s_addr,
			       ICL_TYPE_LONG, reverseSockAddr.sin_port,
			       ICL_TYPE_LONG, hp->clientAddr.sin_addr.s_addr,
			       ICL_TYPE_LONG, hp->clientAddr.sin_port);
		    code = tokenint_InitTokenState(hp, reverseClientBinding);
		    icl_Trace2(fshs_iclSetp, FSHS_CREATEHOSTSUB4,
			       ICL_TYPE_POINTER, hp,
			       ICL_TYPE_LONG, code);
		}
		lock_ObtainWrite(&hp->h.lock);
		if (code == 0) {
		    /* OK--use the reverse binding! */
		    tmpBinding = hp->cbBinding;
		    hp->cbBinding = reverseClientBinding;
		    rpc_binding_free(&tmpBinding, &st);
		    hp->clientAddr.sin_addr.s_addr = reverseSockAddr.sin_addr.s_addr;
		    /* Client-reattach counts on clientAddr.sin_port not
		     * changing here.
		     */
		    /* hp->clientAddr.sin_port = reverseSockAddr.sin_port; */
		} else {
		    /* release what BindingFromInBinding obtained */
		    rpc_binding_free(&reverseClientBinding, &st);
		}
	    }
	    if (code) {
		/* Reverse binding failed; use the given one. */
		lock_ReleaseWrite(&hp->h.lock);
		code = 0;
		if (princNameP) {
		    osi_RestorePreemption(0);
		    rpc_binding_set_auth_info(hp->cbBinding, princNameP,
					      rpc_c_protect_level_pkt_integ,
					      rpc_c_authn_dce_secret,
					      (rpc_auth_identity_handle_t)selfIdentity,
					      rpc_c_authz_dce, &st);
		    (void)osi_PreemptionOff();
		    icl_Trace3(fshs_iclSetp, FSHS_CREATEHOST3_1,
			       ICL_TYPE_POINTER, hp,
			       ICL_TYPE_STRING, princNameP,
			       ICL_TYPE_LONG, st);
		    code = (st == sec_rgy_object_not_found) ? 0 : st;
		}
		if (code == 0) {
		    icl_Trace1(fshs_iclSetp, FSHS_CREATEHOST4,
			       ICL_TYPE_POINTER, hp);
		    code = tokenint_InitTokenState(hp, hp->cbBinding);
		    icl_Trace2(fshs_iclSetp, FSHS_CREATEHOST5,
			       ICL_TYPE_POINTER, hp,
			       ICL_TYPE_LONG, code);
		}
		lock_ObtainWrite(&hp->h.lock);
	    }
	    if (code == 0) {
		hp->h.states &= ~(HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
		now = osi_Time(); 
		if (hp->lastRevoke < now)
		    hp->lastRevoke = now;
	    } else {
		/* 
		 * If we get an error, it means we cannot do the callback work
		 * to the client. Possibly, the client does not provides us a
		 * valid backback address. We should return a meaningful error.
		 */
		code = FSHS_ERR_FATALCONN;
		/* In addition, if the host was marked only as NEEDINIT and
		 * not as DOWN, this call's failure is evidence that the
		 * callback address is useless, so that revocation calls are
		 * doomed.  Mark the host as DOWN so that such calls will fail.
		 */
		hp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
	    }
	}
	/* Catch the case of a secondary DO_RESET: clear the DOWN flag */
	if (code == 0 && (Flags & AFS_FLAG_CONTEXT_DO_RESET)) {
	    /* Leave NEEDINIT flag uncleared, though, so primary call will do it */
	    /* But even if HOSTDOWN was cleared above, if DO_RESET was set, we
	     * must return a TSR code.
	     */
	    hp->h.states &= ~HS_HOST_HOSTDOWN;
	    /* Return the TSR mode bits via the error code */
	    code = (fshs_GetTSRCode(hp) & 0x3ff) + TKN_ERR_NEW_SET_BASE;
	    icl_Trace3(fshs_iclSetp, FSHS_CREATEHOST6,
		       ICL_TYPE_POINTER, hp,
		       ICL_TYPE_LONG, hp->h.states,
		       ICL_TYPE_LONG, code);
	}
	fshs_EndCall(hp);
	lock_ReleaseWrite(&hp->h.lock);
    }
    /* On a new-style probe call, reflect that the host is down */
    if (code == 0
	 && (Flags & AFS_FLAG_CONTEXT_NEW_IF)
	 && ((hp->h.states & HS_HOST_HOSTDOWN)
	     || (isPrimary && hp->h.states & HS_HOST_NEEDINIT)))
	code = TKN_ERR_NEW_NEEDS_RESET;
out:
    if (hostP)
	*hostP = hp;
    else
	fshs_PutHost(hp);

    /* Tell the new-style client that we're a new-style server. */
    if (code == 0) {
	if (Flags & (AFS_FLAG_CONTEXT_DO_RESET |
		     AFS_FLAG_CONTEXT_NEW_IF))
	    code = TKN_ERR_NEW_IF;
    }
    icl_Trace1(fshs_iclSetp, FSHS_CREATEHOST7,
		ICL_TYPE_LONG, code);
    return code;
}


/* 
 * Locate the desired host based on client's uuid. If not found, allocate both
 * a new prime and a secondary host structure (if requested) for the client.
 *
 * Locks: This routine should be called with NO lock held.
 */
#ifdef SGIMIPS
struct fshs_host *fshs_GetHost(
    struct context *cookie)
#else
struct fshs_host *fshs_GetHost(cookie)
    struct context *cookie;
#endif
{
    struct fshs_host *hostp, *sechostp = NULL;
    register int forceCallBack;
    unsigned32 st;
    unsigned32 now;

    icl_Trace1(fshs_iclSetp, FSHS_GETHOST, ICL_TYPE_POINTER, cookie);

    BOMB_IF("FSHS_TSRCRASH") {
	fshs_SimulateTSRCrash();
    }

    lock_ObtainRead(&fshs_hostlock);
    hostp = fshs_FindHost(cookie);  /* return hostp with bumped refcount */
    lock_ReleaseRead(&fshs_hostlock);

    if (hostp) {
	now = osi_Time(); 
	lock_ObtainWrite(&hostp->h.lock);
	if (hostp->lastCall < now)
	    hostp->lastCall = now;
	if (hostp->h.states & (HS_HOST_NEEDINIT | HS_HOST_HOSTDOWN)) {
	    /* down hosts require new init state, so we can do token
	     * initialization again.  This *isn't* true for secondary
	     * interface calls, just primary calls.  That's because
	     * our guarantee that we deliver a token state reset
	     * before doing any work applies only to primary calls, and
	     * because we may need to do secondary calls as part of the
	     * TSR work that we do from the TKN_InitTokenState call.
	     */
	    /* Also require an init-token-state call if the host is only in
	     * NEEDINIT state, since this is then the first primary call on a
	     * host object that was initially set up by a secondary call.
	     */
	    /* The new-interface work requires that when we know that a
	     * new-style client is in DOWN state, even if it's on a secondary
	     * interface, that client must issue the appropriate AFS_SetContext
	     * call to set the host object to UP state.  If it's in NEEDINIT
	     * state, this is not necessary for secondary-interface calls, but
	     * an appropriate AFS_SetContext call is necessary to clear the
	     * NEEDINIT flag.
	     */
	    forceCallBack = 0;
	    if (hostp->flags & FSHS_HOST_NEW_IF) {
		/* new interface */
		if ((hostp->h.states & HS_HOST_HOSTDOWN)
		    || dfsuuid_GetParity(&cookie->clientID) == /*primary */ 0)
		    /* either it's down, or it's primary in NEEDINIT state */
		    forceCallBack = 1;
	    } else {
		/* old interface */
		if (dfsuuid_GetParity(&cookie->clientID) == /*primary */ 0)
		    /* any primary, whether needinit or down, gets callback */
		    forceCallBack = 1;
	    }
	    if (forceCallBack) {
		icl_Trace3(fshs_iclSetp, FSHS_GETHOSTFORCEFAILURE,
			   ICL_TYPE_POINTER, hostp,
			   ICL_TYPE_LONG, hostp->h.states,
			   ICL_TYPE_LONG, hostp->flags);
		lock_ReleaseWrite(&hostp->h.lock);
		fshs_PutHost(hostp);
		return (struct fshs_host *) 0;
	    }
	}
	lock_ReleaseWrite(&hostp->h.lock);
	icl_Trace1(fshs_iclSetp, FSHS_FASTHOST, ICL_TYPE_POINTER, hostp);
    }
    else {
	icl_Trace0(fshs_iclSetp, FSHS_NULLHOST);
	/* hostp == NULL. There is a problem here */
    }

done:
    return hostp;
}


/* 
 * Decrement the host reference count.
 */
#ifdef SGIMIPS
void fshs_PutHost(
    register struct fshs_host *hostp)
#else
void fshs_PutHost(hostp)
    register struct fshs_host *hostp;
#endif
{
    lock_ObtainWrite(&hostp->h.lock);
    hostp->h.refCount--;
    osi_assert(hostp->h.refCount >= 0); /* to be removed */
    lock_ReleaseWrite(&hostp->h.lock);
    return;
}

/*
 * GC obsolete hosts, ie., hosts must be marked FSHS_HOST_STALE.
 * NB: if refCount is NOT zero, host cannot be marked FSHS_HOST_STALE.
 *
 * Called with WRITE fshs_hostlock and WRITE host lock.
 */
#ifdef SGIMIPS
void fshs_GCHost(
    register struct fshs_host *hostp)
#else
void fshs_GCHost(hostp)
    register struct fshs_host *hostp;
#endif
{
    register struct fshs_principal **cp, *princp, *tprincp;
    unsigned32 st;

    icl_Trace2(fshs_iclSetp, FSHS_GCHOST, ICL_TYPE_POINTER, hostp,
	       ICL_TYPE_LONG, hostp->h.refCount);
    osi_assert(hostp->flags & FSHS_HOST_STALE); /* To be removed ??? */

    lock_ObtainWrite(&fshs_princlock);
    cp = &hostp->princListp;
    for (princp = *cp; princp;) {
         tprincp = *cp = princp->next;
	 fshs_FreePrincipal(princp);
	 princp = tprincp;
    }
    lock_ReleaseWrite(&fshs_princlock);

    if (hostp->flags & FSHS_HOST_STALE)  {
	icl_Trace0(fshs_iclSetp, FSHS_GCPRIMEHOST);

	/* free callback RPC binding */
	if (hostp->cbBinding)
	    rpc_binding_free(&hostp->cbBinding, &st);

	/* Remove it from the hash tables */
	fshs_HashOut(hostp);

	/* and also put into FreeQueue */
	fshs_FreeHost(hostp);
    }
}


/* remove host object from any hash tables containing it.  Requires
 * write locks on fshs_hostlock and on specific hostp->h.lock.
 */
int
fshs_HashOut(struct fshs_host *hostp)
{
    struct fshs_host **temp, *hp;
    struct fshs_bucket *hashIDp;

    if (hostp->flags & FSHS_HOST_INPRI) {
	hashIDp = &fshs_priHostID[hostp->priIndex];
	for (temp = &hashIDp->next; hp = *temp; temp = &hp->nextPriID) {
	    if (hp == hostp) {
		*temp = hp->nextPriID;
		break;
	    }
	}
	osi_assert(hp);
    }
    if (hostp->flags & FSHS_HOST_INSEC) {
	hashIDp = &fshs_secHostID[hostp->secIndex];
	for (temp = &hashIDp->next; hp = *temp; temp = &hp->nextSecID) {
	    if (hp == hostp) {
		*temp = hp->nextSecID;
		break;
	    }
	}
	osi_assert(hp);
    }

    /* turn off both flags */
    hostp->flags &= ~(FSHS_HOST_INSEC|FSHS_HOST_INPRI);

    return 0;
}


/*
 * This routine is responsible for moving up to number host entries from the 
 * fshs_fifo queue to the host free linked list. Note that currently if no 
 * entry is moved we  allocate a new bunch of new host entries anyway since we 
 * hate panicing file servers; it shouldn't happen often anyway. 
 *
 * This routine must be called with WRITE fshs_hostlock.
 */
#ifdef SGIMIPS
void fshs_UpdateHostList(
    int number)
#else
void fshs_UpdateHostList(number)
    int number;
#endif
{
    register struct squeue *tq;
    struct squeue *pq;
    register struct fshs_host *hostp;

    icl_Trace0(fshs_iclSetp, FSHS_UPDATEHOSTLIST);
    for (tq = fshs_fifo.prev; tq != &fshs_fifo && number > 0; tq = pq) {
	hostp = FSHS_QTOH(tq);
	lock_ObtainWrite(&hostp->h.lock);
	pq = QPrev(tq);	/* do before can free host object */
	if (hostp->flags & FSHS_HOST_STALE) {
	    /* 
	     * Since only stale entries get GCed 
	     */	
	    fshs_GCHost(hostp);
	    number--;
	} 
	else 
	    lock_ReleaseWrite(&hostp->h.lock);
    }
}

/* used for debugging: ensures that all hosts that call in think
 * that they're new hosts, by preventing fshs_FindHost from finding
 * already existing hosts.
 */
void fshs_ResetAllHosts(void)
{
    struct squeue *tqp;
    struct fshs_host *hostp;

    lock_ObtainWrite(&fshs_hostlock);
    /* run through entire list, unhashing all elements that are hashed
     * in either table.  This will prevent fshs_FindHost from finding
     * this guy again.
     */
    for(tqp = fshs_fifo.prev; tqp != &fshs_fifo; tqp = QPrev(tqp)) {
	hostp = FSHS_QTOH(tqp);
	lock_ObtainWrite(&hostp->h.lock);
	if (!(hostp->flags & FSHS_HOST_STALE)) {
	    if (hostp->flags & (FSHS_HOST_INPRI|FSHS_HOST_INSEC))
		fshs_HashOut(hostp);
	    /* block revokes */
	    hostp->h.states |= (HS_HOST_HOSTDOWN | HS_HOST_NEEDINIT);
	    /* now, discard tokens from this guy, after holding host
	     * so that it doesn't go away and lose our place in the
	     * list.
	     */
	    hostp->h.refCount++;
	    lock_ReleaseWrite(&hostp->h.lock);
	    lock_ReleaseWrite(&fshs_hostlock);
#ifdef SGIMIPS
	    tkm_ResetTokensFromHost(&(hostp->h));
#else
	    tkm_ResetTokensFromHost(hostp);
#endif
	    lock_ObtainWrite(&fshs_hostlock);
	    lock_ObtainWrite(&hostp->h.lock);
	    hostp->h.refCount--;
	}
	lock_ReleaseWrite(&hostp->h.lock);
    }
    lock_ReleaseWrite(&fshs_hostlock);
}

/* function used to simulate a server crash */
void
fshs_SimulateTSRCrash(void)
{
    fshs_postRecovery = 1;
    fshs_tsrParams.serverRestart = osi_Time();	/* new epoch makes client
						 * think server crashed.
						 */
    fshs_tsrParams.endRecoveryTime = osi_Time() + 60;	/* test TSR code */
    fshs_ResetAllHosts();
}
