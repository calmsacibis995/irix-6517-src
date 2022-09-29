#include "types.h"
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ksignal.h>
#include <sys/kthread.h>
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

/*
 * Back off for retransmission timeout, MAXTIMO unit is 1/10th sec.
 */
#define MAXTIMO 900
#define backoff(tim)    ((((tim) << 1) > MAXTIMO) ? MAXTIMO : ((tim) << 1))

static flock_t zero_flock;
extern int first_retry;
extern int normal_retry;
extern lock_t Statmonvp_lock;
char *Lock_client_name = NULL;
size_t Lock_client_namelen = 0;

char *ha_dir = NULL;
char *ha2_dir = NULL;
static int add_direntry(char *, char *);

int
record_remote_name(char *name)
{
	int error = 0;
	int error1=0, error2=0;
	vattr_t vattr;
	vnode_t *vp = NULL;
	vnode_t *svp = NULL;
	int ospl;

	if (!name || strchr(name, '/')) {
		return(EINVAL);
	}
	/*
	 * If we do not have the statmon directory vnode yet, look it up.
	 */
	ospl = mutex_spinlock(&Statmonvp_lock);
	if (!(svp = Statmonvp)) {
		mutex_spinunlock(&Statmonvp_lock, ospl);
		error = lookupname(Statmon_dir, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp, NULL);
		switch (error) {
			case 0:
				break;
			case ENOENT:
				/*
				 * Statmon_dir does not exist, so try to create it.
				 */
				vattr.va_type = VDIR;
				vattr.va_mode = 0755;
				vattr.va_mask = AT_TYPE|AT_MODE;
				error = vn_create(Statmon_dir, UIO_SYSSPACE, &vattr,
					VEXCL, 0, &vp, CRMKDIR, NULL);
				if (error && (error != EEXIST)) {
					cmn_err(CE_WARN, "Unable to create %s, error %d\n",
						Statmon_dir, error);
					return(ENOLCK);
				}
				error = lookupname(Statmon_dir, UIO_SYSSPACE, FOLLOW, NULLVPP,
					&vp, NULL);
				if (error) {
					cmn_err(CE_WARN,
						"Unable to lookup or create %s, error %d\n",
						Statmon_dir, error);
					return(ENOLCK);
				}
				break;
			default:
				cmn_err(CE_WARN, "Unable to open %s, error %d\n", Statmon_dir,
					error);
				ASSERT(vp == NULL);
				return(ENOLCK);
		}
		ospl = mutex_spinlock(&Statmonvp_lock);
		svp = Statmonvp = vp;
	}
	VN_HOLD(svp);
	mutex_spinunlock(&Statmonvp_lock, ospl);
	vattr.va_type = VREG;
	vattr.va_mode = 0444;
	vattr.va_mask = AT_TYPE|AT_MODE;
	vp = NULL;
	VOP_CREATE(svp, name, &vattr, VEXCL, VREAD, &vp, sys_cred, error);	
	switch (error) {
			case 0:
				ASSERT(vp != NULL);
				VOP_FSYNC(vp, FSYNC_WAIT, sys_cred,
					(off_t)0, (off_t)-1, error);
				if (error) {
					NLM_DEBUG(NLMDEBUG_ERROR,
						printf(
							"record_remote_name: fsync error for %s/%s: %d\n",
							Statmon_dir, name, error));
					error = EAGAIN;
				}
				VN_RELE(vp);
				break;
			case EEXIST:
				error = 0;
				break;
			default:
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("record_remote_name: create error for %s/%s: %d\n",
						Statmon_dir, name, error));
				error = EAGAIN;
	}
	VN_RELE(svp);

	/*
	 * add this entry to ha_dirs if they are defined
	 */
        if(ha_dir) {
                error1 = add_direntry( name, ha_dir);
        }
        if(ha2_dir) {
                error2 = add_direntry( name, ha2_dir);
        }
        error |= error1 | error2;

	return(error);
}


