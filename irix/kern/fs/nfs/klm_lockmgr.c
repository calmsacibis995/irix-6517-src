#if 0
static char sccsid[] = 	"@(#)klm_lockmgr.c	2.1 88/05/24 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * From SUN 1.16
 */
#ident "$Revision: 1.75 $"

/*
 * Kernel<->Network Lock-Manager Interface
 *
 * File- and Record-locking requests are forwarded (via RPC) to a
 * Network Lock-Manager running on the local machine.  The protocol
 * for these transactions is defined in /usr/src/protocols/klm_prot.x
 */

#include "types.h"
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ksignal.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/utsname.h>
#include <sys/mac_label.h>
#include <netinet/in.h>
#include <sys/flock.h>
#include <sys/atomic_ops.h>
#include <ksys/vfile.h>
#include <sys/cmn_err.h>

#include "xdr.h"
#include "auth.h"
#include "clnt.h"

#include "lockmgr.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "nlm_rpc.h"
#include "nlm_debug.h"


int nlm1_to_errno[] = {
	0,			/* nlm_granted */
	EACCES,		/* nlm_denied */
	ENOLCK,		/* nlm_denied_nolocks */
	EACCES,		/* nlm_blocked */
	EAGAIN,		/* nlm_denied_grace_period */
	EDEADLK		/* nlm_deadlck */
};

int nlm4_to_errno[] = {
	0,			/* NLM4_GRANTED */
	EACCES,		/* NLM4_DENIED */
	ENOLCK,		/* NLM4_DENIED_NOLOCKS */
	EACCES,		/* NLM4_BLOCKED */
	EAGAIN,		/* NLM4_DENIED_GRACE_PERIOD */
	EDEADLK,	/* NLM4_DEADLCK */
	EROFS,		/* NLM4_ROFS */
	ESTALE,		/* NLM4_STALE_FH */
	EFBIG,		/* NLM4_FBIG */
	EINVAL		/* NLM4_FAILED */
};

extern int GraceWaitTime;	/* grace period wait time in seconds */

sema_t nlm_grace_wait;

/*
 * strictly increasing lock transaction ID
 */
long Cookie = 0;

/*
 * status monitor directory vnode, name and lock
 */
vnode_t *Statmonvp = NULL;
char *Statmon_dir = SM_STATDIR"/"SM_CUR;
lock_t Statmonvp_lock;

/*
 * Send a cancel message and wait for the reply.
 */
