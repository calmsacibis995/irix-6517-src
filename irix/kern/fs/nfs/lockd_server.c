/* lockd_dispatch.c
 */
#ident      "$Revision: 1.52 $"

#define NFSSERVER
#include "types.h"
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/mbuf.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/siginfo.h>	/* for CLD_EXITED exit code */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/capability.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/sema.h>
#include <sys/pcb.h>		/* for setjmp and longjmp */
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#ifdef DEBUG
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#endif /* DEBUG */
#include <string.h>
#include "auth.h"
#include "auth_unix.h"
#include "svc.h"
#include "xdr.h"
#include "nfs.h"
#include "lockmgr.h"
#include "nlm_rpc.h"
#include "nfs_impl.h"
#include "clnt.h"
#include "export.h"
#include "lockd_impl.h"
#include "nlm_svc.h"
#include "nlm_share.h"
#include "nlm_debug.h"

/*
 * NLM wait definitions
 */

#define WAIT_HASH_SLOTS     32
#define WAIT_HASH_MASK      (WAIT_HASH_SLOTS - 1)

#ifdef DEBUG
int nlm_wait_alloc = 0;
#endif /* DEBUG */

static nlm_wait_t *nlm_response_wait_hash[WAIT_HASH_SLOTS];
static nlm_wait_t *nlm_blocked_wait_hash[WAIT_HASH_SLOTS];
static lock_t nlm_wait_lock = 0;
static zone_t *nlm_wait_zone = NULL;

static int create_ha_dir(char **, char *, size_t *);
static int compare_dirname(char *, char *);

/*
 * daemon management
 * these are defined in master.d/lockd
 */
extern int lockd_count;			/* number of lock daemons */

/*
 * data to fill in a flock with zeroes
 */
static flock_t zero_flock = {0, 0, 0, 0, 0, 0};

/*
 * tuanbles
 */
extern int lockd_grace_period;
extern int lock_share_requests;

/*
 * grace period indicator
 */
int in_grace_period = 1;

/*
 * things shared with client locking code
 */
extern long Cookie;
extern lock_t Statmonvp_lock;
extern char *Lock_client_name;
extern size_t Lock_client_namelen;

extern char *ha_dir;
extern char *ha2_dir;
static size_t ha_dirlen = 0;
static size_t ha2_dirlen = 0;
static int ha_mode = 0;

struct lockd_args {
	char	la_name[MAXHOSTNAMELEN + 1];
	int		la_grace;
	u_int	la_lockshares:1;
	u_int	la_setgrace:1;
        u_int   ha_action:2;   
        char    ha_dir[MAXPATHLEN];
};

/*
 * system call entry for setting canonical client name and other options
 */
int
lockd_sys(char *data, int len, rval_t *rvp)
{
	struct lockd_args args;

	/*
	 * clear the return value
	 */
	if (rvp) {
		rvp->r_v.r_v1 = rvp->r_v.r_v2 = 0;
	}
	/*
	 * make sure we have the correct permissions
	 */
	if (!cap_able(CAP_DAC_WRITE | CAP_MAC_READ | CAP_MAC_WRITE | CAP_NETWORK_MGT)) {
		return(EPERM);
	}
	/*
	 * copy in the argument
	 * it contains no addresses or longs, so no translation is required
	 * make sure the specified length is correct
	 */
	if (len != sizeof(args)) {
		return(EINVAL);
	}
	if (copyin(data, &args, sizeof(args))) {
		return(EFAULT);
	}


	/*
	 * for HA systems,  first check if ha_action is set.  If so,
	 * add or remove the HA directory accordingly.
 	 */
	if (args.ha_action ==1) {  /* add ha_dir */
		if (ha_dir == NULL) {
			if(create_ha_dir(&ha_dir, args.ha_dir, &ha_dirlen))
				return(EFAULT);;
			ha_mode = 1;
		} else if (ha2_dir == NULL) {
			if(create_ha_dir(&ha2_dir, args.ha_dir, &ha2_dirlen))
				return(EFAULT);
			ha_mode = 1;
		} else {
			cmn_err(CE_WARN, "Error: Two ha_dirs already exist\n");
			return(EFAULT);

		}

	} else if (args.ha_action == 2) { /* delete ha_dir */
		if(args.ha_dir[0] == '\0') {
			/*  delete both ha directories */
			if(ha_dir) {
				NLM_KMEM_FREE(ha_dir, ha_dirlen);
				ha_dir = NULL;
				ha_dirlen = 0;
			}
			if(ha2_dir) {
				NLM_KMEM_FREE(ha2_dir, ha2_dirlen);
				ha2_dir = NULL;
				ha2_dirlen = 0;
			}
			ha_mode = 0;
		} else if (compare_dirname(ha_dir, args.ha_dir)==0) {
			NLM_KMEM_FREE(ha_dir, ha_dirlen);
		        ha_dir = NULL;
			ha_dirlen = 0;
			if (ha2_dir == NULL)
				ha_mode = 0;
		} else if (compare_dirname(ha2_dir, args.ha_dir)==0) {
			NLM_KMEM_FREE(ha2_dir, ha2_dirlen);
			ha2_dir = NULL;
			ha2_dirlen = 0;
			if (ha_dir == NULL)
				ha_mode = 0;
		} else {   
			return(EFAULT);
		}
	
	} else if (args.ha_action == 3) {  /* ha_dir query  */
		/*
		 * use ha_action field to return the number of
		 * ha dirs currently active.   Use ha_dir and
		 * la_name fields to return the strings back to
		 * user space 
		 */
		if( !ha_mode) {
			args.ha_action = 0;
			args.ha_dir[0] 	= '\0';
			args.la_name[0]	= '\0';
		} else {
			if(ha_dir && ha2_dir) {
				args.ha_action = 2;
				strcpy(args.ha_dir, ha_dir);
				strcpy(args.la_name, ha2_dir);
			} else if(ha_dir) {
				args.ha_action = 1;
				strcpy(args.ha_dir, ha_dir);
				args.la_name[0] = '\0';
			} else if(ha2_dir) {
				args.ha_action = 1;
				strcpy(args.ha_dir, ha2_dir);
				args.la_name[0] = '\0' ;
			} else {
				cmn_err(CE_WARN,
				    "error: ha_mode set, but ha_dirs empty\n");
				return(EFAULT);
			}	
		}	
		if( copyout( &args, data, sizeof(args)) < 0)
			return(EFAULT);
			
	}  else {  /* ha_action == 0 i.e register canonical name */

		/*
		 * if the name has not been set,or the new name is longer than will
		 * fit in the memory allocated, free the existing memory and get
		 * some more of the appropriate length
		 */
		if (Lock_client_namelen <= strlen(args.la_name)) {
			if (Lock_client_name) {
		 	 	/*
				 * free the memory previously allocated for the 
				 * canonical name
				 */
				NLM_KMEM_FREE(Lock_client_name, Lock_client_namelen);
			}
			/*
			 * record the memory length so we know how much to free 
			 * later.  allocate new memory for the canonical name
			 */
			Lock_client_namelen = strlen(args.la_name) + 1;
			Lock_client_name = NLM_KMEM_ALLOC(Lock_client_namelen, KM_SLEEP);
		}
		/*
		 * copy the canonical cleint name
		 */
		(void)strcpy(Lock_client_name, args.la_name);
		/*
		 * if the grace period is to be set, set it
		 */
		if (args.la_setgrace) {
			lockd_grace_period = args.la_grace;
		}
		/*
		 * set the value for locking share requests
		 */
		lock_share_requests = (int)args.la_lockshares;
		
	}  /* end of (ha_action==0)  */

	return(0);
}


/* ARGSUSED */
static void
end_grace_period(void *arg)
{
	in_grace_period = 0;
}

void
start_lockd(void)
{
	extern void cancel_mon(void);
	extern int nproc;
	extern int max_lockd_procs;

	NLM_DEBUG(NLMDEBUG_PROCS,
		printf("start_lockd: new daemon started, count %d\n", lockd_count));
	if (atomicAddInt(&lockd_count, 1) == 1) {
		(void)timeout(end_grace_period, NULL, lockd_grace_period * HZ);
		cancel_mon();
	}
	if (!max_lockd_procs) {
		max_lockd_procs = (2 * nproc) / 3;
	}
}

void
stop_lockd(void)
{
	vnode_t *vp = NULL;
	int ospl;

	/*
	 * The last daemon to exit releases all locks.
	 * daemons exiting on receipt of a signal will have code 1
	 */
	if (atomicAddInt(&lockd_count, -1) == 0) {
		release_all_locks(NULL);
		ospl = mutex_spinlock(&Statmonvp_lock);
		if (vp = Statmonvp) {
			Statmonvp = NULL;
			mutex_spinunlock(&Statmonvp_lock, ospl);
			VN_RELE(vp);
		} else {
			mutex_spinunlock(&Statmonvp_lock, ospl);
		}
	}
	NLM_DEBUG(NLMDEBUG_PROCS,
		printf("stop_lockd: daemon exiting, count %d\n", lockd_count));
	ASSERT(lockd_count >= 0);
}

/*
 * -----------------
 * Support functions
 * -----------------
 */


static int
create_ha_dir(char **ha_dir, char *name, size_t *dir_namelen )
{
	int error = 0;
	vattr_t vattr;
	vnode_t *vp;

	/*
	 * record the memory length so we know how much to free later
	 */
	*dir_namelen = strlen(name) + 1;
	*ha_dir = NLM_KMEM_ALLOC(*dir_namelen, KM_SLEEP);
	strcpy(*ha_dir, name); 

	/* 
	 * now create the vnode for this directory
	 */
	vattr.va_type = VDIR;
        vattr.va_mode = 0755;
        vattr.va_mask = AT_TYPE|AT_MODE;
	error = vn_create(*ha_dir, UIO_SYSSPACE, &vattr,
                    VEXCL, 0, &vp, CRMKDIR, NULL);

	if(error && error != EEXIST) {
		cmn_err(CE_WARN,
			"Unable to create %s, error %d\n",
			*ha_dir, error);
		return error;
	}
	if(vp && !error)
		VN_RELE(vp);
	return 0;
}