static int
add_direntry( char *name, char *ha_dir)
{
        vattr_t vattr;
        vnode_t *vp = NULL;
	vnode_t *ha_vp = NULL;
        int error =0;
	
	error = lookupname(ha_dir, UIO_SYSSPACE, FOLLOW, NULLVPP, &ha_vp, NULL);
	if (error) {
		cmn_err(CE_WARN,
			"add_direntry: lookupname failed: %s, %d\n",ha_dir, error); 
		return error;
	}

	vattr.va_type = VREG;
	vattr.va_mode = 0444;
	vattr.va_mask = AT_TYPE|AT_MODE;
	VOP_CREATE(ha_vp, name, &vattr, VEXCL, VREAD, &vp, sys_cred, error);
	switch (error) {
            case 0:
                VOP_FSYNC(vp, FSYNC_WAIT, sys_cred, (off_t)0, (off_t)-1, error);
                if (error) {
                    NLM_DEBUG(NLMDEBUG_ERROR,
                        printf(
                            "record_remote_name: fsync error for %s/%s: %d\n",
                            ha_dir, name, error));
                    error = EAGAIN;
                }
                VN_RELE(vp);
                break;
            case EEXIST:
                error = 0;
                break;
            default:
                NLM_DEBUG(NLMDEBUG_ERROR,
                    printf("record_remote_name: create error for %s/%s: %d\n",
                        ha_dir, name, error));
                error = EAGAIN;
	}
	VN_RELE(ha_vp);

        return error;
}


void
nlm_testrply_to_flock(nlm_testrply *rep, flock_t *ld)
{
	switch (rep->stat) {
		case nlm_denied:
			ld->l_pid = (pid_t)rep->nlm_testrply_u.holder.svid;
			ld->l_type = rep->nlm_testrply_u.holder.exclusive ? F_WRLCK :
				F_RDLCK;
			ld->l_whence = (short)0;
			ld->l_start = (off_t)rep->nlm_testrply_u.holder.l_offset;
			ld->l_len = (off_t)rep->nlm_testrply_u.holder.l_len;
			ld->l_sysid = 0;
			break;
		case nlm_granted:
			ld->l_type = F_UNLCK;
			break;
		default:
			*ld = zero_flock;
	}
}

void
nlm4_testrply_to_flock(nlm4_testrply *rep, flock_t *ld)
{
	switch (rep->stat) {
		case NLM4_DENIED:
			ld->l_pid = (pid_t)rep->nlm4_testrply_u.holder.svid;
			ld->l_type = rep->nlm4_testrply_u.holder.exclusive ? F_WRLCK :
				F_RDLCK;
			ld->l_whence = (short)0;
			ld->l_start = (off_t)rep->nlm4_testrply_u.holder.l_offset;
			ld->l_len = (off_t)rep->nlm4_testrply_u.holder.l_len;
			ld->l_sysid = 0;
			break;
		case NLM4_GRANTED:
			ld->l_type = F_UNLCK;
			break;
		default:
			*ld = zero_flock;
	}
}

void
nlm_grace_wakeup(sema_t *sp, kthread_t *kt)
{
	/* XXX this is full of races .. */
	(void)wsema(sp, kt);
}

int
nlm_rpc_call(lockreq_t *lrp, int nlmvers, cred_t *cred, int retry, int signals)
{
	CLIENT			*client;
	bhv_desc_t 			*bdp = 
	bhv_base_unlocked(VFS_BHVHEAD((lrp->lr_handle->lh_vp)->v_vfsp));
	mntinfo_t 			*mi = vfs_bhvtomi(bdp);
	struct rpc_err	rpc_err;
	enum clnt_stat	status;
	bool_t			tryagain;
	int				error = 0;
	struct timeval	wait;
	int				timeo;

	ASSERT(lrp);
	ASSERT(lrp->lr_xdrargs != NULL);
	ASSERT(lrp->lr_xdrreply != NULL);
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CALL,
		printf("nlm_rpc_call: NLM vers %d call: pid %d, proc %s, cookie %s\n",
			nlmvers, (int)current_pid(),
			nlmproc_to_str(nlmvers, lrp->lr_proc),
			netobj_to_str((netobj *)&lrp->lr_args_u)));
	/*
	 * If we don't have the port number, get it from the portmapper...
	 */
	if (retry || mi->mi_lmaddr.sin_port == 0) {
		error = getport_loop(&mi->mi_lmaddr, (u_long)NLM_PROG, (u_long)nlmvers,
			(u_long)IPPROTO_UDP, 1);
		switch (error) {
			case 0:
				break;
			case ENOENT:
				/*
				 * program/version not supported, return ENOPKG
				 */
				NLM_DEBUG(NLMDEBUG_NOPROG,
					printf("nlm_rpc_call: NLM_PROG, version %d not "
						"supported for server %s\n", nlmvers,
						mi->mi_hostname));
				error = ENOPKG;
				return(error);
			case EINTR:
				NLM_DEBUG(NLMDEBUG_TRACE,
					printf("nlm_rpc_call: getport_loop interrupted\n"));
				return(error);
			default:
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("nlm_rpc_call: getport_loop error %d, host %s\n",
						error, mi->mi_hostname));
				return(error);
		}
	}