/* ARGSUSED */
int
send_nlm_cancel(lockreq_t *lrp, int vers, cred_t *cred, long cookie,
	int reclaim, int timeo)
{
	int		s;
	k_sigset_t	set;
	u_int		oldsig = 0;
	int		error = 0;
	int		retry = 0;
	nlm4_stats	status;
	uthread_t	*ut = curuthread;
	nlm_lock	alock1;
	nlm4_lock	alock4;

	NLM_DEBUG(NLMDEBUG_CANCEL,
		printf("send_nlm_cancel: cancelling %s, pid %d, cookie %s\n",
			nlmproc_to_str(vers, lrp->lr_proc), current_pid(),
			netobj_to_str((netobj *)&lrp->lr_args_u)));
	/*
	 * This bit of code is important.  We need to save the current
	 * signals and clear ut_sig so that the cancel can be sent and
	 * its reply received.
	 */
	sigemptyset(&set);
	s = ut_lock(ut);
	set = ut->ut_sig;
	sigdiffset(&set, ut->ut_sighold);
	if (!sigisempty(&set)) {    /* check used by issig() */
		set = ut->ut_sig;
		sigemptyset(&ut->ut_sig);
	}
	oldsig = ut->ut_cursig;
	ut->ut_cursig = 0;
	ut_unlock(ut, s);
	do {
		/*
		 * Issue the RPC call.
		 * Only when calls time out do we want to tell nlm_rpc_call that
		 * the call is being retried.  This will cause nlm_rpc_call to
		 * ask the server for the port number.
		 */
		error = nlm_rpc_call(lrp, vers, cred, error == ETIMEDOUT, 0);
		retry++;
		switch (error) {
		case 0:
			if (lrp->nlm4_reply.statreply.stat.stat == NLM4_DENIED) {
				if (lrp->lr_proc == NLM_UNLOCK || 
				    lrp->lr_proc == NLMPROC_UNLOCK) {
					retry = 0;
					break;
				}
				switch (vers) {
				case NLM_VERS: 
					cookie = atomicAddLong(&Cookie, 1);
					lrp->lr_proc = NLM_UNLOCK; 
					alock1 = lrp->nlm1_args.lockargs.alock;
					lrp->nlm1_args.unlockargs.alock = alock1;  
					lrp->nlm1_args.unlockargs.cookie.n_len = sizeof(cookie);
					lrp->nlm1_args.unlockargs.cookie.n_bytes = (char *)&cookie;
					lrp->lr_xdrargs = (xdrproc_t)xdr_nlm_unlockargs;   
					break;
				case NLM4_VERS: 
					cookie = atomicAddLong(&Cookie, 1);
					lrp->lr_proc = NLMPROC_UNLOCK; 
					alock4 = lrp->nlm4_args.lockargs.alock;
					lrp->nlm4_args.unlockargs.alock = alock4;  
					lrp->nlm4_args.unlockargs.cookie.n_len = sizeof(cookie);
					lrp->nlm4_args.unlockargs.cookie.n_bytes = (char *)&cookie;
					lrp->lr_xdrargs = (xdrproc_t)xdr_nlm4_unlockargs;   
					break;
				default:
					retry = 0;
					break;
				}
			} else {
				retry = 0;
			}
			break;
		case ETIMEDOUT:
			if ((lrp->lr_proc == NLMPROC_CANCEL_MSG) ||
			    (lrp->lr_proc == NLM_CANCEL_MSG) ||
				(lrp->lr_proc == NLMPROC_UNLOCK_MSG) ||
			    (lrp->lr_proc == NLM_UNLOCK_MSG)) {
				bhv_desc_t *bdp = 
	bhv_base_unlocked(VFS_BHVHEAD(lrp->lr_handle->lh_vp->v_vfsp));
				struct mntinfo *mi = vfs_bhvtomi(bdp);
				/*
				 * Wait for the response with no timeout.
				 */
				error = await_nlm_response(lrp->lr_res_wait,
					&mi->mi_lmaddr.sin_addr,
					NULL, &status, 0);
				switch (error) {
				case 0:
					switch (status) {
					case NLM4_GRANTED:
						retry = 0;
						break;
					case NLM4_DENIED_GRACE_PERIOD:
						if (!reclaim)
							retry = 0;
						break;
					case NLM4_DENIED:
						if (lrp->lr_proc == NLM_UNLOCK_MSG || 
							lrp->lr_proc == NLMPROC_UNLOCK_MSG) {
							retry = 0;
							break;
						}
						switch (vers) {
						case NLM_VERS: 
							cookie = atomicAddLong(&Cookie, 1);
							lrp->lr_proc = NLM_UNLOCK_MSG; 
							alock1 = lrp->nlm1_args.lockargs.alock;
							lrp->nlm1_args.unlockargs.alock = alock1;  
							lrp->nlm1_args.unlockargs.cookie.n_len = 
								sizeof(cookie);
							lrp->nlm1_args.unlockargs.cookie.n_bytes = 
								(char *)&cookie;
							lrp->lr_xdrargs = (xdrproc_t)xdr_nlm_unlockargs;   
							break;
						case NLM4_VERS: 
							cookie = atomicAddLong(&Cookie, 1);
							lrp->lr_proc = NLMPROC_UNLOCK_MSG; 
							alock4 = lrp->nlm4_args.lockargs.alock;
							lrp->nlm4_args.unlockargs.alock = alock4;  
							lrp->nlm4_args.unlockargs.cookie.n_len = 
								sizeof(cookie);
							lrp->nlm4_args.unlockargs.cookie.n_bytes = 
								(char *)&cookie;
							lrp->lr_xdrargs = (xdrproc_t)xdr_nlm4_unlockargs;   
							break;
						default:
							retry = 0;
							break;
						}
						break;
					default:
						error = nlm4_to_errno[status];
					}
					break;
				case EINTR:
					s = ut_lock(ut);
					sigorset(&set, &ut->ut_sig);
					sigemptyset(&ut->ut_sig);
					ut_unlock(ut, s);
					if (!oldsig)
						oldsig = ut->ut_cursig;
					ut->ut_cursig = 0;
					error = 0;
					break;
				case ETIMEDOUT:
					/*
					 * The only way await_nlm_response will
					 * time out is if SM_NOTIFY has been
					 * received.  nfs_notify will time out
					 * the request.
					 */
					retry = 0;
					error = 0;
					break;
				default:
					NLM_DEBUG(NLMDEBUG_TRACE,
						printf("send_nlm_cancel: pid %d, "
							"await_nlm_response error %d\n",
							(int)current_pid(), error));
					break;
				}
			} else {
				ASSERT(lrp->lr_handle->lh_timeo);
			}
			break;
		case EINTR:
			/*
			 * Even though we told nlm_rpc_call not to take signals,
			 * it may still take them if it gets one while talking
			 * to the port mapper.
			 */
			s = ut_lock(ut);
			sigorset(&set, &ut->ut_sig);
			sigemptyset(&ut->ut_sig);
			ut_unlock(ut, s);
			if (!oldsig)
				oldsig = ut->ut_cursig;
			ut->ut_cursig = 0;
			error = 0;
			retry = 1;
			break;
		default:
			ASSERT(error != EINTR);
			NLM_DEBUG(NLMDEBUG_TRACE,
				printf("send_nlm_cancel: pid %d, nlm_rpc_call error %d\n",
					(int)current_pid(), error));
			break;
		}
	} while (retry);

	/*
	 * restore the signal that got us here in the first place
	 */
	s = ut_lock(ut);
	if (!sigisempty(&set)) {
		sigorset(&ut->ut_sig, &set);
	}
	/*
	 * restore the old signal if there is one to restore
	 */
	if (oldsig)
		ut->ut_cursig = oldsig;
	ut_unlock(ut, s);

	if (issig(0, SIG_NOSTOPDEFAULT))
		error = EINTR;
	return(error);
}

static int
nlm1_call(register lockhandle_t *lh, register struct flock *ld, int cmd,
	struct cred *cred, int reclaim)
{
#ifdef DEBUG
	int					err;
#endif /* DEBUG */
	nlm_stats			status;
	toid_t				id;
	long				cookie;
	int					error = 0;
	bhv_desc_t 			*bdp = 
			bhv_base_unlocked(VFS_BHVHEAD((lh->lh_vp)->v_vfsp));
	mntinfo_t 			*mi = vfs_bhvtomi(bdp);
	lockreq_t			lock_request;
	union nlm1_reply_u	*replyp = &lock_request.nlm1_reply;

	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CALL,
		printf("NLM call: vnode 0x%p pid %d %s offset %lld len %lld %s\n",
			lh->lh_vp, (int)ld->l_pid, lh->lh_servername, ld->l_start,
			ld->l_len, locktype_to_str(ld->l_type)));
	/*
	 * we assume that the pid and sysid fields of the lock data have been
	 * set to the correct values
	 */
	cookie = atomicAddLong(&Cookie, 1);
	lock_request.lr_handle = lh;
	lock_request.lr_proc = 0;
	lock_request.lr_res_wait = lock_request.lr_blocked_wait = NULL;
	error = nlm1_call_setup(cmd, &lock_request, ld, &cookie, reclaim, 0);
	if (error) {
		goto nlm1_ereturn;
	}
	/*
	 * issue the NLM RPC call
	 */
