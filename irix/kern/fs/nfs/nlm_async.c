#include "types.h"
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/kthread.h>
#include <sys/mbuf.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <ksys/vproc.h>
#include <sys/siginfo.h>	/* for CLD_EXITED exit code */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/sema.h>
#include <sys/atomic_ops.h>
#include <sys/utsname.h>
#include <sys/cmn_err.h>
#include "auth.h"
#include "nfs.h"
#include "clnt.h"
#include "nfs_clnt.h"
#include "auth_unix.h"
#include "svc.h"
#include "xdr.h"
#include "nfs_stat.h"
#include "lockmgr.h"
#include "nlm_rpc.h"
#include "nlm_debug.h"

extern int nlm1_to_errno[];
extern int nlm4_to_errno[];
extern sema_t nlm_grace_wait;
extern int GraceWaitTime;
extern long Cookie;

/*
 * ---------------------------------------------
 * Functions to issue asyncronous NLM RPC calls.
 * ---------------------------------------------
 */

/*
 * Issue an asyncronous NLM RPC call and wait for the results.  The results
 * will come back to the server process.
 */
int
nlm1_async_call(register lockhandle_t *lh, register struct flock *ld, int cmd,
	struct cred *cred, int reclaim)
{
#ifdef DEBUG
	int					err;
#endif /* DEBUG */
	int					retry = 0;
	nlm_stats			status;
	int					timeo;
	toid_t				id;
	long				cookie;
	int					error = 0;
	bhv_desc_t 			*bdp = 
	bhv_base_unlocked(VFS_BHVHEAD(lh->lh_vp->v_vfsp)); 
	mntinfo_t 			*mi = vfs_bhvtomi(bdp);
	lockreq_t			lock_request;

	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CALL,
		printf("Async NLM call: vnode 0x%p pid %d %s offset %lld len %lld %s\n",
			lh->lh_vp, (int)ld->l_pid, lh->lh_servername, ld->l_start,
			ld->l_len, locktype_to_str(ld->l_type)));
	/*
	 * we assume that the pid and sysid fields of the lock data have been
	 * set to the correct values
	 * generate the cookie and set up the call
	 */
	cookie = atomicAddLong(&Cookie, 1);
	lock_request.lr_handle = lh;
	lock_request.lr_proc = 0;
	lock_request.lr_blocked_wait = lock_request.lr_res_wait = NULL;
	error = nlm1_call_setup(cmd, &lock_request, ld, &cookie, reclaim, 1);
	ASSERT(lock_request.lr_res_wait);
	if (error) {
		goto nlm1_ereturn;
	}
	/*
	 * Issue the NLM RPC call.
	 * This call will be asynchronous, so set the timeout to 0, but save
	 * the original value.  It will be needed when waiting for the response
	 * indication from the local server.
	 */
	timeo = lh->lh_timeo;
	lh->lh_timeo = 0;
	do {
		error = nlm_rpc_call(&lock_request, NLM_VERS, cred, retry,
				     !!(mi->mi_flags & MIF_INT));
		retry = 0;
		switch (error) {
		case EAGAIN:
			/*
			 * This call failed with a system error from the remote.  Don't
			 * try it again.
			 */
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm1_async_call: proc %d failed\n",
					lock_request.lr_proc));
			error = ENOLCK;
			break;
		case EINTR:
			if (lock_request.lr_proc == NLM_LOCK_MSG) {
				/*
				 * RPC call error, so cancel the request just in case the
				 * server got it
				 * reset the object handle first, just in case the reply was
				 * received an overwrote it
				 */
				cookie = atomicAddLong(&Cookie, 1);
				release_nlm_wait(lock_request.lr_res_wait);
				if (lock_request.lr_blocked_wait) {
					release_nlm_wait(lock_request.lr_blocked_wait);
					lock_request.lr_blocked_wait = NULL;
				}
				lock_request.lr_res_wait = get_nlm_response_wait(cookie,
					&mi->mi_lmaddr.sin_addr);
				lock_request.lr_proc = NLM_CANCEL_MSG;
				lock_request.lr_xdrargs = xdr_nlm_cancargs;
#ifdef DEBUG
				err = send_nlm_cancel(&lock_request, NLM_VERS, cred, cookie,
					reclaim, 0);
				NLM_DEBUG(NLMDEBUG_ERROR,
					if (err && (err != EINTR))
						printf("nlm1_async_call: cancel error %d\n", err));
#else /* DEBUG */
				(void)send_nlm_cancel(&lock_request, NLM_VERS, cred, cookie,
					reclaim, 0);
#endif /* DEBUG */
			}
			break;
		case 0:
		case ETIMEDOUT:
			/*
			 * only F_GETLK wants ld overwritten
			 */
			ASSERT(lock_request.lr_res_wait);
			error = await_nlm_response(lock_request.lr_res_wait,
				&mi->mi_lmaddr.sin_addr, (cmd == F_GETLK) ? ld : NULL,
				(nlm4_stats *)&status, timeo);
			switch (error) {
				default:
					NLM_DEBUG(NLMDEBUG_ERROR,
						printf("nlm1_async_call: await_nlm_response error %d\n",
							error));
				case EINTR:
					if (lock_request.lr_proc == NLM_LOCK_MSG) {
						/*
						 * RPC call error, so cancel the request just in case
						 * the server got it
						 * reset the object handle first, just in case the
						 * reply was received an overwrote it
						 */
						cookie = atomicAddLong(&Cookie, 1);
						release_nlm_wait(lock_request.lr_res_wait);
						if (lock_request.lr_blocked_wait) {
							release_nlm_wait(lock_request.lr_blocked_wait);
							lock_request.lr_blocked_wait = NULL;
						}
						lock_request.lr_res_wait = get_nlm_response_wait(cookie,
							&mi->mi_lmaddr.sin_addr);
						lock_request.lr_proc = NLM_CANCEL_MSG;
						lock_request.lr_xdrargs = xdr_nlm_cancargs;
#ifdef DEBUG
						err = send_nlm_cancel(&lock_request, NLM_VERS, cred,
							cookie, reclaim, 0);
						NLM_DEBUG(NLMDEBUG_ERROR,
							if (err && (err != EINTR))
								printf("nlm1_async_call: cancel error %d\n",
									err));
#else /* DEBUG */
						(void)send_nlm_cancel(&lock_request, NLM_VERS, cred,
							cookie, reclaim, 0);
#endif /* DEBUG */
					}
					break;
				case ETIMEDOUT:
					retry = 1;
					NLM_DEBUG(NLMDEBUG_TRACE,
						printf("nlm1_async_call: pid %d, wait timed out, "
							"retransmitting request\n", (int)current_pid()));
					break;
				case 0:
					switch (status) {
						case nlm_granted:
							/*
							 * The lock data has already been supplied.
							 */
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
								(mi->mi_flags & MIF_INT) ?
								  (PZERO + 1) : PRIBIO)) {
									ASSERT(mi->mi_flags & MIF_INT);
									untimeout(id);
									error = EINTR;
									goto nlm1_ereturn;
							}
							retry = 1;
							break;
						case nlm_blocked:
							/*
							 * lock requests need to wait, others need to
							 * return an error
							 */
							if (lock_request.lr_proc == NLM_LOCK_MSG) {
								/*
								 * If the server sent us a blocked reply
								 * when we were not expecting one, return
								 * EPROTO.
								 */
								if (!lock_request.lr_blocked_wait || reclaim) {
									cmn_err(CE_WARN,
										"NLM protocol error: server %s "
										"replied with nlm_blocked to a "
										"non-blocking request\n",
										lh->lh_servername);
									error = EPROTO;
									break;
								}
								timeo = 0;
								error = await_nlm_response(
									lock_request.lr_blocked_wait,
									&mi->mi_lmaddr.sin_addr, NULL,
									(nlm4_stats *)&status, 0);
								switch (error) {
									case 0:
										/*
										 * reply received
										 * Translate the NLM status to an
										 * error number.
										 */
										error = nlm1_to_errno[status];
										break;
									case EINTR:
										/*
										 * signalled, so we must cancel the
										 * request
										 */
										NLM_DEBUG(NLMDEBUG_CANCEL,
											printf("nlm1_call: signal during "
												"blocked lock, cancelling\n"));
										cookie = atomicAddLong(&Cookie, 1);
										lock_request.lr_proc = NLM_CANCEL_MSG;
										lock_request.lr_xdrargs =
											xdr_nlm_cancargs;
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
											printf("NLM NLM_LOCK_MSG: pid %d, "
												"wait timed out, "
												"retransmitting blocked "
												"request\n",
												(int)current_pid()));
										retry = 1;;
										break;
									default:
										NLM_DEBUG(NLMDEBUG_TRACE,
											printf("NLM NLM_LOCK_MSG: pid %d, "
												"await_nlm_response error %d\n",
												(int)current_pid(), error));
										break;
								}
							} else {
								error = nlm1_to_errno[status];
							}
							break;
						case nlm_denied:
							if (lock_request.lr_proc == NLM_TEST_MSG) {
								/*
								 * The lock data has already been supplied.
								 */
								error = 0;
							} else {
								error = nlm1_to_errno[status];
							}
							break;
						default:
							error = nlm1_to_errno[status];
					}
			}
		default:
			/*
			 * return the error
			 */
			break;
		}
	} while (retry);