static int
compare_dirname(char *ha_dir, char *dirname)
{
	if(ha_dir == NULL)
		return -1;
	return strcmp(ha_dir, dirname);
}

#define DEQUEUE_RESPONSE_WAIT(waitp, slot) \
{ \
	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS))); \
	if ((waitp)->nw_state & NW_WAITING) { \
		if ((waitp)->nw_prev) { \
			(waitp)->nw_prev->nw_next = (waitp)->nw_next; \
		} else if (nlm_response_wait_hash[(slot)] == (waitp)) { \
			nlm_response_wait_hash[(slot)] = (waitp)->nw_next; \
		} \
		if ((waitp)->nw_next) { \
			(waitp)->nw_next->nw_prev = (waitp)->nw_prev; \
		} \
		(waitp)->nw_next = (waitp)->nw_prev = NULL; \
		bitlock_clr(&(waitp)->nw_state, NW_LOCK, NW_WAITING); \
	} \
}

#define DEQUEUE_BLOCKED_WAIT(waitp, slot) \
{ \
	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS))); \
	if ((waitp)->nw_state & NW_WAITING) { \
		if ((waitp)->nw_prev) { \
			(waitp)->nw_prev->nw_next = (waitp)->nw_next; \
		} else if (nlm_blocked_wait_hash[(slot)] == (waitp)) { \
			nlm_blocked_wait_hash[(slot)] = (waitp)->nw_next; \
		} \
		if ((waitp)->nw_next) { \
			(waitp)->nw_next->nw_prev = (waitp)->nw_prev; \
		} \
		(waitp)->nw_next = (waitp)->nw_prev = NULL; \
		bitlock_clr(&(waitp)->nw_state, NW_LOCK, NW_WAITING); \
	} \
}

#define ENQUEUE_RESPONSE_WAIT(waitp, slot) \
{ \
	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS))); \
	bitlock_set(&(waitp)->nw_state, NW_LOCK, NW_WAITING); \
	if (nlm_response_wait_hash[slot]) { \
		nlm_response_wait_hash[slot]->nw_prev = (waitp); \
	} \
	(waitp)->nw_prev = NULL; \
	(waitp)->nw_next = nlm_response_wait_hash[slot]; \
	nlm_response_wait_hash[slot] = waitp; \
}

#define ENQUEUE_BLOCKED_WAIT(waitp, slot) \
{ \
	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS))); \
	bitlock_set(&(waitp)->nw_state, NW_LOCK, NW_WAITING); \
	if (nlm_blocked_wait_hash[slot]) { \
		nlm_blocked_wait_hash[slot]->nw_prev = (waitp); \
	} \
	(waitp)->nw_prev = NULL; \
	(waitp)->nw_next = nlm_blocked_wait_hash[slot]; \
	nlm_blocked_wait_hash[slot] = waitp; \
}

static nlm_wait_t *
alloc_wait(void)
{
	nlm_wait_t *waitp;

	waitp = (nlm_wait_t *)NLM_ZONE_ZALLOC(nlm_wait_zone, KM_SLEEP);
	sv_init(&waitp->nw_wait, SV_DEFAULT, "NLM wait");
	waitp->nw_next = waitp->nw_prev = NULL;
#if DEBUG
    {
	/* atomicAddInt returns unsigned, so
	 * applying "> 0" to it is not as
	 * strong a test as it could be.
	 */
	int new_nlm_wait_alloc = atomicAddInt(&nlm_wait_alloc, 1);
	ASSERT(new_nlm_wait_alloc > 0);
    }
#endif
	return(waitp);
}

static void
free_wait(nlm_wait_t *waitp)
{
	ASSERT(!(waitp->nw_state & NW_WAITING));
	sv_destroy(&waitp->nw_wait);
	NLM_ZONE_FREE(nlm_wait_zone, waitp);
#if DEBUG
    {
	/* atomicAddInt returns unsigned, so
	 * applying ">= 0" to it is pointless.
	 */
	int new_nlm_wait_alloc = atomicAddInt(&nlm_wait_alloc, -1);
	ASSERT(new_nlm_wait_alloc >= 0);
    }
#endif
}

static void
wake_nlm_message(nlm4_stats status, long cookie, struct flock *flp)
{
	int ospl;
	nlm_wait_t *waitp = NULL;
	int slot;

	NLM_DEBUG(NLMDEBUG_TRACE, printf("wake_nlm_message: %s cookie %d\n",
		nlm4stats_to_str(status), cookie));
	/*
	 * We will get this when an asynchronous lock request has been
	 * issued by some process locally.  Thus, we will need to wake
	 * the waiting process.  Use the cookie to locate the waiting
	 * process.  The cookie uniquely identifies a lock request.
	 */
	slot = (int)(cookie & WAIT_HASH_MASK);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("wake_nlm_message: %s cookie %d hash slot %d\n",
		nlm4stats_to_str(status), cookie, slot));
	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS)));
	ospl = mutex_spinlock(&nlm_wait_lock);
	for (waitp = nlm_response_wait_hash[slot]; waitp; waitp = waitp->nw_next) {
		ASSERT(VALID_ADDR(waitp));
		ASSERT(waitp->nw_state & NW_WAITING);
		/*
		 * compare the supplied cookie to the one in the wait structure
		 * if n_bytes is not long aligned, use bcmp, otherwise cast and
		 * use ==
		 */
		if (cookie == waitp->nw_cookie) {
			bitlock_set(&(waitp)->nw_state, NW_LOCK, NW_REPLY);
			waitp->nw_status = status;
			if (flp) {
				waitp->nw_flock = *flp;
			} else {
				waitp->nw_flock = zero_flock;
			}
			NLM_DEBUG(NLMDEBUG_WAIT,
				printf("wake_nlm_message: wake %d, sv 0x%p\n",
					waitp->nw_cookie, &waitp->nw_wait));
			sv_broadcast(&waitp->nw_wait);
			/*
			 * If the request was granted, we must dequeue the request.
			 * If we do not do this, there is the possibility that a
			 * blocked reply will arrive after the granted reply.
			 */
			if (status == NLM4_GRANTED) {
				DEQUEUE_RESPONSE_WAIT(waitp, slot);
			}
			break;
		}
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_ERROR,
		if (!waitp)
			printf("wake_nlm_message: no wait found for cookie %d\n",
				cookie));
	mutex_spinunlock(&nlm_wait_lock, ospl);
}

static int
wake_nlm_blocked(nlm4_stats status, struct flock *flp)
{
	int ospl;
	nlm_wait_t *waitp = NULL;
	int slot;

	/*
	 * We will get this when an asynchronous lock request has been
	 * issued by some process locally.  Thus, we will need to wake
	 * the waiting process.  Use the cookie to locate the waiting
	 * process.  The cookie uniquely identifies a lock request.
	 */
	slot = (int)(flp->l_pid & WAIT_HASH_MASK);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("wake_nlm_blocked: %s pid %d hash slot %d\n",
		nlm4stats_to_str(status), flp->l_pid, slot));
	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS)));
	ospl = mutex_spinlock(&nlm_wait_lock);
	for (waitp = nlm_blocked_wait_hash[slot]; waitp; waitp = waitp->nw_next) {
		ASSERT(VALID_ADDR(waitp));
		ASSERT(waitp->nw_state & NW_WAITING);
		/*
		 * compare the supplied cookie to the one in the wait structure
		 */
		if (flp->l_pid == waitp->nw_cookie) {
			bitlock_set(&(waitp)->nw_state, NW_LOCK, NW_REPLY);
			waitp->nw_status = status;
			if (flp) {
				waitp->nw_flock = *flp;
			} else {
				waitp->nw_flock = zero_flock;
			}
			NLM_DEBUG(NLMDEBUG_WAIT,
				printf("wake_nlm_blocked: wake %d, sv 0x%p\n",
					waitp->nw_cookie, &waitp->nw_wait));
			sv_broadcast(&waitp->nw_wait);
			/*
			 * If the request was granted, we must dequeue the request.
			 * If we do not do this, there is the possibility that a
			 * blocked reply will arrive after the granted reply.
			 */
			if (status == NLM4_GRANTED) {
				DEQUEUE_BLOCKED_WAIT(waitp, slot);
			}
			break;
		}
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_ERROR,
		if (!waitp) {
			vproc_t *vpr = VPROC_LOOKUP(flp->l_pid);
			printf("wake_nlm_blocked: no wait found for pid %d%s %s "
				"%lld,%lld\n", flp->l_pid, vpr ? "" : "(exited)",
				locktype_to_str(flp->l_type), flp->l_start, flp->l_len);
			if (vpr) VPROC_RELE(vpr);
		}
	);
	mutex_spinunlock(&nlm_wait_lock, ospl);
	return(waitp != NULL);
}

static void
nlm_wait_timeout(nlm_wait_t *waitp)
{
	bitlock_set(&waitp->nw_state, NW_LOCK, NW_TIMEOUT);
	sv_signal(&waitp->nw_wait);
}

/*
 * Wake all requests waiting for IP address ipaddr.
 */