callit:
	error = nlm_rpc_call(&lock_request, NLM_VERS, cred, 0, !!(mi->mi_flags & MIF_INT));
	switch (error) {
	default:
		break;
	case EAGAIN:
		/*
		 * This call failed with a system error from the remote.  Don't
		 * try it again.  Fall back to the asynchronous locking through
		 * the local lockd.
		 * If the proc which failed was anything other than NLM_TEST, we
		 * will switch to asynchronous requests.
		 */
		if (lock_request.lr_proc == NLM_TEST) {
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf(
					"nlm1_call: proc %d failed, fallback to async NLM_TEST\n",
					lock_request.lr_proc));
			bitlock_set(&mi->mi_nlm, MI_NLM_LOCK, MI_NLMASYNC_TEST);
		} else {
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm1_call: proc %d failed, fallback to async NLM\n",
					lock_request.lr_proc));
			bitlock_clr(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM_SYNC);
		}
		break;
	case EINTR:
	case ETIMEDOUT:
		if (lock_request.lr_proc == NLM_LOCK) {
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm1_call: NLM_LOCK error %d\n", error));
			/*
			 * RPC call error, so cancel the request just in case the server
			 * got it
			 * reset the object handle first, just in case the reply was
			 * received and overwrote it
			 */
			cookie = atomicAddLong(&Cookie, 1);
			if (lock_request.lr_blocked_wait) {
				release_nlm_wait(lock_request.lr_blocked_wait);
				lock_request.lr_blocked_wait = NULL;
			}
			ASSERT(!lock_request.lr_res_wait);
			lock_request.lr_proc = NLM_CANCEL;
			lock_request.lr_xdrargs = xdr_nlm_cancargs;
#ifdef DEBUG
			err = send_nlm_cancel(&lock_request, NLM_VERS, cred, cookie,
				reclaim, 0);
			NLM_DEBUG(NLMDEBUG_ERROR,
				if (err && (err != EINTR))
					printf("nlm1_call: cancel error %d\n", err));
#else /* DEBUG */
			(void)send_nlm_cancel(&lock_request, NLM_VERS, cred, cookie,
				reclaim, 0);
#endif /* DEBUG */
		}
		break;
	case 0:
		switch (lock_request.lr_proc) {
		case NLM_LOCK:
			NLM_DEBUG(NLMDEBUG_TRACE,
				printf(
					"NLM NLM_LOCK: pid %d, status %s off %lld len %lld "
					"cookie %s\n", (int)current_pid(),
					nlmstats_to_str(replyp->statreply.stat.stat),
					ld->l_start, ld->l_len,
					netobj_to_str(&replyp->statreply.cookie)));
			/*
			 * Here, it is necessary to add the lock to the list of
			 * those which are held.  This is only so that these may
			 * be reclaimed after the server crashes.  We must be able
			 * to remove the entry when an unlock occurs.
			 * We may want to do this at a higher level, taking advantage
			 * of the file lock list attached to the vnode.
			 * This would mean that recovery takes place at a higher
			 * level as well.
			 */
			switch (replyp->statreply.stat.stat) {
			case nlm_granted:
				error = 0;
				break;
			case nlm_denied_grace_period:
				if (reclaim) {
					error = EPROTO;
					break;
				}
				/*
				 * sleep for a bit and try again
				 */
				id = cputimeout(nlm_grace_wakeup, &nlm_grace_wait,
					GraceWaitTime * HZ, curthreadp);
				if (psema(&nlm_grace_wait,
					(mi->mi_flags & MIF_INT) ? (PZERO + 1) : PRIBIO)) {
						ASSERT(mi->mi_flags & MIF_INT);
						untimeout(id);
						error = EINTR;
						goto nlm1_ereturn;
				}
				goto callit;
			case nlm_blocked:
				/*
				 * If the server sent us a blocked reply when we were
				 * not expecting one, return EPROTO.
				 */
				if (!lock_request.lr_blocked_wait || reclaim) {
					cmn_err(CE_WARN,
						"NLM protocol error: server %s replied with "
						"nlm_blocked to a non-blocking request\n",
						lh->lh_servername);
					error = EPROTO;
					break;
				}
				/*
				 * servers which reply with nlm_blocked
				 * are expected to reply asynchronously
				 * If the server does not respond, it must have
				 * crashed.  In that case, nfs_notify will call
				 * wake_nlm_requests to timeout all requests for
				 * the recovering server.
				 * This is done this way because some servers
				 * will reply with nlm_blocked and cannot accept
				 * a retransmission of the reuest.
				 */
				error = await_nlm_response(lock_request.lr_blocked_wait,
					&mi->mi_lmaddr.sin_addr, NULL,
					(nlm4_stats *)&status, 0);
				switch (error) {
				case 0:
					/*
					 * reply received
					 * Translate the NLM status to
					 * an error number.
					 */
					error = nlm1_to_errno[status];
					break;
				case EINTR:
					/*
					 * signalled, so we must cancel
					 * the request
					 */
					NLM_DEBUG(NLMDEBUG_CANCEL,
						printf("nlm1_call: signal during "
							"blocked lock, cancelling\n"));
					cookie = atomicAddLong(&Cookie, 1);
					release_nlm_wait(lock_request.lr_blocked_wait);
					lock_request.lr_blocked_wait = NULL;
					lock_request.lr_proc = NLM_CANCEL;
					lock_request.lr_xdrargs = xdr_nlm_cancargs;
					error = send_nlm_cancel(&lock_request,
						NLM_VERS, cred, cookie, reclaim,
						0);
					error = EINTR;
					break;
				case ETIMEDOUT:
					/*
					 * timed out, so retry
					 */
					NLM_DEBUG(NLMDEBUG_TRACE,
						printf("NLM NLM_LOCK: pid %d, wait timed "
							"out, retransmitting blocked request\n",
							(int)current_pid()));
					goto callit;
				default:
					NLM_DEBUG(NLMDEBUG_TRACE,
						printf("NLM NLM_LOCK: pid %d, "
							"await_nlm_response error %d\n",
							(int)current_pid(), error));
					break;
				}
				break;
			default:
				/*
				 * Translate the NLM status to an error number.
				 */
				error = nlm1_to_errno[(int)replyp->statreply.stat.stat];
			}
			break;
		case NLM_TEST:
			NLM_DEBUG(NLMDEBUG_TRACE,
				printf("NLM NLM_TEST: pid %d, status %s cookie %s\n",
					(int)current_pid(),
					nlmstats_to_str(replyp->testreply.stat.stat),
					netobj_to_str(&replyp->testreply.cookie)));
			switch (replyp->testreply.stat.stat) {
			case nlm_denied:
			case nlm_granted:
				error = 0;
				nlm_testrply_to_flock(&replyp->testreply.stat, ld);
				NLM_DEBUG(NLMDEBUG_TRACE,
					printf("NLM NLM_TEST: %s (%llx, %llx)\n",
						locktype_to_str(ld->l_type),
						ld->l_start, ld->l_len));
				break;
			case nlm_denied_grace_period:
				/*
				 * sleep for a bit and try again
				 */
				id = cputimeout(nlm_grace_wakeup, &nlm_grace_wait,
					GraceWaitTime * HZ, curthreadp);
				if (psema(&nlm_grace_wait,
					(mi->mi_flags & MIF_INT) ? (PZERO + 1) : PRIBIO)) {
						ASSERT(mi->mi_flags & MIF_INT);
						untimeout(id);
						error = EINTR;
						goto nlm1_ereturn;
				}
				goto callit;
			default:
				/*
				 * Translate the NLM status to an error number.
				 */
				error =
					nlm1_to_errno[(int)replyp->testreply.stat.stat];
				NLM_DEBUG(NLMDEBUG_TRACE,
					printf("nlm1_call: test error %d from server\n",
						error));
			}
			break;
		case NLM_UNLOCK:
			/*
			 * An unlock only returns a status.  Translate it
			 * to an error number.
			 */
			error = nlm1_to_errno[(int)replyp->statreply.stat.stat];
			NLM_DEBUG(NLMDEBUG_TRACE,
				printf("NLM NLM_UNLOCK: pid %d, status %s, error %d, "
					"cookie %s\n", (int)current_pid(),
					nlmstats_to_str(replyp->statreply.stat.stat),
					error, netobj_to_str(&replyp->statreply.cookie)));
		}
	}