get_new_handle:
	timeo = lrp->lr_handle->lh_timeo;
	/*
	 * set up a client handle to talk to the remote lock manager
	 * use the same xid for all retries
	 */
	client = clntkudp_create(&mi->mi_lmaddr, (u_long)NLM_PROG, (u_long)nlmvers,
		first_retry, KUDP_NOINTR, KUDP_XID_CREATE, cred);
	if (client == (CLIENT *) NULL) {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm_rpc_call: clntkudp_create error %d, host %s\n",
				rpc_createerr.cf_error.re_errno, mi->mi_hostname));
		if (rpc_createerr.cf_error.re_errno == EADDRINUSE) {
			delay((timeo * HZ) / 10);
			goto get_new_handle;
		}
		return(ENOLCK);
	}
	/*
	 * If hard mounted fs, retry call forever unless hard error occurs
	 */
	do {
		tryagain = FALSE;

		wait.tv_sec = timeo / 10;
		wait.tv_usec = 100000 * (timeo % 10);
		ASSERT(!timeo || lrp->lr_handle->lh_timeo);
		ASSERT(!lrp->lr_handle->lh_timeo || timeo);
		ASSERT(!timeo || (wait.tv_sec || wait.tv_usec));
		ASSERT(!(wait.tv_sec || wait.tv_usec) || timeo);
		status = CLNT_CALL(client, lrp->lr_proc, lrp->lr_xdrargs,
			(char *)&lrp->lr_args_u, lrp->lr_xdrreply, (char *)&lrp->lr_reply_u,
			wait);
		switch (status) {
			case RPC_SUCCESS:
				/*
				 * the call succeeded, but check for a signal
				 * if we got one, that will take precedence
				 */
				if (signals && issig(0, SIG_NOSTOPDEFAULT)) {
					error = EINTR;
					break;
				} else {
					error = 0;
				}
				break;

			/*
			 * Unrecoverable errors: give up immediately
			 */
			case RPC_AUTHERROR:
			case RPC_CANTENCODEARGS:
			case RPC_CANTDECODERES:
			case RPC_CANTDECODEARGS:
				error = ENOLCK;
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("nlm_rpc_call: %s\n", clnt_sperrno(status)));
				break;

			case RPC_PROGUNAVAIL:
				error = ENOLCK;
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("nlm_rpc_call: NLM prog (%d) unavailable on server "
						"%s\n", NLM_PROG, lrp->lr_handle->lh_servername));
				if (!retry) {
					retry = 1;
					goto get_new_port;
				}
				break;

			case RPC_PROCUNAVAIL:
				/*
				 * ENOSYS will be returned when the requested procedure is not
				 * available.
				 */
				error = ENOSYS;
				NLM_DEBUG(NLMDEBUG_NOPROC,
					printf("nlm_rpc_call: proc %d %s\n", lrp->lr_proc,
						clnt_sperrno(status)));
				break;
			case RPC_VERSMISMATCH:
			case RPC_PROGVERSMISMATCH:
				error = ENOPKG;
				NLM_DEBUG(NLMDEBUG_VERSERR,
					printf("nlm_rpc_call: %s\n", clnt_sperrno(status)));
				break;

			case RPC_INTR:	
				ASSERT(mi->mi_flags & MIF_INT);
				error = EINTR;
				break;

			case RPC_CANTSEND:
#ifdef DEBUG
				CLNT_GETERR(client, &rpc_err);
#endif /* DEBUG */
				NLM_DEBUG(NLMDEBUG_CANTSEND,
					printf("nlm_rpc_call: %s, error %d\n",
						clnt_sperrno(status), rpc_err.re_errno));
			case RPC_TIMEDOUT:
				/*
				 * process signals now
				 * signals take precedence over timeouts, but other
				 * errors will be reported before signals
				 */
				if (signals && issig(0, SIG_NOSTOPDEFAULT)) {
					error = EINTR;
					break;
				} else if (!lrp->lr_handle->lh_timeo) {
					error = ETIMEDOUT;
					break;
				}
				NLM_DEBUG(NLMDEBUG_TRACE,
					if (status == RPC_TIMEDOUT)
						printf("nlm_rpc_call: %s call timed out\n",
							nlmproc_to_str(nlmvers, lrp->lr_proc)));
get_new_port:
				timeo = lm_backoff(timeo);
				error = getport_loop(&mi->mi_lmaddr, (u_long)NLM_PROG,
					(u_long)nlmvers, (u_long)IPPROTO_UDP, 1);
				switch (error) {
					case 0:
						NLM_DEBUG(NLMDEBUG_NOPROG,
							printf("nlm_rpc_call: resetting port number for "
								"server %s (%s, %d)\n", mi->mi_hostname,
								inet_ntoa(mi->mi_lmaddr.sin_addr),
								(int)mi->mi_lmaddr.sin_port));
						/*
						 * we got a port number
						 * reset the lock-manager client handle
						 * leave the xid alone
						 */
						error = clntkudp_init(client, &mi->mi_lmaddr,
							normal_retry, KUDP_NOINTR, KUDP_XID_SAME, cred);
						if (error) {
							CLNT_DESTROY(client);
							client = NULL;
							goto get_new_handle;
						}
						/*
						 * try the call again
						 */
						tryagain = TRUE;
						break;
					case ENOENT:
						NLM_DEBUG(NLMDEBUG_NOPROG,
							printf("nlm_rpc_call: NLM_PROG, version %d no "
								"longer supported for server %s\n",
								nlmvers, mi->mi_hostname));
					case ETIMEDOUT:
						if (mi->mi_flags & MIF_HARD) {
							tryagain = TRUE;
							timeo = backoff(timeo);
						} else {
							tryagain = FALSE;
						}
						break;
					case EINTR:
						NLM_DEBUG(NLMDEBUG_TRACE,
							printf("nlm_rpc_call: getport_loop interrupted\n"));
						break;
					default:
						NLM_DEBUG(NLMDEBUG_ERROR,
							printf("nlm_rpc_call: getport_loop error %d, "
								"host %s\n", error, mi->mi_hostname));
						tryagain = FALSE;
				}
				break;
			case RPC_SYSTEMERROR:
				/*
				 * return EAGAIN here to force the caller to go through the
				 * local lock daemon
				 * this is a compensation for older versions of SGI's lockd
				 * which had a bug resulting in RPC_SYSTEMERROR being
				 * returned
				 */
				NLM_DEBUG(NLMDEBUG_ERROR,
					(CLNT_GETERR(client, &rpc_err),
					printf("nlm_rpc_call: %s returned RPC_SYSTEMERROR, "
						"errno %d\n", mi->mi_hostname, rpc_err.re_errno)));
				error = EAGAIN;
				break;
			default:
				CLNT_GETERR(client, &rpc_err);
				if (rpc_err.re_errno) {
					error = rpc_err.re_errno;
				} else {
					error = ENOLCK;
				}
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("nlm_rpc_call: RPC error %s, errno %d, return %d\n",
						clnt_sperrno(status), rpc_err.re_errno, error));
				tryagain = FALSE;
		}
	} while (tryagain);
	CLNT_DESTROY(client);		/* drop the client handle */
	return(error);
}