void
wake_nlm_requests(struct in_addr *ipaddr)
{
	int ospl;
	nlm_wait_t *waitp;
	int slot;

	NLM_DEBUG(NLMDEBUG_NOTIFY,
		printf("wake_nlm_requests: waking pending requests to IP addr %s\n",
			inet_ntoa(*ipaddr)));
	/*
	 * Traverse the wait entry hash table looking for entries with
	 * IP addresses matching the given IP address.  When one is found,
	 * enter a callout for nlm_wait_timeout to time the request out
	 * after the specified number of ticks (timeo).  If timeo is 0,
	 * just call nlm_wait_timeout.
	 */
	ospl = mutex_spinlock(&nlm_wait_lock);
	for (slot = 0; slot < WAIT_HASH_SLOTS; slot++) {
		for (waitp = nlm_response_wait_hash[slot]; waitp;
			waitp = waitp->nw_next) {
				if (waitp->nw_ipaddr == ipaddr->s_addr) {
					NLM_DEBUG(NLMDEBUG_NOTIFY,
						printf("wake_nlm_requests: waking 0x%p, hash slot %d\n",
							waitp, slot));
					nlm_wait_timeout(waitp);
				}
		}
		for (waitp = nlm_blocked_wait_hash[slot]; waitp;
			waitp = waitp->nw_next) {
				if (waitp->nw_ipaddr == ipaddr->s_addr) {
					NLM_DEBUG(NLMDEBUG_NOTIFY,
						printf("wake_nlm_requests: waking 0x%p, hash slot %d\n",
							waitp, slot));
					nlm_wait_timeout(waitp);
				}
		}
	}
	mutex_spinunlock(&nlm_wait_lock, ospl);
}

#ifdef DEBUG
static int
nlm_response_waiting(int slot, long cookie)
{
	int ospl;
	nlm_wait_t *waitp;

	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS)));
	ospl = mutex_spinlock(&nlm_wait_lock);
	for (waitp = nlm_response_wait_hash[slot]; waitp; waitp = waitp->nw_next) {
		if (waitp->nw_cookie == cookie) {
			return(1);
		}
	}
	mutex_spinunlock(&nlm_wait_lock, ospl);
	return(0);
}
#endif /* DEBUG */

void *
get_nlm_response_wait(long cookie, struct in_addr *ipaddr)
{
	int slot = (int)(cookie & WAIT_HASH_MASK);
	nlm_wait_t *waitp = NULL;
	int ospl;

	ASSERT(ipaddr->s_addr);
	ASSERT(!nlm_response_waiting(slot, cookie));
	waitp = alloc_wait();
	waitp->nw_cookie = cookie;
	waitp->nw_ipaddr = (long)ipaddr->s_addr;
	waitp->nw_state = NW_RESPONSE;
	ospl = mutex_spinlock(&nlm_wait_lock);
	ENQUEUE_RESPONSE_WAIT(waitp, slot);
	mutex_spinunlock(&nlm_wait_lock, ospl);
	return((void *)waitp);
}

#ifdef DEBUG
static int
nlm_blocked_waiting(int slot, pid_t pid)
{
	int ospl;
	nlm_wait_t *waitp;

	ASSERT(((slot) >= 0) && ((slot < WAIT_HASH_SLOTS)));
	ospl = mutex_spinlock(&nlm_wait_lock);
	for (waitp = nlm_blocked_wait_hash[slot]; waitp; waitp = waitp->nw_next) {
		if (waitp->nw_cookie == pid) {
			return(1);
		}
	}
	mutex_spinunlock(&nlm_wait_lock, ospl);
	return(0);
}
#endif /* DEBUG */

void *
get_nlm_blocked_wait(pid_t pid, struct in_addr *ipaddr)
{
	int slot = (int)(pid & WAIT_HASH_MASK);
	nlm_wait_t *waitp = NULL;
	int ospl;

	ASSERT(ipaddr->s_addr);
	ASSERT(!nlm_blocked_waiting(slot, pid));
	waitp = alloc_wait();
	waitp->nw_cookie = pid;
	waitp->nw_ipaddr = (long)ipaddr->s_addr;
	waitp->nw_state = NW_BLOCKED;
	ospl = mutex_spinlock(&nlm_wait_lock);
	ENQUEUE_BLOCKED_WAIT(waitp, slot);
	mutex_spinunlock(&nlm_wait_lock, ospl);
	return((void *)waitp);
}

void
release_nlm_wait(void *wp)
{
	nlm_wait_t *waitp;
	int slot;
	int ospl;

	ASSERT(wp != NULL);
	waitp = (nlm_wait_t *)wp;
	slot = (int)(waitp->nw_cookie & WAIT_HASH_MASK);
	ospl = mutex_spinlock(&nlm_wait_lock);
	if (waitp->nw_state & NW_WAITING) {
		if (waitp->nw_state & NW_RESPONSE) {
			DEQUEUE_RESPONSE_WAIT(waitp, slot);
		} else {
			DEQUEUE_BLOCKED_WAIT(waitp, slot);
		}
	}
	mutex_spinunlock(&nlm_wait_lock, ospl);
	free_wait(waitp);
}

/* ARGSUSED */
int
await_nlm_response(void *wp, struct in_addr *ipaddr, flock_t *ld,
	nlm4_stats *status, int timeo)
{
	int error;
	int ospl;
	toid_t id;
	nlm_wait_t *waitp = (nlm_wait_t *)wp;

	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("await_nlm_response: pid %d\n", (int)current_pid()));
	ASSERT(waitp);
restart:
	ASSERT(waitp->nw_ipaddr);
	ASSERT(!ipaddr || (ipaddr->s_addr == waitp->nw_ipaddr));
	ospl = mutex_spinlock(&nlm_wait_lock);
	/*
	 * check for the reply
	 */
	if (waitp->nw_state & NW_REPLY) {
		*status = waitp->nw_status;
		if (ld) {
			*ld = waitp->nw_flock;
		}
		/*
		 * turn off NW_REPLY here while we still hold nlm_wait_lock
		 * in the case of asynchronous NLM, the caller will need
		 * to wait again if the server returned a blocked status
		 * what we want to avoid is the caller looping and always
		 * seeing NW_REPLY for the same reply
		 */
		bitlock_clr(&waitp->nw_state, NW_LOCK, NW_REPLY);
		mutex_spinunlock(&nlm_wait_lock, ospl);
		ASSERT(!issplhi(getsr()));
		return(0);
	}
	/*
	 * no reply yet, so wait for it
	 */
	if (timeo) {
		id = timeout(nlm_wait_timeout, (void *)waitp, timeo * HZ / 10);
	}
	error = sv_wait_sig(&waitp->nw_wait, PZERO + 1,
		&nlm_wait_lock, ospl);
	if (timeo) {
		untimeout(id);
	}
	switch (error) {
		default:
			NLM_DEBUG(NLMDEBUG_ERROR,
				printf("await_nlm_response: unexpected sv_wait_sig return "
					"value %d\n", error));
		case 0:
			ospl = mutex_spinlock(&nlm_wait_lock);
			switch (waitp->nw_state & NW_MASK) {
				case NW_WAITING:
					mutex_spinunlock(&nlm_wait_lock, ospl);
					goto restart;
				case NW_TIMEOUT | NW_WAITING:
				case 0:
				case NW_TIMEOUT:
					error = ETIMEDOUT;
					break;
				case NW_REPLY | NW_WAITING:
				case NW_REPLY | NW_TIMEOUT | NW_WAITING:
				case NW_REPLY:
				case NW_REPLY | NW_TIMEOUT:
					/*
					 * woken up by NLM response
					 * NW_WAITING is not set, so there is no need to dequeue
					 */
					*status = waitp->nw_status;
					if (ld) {
						*ld = waitp->nw_flock;
					}
					bitlock_clr(&waitp->nw_state, NW_LOCK, NW_REPLY);
					error = 0;
			}
			mutex_spinunlock(&nlm_wait_lock, ospl);
			break;
		case -1:
			error = EINTR;
			break;
	}
	ASSERT(!issplhi(getsr()));
	return(error);
}

/*
 * -----------------------
 * procedures for NLM_PROG
 * -----------------------
 */

/*
 * unsupported NLM or NLM4 procedure
 */
/* ARGSUSED */
static int
nlm_badproc(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	return(NFS_RPC_PROC);
}

/*
 * invalid NLM version (probably version 2)
 */
/* ARGSUSED */
static int
nlm_badvers(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	return(NFS_RPC_VERS);
}

/*
 * procedure 0 for all versions
 */
/* ARGSUSED */
static int
proc_nlm_null(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	return(0);
}

/*
 * -------------------------------------------------------------
 * NLM_VERS and NLM_VERSX client procedures for asynchronous RPC
 * -------------------------------------------------------------
 */

/*
 * NLM_GRANT and NLM_GRANT_MSG
 */
/* ARGSUSED */
static int
proc_nlm_granted(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	nlm_testargs *argp = &args->testargs_1;
	nlm_res *resp = &res->statres_1;
	struct flock fl;

	if (req->rq_vers == NLM_VERSX) {
		if (argp->alock.l_len == 0xffffffff) {
			argp->alock.l_len = 0;
		}
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_GRANTED,
		printf("%s: grant for pid %d, offset %lld len %lld cookie %s\n",
			nlmproc_to_str(req->rq_vers, req->rq_proc), (int)argp->alock.svid,
			(off_t)argp->alock.l_offset, (off_t)argp->alock.l_len,
			netobj_to_str(&argp->cookie)));
	/*
	 * copy the cookie to the response
	 * we'll need it regardless of what happens below
	 */
	resp->cookie = argp->cookie;
	/*
	 * If we're still in the grace period, deny this lock.
	 */
	if (in_grace_period) {
		resp->stat.stat = nlm_denied_grace_period;
	} else if (argp->alock.oh.n_len <
		(sizeof(struct owner_handle) + hostnamelen)) {
			cmn_err(CE_NOTE,
				"!Short object handle length in NLM grant proc %d from %s, "
				"caller name %s\n", req->rq_proc,
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr),
				argp->alock.caller_name ? argp->alock.caller_name : "NULL");
			resp->stat.stat = nlm_denied;
	} else {
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = 0;
		fl.l_pid = argp->alock.svid;
		if (wake_nlm_blocked(NLM4_GRANTED, &fl)) {
			resp->stat.stat = nlm_granted;
		} else {
			resp->stat.stat = nlm_denied;
			NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_GRANTED,
				printf("%s: grant for pid %d denied\n",
					nlmproc_to_str(req->rq_vers, req->rq_proc),
					(int)argp->alock.svid));
		}
	}
	return(0);
}

#define COPYRES		1
#define FREERES		2