nlm1_ereturn:
	if (lock_request.lr_blocked_wait) {
		release_nlm_wait(lock_request.lr_blocked_wait);
#ifdef DEBUG
		lock_request.lr_blocked_wait = NULL;
#endif /* DEBUG */
	}
	nlm1_freeargs(lock_request.lr_proc, &lock_request.nlm1_args);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("NLM call: pid %d %s offset %lld len %lld %s return %d\n",
			(int)current_pid(), lh->lh_servername, ld->l_start, ld->l_len,
			locktype_to_str(ld->l_type), error));
	ASSERT(!issplhi(getsr()));
	ASSERT(!lock_request.lr_blocked_wait && !lock_request.lr_res_wait);
	return(error);
}

static int
nlm4_call(register lockhandle_t *lh, register struct flock *ld, int cmd,
	struct cred *cred, int reclaim)
{
#ifdef DEBUG
	int					err;
#endif /* DEBUG */
	nlm4_stats			status;
	toid_t				id;
	long				cookie;
	int					error = 0;
	bhv_desc_t 			*bdp = 
			bhv_base_unlocked(VFS_BHVHEAD((lh->lh_vp)->v_vfsp)); 
	mntinfo_t 			*mi = vfs_bhvtomi(bdp);
	lockreq_t			lock_request;
	union nlm4_reply_u	*replyp = &lock_request.nlm4_reply;

	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CALL,
		printf("NLM4 call: pid %d %s offset %lld len %lld %s\n",
			(int)current_pid(), lh->lh_servername, ld->l_start, ld->l_len,
			locktype_to_str(ld->l_type)));
	/*
	 * set up the call
	 */
	cookie = atomicAddLong(&Cookie, 1);
	lock_request.lr_handle = lh;
	lock_request.lr_proc = 0;
	lock_request.lr_blocked_wait = lock_request.lr_res_wait = NULL;
	error = nlm4_call_setup(cmd, &lock_request, ld, &cookie, reclaim, 0);
	if (error) {
		goto nlm4_ereturn;
	}
	/*
	 * do the rpc call
	 */