int
nlm1_call_setup(int cmd, lockreq_t *lrp, flock_t *ld, long *cookie,
	int reclaim, int async)
{
	bool_t					blocking = FALSE;
	int						error = 0;
	bhv_desc_t 			*bdp = 
	bhv_base_unlocked(VFS_BHVHEAD((lrp->lr_handle->lh_vp)->v_vfsp));
	mntinfo_t 			*mi = vfs_bhvtomi(bdp);
	nlm_lock				*alockp = NULL;
	struct owner_handle	*ohandle = NULL;
	int						ohlen = sizeof(struct owner_handle) + hostnamelen;

	/*
	 * we assume that the pid and sysid fields of the lock data have been
	 * set to the correct values
	 */
	ASSERT(ld->l_sysid == 0);
	if (!Lock_client_name) {
		return(ENOLCK);
	}
	if (ld->l_type != F_WRLCK && ld->l_type != F_RDLCK && 
			ld->l_type != F_UNLCK) {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm1_call_setup: bad lock type %d\n", ld->l_type));
		return(EINVAL);
	}
	bzero(&lrp->nlm1_args, sizeof(lrp->nlm1_args));
	bzero(&lrp->nlm1_reply, sizeof(lrp->nlm1_reply));
	ohandle = (struct owner_handle *)NLM_KMEM_ZALLOC(ohlen, KM_SLEEP);
	/*
	 * Just in case this might be a lock request, set exclusive and block
	 * to false.  If this is not a lock request, the argument structure
	 * will be fully initialized below.  We expect lock requests to be
	 * the most common.  Doing the initialization here avoids extra
	 * comparisons later.
	 */
	lrp->nlm1_args.lockargs.exclusive = FALSE;
	lrp->nlm1_args.lockargs.block = FALSE;
	switch (cmd) {
		case F_SETLKW:
		case F_SETBSDLKW:
			blocking = lrp->nlm1_args.lockargs.block = TRUE;
		case F_SETLK:
		case F_SETBSDLK:
			switch (ld->l_type) {
			case F_WRLCK:
				lrp->nlm1_args.lockargs.exclusive = TRUE;
			case F_RDLCK:
				/* 
				 * To handle lock requests, it is necessary to
				 * make an entry for the server name in /var/statmon/sm.
				 * This must be done here (i.e., prior to actually
				 * issuing the lock request).  This entry must be made
				 * so that it will persist across a crash.
				 */
				if (async) {
					lrp->lr_proc = NLM_LOCK_MSG;
				} else {
					lrp->lr_proc = NLM_LOCK;
				}
				error = record_remote_name(lrp->lr_handle->lh_servername);
				if (error) {
					goto nlm1_ereturn;
				}
				lrp->nlm1_args.lockargs.cookie.n_len = sizeof(*cookie);
				lrp->nlm1_args.lockargs.cookie.n_bytes = (char *)cookie;
				lrp->nlm1_args.lockargs.reclaim = reclaim;
				lrp->nlm1_args.lockargs.state = 0;
				lrp->lr_xdrargs = (xdrproc_t)xdr_nlm_lockargs;
				alockp = &lrp->nlm1_args.lockargs.alock;
				break;
			case F_UNLCK:
				ASSERT(!reclaim);
				if (async) {
					lrp->lr_proc = NLM_UNLOCK_MSG;
				} else {
					lrp->lr_proc = NLM_UNLOCK;
				}
				lrp->nlm1_args.unlockargs.cookie.n_len = sizeof(*cookie);
				lrp->nlm1_args.unlockargs.cookie.n_bytes = (char *)cookie;
				lrp->lr_xdrargs = (xdrproc_t)xdr_nlm_unlockargs;
				alockp = &lrp->nlm1_args.unlockargs.alock;
				break;
			}
			/*
			 * We set up the reply cookie here merely to provide space
			 * for it.
			 */
			lrp->nlm1_reply.statreply.cookie.n_bytes = (char *)cookie;
			lrp->nlm1_reply.statreply.cookie.n_len = sizeof(*cookie);
			lrp->lr_xdrreply = (xdrproc_t)xdr_nlm_res;
			break;
		case F_GETLK:
			/*
			 * If synchronous tests are not to be done, return EAGAIN.
			 */
			if (async) {
				lrp->lr_proc = NLM_TEST_MSG;
			} else if (mi->mi_nlm & MI_NLMASYNC_TEST) {
				error = EAGAIN;
				goto nlm1_ereturn;
			} else {
				lrp->lr_proc = NLM_TEST;
			}
			lrp->nlm1_args.testargs.exclusive = (ld->l_type == F_WRLCK);
			lrp->nlm1_args.testargs.cookie.n_len = sizeof(*cookie);
			lrp->nlm1_args.testargs.cookie.n_bytes = (char *)cookie;
			alockp = &lrp->nlm1_args.testargs.alock;
			lrp->lr_xdrargs = (xdrproc_t)xdr_nlm_testargs;
			lrp->lr_xdrreply = (xdrproc_t)xdr_nlm_testres;
			/*
			 * We set up the reply cookie and object handle here merely to
			 * provide space for them.
			 */
			lrp->nlm1_reply.testreply.cookie.n_bytes = (char *)cookie;
			lrp->nlm1_reply.testreply.cookie.n_len = sizeof(*cookie);
			lrp->nlm1_reply.testreply.stat.nlm_testrply_u.holder.oh.n_len =
				ohlen;
			lrp->nlm1_reply.testreply.stat.nlm_testrply_u.holder.oh.n_bytes =
				(char *)ohandle;
			break;
		default:
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm1_call_setup: invalid command %d\n", cmd));
			error = EINVAL;
			goto nlm1_ereturn;
	}
	/*
	 * lh_id is a struct netobj, so just copy it to fh in alock
	 */
	alockp->fh = lrp->lr_handle->lh_id;
	/*
	 * allocate and initialize the object handle
	 * object handle is host name followed by process ID
	 */
	alockp->oh.n_len = ohlen;
	alockp->oh.n_bytes = (char *)ohandle;
	ASSERT(strlen(hostname) + sizeof(struct owner_handle) <= ohlen);
	ohandle->oh_pid = ld->l_pid;
	strcpy(ohandle->oh_hostname, hostname);
	alockp->svid = ld->l_pid;
	alockp->caller_name = Lock_client_name;
	alockp->l_offset = ld->l_start;
	alockp->l_len = ld->l_len;
	if (async) {
		lrp->lr_res_wait = get_nlm_response_wait(*cookie,
			&mi->mi_lmaddr.sin_addr);
	}
	if (blocking) {
		lrp->lr_blocked_wait = get_nlm_blocked_wait(ld->l_pid,
			&mi->mi_lmaddr.sin_addr);
	}
	return(0);
	/*
	 * error return case
	 */
nlm1_ereturn:
	if (ohandle) {
		NLM_KMEM_FREE(ohandle, ohlen);
	}
	return(error);
}