/* ARGSUSED */
static void
proc_nlm_res(int cmd, caddr_t src, caddr_t dst, int len)
{
	struct nlm_res *sr = (struct nlm_res *)src;
	struct nlm_res *dr = (struct nlm_res *)dst;

	switch (cmd) {
		case COPYRES:
			dr->stat = sr->stat;
			dup_netobj(&sr->cookie, &dr->cookie);
			break;
		case FREERES:
			free_netobj(&sr->cookie);
			break;
		default:
			ASSERT((cmd == COPYRES) || (cmd == FREERES));
	}
}

/* ARGSUSED */
static int
nlm_test_res(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct flock fl;
	long cookie;

	if (args->testres_1.cookie.n_len) {
		/*
		 * Use the cookie to locate the waiting process and transfer the
		 * args to that process.
		 */
		nlm_testrply_to_flock(&args->testres_1.stat, &fl);
		if ((__psint_t)args->testres_1.cookie.n_bytes & (sizeof(long) - 1)) {
			bcopy(args->testres_1.cookie.n_bytes, &cookie, sizeof(long));
		} else {
			cookie = *(long *)args->testres_1.cookie.n_bytes;
		}
		wake_nlm_message((nlm4_stats)args->testres_1.stat.stat, cookie, &fl);
	} else {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm_test_res: empty cookie from %s\n",
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr)));
	}
	return(0);
}

/* ARGSUSED */
static int
nlm_stat_res(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	long cookie;

	if (args->statres_1.cookie.n_len) {
		/*
		 * Use the cookie to locate the waiting process and transfer the
		 * args to that process.
		 */
		if ((__psint_t)args->statres_1.cookie.n_bytes & (sizeof(long) - 1)) {
			bcopy(args->statres_1.cookie.n_bytes, &cookie, sizeof(long));
		} else {
			cookie = *(long *)args->statres_1.cookie.n_bytes;
		}
		wake_nlm_message((nlm4_stats)args->statres_1.stat.stat, cookie, NULL);
	} else {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm_stat_res: empty cookie from %s\n",
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr)));
	}
	return(0);
}

/* ARGSUSED */
static int
nlm_granted_res(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct flock fl;
	long cookie;

	if (args->testres_1.cookie.n_len) {
		/*
		 * Use the cookie to locate the waiting process and transfer the
		 * args to that process.
		 */
		nlm_testrply_to_flock(&args->testres_1.stat, &fl);
		if ((__psint_t)args->testres_1.cookie.n_bytes & (sizeof(long) - 1)) {
			bcopy(args->testres_1.cookie.n_bytes, &cookie, sizeof(long));
		} else {
			cookie = *(long *)args->testres_1.cookie.n_bytes;
		}
		wake_nlm_message((nlm4_stats)args->testres_1.stat.stat, cookie, &fl);
	} else {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm_granted_res: empty cookie from %s\n",
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr)));
	}
	return(0);
}

/* ARGSUSED */
static int
proc_nlm4_granted(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	nlm4_testargs *argp = &args->testargs_4;
	nlm4_res *resp = &res->statres_4;
	struct flock fl;

	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_GRANTED,
		printf("%s: grant for pid %d, offset %lld len %lld cookie %s\n",
			nlmproc_to_str(req->rq_vers, req->rq_proc), (int)argp->alock.svid,
			(off_t)argp->alock.l_offset, (off_t)argp->alock.l_len,
			netobj_to_str(&argp->cookie)));
	/*
	 * copy the cookie to the response
	 * we'll need it regardless of what happens below
	 */
	resp->cookie = argp->cookie;
	/*
	 * If we're still in the grace period, deny this lock.
	 */
	if (in_grace_period) {
		resp->stat.stat = NLM4_DENIED_GRACE_PERIOD;
		NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_GRANTED,
			printf("%s: grant for pid %d denied (grace period)\n",
				nlmproc_to_str(req->rq_vers, req->rq_proc),
				(int)argp->alock.svid));
	} else if (argp->alock.oh.n_len <
		(sizeof(struct owner_handle) + hostnamelen)) {
			cmn_err(CE_NOTE,
				"!Short object handle length in NLM4 grant proc %d from %s, "
				"caller name %s\n", req->rq_proc,
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr),
				argp->alock.caller_name ? argp->alock.caller_name : "NULL");
			resp->stat.stat = NLM4_FAILED;
	} else {
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = 0;
		fl.l_pid = argp->alock.svid;
		if (wake_nlm_blocked(NLM4_GRANTED, &fl)) {
			resp->stat.stat = NLM4_GRANTED;
		} else {
			resp->stat.stat = NLM4_DENIED;
			NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_GRANTED,
				printf("%s: grant for pid %d denied\n",
					nlmproc_to_str(req->rq_vers, req->rq_proc),
					(int)argp->alock.svid));
		}
	}
	return(0);
}

/* ARGSUSED */
static void
proc_nlm4_res(int cmd, caddr_t src, caddr_t dst, int len)
{
	struct nlm4_res *sr = (struct nlm4_res *)src;
	struct nlm4_res *dr = (struct nlm4_res *)dst;

	switch (cmd) {
		case COPYRES:
			dr->stat = sr->stat;
			dup_netobj(&sr->cookie, &dr->cookie);
			break;
		case FREERES:
			free_netobj(&sr->cookie);
			break;
		default:
			ASSERT((cmd == COPYRES) || (cmd == FREERES));
	}
}

/* ARGSUSED */
static void
proc_nlm4_shareres(int cmd, caddr_t src, caddr_t dst, int len)
{
	struct nlm4_shareres *sr = (struct nlm4_shareres *)src;
	struct nlm4_shareres *dr = (struct nlm4_shareres *)dst;

	switch (cmd) {
		case COPYRES:
			ASSERT(len >= sizeof(struct nlm4_shareres));
			bcopy(src, dst, len);
			dup_netobj(&sr->cookie, &dr->cookie);
			break;
		case FREERES:
			free_netobj(&sr->cookie);
			break;
		default:
			ASSERT((cmd == COPYRES) || (cmd == FREERES));
	}
}

/* ARGSUSED */
static int
nlm4_test_res(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct flock fl;
	long cookie;

	if (args->testres_4.cookie.n_len) {
		/*
		 * Use the cookie to locate the waiting process and transfer the
		 * args to that process.
		 */
		if ((__psint_t)args->testres_4.cookie.n_bytes & (sizeof(long) - 1)) {
			bcopy(args->testres_4.cookie.n_bytes, &cookie, sizeof(long));
		} else {
			cookie = *(long *)args->testres_4.cookie.n_bytes;
		}
		nlm4_testrply_to_flock(&args->testres_4.stat, &fl);
		wake_nlm_message(args->testres_4.stat.stat, cookie, &fl);
	} else {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm4_test_res: empty cookie from %s\n",
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr)));
	}
	return(0);
}

/* ARGSUSED */
static int
nlm4_stat_res(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	long cookie;

	if (args->statres_4.cookie.n_len) {
		/*
		 * Use the cookie to locate the waiting process and transfer the
		 * args to that process.
		 */
		if ((__psint_t)args->statres_4.cookie.n_bytes & (sizeof(long) - 1)) {
			bcopy(args->statres_4.cookie.n_bytes, &cookie, sizeof(long));
		} else {
			cookie = *(long *)args->statres_4.cookie.n_bytes;
		}
		wake_nlm_message(args->statres_4.stat.stat, cookie, NULL);
	} else {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm4_stat_res: empty cookie from %s\n",
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr)));
	}
	return(0);
}

/* ARGSUSED */
static int
nlm4_granted_res(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct flock fl;
	long cookie;

	if (args->testres_4.cookie.n_len) {
		/*
		 * Use the cookie to locate the waiting process and transfer the
		 * args to that process.
		 */
		if ((__psint_t)args->testres_4.cookie.n_bytes & (sizeof(long) - 1)) {
			bcopy(args->testres_4.cookie.n_bytes, &cookie, sizeof(long));
		} else {
			cookie = *(long *)args->testres_4.cookie.n_bytes;
		}
		nlm4_testrply_to_flock(&args->testres_4.stat, &fl);
		wake_nlm_message(args->testres_4.stat.stat, cookie, &fl);
	} else {
		NLM_DEBUG(NLMDEBUG_ERROR,
			printf("nlm4_granted_res: empty cookie from %s\n",
				inet_ntoa(req->rq_xprt->xp_raddr.sin_addr)));
	}
	return(0);
}

/*
 * lockd dispatch table
 * Indexed by version,proc
 */

#define LOCKD_NPROC		24

/*
 * dis_proc			-	procedure to be executed
 * dis_xdrargs		-	xdr function to decode arguments
 * dis_argsz		-	arguement size
 * dis_xdrres		-	xdr function to encode results
 * dis_ressz		-	results size
 * dis_getexp		-	function to get export info entry
 * dis_asyncreply	-	asynchronous reply function
 * dis_idempotent	-	idempotent function flag
 *
 * All asynchronous request messages (e.g., NLM_TEST_MSG, NLM_LOCK_MSG,
 * etc.) must be non-idenpotent because retransmissions must be detected
 * in order to know when to send a portmapper request.
 */