callit:
	error = nlm_rpc_call(&lock_request, NLM4_VERS, cred, 0, !!(mi->mi_flags & MIF_INT));
	/*
	 * if no error, prcess the reply
	 */
	switch (error) {
	default:
		break;
	case EAGAIN:
		/*
		 * This call failed with a system error from the remote.  Don't
		 * try it again.  Fall back to the asynchronous locking through
		 * the local lockd.
		 */
		if (lock_request.lr_proc == NLMPROC_TEST) {
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm4_call: proc %d failed, fallback to async "
					"NLMPROC_TEST\n", lock_request.lr_proc));
			bitlock_set(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM4ASYNC_TEST);
		} else {
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf(
					"nlm4_call: proc %d failed, fallback to async NLM calls\n",
					lock_request.lr_proc));
			bitlock_clr(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM4_SYNC);
		}
		break;
	case EINTR:
	case ETIMEDOUT:
		if (lock_request.lr_proc == NLMPROC_LOCK) {
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm4_call: NLMPROC_LOCK error %d\n", error));
			/*
			 * RPC call error, so cancel the request just in case the server
			 * got it
			 */
			cookie = atomicAddLong(&Cookie, 1);
			if (lock_request.lr_blocked_wait) {
				release_nlm_wait(lock_request.lr_blocked_wait);
				lock_request.lr_blocked_wait = NULL;
			}
			lock_request.lr_proc = NLMPROC_CANCEL;
			lock_request.lr_xdrargs = xdr_nlm4_cancargs;
#ifdef DEBUG
			err = send_nlm_cancel(&lock_request, NLM4_VERS, cred, cookie,
				reclaim, 0);
			NLM_DEBUG(NLMDEBUG_ERROR,
				if (err && (err != EINTR))
					printf("nlm4_call: cancel error %d\n", err));
#else /* DEBUG */
			(void)send_nlm_cancel(&lock_request, NLM4_VERS, cred, cookie,
				reclaim, 0);
#endif /* DEBUG */
		}
		break;
	case 0:
		switch (lock_request.lr_proc) {
		case NLMPROC_LOCK:
			NLM_DEBUG(NLMDEBUG_TRACE,
				printf("NLM4 NLMPROC_LOCK: pid %d, status %s, cookie %s\n",
					(int)current_pid(),
					nlm4stats_to_str(replyp->statreply.stat.stat),
					netobj_to_str(&replyp->statreply.cookie)));
			/*
			 * Here, it is necessary to add the lock to the list of
			 * those which are held.  This is only so that these may
			 * be reclaimed after the server crashes.  We must be
			 * able to remove the entry when an unlock occurs.
			 * We may want to do this at a higher level, taking
			 * advantage of the file lock list attached to the
			 * vnode.  This would mean that recovery takes place at
			 * a higher level as well.
			 */
			switch (replyp->statreply.stat.stat) {
			case NLM4_GRANTED:
				error = 0;
				break;
			case NLM4_DENIED_GRACE_PERIOD:
				if (reclaim) {
					error = EPROTO;
					break;
				}
				/*
				 * sleep for a bit and try again
				 */
				id = cputimeout(nlm_grace_wakeup, &nlm_grace_wait,
					GraceWaitTime * HZ, curthreadp);
				if (psema(&nlm_grace_wait,
					(mi->mi_flags & MIF_INT) ? (PZERO + 1) : PRIBIO)) {
						ASSERT(mi->mi_flags & MIF_INT);
						untimeout(id);
						error = EINTR;
						goto nlm4_ereturn;
				}
				goto callit;
			case NLM4_BLOCKED:
				/*
				 * If the server sent us a blocked reply when we were
				 * not expecting one, return EPROTO.
				 */
				if (!lock_request.lr_blocked_wait || reclaim) {
					cmn_err(CE_WARN,
						"NLM4 protocol error: server %s replied with "
						"NLM4_BLOCKED to a non-blocking request\n",
						lh->lh_servername);
					error = EPROTO;
					break;
				}
				/*
				 * servers which reply with nlm_blocked are expected
				 * to reply asynchronously
				 * if they don't, we just time out and retry anyway
				 */
				error = await_nlm_response(lock_request.lr_blocked_wait,
					&mi->mi_lmaddr.sin_addr, NULL, &status, 0);
				switch (error) {
				case 0:
					/*
					 * reply received
					 * Translate the NLM status to
					 * an error number.
					 */
					error = nlm4_to_errno[status];
					break;
				case EINTR:
					/*
					 * signalled, so we must cancel
					 * the request
					 */
					NLM_DEBUG(NLMDEBUG_CANCEL,
						printf("nlm4_call: signal during "
							"blocked lock, cancelling\n"));
					cookie = atomicAddLong(&Cookie, 1);
					release_nlm_wait(lock_request.lr_blocked_wait);
					lock_request.lr_blocked_wait = NULL;
					lock_request.lr_proc = NLMPROC_CANCEL;
					lock_request.lr_xdrargs = xdr_nlm4_cancargs;
					error = send_nlm_cancel(&lock_request,
						NLM4_VERS, cred, cookie, reclaim,
						0);
					error = EINTR;
					break;
				case ETIMEDOUT:
					NLM_DEBUG(NLMDEBUG_TRACE,
						printf("NLM4 NLMPROC_LOCK: pid %d, "
							"retransmitting blocked request\n",
							(int)current_pid()));
					goto callit;
				default:
					NLM_DEBUG(NLMDEBUG_TRACE,
						printf("NLM4 NLMPROC_LOCK: pid %d, "
							"await_nlm_response error %d\n",
							(int)current_pid(), error));
					break;
				}
				break;
			default:
				/*
				 * Translate the NLM status to an error number.
				 */
				error =
					nlm4_to_errno[(int)replyp->statreply.stat.stat];
			}
			break;
		case NLMPROC_TEST:
			NLM_DEBUG(NLMDEBUG_TRACE,
				printf("NLM4 NLMPROC_TEST: pid %d, status %s, cookie %s\n",
					(int)current_pid(),
					nlm4stats_to_str(replyp->testreply.stat.stat),
					netobj_to_str(&replyp->testreply.cookie)));
			switch (replyp->testreply.stat.stat) {
			case NLM4_DENIED:
				/*
				 * lock held, get the holder information
				 */
			case NLM4_GRANTED:
				error = 0;
				nlm4_testrply_to_flock(&replyp->testreply.stat, ld);
				NLM_DEBUG(NLMDEBUG_TRACE,
					printf("NLM4 NLMPROC_TEST: %s (%llx, %llx)\n",
						locktype_to_str(ld->l_type),
						ld->l_start, ld->l_len));
				break;
			case NLM4_DENIED_GRACE_PERIOD:
				/*
				 * sleep for a bit and try again
				 */
				id = cputimeout(nlm_grace_wakeup, &nlm_grace_wait,
					GraceWaitTime * HZ, curthreadp);
				if (psema(&nlm_grace_wait,
					(mi->mi_flags & MIF_INT) ? (PZERO + 1) : PRIBIO)) {
						ASSERT(mi->mi_flags & MIF_INT);
						untimeout(id);
						error = EINTR;
						goto nlm4_ereturn;
				}
				goto callit;
			default:
				/*
				 * Translate the NLM4 status to an error number.
				 */
				error =
					nlm4_to_errno[(int)replyp->testreply.stat.stat];
				NLM_DEBUG(NLMDEBUG_TRACE,
					printf("nlm4_call: test error %d from server\n",
						error));
			}
			break;
		case NLMPROC_UNLOCK:
			/*
			 * An unlock only returns a status.  Translate it
			 * to an error number.
			 */
			error = nlm4_to_errno[(int)replyp->statreply.stat.stat];
			NLM_DEBUG(NLMDEBUG_TRACE,
				printf("NLM4 NLMPROC_UNLOCK: pid %d, status %s, error %d, "
					"cookie %s\n", (int)current_pid(),
					nlm4stats_to_str(replyp->statreply.stat.stat),
					error, netobj_to_str(&replyp->statreply.cookie)));
		}
	}