int
nlm4_call_setup(int cmd, lockreq_t *lrp, flock_t *ld, long *cookie,
	int reclaim, int async)
{
	int					error = 0;
	bhv_desc_t 				*bdp = 
	bhv_base_unlocked(VFS_BHVHEAD((lrp->lr_handle->lh_vp)->v_vfsp));
	mntinfo_t 				*mi = vfs_bhvtomi(bdp);
	nlm4_lock				*alockp = NULL;
	bool_t					blocking = FALSE;
	struct owner_handle			*ohandle = NULL;
	int					ohlen = sizeof(struct owner_handle) + hostnamelen;

	/*
	 * we assume that the pid and sysid fields of the lock data have been
	 * set to the correct values
	 */
	ASSERT(ld->l_sysid == 0);
	if (!Lock_client_name) {
		return(ENOLCK);
	}
	if (ld->l_type != F_WRLCK && ld->l_type != F_RDLCK && 
			ld->l_type != F_UNLCK) {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm4_call_setup: bad lock type %d\n", ld->l_type));
		return(EINVAL);
	}
	bzero(&lrp->nlm4_args, sizeof(lrp->nlm4_args));
	bzero(&lrp->nlm4_reply, sizeof(lrp->nlm4_reply));
	ohandle = (struct owner_handle *)NLM_KMEM_ZALLOC(ohlen, KM_SLEEP);
	lrp->nlm4_args.lockargs.exclusive = FALSE;
	lrp->nlm4_args.lockargs.block = FALSE;
	switch (cmd) {
		case F_SETLKW:
		case F_SETBSDLKW:
			blocking = lrp->nlm4_args.lockargs.block = TRUE;
		case F_SETLK:
		case F_SETBSDLK:
			switch (ld->l_type) {
			case F_WRLCK:
				lrp->nlm4_args.lockargs.exclusive = TRUE;
			case F_RDLCK:
				/* 
				 * To handle lock requests, it is necessary to
				 * make an entry for the server name in /var/statmon/sm.
				 * This must be done here (i.e., prior to actually
				 * issuing the lock request).  This entry must be made
				 * so that it will persist across a crash.
				 */
				if (async) {
					lrp->lr_proc = NLMPROC_LOCK_MSG;
				} else {
					lrp->lr_proc = NLMPROC_LOCK;
				}
				error = record_remote_name(lrp->lr_handle->lh_servername);
				if (error) {
					goto nlm4_ereturn;
				}
				lrp->nlm4_args.lockargs.cookie.n_len = sizeof(*cookie);
				lrp->nlm4_args.lockargs.cookie.n_bytes = (char *)cookie;
				lrp->nlm4_args.lockargs.reclaim = reclaim;
				lrp->nlm4_args.lockargs.state = 0;
				lrp->lr_xdrargs = (xdrproc_t)xdr_nlm4_lockargs;
				alockp = &lrp->nlm4_args.lockargs.alock;
				break;
			case F_UNLCK:
				ASSERT(!reclaim);
				if (async) {
					lrp->lr_proc = NLMPROC_UNLOCK_MSG;
				} else {
					lrp->lr_proc = NLMPROC_UNLOCK;
				}
				lrp->nlm4_args.unlockargs.cookie.n_len = sizeof(*cookie);
				lrp->nlm4_args.unlockargs.cookie.n_bytes = (char *)cookie;
				lrp->lr_xdrargs = (xdrproc_t)xdr_nlm4_unlockargs;
				alockp = &lrp->nlm4_args.unlockargs.alock;
				break;
			}
			/*
			 * We set up the reply cookie here merely to provide space
			 * for it.
			 */
			lrp->nlm4_reply.statreply.cookie.n_bytes = (char *)cookie;
			lrp->nlm4_reply.statreply.cookie.n_len = sizeof(*cookie);
			lrp->lr_xdrreply = (xdrproc_t)xdr_nlm4_res;
			break;
		case F_GETLK:
			/*
			 * If synchronous tests are not to be done, return EAGAIN.
			 */
			if (async) {
				lrp->lr_proc = NLMPROC_TEST_MSG;
			} else if (mi->mi_nlm & MI_NLM4ASYNC_TEST) {
				error = EAGAIN;
				goto nlm4_ereturn;
			} else {
				lrp->lr_proc = NLMPROC_TEST;
			}
			lrp->nlm4_args.testargs.exclusive = (ld->l_type == F_WRLCK);
			lrp->nlm4_args.testargs.cookie.n_len = sizeof(*cookie);
			lrp->nlm4_args.testargs.cookie.n_bytes = (char *)cookie;
			alockp = &lrp->nlm4_args.testargs.alock;
			lrp->lr_xdrargs = (xdrproc_t)xdr_nlm4_testargs;
			lrp->lr_xdrreply = (xdrproc_t)xdr_nlm4_testres;
			/*
			 * We set up the reply cookie and object handle here merely to
			 * provide space for them.
			 */
			lrp->nlm4_reply.testreply.cookie.n_bytes = (char *)cookie;
			lrp->nlm4_reply.testreply.cookie.n_len = sizeof(*cookie);
			lrp->nlm4_reply.testreply.stat.nlm4_testrply_u.holder.oh.n_len =
				ohlen;
			lrp->nlm4_reply.testreply.stat.nlm4_testrply_u.holder.oh.n_bytes =
				(char *)ohandle;
			break;
		default:
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm4_call_setup: invalid command %d\n", cmd));
			error = EINVAL;
			goto nlm4_ereturn;
	}
	/*
	 * lh_id is a struct netobj, so just copy it to fh in alock
	 */
	alockp->fh = lrp->lr_handle->lh_id;
	/*
	 * allocate and initialize the object handle
	 * object handle is host name followed by process ID
	 */
	alockp->oh.n_len = ohlen;
	alockp->oh.n_bytes = (char *)ohandle;
	ASSERT(strlen(hostname) + sizeof(ld->l_pid) <= ohlen);
	ohandle->oh_pid = ld->l_pid;
	strcpy(ohandle->oh_hostname, hostname);
	alockp->svid = ld->l_pid;
	alockp->caller_name = Lock_client_name;
	alockp->l_offset = ld->l_start;
	alockp->l_len = ld->l_len;
	if (async) {
		lrp->lr_res_wait = get_nlm_response_wait(*cookie,
			&mi->mi_lmaddr.sin_addr);
	}
	if (blocking) {
		lrp->lr_blocked_wait = get_nlm_blocked_wait(ld->l_pid,
			&mi->mi_lmaddr.sin_addr);
	}
	return(0);
	/*
	 * error return case
	 */
nlm4_ereturn:
	if (ohandle) {
		NLM_KMEM_FREE(ohandle, ohlen);
	}
	return(error);
}