struct lockddisp {
	int			(*dis_proc)(struct svc_req *, union nlm_args_u *,
						union nlm_res_u *, struct exportinfo *, nlm4_stats *);
	xdrproc_t	dis_xdrargs;		/* xdr routine to get args */
	int			dis_argsz;		/* sizeof args */
	xdrproc_t	dis_xdrres;		/* xdr routine to put results */
	int			dis_ressz;		/* size of results */
	int			(*dis_getexp)(struct svc_req *, union nlm_args_u *,
						struct exportinfo **);
	int			(*dis_asyncreply)(struct svc_req *, xdrproc_t,
						union nlm_res_u *);	/* async reply proc */
	bool_t		dis_idempotent;		/* idempontency flag */
	void		(*dis_res)(int, caddr_t, caddr_t, int);	/* funtion to dup & free results */
} lockddisptab[][LOCKD_NPROC]  = {
	{
	/*
	 * VERSION 1 -- NLM_VERS
	 */
	/* NULLPROC = 0 */
	{proc_nlm_null, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	/* NLM_TEST = 1 */
	{proc_nlm_test, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_testres, sizeof(struct nlm_testres), nlm_test_exi, NULL,
		TRUE, NULL},
	/* NLM_LOCK = 2 */
	{proc_nlm_lock, xdr_nlm_lockargs, sizeof(struct nlm_lockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_lock_exi, NULL,
		FALSE, proc_nlm_res},
	/* NLM_CANCEL = 3 */
	{proc_nlm_cancel, xdr_nlm_cancargs, sizeof(struct nlm_cancargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_cancel_exi, NULL, FALSE,
		proc_nlm_res},
	/* NLM_UNLOCK = 4 */
	{proc_nlm_unlock, xdr_nlm_unlockargs, sizeof(struct nlm_unlockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_unlock_exi, NULL, FALSE,
		proc_nlm_res},
	/* NLM_GRANTED = 5 */
	{proc_nlm_granted, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_res, sizeof(struct nlm_res), NULL, NULL, FALSE, proc_nlm_res},
	/* NLM_TEST_MSG = 6 */
	{proc_nlm_test, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_testres, sizeof(struct nlm_testres), nlm_test_exi, nlm_reply,
		TRUE, NULL},
	/* NLM_LOCK_MSG = 7 */
	{proc_nlm_lock, xdr_nlm_lockargs, sizeof(struct nlm_lockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_lock_exi, nlm_reply,
		FALSE, proc_nlm_res},
	/* NLM_CANCEL_MSG = 8 */
	{proc_nlm_cancel, xdr_nlm_cancargs, sizeof(struct nlm_cancargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_cancel_exi, nlm_reply,
		FALSE, proc_nlm_res},
	/* NLM_UNLOCK_MSG = 9 */
	{proc_nlm_unlock, xdr_nlm_unlockargs, sizeof(struct nlm_unlockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_unlock_exi, nlm_reply,
		FALSE, proc_nlm_res},
	/* NLM_GRANTED_MSG = 10 */
	{proc_nlm_granted, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_res, sizeof(struct nlm_res), NULL, nlm_reply, FALSE,
		proc_nlm_res},
	/* NLM_TEST_RES = 11 */
	{nlm_test_res, xdr_nlm_testres, sizeof(struct nlm_testres),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_LOCK_RES = 12 */
	{nlm_stat_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_CANCEL_RES = 13 */
	{nlm_stat_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_UNLOCK_RES = 14 */
	{nlm_stat_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_GRANTED_RES = 15 */
	{nlm_granted_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* empty space, 16 through 19 */
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	/* NLM_SHARE = 20 */
	/* NLM_UNSHARE = 21 */
	/* NLM_NM_LOCK = 22 */
	/* NLM_FREE_ALL = 23 */
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL}
	},
	{
	/*
	 * VERSION 2 -- non-existent
	 */
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	/* empty space, 16 through 19 */
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badvers, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL}
	},
	{
	/*
	 * VERSION 3 -- NLM_VERSX
	 */
	/* NULLPROC = 0 */
	{proc_nlm_null, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	/* NLM_TEST = 1 */
	{proc_nlm_test, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_testres, sizeof(struct nlm_testres), nlm_test_exi, NULL,
		TRUE, NULL},
	/* NLM_LOCK = 2 */
	{proc_nlm_lock, xdr_nlm_lockargs, sizeof(struct nlm_lockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_lock_exi, NULL,
		FALSE, proc_nlm_res},
	/* NLM_CANCEL = 3 */
	{proc_nlm_cancel, xdr_nlm_cancargs, sizeof(struct nlm_cancargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_cancel_exi, NULL, FALSE,
		proc_nlm_res},
	/* NLM_UNLOCK = 4 */
	{proc_nlm_unlock, xdr_nlm_unlockargs, sizeof(struct nlm_unlockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_unlock_exi, NULL, FALSE,
		proc_nlm_res},
	/* NLM_GRANTED = 5 */
	{proc_nlm_granted, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_res, sizeof(struct nlm_res), NULL, NULL, FALSE, proc_nlm_res},
	/* NLM_TEST_MSG = 6 */
	{proc_nlm_test, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_testres, sizeof(struct nlm_testres), nlm_test_exi, nlm_reply,
		TRUE, NULL},
	/* NLM_LOCK_MSG = 7 */
	{proc_nlm_lock, xdr_nlm_lockargs, sizeof(struct nlm_lockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_lock_exi, nlm_reply,
		FALSE, proc_nlm_res},
	/* NLM_CANCEL_MSG = 8 */
	{proc_nlm_cancel, xdr_nlm_cancargs, sizeof(struct nlm_cancargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_cancel_exi, nlm_reply,
		FALSE, proc_nlm_res},
	/* NLM_UNLOCK_MSG = 9 */
	{proc_nlm_unlock, xdr_nlm_unlockargs, sizeof(struct nlm_unlockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_unlock_exi, nlm_reply,
		FALSE, proc_nlm_res},
	/* NLM_GRANTED_MSG = 10 */
	{proc_nlm_granted, xdr_nlm_testargs, sizeof(struct nlm_testargs),
	    xdr_nlm_res, sizeof(struct nlm_res), NULL, nlm_reply, FALSE,
		proc_nlm_res},
	/* NLM_TEST_RES = 11 */
	{nlm_test_res, xdr_nlm_testres, sizeof(struct nlm_testres),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_LOCK_RES = 12 */
	{nlm_stat_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_CANCEL_RES = 13 */
	{nlm_stat_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_UNLOCK_RES = 14 */
	{nlm_stat_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLM_GRANTED_RES = 15 */
	{nlm_granted_res, xdr_nlm_res, sizeof(struct nlm_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* empty space, 16 through 19 */
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	/* NLM_SHARE = 20 */
	{proc_nlm4_share, xdr_nlm_shareargs, sizeof(struct nlm_shareargs),
	    xdr_nlm_shareres, sizeof(struct nlm_shareres), nlm4_share_exi, NULL,
		FALSE, proc_nlm4_shareres},
	/* NLM_UNSHARE = 21 */
	{proc_nlm4_share, xdr_nlm_shareargs, sizeof(struct nlm_shareargs),
	    xdr_nlm_shareres, sizeof(struct nlm_shareres), nlm4_share_exi, NULL,
		FALSE, proc_nlm4_shareres},
	/* NLM_NM_LOCK = 22 */
	{proc_nlm_lock, xdr_nlm_lockargs, sizeof(struct nlm_lockargs),
	    xdr_nlm_res, sizeof(struct nlm_res), nlm_lock_exi, NULL,
		FALSE, proc_nlm_res},
	/* NLM_FREE_ALL = 23 */
	{nlm_free_all, xdr_nlm_notify, sizeof(struct nlm_notify),
	    xdr_void, 0, NULL, NULL, FALSE, NULL}
	},
	{
	/*
	 * VERSION 4 -- NLM4_VERS
	 */
	/* NULLPROC = 0 */
	{proc_nlm_null, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	/* NLMPROC_TEST = 1 */
	{proc_nlm4_test, xdr_nlm4_testargs, sizeof(struct nlm4_testargs),
	    xdr_nlm4_testres, sizeof(struct nlm4_testres), nlm4_test_exi, NULL,
		TRUE, NULL},
	/* NLMPROC_LOCK = 2 */
	{proc_nlm4_lock, xdr_nlm4_lockargs, sizeof(struct nlm4_lockargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), nlm4_lock_exi, NULL,
		FALSE, proc_nlm4_res},
	/* NLMPROC_CANCEL = 3 */
	{proc_nlm4_cancel, xdr_nlm4_cancargs, sizeof(struct nlm4_cancargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), nlm4_cancel_exi, NULL,
		FALSE, proc_nlm4_res},
	/* NLM_UNLOCK = 4 */
	{proc_nlm4_unlock, xdr_nlm4_unlockargs, sizeof(struct nlm4_unlockargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), nlm4_unlock_exi, NULL,
		FALSE, proc_nlm4_res},
	/* NLMPROC_GRANTED = 5 */
	{proc_nlm4_granted, xdr_nlm4_testargs, sizeof(struct nlm4_testargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), NULL, NULL, FALSE,
		proc_nlm4_res},
	/* NLMPROC_TEST_MSG = 6 */
	{proc_nlm4_test, xdr_nlm4_testargs, sizeof(struct nlm4_testargs),
	    xdr_nlm4_testres, sizeof(struct nlm4_testres), nlm4_test_exi,
		nlm_reply, TRUE, NULL},
	/* NLMPROC_LOCK_MSG = 7 */
	{proc_nlm4_lock, xdr_nlm4_lockargs, sizeof(struct nlm4_lockargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), nlm4_lock_exi, nlm_reply,
		FALSE, proc_nlm4_res},
	/* NLMPROC_CANCEL_MSG = 8 */
	{proc_nlm4_cancel, xdr_nlm4_cancargs, sizeof(struct nlm4_cancargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), nlm4_cancel_exi, nlm_reply,
		FALSE, proc_nlm4_res},
	/* NLMPROC_UNLOCK_MSG = 9 */
	{proc_nlm4_unlock, xdr_nlm4_unlockargs, sizeof(struct nlm4_unlockargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), nlm4_unlock_exi, nlm_reply,
		FALSE, proc_nlm4_res},
	/* NLMPROC_GRANTED_MSG = 10 */
	{proc_nlm4_granted, xdr_nlm4_testargs, sizeof(struct nlm4_testargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), NULL, nlm_reply, FALSE,
		proc_nlm4_res},
	/* NLMPROC_TEST_RES = 11 */
	{nlm4_test_res, xdr_nlm4_testres, sizeof(struct nlm4_testres),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLMPROC_LOCK_RES = 12 */
	{nlm4_stat_res, xdr_nlm4_res, sizeof(struct nlm4_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLMPROC_CANCEL_RES = 13 */
	{nlm4_stat_res, xdr_nlm4_res, sizeof(struct nlm4_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLMPROC_UNLOCK_RES = 14 */
	{nlm4_stat_res, xdr_nlm4_res, sizeof(struct nlm4_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* NLMPROC_GRANTED_RES = 15 */
	{nlm4_granted_res, xdr_nlm4_res, sizeof(struct nlm4_res),
	    NULL, 0, NULL, NULL, TRUE, NULL},
	/* empty space, 16 through 19 */
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	{nlm_badproc, xdr_void, 0,
	    xdr_void, 0, NULL, NULL, TRUE, NULL},
	/* NLMPROC_SHARE = 20 */
	{proc_nlm4_share, xdr_nlm4_shareargs, sizeof(struct nlm4_shareargs),
	    xdr_nlm4_shareres, sizeof(struct nlm4_shareres), nlm4_share_exi, NULL,
		FALSE, proc_nlm4_shareres},
	/* NLMPROC_UNSHARE = 21 */
	{proc_nlm4_share, xdr_nlm4_shareargs, sizeof(struct nlm4_shareargs),
	    xdr_nlm4_shareres, sizeof(struct nlm4_shareres), nlm4_share_exi, NULL,
		FALSE, proc_nlm4_shareres},
	/* NLMPROC_NM_LOCK = 22 */
	{proc_nlm4_lock, xdr_nlm4_lockargs, sizeof(struct nlm4_lockargs),
	    xdr_nlm4_res, sizeof(struct nlm4_res), nlm4_lock_exi, NULL,
		FALSE, proc_nlm4_res},
	/* NLMPROC_FREE_ALL = 23 */
	{nlm4_free_all, xdr_nlm4_notify, sizeof(struct nlm4_notify),
	    xdr_void, 0, NULL, NULL, FALSE, NULL}
	}
};

#ifdef DEBUG
int
nlm_valid_repy_stat(int vers, int proc, union nlm_res_u *res)
{
	int valid = 1;

	switch (vers) {
		case NLM_VERS:
		case NLM_VERSX:
			switch (proc) {
				case 0:
					valid = 1;
					break;
				case NLM_CANCEL:
				case NLM_UNLOCK:
				case NLM_GRANTED:
				case NLM_CANCEL_MSG:
				case NLM_UNLOCK_MSG:
				case NLM_GRANTED_MSG:
					valid = (res->statres_1.stat.stat != nlm_blocked);
					break;
				case NLM_LOCK_MSG:
				case NLM_LOCK:
				case NLM_NM_LOCK:
					valid = 1;
					break;
				case NLM_TEST:
				case NLM_TEST_MSG:
					valid = (res->testres_1.stat.stat != nlm_blocked);
					break;
				case NLM_TEST_RES:
				case NLM_LOCK_RES:
				case NLM_CANCEL_RES:
				case NLM_UNLOCK_RES:
				case NLM_GRANTED_RES:
				case NLM_FREE_ALL:
					valid = (res->testres_1.stat.stat != nlm_blocked);
					break;
				case NLM_SHARE:
				case NLM_UNSHARE:
					valid = (res->shareres_4.stat != nlm_blocked);
					break;
				default:
					valid = 0;
			}
			break;
		case NLM4_VERS:
			switch (proc) {
				case 0:
					valid = 1;
					break;
				case NLMPROC_CANCEL:
				case NLMPROC_UNLOCK:
				case NLMPROC_GRANTED:
				case NLMPROC_CANCEL_MSG:
				case NLMPROC_UNLOCK_MSG:
				case NLMPROC_GRANTED_MSG:
					valid = (res->statres_4.stat.stat != NLM4_BLOCKED);
					break;
				case NLMPROC_LOCK:
				case NLMPROC_NM_LOCK:
				case NLMPROC_LOCK_MSG:
					valid = 1;
					break;
				case NLMPROC_TEST:
				case NLMPROC_TEST_MSG:
					valid = (res->testres_4.stat.stat != NLM4_BLOCKED);
					break;
				case NLMPROC_TEST_RES:
				case NLMPROC_LOCK_RES:
				case NLMPROC_CANCEL_RES:
				case NLMPROC_UNLOCK_RES:
				case NLMPROC_GRANTED_RES:
				case NLMPROC_FREE_ALL:
					valid = (res->shareres_4.stat != NLM4_BLOCKED);
					break;
				case NLMPROC_SHARE:
				case NLMPROC_UNSHARE:
					valid = (res->shareres_4.stat != NLM4_BLOCKED);
					break;
				default:
					valid = 0;
			}
			break;
		default:
			valid = 0;
	}
	return(valid);
}
#endif /* DEBUG */

enum nlmdup_state {NLM_DUP_UNUSED, NLM_DUP_NEW, NLM_DUP_WORKING, NLM_DUP_DONE};

struct nlm_dup_record {
	enum nlmdup_state		nd_state;
	int						nd_slot;
	int32_t					nd_xid;
	struct sockaddr_in		nd_addr;
	union nlm_res_u			nd_result;
	void					(*nd_res)(int, caddr_t, caddr_t, int);
#ifdef DEBUG
	int						nd_proc;
	int						nd_vers;
#endif /* DEBUG */
	struct nlm_dup_record	*nd_next;
	struct nlm_dup_record	*nd_prev;
};

#define NLM_DUPHASH_SLOTS	256
#define NLM_DUPHASH_MASK	(NLM_DUPHASH_SLOTS - 1)

struct nlm_dup_record *NLM_dup_table[NLM_DUPHASH_SLOTS];
struct nlm_dup_record *NLM_dupreqs;
int current_maxdupreqs = 0;
extern int nlm_maxdupreqs;
static int nlmdup_index = 0;
static mutex_t nlmduplock;
static dupwarn_printed = 0;

#define DR_HASH(xid, ip, port) \
	(((int)((xid) + (ip) + (port))) & NLM_DUPHASH_MASK)

static void
nlm_dup_init(void)
{
	mutex_init(&nlmduplock, MUTEX_DEFAULT, "nlm dup");
}

static struct nlm_dup_record *
nlm_dup(struct svc_req *req, caddr_t res)
{
	int slot;
	struct nlm_dup_record *dr;
	int32_t xid = req_to_xid(req);
	struct sockaddr_in *sin = svc_getcaller(req->rq_xprt);

	slot = DR_HASH(xid, sin->sin_addr.s_addr, sin->sin_port);
	mutex_lock(&nlmduplock, PZERO);
	if (!NLM_dupreqs) {
		int i;

		if (nlm_maxdupreqs == 0) {
			/*
			 * Auto-configure based on physical memory.
			 * Use the minimum for 16MB and 32 MB systems, then
			 * scale up to about 500 MB where we hit the max.
			 */
			i = physmem;
			i -= 4096;	/* min pages on small machines */
			i /= 30;	/* pages per request entry */
			if (i < 50) {
				i = 50; /* min size of table */
			}
			if (i > 4096) {
				i = 4096; /* max size of table */
			}
			nlm_maxdupreqs = i;
		}

		current_maxdupreqs = nlm_maxdupreqs;
		NLM_dupreqs = (struct nlm_dup_record *) kmem_zalloc(
			  sizeof (struct nlm_dup_record) * current_maxdupreqs,
					     KM_NOSLEEP);
		dr = NLM_dupreqs;
		if (!dr) {
			mutex_unlock(&nlmduplock);
			/*
			 * No table was allocated.  To avoid retransmissions, we
			 * treat this as a duplicate.  We assume that this only
			 * means that the caller will get the callback port before
			 * making a callback.
			 */
			if (!dupwarn_printed) {
				cmn_err(CE_WARN,
					"NLM: unable to allocate duplicate request table\n");
				dupwarn_printed = 1;
			}
			return (NULL);
		}
		(void) bzero(NLM_dup_table, sizeof(NLM_dup_table));
		for (i = 0; i < nlm_maxdupreqs; i++) {
		/*
		 * Add each entry in the table to some hash chain
		 * (Doesn't matter which) to get things going.
		 */
			dr->nd_state = NLM_DUP_UNUSED;
			dr->nd_slot = i & NLM_DUPHASH_MASK;
			dr->nd_next = NLM_dup_table[i & NLM_DUPHASH_MASK];
			if (dr->nd_next) {
				dr->nd_next->nd_prev = dr;
			}
			NLM_dup_table[i & NLM_DUPHASH_MASK] = dr;
			dr++;
		}
	}

	/*
	 * Search the chain for an entry with a matching IP address and
	 * transaction ID.
	 */
	for (dr = NLM_dup_table[slot]; dr; dr = dr->nd_next) {
		if ((dr->nd_addr.sin_addr.s_addr == sin->sin_addr.s_addr) &&
		(dr->nd_addr.sin_port == sin->sin_port) &&
		(dr->nd_xid == xid)) {
			if (dr->nd_state == NLM_DUP_NEW)
				dr->nd_state = NLM_DUP_WORKING;
			else if (dr->nd_state == NLM_DUP_DONE) {
				if (dr->nd_res) {
					(*dr->nd_res)(COPYRES, (caddr_t)&dr->nd_result, res,
						sizeof(dr->nd_result));
				} else {
					bcopy((caddr_t)&dr->nd_result, res, sizeof(dr->nd_result));
				}
			}
			mutex_unlock(&nlmduplock);
			return (dr);
		}
	}

	/*
	 * At this point, we know it is NOT a duplicate. 
	 * Recycle the oldest entry in the table (round robin)
	 */
	dr = &NLM_dupreqs[nlmdup_index];
	if (++nlmdup_index >= current_maxdupreqs) {
		nlmdup_index = 0;
		if (current_maxdupreqs != nlm_maxdupreqs) {
			/*
			 * if the size has changed after reboot, throw old away
			 * and start over. Better than crashing.
			 */
			kmem_free(NLM_dupreqs, sizeof (struct dupreq) * current_maxdupreqs);
			NLM_dupreqs = NULL;
			dupwarn_printed = 0;
			mutex_unlock(&nlmduplock);
			cmn_err(CE_NOTE,
				"NLM: nlm_maxdupreqs changed, old table discarded\n");
			return (NULL);
		}
	}

	if (dr->nd_prev) {
		/*
		 * remove from not the front
		 */
		ASSERT(dr->nd_prev->nd_next == dr);
		dr->nd_prev->nd_next = dr->nd_next;
	} else {
		/*
		 * Remove from front
		 */
		NLM_dup_table[dr->nd_slot] = dr->nd_next;
	}
	if (dr->nd_next) {
		/*
		 * Remove from not the end
		 */
		ASSERT(dr->nd_next->nd_prev == dr);
		dr->nd_next->nd_prev = dr->nd_prev;
	}
	if (dr->nd_res) {
		(*dr->nd_res)(FREERES, (caddr_t)&dr->nd_result, NULL,
			sizeof(dr->nd_result));
	}
	bzero((caddr_t)&dr->nd_result, sizeof(dr->nd_result));
	dr->nd_xid = xid;
	dr->nd_state = NLM_DUP_NEW;
	dr->nd_slot = slot;
	dr->nd_addr = *sin;
	dr->nd_next = NLM_dup_table[slot];
	dr->nd_prev = NULL;
	NLM_dup_table[slot] = dr;
	if (dr->nd_next) {
		/*
		 * If it is not the only element in the chain,
		 * keep a previous pointer so unlinks happen fast.
		 */
		dr->nd_next->nd_prev = dr;
	}

	mutex_unlock(&nlmduplock);
	return (dr);
}

static void
nlm_dupdone(struct nlm_dup_record *drp, caddr_t res,
	void (*resfunc)(int, caddr_t, caddr_t, int), nlm4_stats reply_status)
{
	mutex_lock(&nlmduplock, PZERO);
	if (reply_status != NLM4_DENIED_GRACE_PERIOD) {
		drp->nd_state = NLM_DUP_DONE;
		drp->nd_res = resfunc;
		if (resfunc) {
			(*resfunc)(COPYRES, res, (caddr_t)&drp->nd_result,
				sizeof(drp->nd_result));
		} else {
			bcopy(res, (caddr_t)&drp->nd_result, sizeof(drp->nd_result));
		}
	} else {
		if (drp->nd_prev) {
			/*
			 * remove from not the front
			 */
			ASSERT(drp->nd_prev->nd_next == drp);
			drp->nd_prev->nd_next = drp->nd_next;
		} else {
			/*
			 * Remove from front
			 */
			NLM_dup_table[drp->nd_slot] = drp->nd_next;
		}
		if (drp->nd_next) {
			/*
			 * Remove from not the end
			 */
			ASSERT(drp->nd_next->nd_prev == drp);
			drp->nd_next->nd_prev = drp->nd_prev;
		}
		drp->nd_next = drp->nd_prev = NULL;
		drp->nd_slot = 0;
		drp->nd_xid = 0;
		drp->nd_state = NLM_DUP_UNUSED;
		drp->nd_addr.sin_addr.s_addr = 0;
	}
	mutex_unlock(&nlmduplock);
}

void
nlm_dup_flush(u_long ipaddr)
{
	struct nlm_dup_record *dr;
	int i;

	mutex_lock(&nlmduplock, PZERO);
	for (dr = NLM_dupreqs, i = 0;
		i < MIN(nlm_maxdupreqs, current_maxdupreqs);
		i++, dr++) {
			if (dr->nd_addr.sin_addr.s_addr == ipaddr) {
				if (dr->nd_prev) {
					/*
					 * remove from not the front
					 */
					ASSERT(dr->nd_prev->nd_next == dr);
					dr->nd_prev->nd_next = dr->nd_next;
				} else {
					/*
					 * Remove from front
					 */
					NLM_dup_table[dr->nd_slot] = dr->nd_next;
				}
				if (dr->nd_next) {
					/*
					 * Remove from not the end
					 */
					ASSERT(dr->nd_next->nd_prev == dr);
					dr->nd_next->nd_prev = dr->nd_prev;
				}
				if (dr->nd_res) {
					(*dr->nd_res)(FREERES, (caddr_t)&dr->nd_result, NULL,
						sizeof(dr->nd_result));
				}
				dr->nd_addr.sin_addr.s_addr = 0;
				dr->nd_state = NLM_DUP_UNUSED;
				dr->nd_slot = 0;
				dr->nd_xid = 0;
				dr->nd_prev = dr->nd_next= NULL;
			}
	}
	mutex_unlock(&nlmduplock);
}

#ifdef DEBUG
static void
nlm_dupdebug(struct nlm_dup_record *ndrp, int proc, int vers)
{
	ASSERT(ndrp);
	switch (ndrp->nd_state) {
		case NLM_DUP_NEW:
		case NLM_DUP_WORKING:
			ndrp->nd_proc = proc;
			ndrp->nd_vers = vers;
			break;
		case NLM_DUP_DONE:
			ASSERT(ndrp->nd_proc == proc);
			ASSERT(ndrp->nd_vers == vers);
			break;
		default:
			printf("nlm_dupdebug: bad state %d\n", ndrp->nd_state);
			ASSERT((ndrp->nd_state == NLM_DUP_NEW) ||
				(ndrp->nd_state == NLM_DUP_DONE));
	}
}
#endif /* DEBUG */

extern int checkauth(struct exportinfo *, struct svc_req *, struct cred *);

/* ARGSUSED */
int
lockd_dispatch(struct svc_req *req, XDR *xdrin, caddr_t args, caddr_t res)
{
	struct nlm_dup_record *ndrp = NULL;
	bool_t resfree = FALSE;
	bool_t reqfree = TRUE;
	int which;
	int vers;
	register struct lockddisp *disp = NULL;
	int error = 0;
	struct exportinfo *exi = NULL;
	struct in_addr *replyaddr = &req->rq_xprt->xp_raddr.sin_addr;
	nlm4_stats reply_status;

	/*
	 * only accept calls for NLM_PROG
	 */
	if (req->rq_prog != NLM_PROG) {
		cmn_err(CE_NOTE, "lockd_dispatch: bad prog number (%d)\n",
			req->rq_prog);
		error = NFS_RPC_PROG;
		goto done;
	}
	/*
	 * make sure the procedure is contained within the table
	 */
	which = req->rq_proc;
	if (which < 0 || which >= LOCKD_NPROC) {
		error = NFS_RPC_PROC;
		cmn_err(CE_NOTE, "lockd_dispatch: bad proc number (%d)\n", which);
		goto done;
	}
	/*
	 * make sure the version is in the table
	 */
	vers = req->rq_vers;
	if (vers < NLM_VERS || vers > NLM4_VERS) {
		error = NFS_RPC_VERS;
		cmn_err(CE_NOTE, "lockd_dispatch: bad version number (%d)\n", vers);
		goto done;
	}
	disp = &lockddisptab[vers - NLM_VERS][which];

	NLM_DEBUG(NLMDEBUG_DISPATCH,
		printf("lockd_dispatch: vers %d, proc %s\n", vers,
			nlmproc_to_str(vers, which)));

	/*
	 * decode the arguments, calling the xdr proc directly
	 */
	bzero(args, disp->dis_argsz);
	bzero(res, disp->dis_ressz);
	if (! ((xdrproc_ansi_t)disp->dis_xdrargs)(xdrin, args)) {
		error = NFS_RPC_DECODE;
		cmn_err(CE_NOTE,
			"lockd_dispatch: bad getargs, version %d, proc %d, source %s\n",
			vers, which, inet_ntoa(*replyaddr));
		goto done;
	}

	if (disp->dis_getexp) {
		/*
		 * Get the export entry and check the authentication.
		 * If the export entry cannot be gotten, pass a NULL export entry
		 * pointer to the dispatch procedure.  It will cause an ESTALE
		 * error and an appropriate reply for the client.
		 * Authentication checking is the same as for NFS.
		 */
		if ((disp->dis_getexp)(req, (union nlm_args_u *)args, &exi)) {
			if (!checkauth(exi, req, get_current_cred())) {
				cmn_err(CE_NOTE,
					"lockd_dispatch: weak authentication, proc %d, version "
					"%d, source IP address=%s\n", which, vers,
					inet_ntoa(*replyaddr));
				error = NFS_RPC_AUTH;
				goto done;
			}
		} else {
			error = (disp->dis_proc)(req, (union nlm_args_u *)args,
				(union nlm_res_u *)res, NULL, &reply_status);
			goto done;
		}
	}

	/*
	 * Request caching:  throw all duplicate requests away while in
	 * progress.  The final response for non-idempotent requests is
	 * stored for re-sending the reply, so that non-idempotent requests
	 * are not re-done (until the cache rolls over).
	 * We also use the duplicate request cache to detect retransmissions
	 * of idempotent requests.  This way, we can know when to call the
	 * client's port mapper (see nlm_reply and get_address above).
	 */

	if (disp->dis_idempotent) {
		error = (disp->dis_proc)(req, (union nlm_args_u *)args,
			(union nlm_res_u *)res, exi, &reply_status);
	} else {
		/*
		 * Check for a duplicate based on the cookie and the IP
		 * address.  Whether or not this is a duplicate is used to
		 * determine whether or not to get the port number for the
		 * client's lockd.
		 */
		if (ndrp = nlm_dup(req, res)) {
			switch (ndrp->nd_state) {
				case NLM_DUP_NEW:
					NLM_DEBUG(NLMDEBUG_TRACE,
						printf("lockd_dispatch: new request, proc %s, "
							"vers %d\n", nlmproc_to_str(vers, which),
							vers));
					/*
					 * Call service routine with arg struct and 
					 * results struct
					 * Only do this when the call appears to not be a
					 * retransmission or the operation is idempotent.
					 */
					error = (disp->dis_proc)(req, (union nlm_args_u *)args,
						(union nlm_res_u *)res, exi, &reply_status);
					NLM_DEBUG(NLMDEBUG_DUP,
						nlm_dupdebug(ndrp, which, vers));
					nlm_dupdone(ndrp, res, disp->dis_res, reply_status);
					break;
				case NLM_DUP_WORKING:
					/*
					 * drop duplicates of in-progress requests
					 */
					error = NFS_RPC_DUP;
					NLM_DEBUG(NLMDEBUG_DUP,
						printf("lockd_dispatch: dropping duplicate for "
							"in-progress request\n"));
					goto done;
				case NLM_DUP_DONE:
					resfree = disp->dis_res ? TRUE : FALSE;
					NLM_DEBUG(NLMDEBUG_DUP,
						nlm_dupdebug(ndrp, which, vers));
					NLM_DEBUG(NLMDEBUG_DUP,
						printf("lockd_dispatch: retransmitted request, "
							"proc %s, vers %d, cookie %s, dupslot %d\n",
							nlmproc_to_str(vers, which), vers,
							netobj_to_str((netobj *)args), ndrp->nd_slot));
					break;
				default:
					NLM_DEBUG(NLMDEBUG_DUP,
						cmn_err(CE_PANIC,
							"lockd_dispatch: bad nlm dup state %d\n",
							(int)ndrp->nd_state));
					ndrp->nd_state = NLM_DUP_WORKING;
			}
		} else {
			error = NFS_RPC_DUP;
		}
	}

done:
	if (exi) {
		export_unlock();
	}
	ASSERT((error == NFS_RPC_OK) || (error == NFS_RPC_PROG) ||
		(error == NFS_RPC_PROC) || (error == NFS_RPC_VERS) ||
		(error == NFS_RPC_DECODE) || (error == NFS_RPC_AUTH) ||
		(error == NFS_RPC_DUP));
	if (disp) {
		/*
		 * If error is less than 0, do not generate a reply.  It is
		 * a duplicate of an in-progress request.
		 */
		if (error >= 0) {
			if (disp->dis_xdrres) {
				ASSERT(error ||
					nlm_valid_repy_stat(vers, which, (union nlm_res_u *)res));
				if (disp->dis_asyncreply) {
					if (!error) {
						/*
						 * Asynchronous functions have dis_asyncreply set to
						 * something, so call that function.  If there is an
						 * error, we don't care -- the client will retransmit
						 * the request.
						 */
						(disp->dis_asyncreply)(req, disp->dis_xdrres,
							(union nlm_res_u *)res);
					}
				} else {
					/*
					 * Use nfs_sendreply to send the reply.  This is for
					 * the synchronous functions.
					 */
					error = nfs_sendreply(req->rq_xprt, disp->dis_xdrres,
						res, error, (u_int)NLM_VERS,
						(u_int)NLM4_VERS);
					if (error) {
						cmn_err(CE_WARN, "lockd_dispatch: bad sendreply, "
							"proc %d, vers %d, error %d\n", which, vers, error);
					}
					reqfree = FALSE;
				}
			} else if (error > 0) {
				/*
				 * Use nfs_sendreply to send the error reply.
				 * In this case, the xdr function for the results is not
				 * defined, so just use xdr_void.
				 */
				error = nfs_sendreply(req->rq_xprt, (xdrproc_t)xdr_void,
					res, error, (u_int)NLM_VERS, (u_int)NLM4_VERS);
				if (error) {
					cmn_err(CE_WARN, "lockd_dispatch: bad error sendreply, "
						"proc %d, vers %d, error %d\n", which, vers, error);
				}
				reqfree = FALSE;
			}
		}
		/*
		 * Free arguments struct
		 * The results struct shares data with the args.  We cannot
		 * free the args until the reply has been sent.
		 */
		xdrin->x_op = XDR_FREE;
		if (!((xdrproc_ansi_t)disp->dis_xdrargs)(xdrin, args) ) {
			cmn_err(CE_WARN, "lockd_dispatch: bad freeargs\n");
		}
		if (resfree) {
			(*disp->dis_res)(FREERES, res, NULL, disp->dis_ressz);
		}
	}
	
	if (reqfree)
		krpc_toss(req->rq_xprt);

	return(0);
}

void
lockd_init(void)
{
	spinlock_init(&nlm_wait_lock, "NLM wait lock");
	nlm_wait_zone = kmem_zone_init(sizeof(nlm_wait_t), "NLM wait structures");
	share_init();
	nlmsvc_init();
	klm_init();
	nlm_dup_init();
#ifdef DEBUG
	nlmdebug_init();
#endif /* DEBUG */
}

#ifdef DEBUG
/*
 * idbg functions for debugging nlm_response_wait_hash and
 * nlm_blocked_wait_hash
 */
static char *
nwstate_to_str(u_int state)
{
	static char str[32];

	str[0] = '\0';
	if (state & NW_WAITING) {
		strcat(str, "NW_WAITING ");
	}
	if (state & NW_TIMEOUT) {
		strcat(str, "NW_TIMEOUT ");
	}
	if (state & NW_REPLY) {
		strcat(str, "NW_REPLY ");
	}
	return(str);
}

static void
print_nlm_wait(nlm_wait_t *waitp)
{
	qprintf("NLM wait structure at 0x%lx\n", waitp);
	qprintf("      nw_next: 0x%lx\n", waitp->nw_next);
	qprintf("      nw_prev: 0x%lx\n", waitp->nw_prev);
	qprintf("     nw_state: %s\n", nwstate_to_str(waitp->nw_state));
	qprintf("    nw_cookie: %ld\n", waitp->nw_cookie);
	qprintf("    nw_status: %s\n", nlm4stats_to_str(waitp->nw_status));
	qprintf("    &nw_flock: 0x%lx\n", &waitp->nw_flock);
	qprintf("    nw_ipaddr: %s\n",
		inet_ntoa(*(struct in_addr *)&waitp->nw_ipaddr));
	qprintf("      nw_wait: 0x%lx\n", &waitp->nw_wait);
}

void
idbg_nlmwait(__psint_t addr)
{
	nlm_wait_t *waitp;

	if (addr >= 0) {
		/*
		 * print a specified wait has slot
		 */
		if (addr < WAIT_HASH_SLOTS) {
			qprintf("NLM response wait hash slot %ld\n", addr);
			for (waitp = nlm_response_wait_hash[addr]; waitp;
				waitp = waitp->nw_next) {
					print_nlm_wait(waitp);
			}
			qprintf("NLM blocked wait hash slot %ld\n", addr);
			for (waitp = nlm_blocked_wait_hash[addr]; waitp;
				waitp = waitp->nw_next) {
					print_nlm_wait(waitp);
			}
		} else {
			qprintf("NLM wait hash slot %ld too large\n", addr);
		}
	} else if (addr == -1) {
		/*
		 * print the whole hash table
		 */
		qprintf("NLM response wait hash\n");
		for (addr = 0; addr < WAIT_HASH_SLOTS; addr++) {
			for (waitp = nlm_response_wait_hash[addr]; waitp;
				waitp = waitp->nw_next) {
					print_nlm_wait(waitp);
			}
		}
		qprintf("NLM blocked wait hash\n");
		for (addr = 0; addr < WAIT_HASH_SLOTS; addr++) {
			for (waitp = nlm_blocked_wait_hash[addr]; waitp;
				waitp = waitp->nw_next) {
					print_nlm_wait(waitp);
			}
		}
	} else {
		/*
		 * print a specified wait structure
		 */
		print_nlm_wait((nlm_wait_t *)addr);
	}
}

/* ARGSUSED */
void
idbg_waithash(__psint_t addr)
{
	int slot;

	qprintf("NLM wait hash, size %d\n", WAIT_HASH_SLOTS);
	for (slot = 0; slot < WAIT_HASH_SLOTS; slot++) {
		qprintf("nlm_response_wait_hash[%d] = 0x%p\n", slot,
			nlm_response_wait_hash[slot]);
		qprintf("nlm_blocked_wait_hash[%d] = 0x%p\n", slot,
			nlm_blocked_wait_hash[slot]);
	}
}

static char *
dupstate_to_str(enum nlmdup_state state)
{
	static char buf[32];

	switch (state) {
		case NLM_DUP_UNUSED:
			return("NLM_DUP_UNUSED");
		case NLM_DUP_NEW:
			return("NLM_DUP_NEW");
		case NLM_DUP_WORKING:
			return("NLM_DUP_WORKING");
		case NLM_DUP_DONE:
			return("NLM_DUP_DONE");
		default:
			sprintf(buf, "Bad state %d", (int)state);
	}
	return(buf);
}

static void
print_nlm_dup(struct nlm_dup_record *dr)
{
	qprintf("NLM dup record at 0x%p\n", dr);
	qprintf("    nd_state:  %s\n", dupstate_to_str(dr->nd_state));
	qprintf("     nd_slot:  %d\n", dr->nd_slot);
	qprintf("     nd_addr:  %s\n", inet_ntoa(dr->nd_addr.sin_addr));
	qprintf("     nd_proc:  %s\n", nlmproc_to_str(dr->nd_vers, dr->nd_proc));
	qprintf("     nd_vers:  %d\n", dr->nd_vers);
}

/* ARGSUSED */
void
idbg_nlmdup(__psint_t addr)
{
	struct nlm_dup_record *dr;
	int i;

	if (addr == -1) {
		for (dr = NLM_dupreqs, i = 0;
			i < MIN(nlm_maxdupreqs, current_maxdupreqs);
			i++, dr++) {
				if (dr->nd_state != NLM_DUP_UNUSED) {
					print_nlm_dup(dr);
				}
		}
	} else if (addr < 0) {
		print_nlm_dup((struct nlm_dup_record *)addr);
	} else if (addr < NLM_DUPHASH_SLOTS) {
		for (dr = NLM_dup_table[addr]; dr; dr = dr->nd_next) {
			print_nlm_dup(dr);
		}
	} else {
		qprintf("invalid address %d\n", addr);
	}
}
#endif /* DEBUG */