nlm4_ereturn:
	if (lock_request.lr_blocked_wait) {
		release_nlm_wait(lock_request.lr_blocked_wait);
#ifdef DEBUG
		lock_request.lr_blocked_wait = NULL;
#endif /* DEBUG */
	}
	nlm4_freeargs(lock_request.lr_proc, &lock_request.nlm4_args);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("NLM4 call: pid %d %s offset %lld len %lld %s error %d\n",
			(int)current_pid(), lh->lh_servername, ld->l_start, ld->l_len,
			locktype_to_str(ld->l_type), error));
	NLM_DEBUG(NLMDEBUG_ERROR,
		if (error)
			printf("NLM4 call: pid %d %s offset %lld len %lld %s error %d\n",
				(int)current_pid(), lh->lh_servername, ld->l_start, ld->l_len,
				locktype_to_str(ld->l_type), error));
	ASSERT(!issplhi(getsr()));
	ASSERT(!lock_request.lr_blocked_wait && !lock_request.lr_res_wait);
	return(error);
}

void
klm_init(void)
{
	static klm_initialized = 0;

	if (atomicAddInt(&klm_initialized, 1) == 1) {
		initnsema(&nlm_grace_wait, 0, "NLM grace wait");
		spinlock_init(&Statmonvp_lock, "Statmonvp lock");
	}
}

void
klm_shutdown(void)
{
	vnode_t *vp;
	int ospl;

	ospl = mutex_spinlock(&Statmonvp_lock);
	if (Statmonvp) {
		vp = Statmonvp;
		Statmonvp = NULL;
		mutex_spinunlock(&Statmonvp_lock, ospl);
		VN_RELE(vp);
	} else {
		mutex_spinunlock(&Statmonvp_lock, ospl);
	}
}

struct killa {
	sysarg_t pid;
	sysarg_t signo;
};

extern int kill(struct killa *);

/*
 * reclaim the locks for a specified mount point
 */