nlm1_ereturn:
	if (lock_request.lr_res_wait) {
		release_nlm_wait(lock_request.lr_res_wait);
#ifdef DEBUG
		lock_request.lr_res_wait = NULL;
#endif /* DEBUG */
	}
	nlm1_freeargs(lock_request.lr_proc, &lock_request.nlm1_args);
	ASSERT(!issplhi(getsr()));
	ASSERT(!lock_request.lr_res_wait && !lock_request.lr_blocked_wait);
	return(error);
}

int
nlm4_async_call(register lockhandle_t *lh, register struct flock *ld, int cmd,
	struct cred *cred, int reclaim)
{
#ifdef DEBUG
	int					err;
#endif /* DEBUG */
	int					retry = 0;
	nlm4_stats			status;
	int					timeo;
	toid_t				id;
	long				cookie;
	int					error = 0;
	bhv_desc_t 			*bdp = 
	bhv_base_unlocked(VFS_BHVHEAD(lh->lh_vp->v_vfsp)); 
	mntinfo_t 			*mi = vfs_bhvtomi(bdp);
	lockreq_t			lock_request;

	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CALL,
		printf("Async NLM4 call: pid %d vp 0x%p %s offset %lld len %lld %s\n",
			(int)current_pid(), lh->lh_vp, lh->lh_servername, ld->l_start,
			ld->l_len, locktype_to_str(ld->l_type)));
	/*
	 * we assume that the pid and sysid fields of the lock data have been
	 * set to the correct values
	 */
	cookie = atomicAddLong(&Cookie, 1);
	lock_request.lr_handle = lh;
	lock_request.lr_proc = 0;
	lock_request.lr_blocked_wait = lock_request.lr_res_wait = NULL;
	error = nlm4_call_setup(cmd, &lock_request, ld, &cookie, reclaim, 1);
	if (error) {
		goto nlm4_ereturn;
	}
	/*
	 * Issue the NLM RPC call.
	 * This call will be asynchronous, so set the timeout to 0, but save
	 * the original value.  It will be needed when waiting for the response
	 * indication from the local server.
	 */
	timeo = lh->lh_timeo;
	lh->lh_timeo = 0;
	do {
		error = nlm_rpc_call(&lock_request, NLM4_VERS, cred, retry,
			!!(mi->mi_flags & MIF_INT));
		/*
		 * reset retry to 0 here because nlm_rpc_call needs to know if this
		 * was a retry so it can contact the port mapper
		 */
		retry = 0;
		switch (error) {
		case EAGAIN:
			/*
			 * This call failed with a system error from the remote.  Don't
			 * try it again.
			 */
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm4_async_call: proc %d failed\n",
					lock_request.lr_proc));
			error = ENOLCK;
			break;
		case EINTR:
			if (lock_request.lr_proc == NLMPROC_LOCK_MSG) {
				/*
				 * RPC call error, so cancel the request just in case the server
				 * got it
				 * reset the object handle first, just in case the reply was
				 * received an overwrote it
				 */
				cookie = atomicAddLong(&Cookie, 1);
				release_nlm_wait(lock_request.lr_res_wait);
				if (lock_request.lr_blocked_wait) {
					release_nlm_wait(lock_request.lr_blocked_wait);
					lock_request.lr_blocked_wait = NULL;
				}
				lock_request.lr_res_wait = get_nlm_response_wait(cookie,
					&mi->mi_lmaddr.sin_addr);
				lock_request.lr_proc = NLMPROC_CANCEL_MSG;
				lock_request.lr_xdrargs = xdr_nlm4_cancargs;
#ifdef DEBUG
				err = send_nlm_cancel(&lock_request, NLM4_VERS, cred, cookie,
					reclaim, 0);
				NLM_DEBUG(NLMDEBUG_ERROR,
					if (err && (err != EINTR))
						printf("nlm4_async_call: cancel error %d\n", err));
#else /* DEBUG */
				(void)send_nlm_cancel(&lock_request, NLM4_VERS, cred, cookie,
					reclaim, 0);
#endif /* DEBUG */
			}
			break;
		case 0:
		case ETIMEDOUT:
			/*
			 * only F_GETLK wants ld overwritten
			 */
			error = await_nlm_response(lock_request.lr_res_wait,
				&mi->mi_lmaddr.sin_addr, (cmd == F_GETLK) ? ld : NULL,
				&status, timeo);
			switch (error) {
				default:
				case EINTR:
					NLM_DEBUG(NLMDEBUG_ERROR,
						printf("nlm4_async_call: await_nlm_response error %d\n",
							error));
					if (lock_request.lr_proc == NLMPROC_LOCK_MSG) {
						/*
						 * RPC call error, so cancel the request just in
						 * case the server got it
						 * reset the object handle first, just in case the
						 * reply was received an overwrote it
						 */
						cookie = atomicAddLong(&Cookie, 1);
						release_nlm_wait(lock_request.lr_res_wait);
						if (lock_request.lr_blocked_wait) {
							release_nlm_wait(lock_request.lr_blocked_wait);
							lock_request.lr_blocked_wait = NULL;
						}
						lock_request.lr_res_wait = get_nlm_response_wait(cookie,
							&mi->mi_lmaddr.sin_addr);
						lock_request.lr_proc = NLMPROC_CANCEL_MSG;
						lock_request.lr_xdrargs = xdr_nlm4_cancargs;
#ifdef DEBUG
						err = send_nlm_cancel(&lock_request, NLM4_VERS, cred,
							cookie, reclaim, 0);
						NLM_DEBUG(NLMDEBUG_ERROR,
							if (err && (err != EINTR))
								printf("nlm4_async_call: cancel error %d\n",
									err));
#else /* DEBUG */
						(void)send_nlm_cancel(&lock_request, NLM4_VERS, cred,
							cookie, reclaim, 0);
#endif /* DEBUG */
					}
					break;
				case ETIMEDOUT:
					retry = 1;
					break;
				case 0:
					switch (status) {
						case NLM4_GRANTED:
							/*
							 * The lock data has already been supplied.
							 */
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
								(mi->mi_flags & MIF_INT) ?
								  (PZERO + 1) : PRIBIO)) {
									ASSERT(mi->mi_flags & MIF_INT);
									untimeout(id);
									error = EINTR;
									goto nlm4_ereturn;
							}
							retry = 1;
							break;
						case NLM4_BLOCKED:
							/*
							 * lock requests need to wait, others need to
							 * return an error
							 */
							if (lock_request.lr_proc == NLMPROC_LOCK_MSG) {
								/*
								 * If the server sent us a blocked reply
								 * when we were not expecting one, return
								 * EPROTO.
								 */
								if (!lock_request.lr_blocked_wait || reclaim) {
									cmn_err(CE_WARN,
										"NLM4 protocol error: server %s "
										"replied with NLM4_BLOCKED to a "
										"non-blocking request\n",
										lh->lh_servername);
									error = EPROTO;
									break;
								}
								/*
								 * set the timeout to 0 for an indefinite
								 * wait
								 */
								timeo = 0;
								error = await_nlm_response(
									lock_request.lr_blocked_wait,
									&mi->mi_lmaddr.sin_addr, NULL, &status,
									0);
								switch (error) {
									case 0:
										/*
										 * reply received
										 * Translate the NLM status to an
										 * error number.
										 */
										error = nlm1_to_errno[status];
										break;
									case EINTR:
										/*
										 * signalled, so we must cancel the
										 * request
										 */
										NLM_DEBUG(NLMDEBUG_CANCEL,
											printf("nlm4_call: signal during "
												"blocked lock, cancelling\n"));
										cookie = atomicAddLong(&Cookie, 1);
										lock_request.lr_proc =
											NLMPROC_CANCEL_MSG;
										lock_request.lr_xdrargs =
											xdr_nlm4_cancargs;
										error = send_nlm_cancel(&lock_request,
											NLM4_VERS, cred, cookie, reclaim,
											0);
										error = EINTR;
										break;
									case ETIMEDOUT:
										/*
										 * timed out, so retry
										 */
										NLM_DEBUG(NLMDEBUG_TRACE,
											printf("NLM4 NLMPROC_LOCK_MSG: "
												"pid %d, wait timed out, "
												"retransmitting blocked "
												"request\n",
												(int)current_pid()));
										retry = 1;
										break;
									default:
										NLM_DEBUG(NLMDEBUG_TRACE,
											printf("NLM4 NLMPROC_LOCK_MSG: "
												"pid %d, await_nlm_response "
												"error %d\n",
												(int)current_pid(), error));
										break;
								}
							} else {
								error = nlm4_to_errno[status];
							}
							break;
						case NLM4_DENIED:
							if (lock_request.lr_proc == NLMPROC_TEST_MSG) {
								/*
								 * The lock data has already been supplied.
								 */
								error = 0;
							} else {
								error = nlm4_to_errno[status];
							}
							break;
						default:
							error = nlm4_to_errno[status];
					}
			}
		default:
			/*
			 * return the error
			 */
			break;
		}
	} while (retry);
nlm4_ereturn:
	if (lock_request.lr_res_wait) {
		release_nlm_wait(lock_request.lr_res_wait);
#ifdef DEBUG
		lock_request.lr_res_wait = NULL;
#endif /* DEBUG */
	}
	if (lock_request.lr_blocked_wait) {
		release_nlm_wait(lock_request.lr_blocked_wait);
		lock_request.lr_blocked_wait = NULL;
	}
	nlm4_freeargs(lock_request.lr_proc, &lock_request.nlm4_args);
	ASSERT(!issplhi(getsr()));
	ASSERT(!lock_request.lr_res_wait && !lock_request.lr_blocked_wait);
	return(error);
}