/*
 * Free arguents allocated by nlm4_call_setup
 * This frees the space allocated for the object handle.  This space
 * is shared by both the arguments and the reply.
 */
void
nlm4_freeargs(int proc, union nlm4_args_u *args)
{
	caddr_t buf = NULL;
	int len = 0;

	switch (proc) {
		case 0:
			break;
		case NLMPROC_TEST:
		case NLMPROC_TEST_MSG:
			buf = args->testargs.alock.oh.n_bytes;
			len = args->testargs.alock.oh.n_len;
			break;
		case NLMPROC_LOCK:
		case NLMPROC_LOCK_MSG:
			buf = args->lockargs.alock.oh.n_bytes;
			len = args->lockargs.alock.oh.n_len;
			break;
		case NLMPROC_UNLOCK:
		case NLMPROC_UNLOCK_MSG:
			buf = args->unlockargs.alock.oh.n_bytes;
			len = args->unlockargs.alock.oh.n_len;
			break;
		case NLMPROC_CANCEL:
		case NLMPROC_CANCEL_MSG:
			buf = args->cancelargs.alock.oh.n_bytes;
			len = args->cancelargs.alock.oh.n_len;
			break;
		default:
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm4_freeargs: invalid procedure number %d\n", proc));
			return;
	}
	ASSERT(!len || buf);
	if (buf) {
		NLM_KMEM_FREE(buf, len);
	}
}