static int
reclaim_locks(mntinfo_t *mi)
{
	struct flock fl;
	struct killa killargs;
	struct vnode *vp;
	struct rnode *rp;
	struct filock *flp;
	lockhandle_t lh;
	int error = 0;
	vmap_t vmap;

	NLM_DEBUG(NLMDEBUG_NOTIFY,
		printf("reclaim_locks: host %s\n", mi->mi_hostname));
	/*
	 * traverse all of the vnodes on the mount point, recovering all of
	 * the locks
	 * it is assumed that the caller has blocked out umount
	 */
	milock_mp(mi);
	for (rp = mi->mi_rnodes; rp != 0; rp = rp->r_mnext) {
		rclrflag(rp, RRECLAIMED);
	}
again:
	for (rp = mi->mi_rnodes; rp != 0; rp = rp->r_mnext) {
		/*
		 * ignore rnodes without vnodes and vnodes with a reference count
		 * of 0
		 */
		if ((vp = rtov(rp)) == NULL)
			continue;
		if (vp->v_count == 0) {
			ASSERT(!vp->v_filocks);
			continue;
		}
		if (rp->r_flags & RRECLAIMED)
			continue;
		VMAP(vp, vmap);
		miunlock_mp(mi);
		/*
		 * get a reference to the vnode
		 * if this fails, start over
		 */
		if (vp = vn_get(vp, &vmap, 0)) {
			/*
			 * do the reclaim
			 * traverse the list of file locks for this vnode and reclaim
			 * them
			 */
			lh.lh_vp = vp;
			lh.lh_servername = mi->mi_hostname;
			lh.lh_timeo = mi->mi_timeo;
			mp_mutex_lock(&vp->v_filocksem, PZERO);
			switch (mi->mi_nlmvers) {
			case NLM_VERS:
				lh.lh_id.n_bytes = (char *)rtofh(rp);
				lh.lh_id.n_len = sizeof (fhandle_t);
				for (flp = vp->v_filocks; flp; flp = flp->next) {
					fl = flp->set;
					/*
					 * undo the adjustments made by reclock
					 */
					if (fl.l_end == MAXEND)
						fl.l_len = 0;
					else
						fl.l_len -= (fl.l_start-1);
					error = nlm1_call(&lh, &fl, F_SETLK, sys_cred, 1);
					if (error) {
						NLM_DEBUG(NLMDEBUG_NOTIFY,
							printf("reclaim_locks: reclaim failure, "
								"pid %d, NLM vers %d, error %d\n",
								flp->set.l_pid, mi->mi_nlmvers, error));
						killargs.pid = flp->set.l_pid;
						killargs.signo = SIGUSR1;
						kill(&killargs);
					}
				}
				break;
			case NLM4_VERS:
				lh.lh_id.n_bytes = ((nfs_fhandle *)rtofh(rp))->fh_buf;
				lh.lh_id.n_len = ((nfs_fhandle *)rtofh(rp))->fh_len;
				for (flp = vp->v_filocks; flp; flp = flp->next) {
					fl = flp->set;
					/*
					 * undo the adjustments made by reclock
					 */
					if (fl.l_end == MAXEND)
						fl.l_len = 0;
					else
						fl.l_len -= (fl.l_start-1);
					error = nlm4_call(&lh, &fl, F_SETLK, sys_cred, 1);
					if (error) {
						NLM_DEBUG(NLMDEBUG_NOTIFY,
							printf("reclaim_locks: reclaim failure, "
								"pid %d, NLM vers %d, error %d\n",
								flp->set.l_pid, mi->mi_nlmvers, error));
						killargs.pid = flp->set.l_pid;
						killargs.signo = SIGUSR1;
						kill(&killargs);
					}
				}
				break;
			default:
				cmn_err(CE_ALERT,
					"reclaim_locks: invalid NLM version %d, server %s, "
					"NFS dev 0x%x\n", mi->mi_nlmvers, mi->mi_hostname,
					vp->v_vfsp->vfs_fsid.val[0]);
				for (flp = vp->v_filocks; flp; flp = flp->next) {
					killargs.pid = flp->set.l_pid;
					killargs.signo = SIGUSR1;
					kill(&killargs);
				}
			}
			mp_mutex_unlock(&vp->v_filocksem);
			VN_RELE(vp);
		}
		/*
		 * always mark the rnode as having its file locks reclaimed so
		 * we do not get into infinite loops
		 */
		rsetflag(rp, RRECLAIMED);
		milock_mp(mi);
		goto again;
	}
	miunlock_mp(mi);
	return(0);
}

extern lock_t mntinfolist_lock;

extern mntinfo_t *mntinfolist;

/*
 * This is called by syssgi for the command SGI_NFSNOTIFY.  It is used
 * when the status monitor has been notified that a remote system (either
 * client or server) has crashed and restarted.  It does not matter to
 * us whether it was a client or a server.  It may have been both.  We
 * do not need to know.
 *
 * For the given IP address, first call lockd_cleanlocks, passing it the
 * IP address.  This function will do the necessary cleanup if the IP
 * address happens to belong to a client.  The next thing to do is to
 * walk through the list of mounted NFS file systems (mntinfolist) looking
 * for mount points with an IP address (mi_addr) matching the one we were
 * given.  When a match is found, we recover any locks we might hold.
 */