/*
 * Free arguents allocated by nlm4_call_setup
 * This frees the space allocated for the object handle.  This space
 * is shared by both the arguments and the reply.
 */
void
nlm1_freeargs(int proc, union nlm1_args_u *args)
{
	caddr_t buf = NULL;
	int len = 0;

	switch (proc) {
		case 0:
			break;
		case NLM_TEST:
		case NLM_TEST_MSG:
			buf = args->testargs.alock.oh.n_bytes;
			len = args->testargs.alock.oh.n_len;
			break;
		case NLM_LOCK:
		case NLM_LOCK_MSG:
			buf = args->lockargs.alock.oh.n_bytes;
			len = args->lockargs.alock.oh.n_len;
			break;
		case NLM_UNLOCK:
		case NLM_UNLOCK_MSG:
			buf = args->unlockargs.alock.oh.n_bytes;
			len = args->unlockargs.alock.oh.n_len;
			break;
		case NLM_CANCEL:
		case NLM_CANCEL_MSG:
			buf = args->cancelargs.alock.oh.n_bytes;
			len = args->cancelargs.alock.oh.n_len;
			break;
		default:
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("nlm1_freeargs: invalid procedure number %d\n", proc));
			return;
	}
	ASSERT(!len || buf);
	if (buf) {
		NLM_KMEM_FREE(buf, len);
	}
}