/* ARGSUSED */
int
nfs_notify(char *addr, int addrlen, rval_t *rvp)
{
	mntinfo_t *mi;
	int ospl;
	int error = 0;
	struct in_addr hostaddr;
	struct sockaddr_in lmaddr;

	/*
	 * clear the return value
	 */
	if (rvp) {
		rvp->r_v.r_v1 = rvp->r_v.r_v2 = 0;
	}
	/*
	 * make sure we have the correct permissions
	 */
	if (!_CAP_ABLE(CAP_DAC_OVERRIDE) || !_CAP_ABLE(CAP_MAC_READ) ||
		!_CAP_ABLE(CAP_MAC_WRITE) || !_CAP_ABLE(CAP_NETWORK_MGT)) {
			return(EPERM);
	}
	/*
	 * copy in the ip address
	 */
	error = copyin(addr, &hostaddr, addrlen);
	if (error) {
		NLM_DEBUG(NLMDEBUG_NOTIFY, printf("nfs_notify: copyin error\n"));
		return(error);
	}
	NLM_DEBUG(NLMDEBUG_NOTIFY,
		printf("nfs_notify: notification for IP addr %s\n",
			inet_ntoa(hostaddr)));
	/*
	 * Server cleanup first.
	 *
	 * Clean up any locks held by the client.  Also clean up blocked
	 * lock requests.
	 */
	lockd_cleanlocks(hostaddr.s_addr);
	/*
	 * Reload the cached address for the client's lock daemon.
	 * If the client's daemon is not yet up, any cached address will
	 * be flushed.
	 */
	(void)get_address(NLM_VERS, hostaddr.s_addr, &lmaddr, TRUE);
	/*
	 * Flush out all nlm duplicate request cache entries for this IP
	 * address.
	 */
	nlm_dup_flush(hostaddr.s_addr);
	/*
	 * Now for the client cleanup.
	 * First, reclaim any locks held by local processes.
	 * Next wake up blocked lock requests waiting for granted messages.
	 */
	ospl = mutex_spinlock(&mntinfolist_lock);
	/*
	 * clear all of the reclaim indicators
	 */
	for (mi = mntinfolist; mi; mi = mi->mi_next) {
		bitlock_clr(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM_RECLAIM);
	}
	/*
	 * traverse the list again
	 * for each mount point which has not been reclaimed, mark it as reclaimed
	 * if the mount point server name matches the one for which locks are
	 * to be reclaimed, reclaim the locks
	 */
	mi = mntinfolist;
	while (mi) {
		if (!(mi->mi_nlm & (MI_NLM_RECLAIM | MI_NLM_RECLAIMING)) &&
			(hostaddr.s_addr == mi->mi_addr.sin_addr.s_addr)) {
				/*
				 * indicate that lock reclaiming is being done for this
				 * mount point
				 * this is to keep it from being unmounted
				 */
				bitlock_set(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM_RECLAIMING);
				mutex_spinunlock(&mntinfolist_lock, ospl);
				/*
				 * relciam the locks for this mount point
				 * then unlock the vfs
				 */
				error = reclaim_locks(mi);
				bitlock_set(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM_RECLAIM);
				bitlock_clr(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM_RECLAIMING);
				/*
				 * lock the mount list and start at the beginning again
				 */
				(void)mutex_spinlock(&mntinfolist_lock);
				mi = mntinfolist;
		} else {
			/*
			 * on to the next mount point
			 */
			mi = mi->mi_next;
		}
	}
	mutex_spinunlock(&mntinfolist_lock, ospl);
	/*
	 * Wake up any lock requests waiting for reply from this IP address.
	 */
	wake_nlm_requests(&hostaddr);
	return(0);
}

/*
 * klm_lockctl - process a lock/unlock/test-lock request
 *
 */
int
klm_lockctl(lh, ld, cmd, cred)
	register lockhandle_t *lh;
	register struct flock *ld;
	int cmd;
	struct cred *cred;
{
	bhv_desc_t 	*bdp = 
			bhv_base_unlocked(VFS_BHVHEAD((lh->lh_vp)->v_vfsp)); 
	mntinfo_t 	*mi = vfs_bhvtomi(bdp);
	register int	error;

	/*
	 * Try talking directly to the remote lock manager via the NLM
	 * protocol first.  If that fails, EAGAIN will be returned if
	 * the request should be retried with the local lock manager
	 * as a proxy.
	 */
	ASSERT(ld->l_sysid == 0);
	ASSERT(ld->l_pid == current_pid());
	switch (mi->mi_nlmvers) {
	case NLM_VERS:
		if (mi->mi_nlm & MI_NLM_SYNC) {
			error = nlm1_call(lh, ld, cmd, cred, 0);
			switch (error) {
			case ENOSYS:
				/*
				 * ENOSYS will be returned for cases the requested
				 * procedure is unavailable.  In this case, we want
				 * to discontinue use of the synchronous locking
				 * functions, switching to the asynchronous ones.
				 * Clear MI_NLM_SYNC from mi_nlm and retry the
				 * request through the local lockd.  Simply pass
				 * through to the EAGAIN case to retry.
				 */
				bitlock_clr(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM_SYNC);
			case EAGAIN:
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("klm_lockctl: fallback to async NLM\n"));
				error = nlm1_async_call(lh, ld, cmd, cred, 0);
				break;
			case ENOPKG:
				/*
				 * ENOPKG will be returned for cases where the
				 * program or version is not supported.  Return
				 * ENOLCK.
				 */
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("klm_lockctl: NLM version %d not "
						"supported on server %s\n", NLM_VERS,
						mi->mi_hostname));
			default:
				/*
				 * The default is to return the error.
				 */
				break;
			}
		} else {
			error = nlm1_async_call(lh, ld, cmd, cred, 0);
		}
		break;
	case NLM4_VERS:
		ASSERT(mi->mi_vers == NFS3_VERSION);
		if (mi->mi_nlm & MI_NLM4_SYNC) {
			error = nlm4_call(lh, ld, cmd, cred, 0);
			switch (error) {
			case ENOSYS:
				/*
				 * ENOSYS will be returned for cases
				 * the requested procedure is
				 * unavailable.  In this case, we want
				 * to discontinue attempts to communicate
				 * directly with the server lockd.
				 * Clear MI_NLM4_SYNC from mi_nlm and
				 * retry the request through the local
				 * lockd.  Simply pass through to the
				 * EAGAIN case to retry.
				 */
				bitlock_clr(&mi->mi_nlm, MI_NLM_LOCK, MI_NLM4_SYNC);
			case EAGAIN:
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("klm_lockctl: fallback to async NLM\n"));
				error = nlm4_async_call(lh, ld, cmd, cred, 0);
			case ENOPKG:
				/*
				 * ENOPKG will be returned for cases where the
				 * program or version is not supported.
				 */
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("klm_lockctl: NLM version %d not "
						"supported on server %s\n", NLM4_VERS,
						mi->mi_hostname));
			default:
				/*
				 * The default is to return the error.
				 */
				break;
			}
		} else {
			error = nlm4_async_call(lh, ld, cmd, cred, 0);
		}
		break;
	default:
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("klm_lockctl: invalid NFS version: %d\n",
				mi->mi_vers));
		error = EINVAL;
	}
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("klm_lockctl: pid %d return %d\n", current_pid(),
				error));
	ASSERT(!issplhi(getsr()));
	return(error);

}
