/* @(#)nfs_server.c	1.7 88/08/06 NFSSRC4.0 from 2.85 88/02/08 SMI
 *
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */
#ident      "$Revision: 1.47 $"

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
#include <sys/cmn_err.h>
#include <sys/acct.h>
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
#include "sys/kabi.h"

extern mac_label *mac_equal_equal_lp;
extern int in_grace_period;
extern int nlm_callback_retry;
extern char *Lock_client_name;
extern size_t Lock_client_namelen;

/*
 * Special internal error codes (all < 0)
 * These are primarily used in lockd_dispatch to control the sending or not
 * of replies.
 */
#define ERR_NOREPLY			NFS_RPC_DUP	/* do not send a reply (for delayed
											replies as well as errors) */

/*
 * vnode table to keep track of vnodes held with remote locks
 */
#define VNODE_TABLE_SIZE	\
	((128 - (sizeof(long) + sizeof(struct vnode_table *))) / sizeof(vnode_t *))

struct vnode_table {
	vnode_t				*vt_vnodes[VNODE_TABLE_SIZE];
	struct vnode_table	*vt_next;
	long				vt_count;
};

static struct vnode_table *VnodeTable;
static mutex_t VnodeTableLock;
#ifdef DEBUG
int holdcount = 0;
#endif /* DEBUG */
static int lockd_procs = 0;
extern int max_lockd_procs;

/*
 * translator for nlm4 to nlm1
 */
static nlm_stats nlm4stats_to_nlm[] = {
	nlm_granted,				/* NLM4_GRANTED */
	nlm_denied,					/* NLM4_DENIED */
	nlm_denied_nolocks,			/* NLM4_DENIED_NOLOCKS */
	nlm_blocked,				/* NLM4_BLOCKED */
	nlm_denied_grace_period,	/* NLM4_DENIED_GRACE_PERIOD */
	nlm_deadlck,				/* NLM4_DEADLCK */
	nlm_denied,					/* NLM4_ROFS */
	nlm_denied,					/* NLM4_STALE_FH */
	nlm_denied,					/* NLM4_FBIG */
	nlm_denied					/* NLM4_FAILED */
};

/*
 * tuanbles
 */
extern int lockd_grace_period;

/*
 * things shared with client locking code
 */
extern long Cookie;

extern struct vnode *fhtovp(fhandle_t *, struct exportinfo *);

/*
 * vnode table maintenance
 */
static void
toss_vnode(vnode_t *vp)
{
	struct vnode_table *vt;
	struct vnode_table *prev;
	int i;
	int found;
	int diff;

	mutex_enter(&VnodeTableLock);
	/*
	 * couple checking for remote locks with the clearing of
	 * VLOCKHOLD to prevent a race with local_lock
	 */
	mp_mutex_lock(&vp->v_filocksem, PZERO);
	if (!remotelocks(vp)) {
		bitlock_clr(&vp->v_flag, VLOCK, VLOCKHOLD);
	} else {
		mp_mutex_unlock(&vp->v_filocksem);
		mutex_exit(&VnodeTableLock);
		return;
	}
#ifdef DEBUG
	for (found = 0, prev = NULL, vt = VnodeTable; vt; ) {
#else /* DEBUG */
	for (found = 0, prev = NULL, vt = VnodeTable; vt && !found; ) {
#endif /* DEBUG */
		ASSERT(vt->vt_count <= VNODE_TABLE_SIZE);
		/*
		 * Search through the table for the vnode.
		 * When found, remove it and compress the table.
		 */
		for (i = 0; i < vt->vt_count; i++) {
			if (vt->vt_vnodes[i] == vp) {
				ASSERT(!(vp->v_flag & VLOCKHOLD));
				found++;
				vt->vt_count--;
				switch (diff = vt->vt_count - i) {
					case 0:
						break;
					case 1:
						vt->vt_vnodes[i] = vt->vt_vnodes[i+1];
						break;
					default:
						bcopy(&vt->vt_vnodes[i+1], &vt->vt_vnodes[i],
							diff * sizeof(vnode_t *));
				}
				vt->vt_vnodes[vt->vt_count] = NULL;
#ifndef DEBUG
				break;
#endif /* DEBUG */
			}
		}
		/*
		 * If the table is empty, get rid of it.
		 */
		if (vt->vt_count == 0) {
			if (prev) {
				prev->vt_next = vt->vt_next;
				NLM_KMEM_FREE(vt, sizeof(struct vnode_table));
				vt = prev->vt_next;
			} else {
				VnodeTable = vt->vt_next;
				NLM_KMEM_FREE(vt, sizeof(struct vnode_table));
				vt = VnodeTable;
			}
		} else {
			prev = vt;
			vt = vt->vt_next;
		}
	}
#ifdef DEBUG
	if (found)
		holdcount--;
	ASSERT(holdcount >= 0);
#endif /* DEBUG */
	mp_mutex_unlock(&vp->v_filocksem);
	mutex_exit(&VnodeTableLock);
	if (found) {
		VN_RELE(vp);
	}
	NLM_DEBUG(NLMDEBUG_VNODE, printf("toss_vnode: exit, found = %d\n", found));
	ASSERT((found == 0) || (found == 1));
}

static void
save_vnode(vnode_t *vp)
{
	struct vnode_table *vt;

	ASSERT(vp->v_flag & VLOCKHOLD);
	VN_HOLD(vp);
	mutex_enter(&VnodeTableLock);
#ifdef DEBUG
	holdcount++;
	ASSERT(holdcount >= 0);
#endif /* DEBUG */
	for (vt = VnodeTable; vt; vt = vt->vt_next) {
		ASSERT(vt->vt_count <= VNODE_TABLE_SIZE);
		if (vt->vt_count < VNODE_TABLE_SIZE) {
			vt->vt_vnodes[vt->vt_count] = vp;
			vt->vt_count++;
			mutex_exit(&VnodeTableLock);
			return;
		}
	}
	vt = NLM_KMEM_ZALLOC(sizeof(struct vnode_table), KM_SLEEP);
	vt->vt_vnodes[0] = vp;
	vt->vt_count = 1;
	vt->vt_next = VnodeTable;
	VnodeTable = vt;
	mutex_exit(&VnodeTableLock);
}

/*
 * release all locks for the given vfs
 * if vfs is NULL, release all locks
 */
void
release_all_locks(vfs_t *vfsp)
{
	struct vnode_table *prev;
	int i;
	int diff;
	struct vnode_table *vt;

	if (!vfsp) {
		/*
		 * release remote locks for all vnodes in the table
		 */
		mutex_enter(&VnodeTableLock);
		while (vt = VnodeTable) {
			VnodeTable = vt->vt_next;
			ASSERT(vt->vt_count <= VNODE_TABLE_SIZE);
			while (vt->vt_count--) {
				release_remote_locks(vt->vt_vnodes[vt->vt_count]);
				bitlock_clr(&vt->vt_vnodes[vt->vt_count]->v_flag, VLOCK,
					VLOCKHOLD);
				VN_RELE(vt->vt_vnodes[vt->vt_count]);
#ifdef DEBUG
				holdcount--;
				ASSERT(holdcount >= 0);
#endif /* DEBUG */
			}
			NLM_KMEM_FREE(vt, sizeof(struct vnode_table));
		}
		ASSERT(VnodeTable == NULL);
		ASSERT(!holdcount);
		mutex_exit(&VnodeTableLock);
	} else {
		mutex_enter(&VnodeTableLock);
		for (prev = NULL, vt = VnodeTable; vt; ) {
			for (i = 0; i < vt->vt_count; i++) {
				if (vt->vt_vnodes[i]->v_vfsp == vfsp) {
					/*
					 * release all remote locks for this vnode
					 */
					release_remote_locks(vt->vt_vnodes[i]);
					bitlock_clr(&vt->vt_vnodes[i]->v_flag, VLOCK, VLOCKHOLD);
					VN_RELE(vt->vt_vnodes[i]);
#ifdef DEBUG
					holdcount--;
					ASSERT(holdcount >= 0);
#endif /* DEBUG */
					/*
					 * remove the vnode from the table
					 */
					vt->vt_count--;
					switch (diff = vt->vt_count - i) {
						case 0:
							break;
						case 1:
							vt->vt_vnodes[i] = vt->vt_vnodes[i+1];
							break;
						default:
							bcopy(&vt->vt_vnodes[i+1], &vt->vt_vnodes[i],
								diff * sizeof(vnode_t *));
					}
					vt->vt_vnodes[vt->vt_count] = NULL;
				}
			}
			/*
			 * If the table is empty, get rid of it.
			 */
			if (vt->vt_count == 0) {
				if (prev) {
					prev->vt_next = vt->vt_next;
					NLM_KMEM_FREE(vt, sizeof(struct vnode_table));
					vt = prev->vt_next;
				} else {
					VnodeTable = vt->vt_next;
					NLM_KMEM_FREE(vt, sizeof(struct vnode_table));
					vt = VnodeTable;
				}
			} else {
				prev = vt;
				vt = vt->vt_next;
			}
		}
		mutex_exit(&VnodeTableLock);
	}
}

/*
 * Clean out any locks held for the given sysid.
 */
void
lockd_cleanlocks(__uint32_t sysid)
{
	int diff;
	struct vnode_table *vt;
	struct vnode_table *prev;
	vnode_t *vp;
	int i;

	/*
	 * cancel blocked locks for the given sysid
	 * this should cause all blocked nfsd/lockd processes to exit
	 * this must be done before cleanlocks
	 */
	cancel_blocked_locks(sysid);
	ASSERT(!locks_pending(IGN_PID, sysid));
	mutex_enter(&VnodeTableLock);
restart:
	for (prev = NULL, vt = VnodeTable; vt; prev = vt, vt = vt->vt_next ) {
		ASSERT(vt->vt_count <= VNODE_TABLE_SIZE);
		for (i = 0; i < vt->vt_count; ) {
			vp = vt->vt_vnodes[i];
			NLM_DEBUG(NLMDEBUG_NOTIFY,
				printf("lockd_cleanlocks: cleaning vnode 0x%p\n", vp));
			/*
			 * release the locks held for the given sysid
			 * after cleanlocks completes, there should be no locks
			 * held on this vnode for the given sysid
			 */
			cleanlocks(vp, IGN_PID, sysid);
			ASSERT(!haslocks(vp, IGN_PID, sysid));
			/*
			 * now check to see if we need to release the vnode
			 */
			mp_mutex_lock(&vp->v_filocksem, PZERO);
			if (!remotelocks(vp)) {
				NLM_DEBUG(NLMDEBUG_NOTIFY,
					printf("lockd_cleanlocks: removing vnode 0x%p from table\n",
						vp));
				bitlock_clr(&vp->v_flag, VLOCK, VLOCKHOLD);
				mp_mutex_unlock(&vp->v_filocksem);
				VN_RELE(vp);
#ifdef DEBUG
				holdcount--;
				ASSERT(holdcount >= 0);
#endif /* DEBUG */
				/*
				 * Take the entry out of the table by compression.
				 */
				vt->vt_count--;
				ASSERT(vt->vt_count >= i);
				switch (diff = vt->vt_count - i) {
					case 0:
						break;
					case 1:
						vt->vt_vnodes[i] = vt->vt_vnodes[i+1];
						break;
					default:
						bcopy(&vt->vt_vnodes[i+1], &vt->vt_vnodes[i],
							diff * sizeof(vnode_t *));
				}
				vt->vt_vnodes[vt->vt_count] = NULL;
				/*
				 * If the table is empty, get rid of it.
				 */
				if (vt->vt_count == 0) {
					if (prev) {
						prev->vt_next = vt->vt_next;
						NLM_KMEM_FREE(vt, sizeof(struct vnode_table));
						vt = prev;
					} else {
						VnodeTable = vt->vt_next;
						NLM_KMEM_FREE(vt, sizeof(struct vnode_table));
						goto restart;
					}
					/*
					 * since the count of vnodes has gone to 0, we
					 * must break out of the inner for loop
					 */
					break;
				}
				/*
				 * We compressed the table, so we do not want to increment
				 * the index to the next entry.  The next entry to be
				 * examined has been moved down one slot.
				 */
			} else {
				mp_mutex_unlock(&vp->v_filocksem);
				/*
				 * Increment the index to the next entry.
				 */
				i++;
			}
		}
	}
	mutex_exit(&VnodeTableLock);
}

/*
 * Get the vnode reference before a fork() so the child does not have to
 * refer to an unprotected exportinfo.  If successful, caller must
 * eventually VN_RELE(*vpp).
 */
int
local_fhtovp(struct exportinfo *exi, fhandle_t *fhp, vnode_t **vpp)
{
	int error = 0;

	if (!fhp) {
		NLM_DEBUG(NLMDEBUG_ERROR, printf("local_fhtovp: NULL file handle\n"));
		error = EINVAL;
	} else if (!(*vpp = fhtovp(fhp, exi))) {
		NLM_DEBUG(NLMDEBUG_STALE, printf("local_fhtovp: stale file handle\n"));
		error = ESTALE;
	}
	return (error);
}

int
local_lock_vp(vnode_t *vp, struct flock *flp, int block)
{
	int cmd = block ? F_RSETLKW : F_RSETLK;
	int error;

	VOP_FRLOCK(vp, cmd, flp, FREAD | FWRITE, (off_t)0,
		   VRWLOCK_NONE, sys_cred, error);
	if (!error) {
		/*
		 * make sure we only do one VN_HOLD
		 * if VLOCKHOLD is set, release the vnode, otherwise, set
		 * VLOCKHOLD and don't release the vnode
		 * the vnode will be release in local_unlock only when no
		 * remote locks are held
		 */
		if (!(bitlock_set(&vp->v_flag, VLOCK, VLOCKHOLD) & VLOCKHOLD)) {
			save_vnode(vp);
		}
		ASSERT(vp->v_count > 1);
	}
	return(error);
}

int
local_unlock_vp(vnode_t *vp, struct flock *flp, int cancel)
{
	int error;

	if (cancel) {
		NLM_DEBUG(NLMDEBUG_TRACE,
			printf("local_unlock: cancel for pending request\n"));
		error = cancel_lock(vp, flp);
	} else {
		/*
		 * do the unlock request
		 */
		VOP_FRLOCK(vp, F_RSETLK, flp, 0, (off_t)0, VRWLOCK_NONE, 
			   sys_cred, error);
		NLM_DEBUG(NLMDEBUG_ERROR,
			if (error && (error != EINTR))
				printf("local_unlock: VOP_FRLOCK error %d\n", error));
		ASSERT(!haslock(vp, flp));
	}
	/*
	 * release the vnode if no remote locks are held
	 */
	if (vp->v_flag & VLOCKHOLD)
		toss_vnode(vp);
	return(error);
}

int
local_lock(struct exportinfo *exi, fhandle_t *fhp, struct flock *flp, int block)
{
	vnode_t *vp = NULL;
	int error;

	if (!(error = local_fhtovp(exi, fhp, &vp))) {
		error = local_lock_vp(vp, flp, block);
		VN_RELE(vp);
	}
	return(error);
}

int
local_unlock(struct exportinfo *exi, fhandle_t *fhp, struct flock *flp,
	int cancel)
{
	vnode_t *vp = NULL;
	int error;

	if (!(error = local_fhtovp(exi, fhp, &vp))) {
		error = local_unlock_vp(vp, flp, cancel);
		VN_RELE(vp);
	}
	return(error);
}

static int
local_test(struct exportinfo *exi, fhandle_t *fhp, struct flock *flp)
{
	int error;
	vnode_t *vp = NULL;

	if (!fhp) {
		NLM_DEBUG(NLMDEBUG_ERROR, printf("local_test: NULL file handle\n"));
		error = EINVAL;
	} else if (!(vp = fhtovp(fhp, exi))) {
		NLM_DEBUG(NLMDEBUG_STALE, printf("local_test: stale file handle\n"));
		error = ESTALE;
	} else {
		/*
		 * here, we have the vnode
		 * do the lock request
		 */
		VOP_FRLOCK(vp, F_RGETLK, flp, FREAD | FWRITE, (off_t)0,
			VRWLOCK_NONE, sys_cred, error);
		NLM_DEBUG(NLMDEBUG_ERROR,
			if (error && (error != EINTR))
				printf("local_test: VOP_FRLOCK error %d\n", error));
		/*
		 * always release the vnode since no lock was acquired
		 */
		VN_RELE(vp);
	}
	return(error);
}

/*
 * convert error number to NLM status
 */
static nlm_stats
errno_to_nlm(int error)
{
	nlm_stats result;

	switch (error) {
		case 0:
			result = nlm_granted;
			break;
		case ENOLCK:
		case ENOMEM:
			result = nlm_denied_nolocks;
			break;
		case EDEADLK:
			result = nlm_deadlck;
			break;
		case EBUSY:
			result = nlm_blocked;
			break;
		default:
			result = nlm_denied;
			break;
	}

	return (result);
}

/*
 * convert error number to NLM4 status
 */
nlm4_stats
errno_to_nlm4(int error)
{
	nlm4_stats result;

	switch (error) {
		case 0:
			result = NLM4_GRANTED;
			break;
		case ENOLCK:
		case ENOMEM:
			result = NLM4_DENIED_NOLOCKS;
			break;
		case EDEADLK:
			result = NLM4_DEADLCK;
			break;
		case EROFS:
			result = NLM4_ROFS;
			break;
		case ESTALE:
			result = NLM4_STALE_FH;
			break;
		case EFBIG:
			result = NLM4_FBIG;
			break;
		case EACCES:
		case EAGAIN:
		case EINVAL:
			result = NLM4_DENIED;
			break;
		case EBUSY:
			result = NLM4_BLOCKED;
			break;
		default:
			result = NLM4_FAILED;
	}

	return (result);
}

void
dup_netobj(netobj *source, netobj *target)
{
	ASSERT(source && target);
	if (source->n_len && source->n_bytes) {
		target->n_len = source->n_len;
		target->n_bytes = NLM_KMEM_ALLOC(source->n_len, KM_SLEEP);
		bcopy(source->n_bytes, target->n_bytes, target->n_len);
	} else {
		target->n_len = 0;
		target->n_bytes = NULL;
	}
}

void
free_netobj(netobj *obj)
{
	if (obj->n_len && obj->n_bytes) {
		NLM_KMEM_FREE(obj->n_bytes, obj->n_len);
	}
}

/*
 * -------------------------------
 * Asynchronous NLM reply function
 * -------------------------------
 */

/*
 * client address and port cache
 */
#define ADDRHASH_SIZE	101
struct addr_entry {
	struct sockaddr_in	addr;
	struct addr_entry	*next;
};
static lock_t	addrhash_lock;
struct addr_entry *addrhash[ADDRHASH_SIZE];

/*
 * message procedure to response procedure translators
 */
static int nlm1msg_to_resp[] = {
	0,				/* NULLPROC */
	0,				/* NLM_TEST */
	0,				/* NLM_LOCK */
	0,				/* NLM_CANCEL */
	0,				/* NLM_UNLOCK */
	0,				/* NLM_GRANTED */
	NLM_TEST_RES,	/* NLM_TEST_MSG */
	NLM_LOCK_RES,	/* NLM_LOCK_MSG */
	NLM_CANCEL_RES,	/* NLM_CANCEL_MSG */
	NLM_UNLOCK_RES,	/* NLM_UNLOCK_MSG */
	NLM_GRANTED_RES	/* NLM_GRANTED_MSG */
};
static int nlm4msg_to_resp[] = {
	0,					/* NULLPROC */
	0,					/* NLMPROC_TEST */
	0,					/* NLMPROC_LOCK */
	0,					/* NLMPROC_CANCEL */
	0,					/* NLMPROC_UNLOCK */
	0,					/* NLMPROC_GRANTED */
	NLMPROC_TEST_RES,	/* NLMPROC_TEST_MSG */
	NLMPROC_LOCK_RES,	/* NLMPROC_LOCK_MSG */
	NLMPROC_CANCEL_RES,	/* NLMPROC_CANCEL_MSG */
	NLMPROC_UNLOCK_RES,	/* NLMPROC_UNLOCK_MSG */
	NLMPROC_GRANTED_RES	/* NLMPROC_GRANTED_MSG */
};

/*
 * look up a cached address and fill in the sockaddr_in
 */
static int
find_address(u_long ipaddr, struct sockaddr_in *sa)
{
	struct addr_entry *ep;
	int found = 0;
	int slot = ipaddr % ADDRHASH_SIZE;
	int ospl;

	ospl = mutex_spinlock(&addrhash_lock);
	for (ep = addrhash[slot]; ep; ep = ep->next) {
		if (ep->addr.sin_addr.s_addr == ipaddr) {
			*sa = ep->addr;
			found = 1;
			break;
		}
	}
	mutex_spinunlock(&addrhash_lock, ospl);
	return(found);
}

/*
 * Remove an IP address from the cache.
 */
static void
remove_address(u_long ipaddr)
{
	struct addr_entry *ep;
	struct addr_entry *prev;
	int slot = ipaddr % ADDRHASH_SIZE;
	int ospl;

	ospl = mutex_spinlock(&addrhash_lock);
	for (prev = NULL, ep = addrhash[slot]; ep; prev = ep, ep = ep->next) {
		if (ep->addr.sin_addr.s_addr == ipaddr) {
			if (prev) {
				prev->next = ep->next;
			} else {
				addrhash[slot] = ep->next;
			}
			break;
		}
	}
	mutex_spinunlock(&addrhash_lock, ospl);
	if (ep) {
		NLM_KMEM_FREE(ep, sizeof(struct addr_entry));
	}
}

/*
 * cache an address and port
 */
static void
save_address(struct sockaddr_in *sa)
{
	int slot = sa->sin_addr.s_addr % ADDRHASH_SIZE;
	int ospl;
	int found = 0;
	struct addr_entry *ep;
	struct addr_entry *newep;

	newep = NLM_KMEM_ALLOC(sizeof(struct addr_entry), KM_SLEEP);
	ospl = mutex_spinlock(&addrhash_lock);
	for (ep = addrhash[slot]; ep; ep = ep->next) {
		if (ep->addr.sin_addr.s_addr == sa->sin_addr.s_addr) {
			found = 1;
			break;
		}
	}
	if (!found) {
		newep->addr = *sa;
		newep->next = addrhash[slot];
		addrhash[slot] = newep;
		mutex_spinunlock(&addrhash_lock, ospl);
	} else {
		ep->addr = *sa;
		mutex_spinunlock(&addrhash_lock, ospl);
		NLM_KMEM_FREE(newep, sizeof(struct addr_entry));
	}
}

/*
 * get the client address and port for the specified version of NLM_PROG
 * use a cached address unless this is a retransmission
 * if no address is cached, contact the port mapper on the client to get
 * the port
 */
int
get_address(u_long nlmvers, long ipaddr, struct sockaddr_in *sa, bool_t retrans)
{
	int error = 0;

	/*
	 * Look up the address in the cache.
	 * If not found or retransmission, get the port number again.
	 * Retransmissions need to get the port number again since the cached
	 * one may no longer be valid.
	 */
	if (retrans || !find_address(ipaddr, sa)) {
		/*
		 * failed to find the address in the cache or need to look it up
		 * again, so partially fill in the sockaddr_in with the address
		 * family and the supplied IP address
		 * call getport_loop to talk to the client's port mapper
		 * tell getport_loop not to retry indefinitely so we don't tie up
		 * nfsd with these operations
		 */
		bzero(sa, sizeof(struct sockaddr_in));
		sa->sin_family = AF_INET;
		sa->sin_addr.s_addr = ipaddr;
		error = getport_loop(sa, (u_long)NLM_PROG, nlmvers,
			(u_long)IPPROTO_UDP, 1);
		switch (error) {
			case 0:
				/*
				 * got the address and port from the port mapper
				 * cache it and return it to the caller
				 */
				save_address(sa);
				break;
			case ENOENT:
				/*
				 * program/version not supported, return ENOPKG
				 */
				NLM_DEBUG(NLMDEBUG_NOPROG,
					printf(
						"get_address: NLM_PROG not supported for server %s\n",
						inet_ntoa(sa->sin_addr)));
				remove_address(ipaddr);
				error = ENOPKG;
				break;
			default:
				NLM_DEBUG(NLMDEBUG_ERROR,
					printf("get_address: getport_loop error %d, host 0x%x\n",
						error, (int)ipaddr));
				remove_address(ipaddr);
				break;
		}
	}
	return(error);
}

static int
nlm_callback(int vers, int proc, struct in_addr *addr,
	xdrproc_t xdr_args, caddr_t args, xdrproc_t xdr_res, caddr_t res,
	bool_t retrans, int timeo)
{
	struct timeval tv;
	struct rpc_err rpc_err;
	enum clnt_stat status;
	CLIENT *client;
	int error;
	struct sockaddr_in lmaddr;
	int retry;
	int saved_timeo = timeo;
	int warned = 0;

call_again:
	retry = nlm_callback_retry;
	/*
	 * get the port number for the client's lock daemon
	 */
	error = get_address((u_long)vers, (long)addr->s_addr, &lmaddr, retrans);
	if (error) {
		ASSERT((error == ENOENT) || (error == EINTR));
		return(error);
	}
	NLM_DEBUG(NLMDEBUG_REPLY,
		printf("nlm_callback: NLM vers %d proc %s call to %s port %d\n", vers,
			nlmproc_to_str(vers, proc), inet_ntoa(lmaddr.sin_addr),
			lmaddr.sin_port));
	/*
	 * set up a client handle to talk to the remote lock manager
	 * use the same xid for all retries
	 */
	do {
		client = clntkudp_create(&lmaddr, (u_long)NLM_PROG, (u_long)vers,
			1, KUDP_INTR, KUDP_XID_CREATE, sys_cred);
		if (client == (CLIENT *) NULL) {
			NLM_DEBUG(NLMDEBUG_ERROR, printf("nlm_callback: %s, error %d\n",
				clnt_sperrno(rpc_createerr.cf_stat),
				rpc_createerr.cf_error.re_errno));
			if (rpc_createerr.cf_error.re_errno != EADDRINUSE) {
				return(rpc_createerr.cf_error.re_errno);
			}
			delay((saved_timeo * HZ) / 10);
		}
	} while (!client);
retransmit:
	tv.tv_sec = timeo / 10;
	tv.tv_usec = 100000 * (timeo % 10);
	status = CLNT_CALL(client, proc, xdr_args, args, xdr_res, res, tv);
	switch (status) {
		case RPC_SUCCESS:
			error = 0;
			break;
		case RPC_TIMEDOUT:
			if (timeo) {
				retrans = TRUE;
				timeo = lm_backoff(timeo);
				if (retry > 0) {
					retry--;
					goto retransmit;
				} else {
					CLNT_DESTROY(client);
					client = NULL;
					goto call_again;
				}
			} else {
				error = 0;
			}
			break;

		case RPC_INTR:
			error = EINTR;
			break;

		default:
			CLNT_GETERR(client, &rpc_err);
			error = rpc_err.re_errno;
			if (!warned) {
				cmn_err(CE_WARN,
					"nlm_callback: RPC error %s, errno %d, NLM vers %d, "
					"ipaddr %lx\n", clnt_sperrno(rpc_err.re_status), error,
					vers, (long)addr->s_addr);
				warned = 1;
			}
			retrans = TRUE;
			CLNT_DESTROY(client);
			client = NULL;
			delay((timeo * HZ) / 10);
			goto call_again;
	}
	CLNT_DESTROY(client);
	return(error);
}

/*
 * generic NLM/NLM4 reply function for asynchronous RPC
 */
/* ARGSUSED */
int
nlm_reply(struct svc_req *req, xdrproc_t xdrres, union nlm_res_u *resp)
{
	int replyproc;

	/*
	 * translate the message procedure number to the corresponding
	 * response procedure number
	 */
	switch (req->rq_vers) {
		case NLM_VERS:
		case NLM_VERSX:
			if ((req->rq_proc < NLM_TEST_MSG) ||
				(req->rq_proc > NLM_GRANTED_MSG)) {
					NLM_DEBUG(NLMDEBUG_ERROR,
						printf("nlm_reply: NLM proc %d out of reply range\n",
							req->rq_proc));
					return(EINVAL);
			}
			replyproc = nlm1msg_to_resp[req->rq_proc];
			break;
		case NLM4_VERS:
			if ((req->rq_proc < NLMPROC_TEST_MSG) ||
				(req->rq_proc > NLMPROC_GRANTED_MSG)) {
					NLM_DEBUG(NLMDEBUG_ERROR,
						printf("nlm_reply: NLM4 proc %d out of reply range\n",
							req->rq_proc));
					return(EINVAL);
			}
			replyproc = nlm4msg_to_resp[req->rq_proc];
			break;
		default:
			NLM_DEBUG(NLMDEBUG_ERROR, printf("nlm_reply: bad NLM version %d\n",
				req->rq_vers));
			return(EINVAL);
	}
	return(nlm_callback(req->rq_vers, replyproc,
		&req->rq_xprt->xp_raddr.sin_addr, xdrres, (caddr_t)resp,
		xdr_void, NULL, TRUE, 0));
}

/*
 * NLM_TEST and NLM_TEST_MSG
 */
/* ARGSUSED */
int
nlm_test_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh;
	int status = 1;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->testargs_1.alock.fh.n_len ||
		args->testargs_1.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm_test_exi: caller %s fh %s\n",
		args->testargs_1.alock.caller_name,
		netobj_to_str(&args->testargs_1.alock.fh)));
	fh = (fhandle_t *)args->testargs_1.alock.fh.n_bytes;
	if (fh && (args->testargs_1.alock.fh.n_len == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

int
proc_nlm_test(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	int error;
	fhandle_t *fhp = NULL;
	nlm_testargs *argp = &args->testargs_1;
	nlm_testres *resp = &res->testres_1;
	struct flock fl;
	struct in_addr *ipaddr = &req->rq_xprt->xp_raddr.sin_addr;

	ASSERT(!args->testargs_1.alock.fh.n_len ||
		args->testargs_1.alock.fh.n_bytes);
	/*
	 * Check for NLM_VERSX.  Convert the length field of the lock
	 * descriptor if necessary.
	 */
	if (req->rq_vers == NLM_VERSX) {
		if (argp->alock.l_len == 0xffffffff) {
			argp->alock.l_len = 0;
		}
	}
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("proc_nlm_test(%d): sysid %s offset %d len %d type %s\n",
			req->rq_proc, inet_ntoa(*ipaddr), argp->alock.l_offset,
			argp->alock.l_len, argp->exclusive ? "F_WRLCK" : "F_RDLCK"));
	/*
	 * copy the cookie to the response
	 * we'll need it regardless of what happens below
	 * we only copy the fields of the netobj, the underlying data is shared
	 */
	resp->cookie = argp->cookie;
	/*
	 * If we're still in the grace period, deny this lock.
	 */
	if (in_grace_period) {
		resp->stat.stat = nlm_denied_grace_period;
		bzero(&resp->stat.nlm_testrply_u, sizeof(resp->stat.nlm_testrply_u));
	} else {
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = ipaddr->s_addr;
		fl.l_pid = argp->alock.svid;
		/*
		 * this is NLM version 1 or 3, so the file handle is an NFS
		 * version 2 file handle
		 */
		if (argp->alock.fh.n_len == NFS_FHSIZE) {
			fhp = (fhandle_t *)argp->alock.fh.n_bytes;
			/*
			 * test the lock
			 */
			error = local_test(exi, fhp, &fl);
			if (error) {
				/*
				 * an error occurred, translate it to an NLM status code
				 * and return it to the client
				 */
				resp->stat.stat = errno_to_nlm(error);
				bzero(&resp->stat.nlm_testrply_u,
					sizeof(resp->stat.nlm_testrply_u));
			} else if (fl.l_type == F_UNLCK) {
				/*
				 * no conflicing lock is held
				 * the NLM status is nlm_granted
				 */
				resp->stat.stat = nlm_granted;
				bzero(&resp->stat.nlm_testrply_u,
					sizeof(resp->stat.nlm_testrply_u));
			} else {
				/*
				 * a conflicting lock is held
				 * set the status to nlm_denied and fill in the lock
				 * data
				 */
				resp->stat.stat = nlm_denied;
				resp->stat.nlm_testrply_u.holder.svid = fl.l_pid;
				resp->stat.nlm_testrply_u.holder.l_offset = (u_int)fl.l_start;
				resp->stat.nlm_testrply_u.holder.l_len = (u_int)fl.l_len;
				resp->stat.nlm_testrply_u.holder.exclusive =
					(fl.l_type == F_WRLCK);
				resp->stat.nlm_testrply_u.holder.oh.n_len = 0;
				resp->stat.nlm_testrply_u.holder.oh.n_bytes = NULL;
			}
		} else {
			resp->stat.stat = nlm_denied;
			bzero(&resp->stat.nlm_testrply_u,
				sizeof(resp->stat.nlm_testrply_u));
		}
	}
	NLM_DEBUG(NLMDEBUG_REPLY | NLMDEBUG_TRACE,
		printf("proc_nlm_test(%d): reply sysid %s stat %s offset %d len %d "
			"type %s\n", req->rq_proc, inet_ntoa(*ipaddr),
			nlmstats_to_str(resp->stat.stat),
			resp->stat.nlm_testrply_u.holder.l_offset,
			resp->stat.nlm_testrply_u.holder.l_len,
			resp->stat.nlm_testrply_u.holder.exclusive ? "F_WRLCK" :
			"F_RDLCK"));
	*reply_status = (nlm4_stats)resp->stat.stat;
	return(0);
}

/*
 * NLM_LOCK and NLM_LOCK_MSG
 */
/* ARGSUSED */
int
nlm_lock_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh;
	int status = 1;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->lockargs_1.alock.fh.n_len ||
		args->lockargs_1.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm_lock_exi: caller %s fh %s\n",
		args->lockargs_1.alock.caller_name,
		netobj_to_str(&args->lockargs_1.alock.fh)));
	fh = (fhandle_t *)args->lockargs_1.alock.fh.n_bytes;
	if (fh && (args->lockargs_1.alock.fh.n_len == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

int
proc_nlm_lock(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	bool_t retrans;
	struct in_addr ipaddr = req->rq_xprt->xp_raddr.sin_addr;
	union nlm_args_u grantargs;
	union nlm_res_u grantres;
	char *name = NULL;
	int namelen = 0;
	int error = 0;
	nlm_lockargs *argp = &args->lockargs_1;
	nlm_res *resp = &res->statres_1;
	struct flock fl;
	extern int fork(void *uap, rval_t *rvp);
	rval_t rv;
	int vers = req->rq_vers;
	int callback_proc; 
	int timeo;
	long cookie;
	void *res_wait = NULL;
	int res_timeo = nlm_granted_timeout;
	nlm_stats callback_status;
	vnode_t *vp = NULL;

	ASSERT(!args->lockargs_1.alock.fh.n_len ||
		args->lockargs_1.alock.fh.n_bytes);
	if (req->rq_vers == NLM_VERSX) {
		if (argp->alock.l_len == 0xffffffff) {
			argp->alock.l_len = 0;
		}
	}
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_LOCK,
		printf("proc_nlm_lock(%d): sysid %s offset %d len %d type %s %s\n",
			req->rq_proc, inet_ntoa(ipaddr),
			argp->alock.l_offset, argp->alock.l_len,
			argp->exclusive ? "F_WRLCK" : "F_RDLCK",
			argp->reclaim ? "reclaim" : ""));
	/*
	 * copy the cookie to the response
	 * we'll need it regardless of what happens below
	 */
	resp->cookie = argp->cookie;
	/*
	 * If we're still in the grace period, deny this lock unless it is a
	 * reclaim.
	 */
	if (in_grace_period && !argp->reclaim) {
		resp->stat.stat = nlm_denied_grace_period;
		error = 0;
	} else if (argp->alock.fh.n_len == NFS_FHSIZE) {
		/*
		 * record the client name unless this is a non-monitored lock
		 */
		if (req->rq_proc != NLM_NM_LOCK) {
			/*
			 * Encode the client name and the IP address to which
			 * it sent the request into a single name string.  The
			 * name is encoded as <client name>,<IP address> where
			 * <client name> is the caller name sent by the client
			 * and <IP address> is the IP address to which the
			 * client sent the RPC request.  The length of this
			 * string will be the length of the client name plus
			 * 15 for the largest possible IP address plus 1 for
			 * the separator (a ':') plus 1 for the NULL
			 * terminator.
			 * The purpose of this encoding is to enable statd
			 * to do notification for multiple interfaces
			 * without having to query for the configured
			 * interfaces and send crash notifications for
			 * all.  This is only done on the server side as
			 * the server could be receiving requests on any
			 * configured interface.
			 * Detect NULL client names so we don't panic the system.
			 * Also detect client names of 0 length.
			 */
			if (argp->alock.caller_name &&
				(namelen = strlen(argp->alock.caller_name))) {
					/*
					 * Do not record the name for requests comming in over
					 * loopback.
					 */
					if (nfs_srcaddr(req->rq_xprt)->s_addr != INADDR_LOOPBACK) {
						namelen += 17;
						name = NLM_KMEM_ALLOC(namelen, KM_SLEEP);
						(void)sprintf(name, "%s:%s", argp->alock.caller_name,
							inet_ntoa(*nfs_srcaddr(req->rq_xprt)));
						error = record_remote_name(name);
						NLM_KMEM_FREE(name, namelen);
					}
			} else {
				error = EINVAL;
			}
			if (error) {
				cmn_err(CE_WARN,
					"!Network lock manager: bad client name \"%s\" from %s "
					"port %d",
					argp->alock.caller_name ? argp->alock.caller_name : "NULL",
					inet_ntoa(req->rq_xprt->xp_raddr.sin_addr),
					req->rq_xprt->xp_raddr.sin_port);
				*reply_status = (nlm4_stats)(resp->stat.stat = nlm_denied);
				return(0);
			}
		} else if (req->rq_vers != NLM_VERSX) {
			return(NFS_RPC_PROC);
		}
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = ipaddr.s_addr;
		fl.l_pid = argp->alock.svid;
		/*
		 * Get a vnode reference from the file handle and attempt
		 * the lock.  The vnode reference must be released by this
		 * thread or by the forked child.
		 */
		error = local_fhtovp(exi, (fhandle_t *)argp->alock.fh.n_bytes, &vp);
		if (!error)
			error = local_lock_vp(vp, &fl, 0);
		switch (error) {
			case EACCES:
			case EAGAIN:
				ASSERT(vp);
				if (argp->block) {
					/*
					 * limit the number of blocked lockd processes
					 */
					if (atomicAddInt(&lockd_procs, 1) > max_lockd_procs) {
						atomicAddInt(&lockd_procs, -1);
						resp->stat.stat = nlm_denied_nolocks;
						error = 0;
						break;
					}
					/*
					 * fill in the flock structure again from
					 * the test args in case it was modified in
					 * local_lock processing
					 */
					fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
					fl.l_whence = 0;
					fl.l_start = (off_t)argp->alock.l_offset;
					fl.l_len = (off_t)argp->alock.l_len;
					fl.l_sysid = ipaddr.s_addr;
					fl.l_pid = argp->alock.svid;
					/*
					 * This is a blocking request, so we must call local_lock
					 * again, telling it to block.  First, set up the args
					 * for the grant callback, copying the data from the
					 * lock args.  We must copy because the args will get
					 * freed or reused.
					 */
					bzero(&grantargs, sizeof(grantargs));
					bzero(&grantres, sizeof(grantres));
					grantargs.testargs_1.exclusive = fl.l_type == F_WRLCK;
					grantargs.testargs_1.alock.svid = fl.l_pid;
					grantargs.testargs_1.alock.l_offset = (u_int)fl.l_start;
					grantargs.testargs_1.alock.l_len = (u_int)fl.l_len;
					cookie = atomicAddLong(&Cookie, 1);
					grantargs.testargs_1.cookie.n_len = sizeof(cookie);
					grantargs.testargs_1.cookie.n_bytes = (char *)&cookie;
					dup_netobj(&argp->alock.fh, &grantargs.testargs_1.alock.fh);
					dup_netobj(&argp->alock.oh, &grantargs.testargs_1.alock.oh);
					if (argp->alock.caller_name) {
						namelen = strlen(argp->alock.caller_name) + 1;
						grantargs.testargs_1.alock.caller_name =
							NLM_KMEM_ALLOC(namelen, KM_SLEEP);
						bcopy(argp->alock.caller_name,
							grantargs.testargs_1.alock.caller_name, namelen);
					}
					/*
					 * set the callback proc
					 * according to the XNFS specification, the asynchronous
					 * callback must be used if an asynchronous lock call
					 * was used
					 */
					if (req->rq_proc == NLM_LOCK_MSG) {
						callback_proc = NLM_GRANTED_MSG;
						timeo = 0;
						retrans = TRUE;
					} else {
						callback_proc = NLM_GRANTED;
						timeo = nlm_granted_timeout;
						retrans = FALSE;
					}
					/*
					 * Reset error to 0 so we don't return an incorrect
					 * error.
					 */
					error = 0;
					/*
					 * Create a new process.
					 * Do not create as a sys process.  The child should be
					 * killable.
					 * The parent will return and reply normally.
					 * The child will block and make an NLM_GRANTED callback
					 * to the client.
					 */
					error = fork(NULL, &rv);
					if (!error) {
						if (rv.r_val1 != 0) {
							vp = NULL; /* child is responsible */
							resp->stat.stat = nlm_blocked;
							NLM_DEBUG(NLMDEBUG_BLOCKED,
								printf("proc_nlm_lock: reply nlm_blocked, "
									"offset %lld len %lld client %s pid "
									"%d\n", fl.l_start, fl.l_len,
									argp->alock.caller_name, fl.l_pid));
						} else {
							NLM_DEBUG(NLMDEBUG_PROCS,
								printf("proc_nlm_lock: new process for "
									"client proc %d\n", fl.l_pid));
							/*
							 * This process will need to exit when it
							 * is through.  It will also need to send a
							 * granted message to the client.
							 * After this point, the child process must
							 * not reference args or res or any data they
							 * might point to.
							 * Also, the child must not return.
							 */
							error = local_lock_vp(vp, &fl, 1);
							NLM_DEBUG(NLMDEBUG_ERROR,
								if (error && (error != EINTR) &&
									(error != EBUSY))
										printf("proc_nlm_lock: lock error %d\n",
											error));
							grantres.statres_1.stat.stat = errno_to_nlm(error);
							if (grantres.statres_1.stat.stat == nlm_granted) {
								NLM_DEBUG(NLMDEBUG_BLOCKED,
									printf("proc_nlm_lock: proc %lld granting, "
										"offset %lld len %lld client %s pid "
										"%d\n", get_thread_id(),
										fl.l_start, fl.l_len,
										grantargs.testargs_1.alock.caller_name,
										fl.l_pid));
								ASSERT(!error);
								if (!timeo) {
									res_wait = get_nlm_response_wait(cookie,
										&ipaddr);
								}
retransmit:
								error = nlm_callback(vers, callback_proc,
									&ipaddr, xdr_nlm_testargs,
									(caddr_t)&grantargs, xdr_nlm_res,
									(caddr_t)&grantres, retrans, timeo);
								switch (error) {
									case 0:
										if (timeo) {
											callback_status =
												grantres.statres_1.stat.stat;
										} else {
											ASSERT(res_wait);
											error = await_nlm_response(res_wait,
												&ipaddr, NULL,
												(nlm4_stats *)&callback_status,
												res_timeo);
											switch (error) {
												case 0:
													break;
												default:
													NLM_DEBUG(NLMDEBUG_ERROR,
														printf("proc_nlm_lock:"
														" await_nlm_response "
														"error %d\n", error));
												case EINTR:
													break;
												case ETIMEDOUT:
													NLM_DEBUG(NLMDEBUG_TRACE,
														printf("proc_nlm_lock:"
															" wait timed out, "
															"retransmitting "
															"request\n"));
													res_timeo = lm_backoff(
														res_timeo);
													retrans = TRUE;
													goto retransmit;
											}
										}
										if (callback_status != nlm_granted) {
											NLM_DEBUG(NLMDEBUG_TRACE |
												NLMDEBUG_GRANTED,
												printf("NLM lock grant denied, status %d\n",
												callback_status));
											fl.l_type = F_UNLCK;
											error = local_unlock_vp(vp, &fl, 0);
											if (error) {
												NLM_DEBUG(NLMDEBUG_ERROR,
													printf("unable to unlock after denied NLM grant, error %d\n",
													error));
											}
										}
										break;
									case ETIMEDOUT:
										/*
										 * handle cases where getting the port
										 * number times out
										 */
										retrans = TRUE;
										goto retransmit;
									case EINTR:
									case ENOENT:
										error = 0;
										break;
									default:
										cmn_err(CE_WARN,
											"Blocked NLM %d lock request "
											"grant error, client %s: "
											"error %d\n", vers,
											inet_ntoa(ipaddr), error);
								}
								if (res_wait) {
									release_nlm_wait(res_wait);
								}
							} else {
								switch (error) {
									case EBUSY:		/* lock request pending */
									case EINTR:		/* cancelled lock */
									case ESTALE:	/* stale file handle */
										break;
									default:
										cmn_err(CE_WARN,
											"Blocked NLM %d lock request not "
											"granted, client %s: %s error %d\n",
											vers, inet_ntoa(ipaddr),
											nlmstats_to_str(
											grantres.statres_1.stat.stat),
											error);
								}
							}
							/*
							 * No matter what, the data allocated above for
							 * the grant callback must be freed.
							 */
							free_netobj(&grantargs.testargs_1.alock.fh);
							free_netobj(&grantargs.testargs_1.alock.oh);
							NLM_KMEM_FREE(
								grantargs.testargs_1.alock.caller_name,
								namelen);
							NLM_DEBUG(NLMDEBUG_PROCS,
								printf("proc_nlm_lock: process for "
									"client proc %d exiting\n", fl.l_pid));
							VN_RELE(vp);
							atomicAddInt(&lockd_procs, -1);
							exit(CLD_EXITED, 0);
						}
					} else {
						/*
						 * No process could be created to block on the
						 * lock.  This requires an nlm_denied_nolocks
						 * reply.
						 */
						NLM_DEBUG(NLMDEBUG_ERROR,
							printf("proc_nlm_lock: fork error %d\n", error));
						error = 0;
						resp->stat.stat = nlm_denied_nolocks;
						/*
						 * Make sure we free the grant callback data.
						 */
						free_netobj(&grantargs.testargs_1.alock.fh);
						free_netobj(&grantargs.testargs_1.alock.oh);
						NLM_KMEM_FREE(grantargs.testargs_1.alock.caller_name,
							namelen);
					}
				} else {
					resp->stat.stat = errno_to_nlm(error);
					error = 0;
				}
				break;
			default:
				resp->stat.stat = errno_to_nlm(error);
				error = 0;
				break;
		}
		if (vp)
			VN_RELE(vp);
	} else {
		error = 0;
		resp->stat.stat = nlm_denied;
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_REPLY | NLMDEBUG_LOCK,
		printf("proc_nlm_lock(%d): reply sysid %s error %d stat %s\n",
			req->rq_proc, inet_ntoa(ipaddr), error,
			nlmstats_to_str(resp->stat.stat)));
	*reply_status = (nlm4_stats)resp->stat.stat;
	return(error);
}

/*
 * NLM_UNLOCK and NLM_UNLOCK_MSG
 */
/* ARGSUSED */
int
nlm_unlock_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh;
	int status = 0;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->unlockargs_1.alock.fh.n_len ||
		args->unlockargs_1.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm_unlock_exi: caller %s fh %s\n",
		args->unlockargs_1.alock.caller_name,
		netobj_to_str(&args->unlockargs_1.alock.fh)));
	fh = (fhandle_t *)args->unlockargs_1.alock.fh.n_bytes;
	if (fh && (args->unlockargs_1.alock.fh.n_len == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi) {
			status = 1;
		}
	}
	return(status);
}

int
proc_nlm_unlock(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct in_addr *ipaddr = &req->rq_xprt->xp_raddr.sin_addr;
	fhandle_t *fhp = NULL;
	nlm_unlockargs *argp = &args->unlockargs_1;
	nlm_res *resp = &res->statres_1;
	struct flock fl;

	ASSERT(!args->unlockargs_1.alock.fh.n_len ||
		args->unlockargs_1.alock.fh.n_bytes);
	if (req->rq_vers == NLM_VERSX) {
		if (argp->alock.l_len == 0xffffffff) {
			argp->alock.l_len = 0;
		}
	}
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("proc_nlm_unlock(%d): sysid %s offset %d len %d\n",
			req->rq_proc, inet_ntoa(*ipaddr),
			argp->alock.l_offset, argp->alock.l_len));
	/*
	 * copy the cookie to the response
	 * we'll need it regardless of what happens below
	 */
	resp->cookie = argp->cookie;
	/*
	 * fill in the flock structure from the test args
	 */
	fl.l_type = F_UNLCK;
	fl.l_whence = 0;
	fl.l_start = (off_t)argp->alock.l_offset;
	fl.l_len = (off_t)argp->alock.l_len;
	fl.l_sysid = ipaddr->s_addr;
	fl.l_pid = argp->alock.svid;
	/*
	 * this is NLM version 1 or 3, so the file handle is an NFS
	 * version 2 file handle
	 */
	if (argp->alock.fh.n_len == NFS_FHSIZE) {
		fhp = (fhandle_t *)argp->alock.fh.n_bytes;
		resp->stat.stat = errno_to_nlm(local_unlock(exi, fhp, &fl, 0));
	} else {
		resp->stat.stat = nlm_denied;
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_REPLY,
		printf("proc_nlm_unlock(%d): reply sysid %s stat %s\n", req->rq_proc,
			inet_ntoa(*ipaddr), nlmstats_to_str(resp->stat.stat)));
	*reply_status = (nlm4_stats)resp->stat.stat;
	return(0);
}

/*
 * NLM_CANCEL and NLM_CANCEL_MSG
 */
/* ARGSUSED */
int
nlm_cancel_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh;
	int status = 1;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->cancelargs_1.alock.fh.n_len ||
		args->cancelargs_1.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm_cancel_exi: caller %s fh %s\n",
		args->cancelargs_1.alock.caller_name,
		netobj_to_str(&args->cancelargs_1.alock.fh)));
	fh = (fhandle_t *)args->cancelargs_1.alock.fh.n_bytes;
	if (fh && (args->cancelargs_1.alock.fh.n_len == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

int
proc_nlm_cancel(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct in_addr *ipaddr = &req->rq_xprt->xp_raddr.sin_addr;
	fhandle_t *fhp = NULL;
	nlm_cancargs *argp = &args->cancelargs_1;
	nlm_res *resp = &res->statres_1;
	struct flock fl;

	ASSERT(!args->cancelargs_1.alock.fh.n_len ||
		args->cancelargs_1.alock.fh.n_bytes);
	if (req->rq_vers == NLM_VERSX) {
		if (argp->alock.l_len == 0xffffffff) {
			argp->alock.l_len = 0;
		}
	}
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CANCEL,
		printf("proc_nlm_cancel(%d): sysid %s offset %d len %d\n",
			req->rq_proc, inet_ntoa(*ipaddr),
			argp->alock.l_offset, argp->alock.l_len));
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
	} else {
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = F_UNLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = ipaddr->s_addr;
		fl.l_pid = argp->alock.svid;
		/*
		 * this is NLM version 1 or 3, so the file handle is an NFS
		 * version 2 file handle
		 */
		if (argp->alock.fh.n_len == NFS_FHSIZE) {
			fhp = (fhandle_t *)argp->alock.fh.n_bytes;
			resp->stat.stat = errno_to_nlm(local_unlock(exi, fhp, &fl, 1));
		} else {
			resp->stat.stat = nlm_denied;
		}
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_REPLY | NLMDEBUG_CANCEL,
		printf("proc_nlm_cancel(%d): reply sysid %s stat %s\n", req->rq_proc,
			inet_ntoa(*ipaddr), nlmstats_to_str(resp->stat.stat)));
	*reply_status = (nlm4_stats)resp->stat.stat;
	return(0);
}

/* ARGSUSED */
int
nlm_free_all(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	destroy_client_shares(req, args->notify_1.name);
	return(0);
}

/* ARGSUSED */
int
nlm4_test_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh = NULL;
	int status = 1;
	int fhlen;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->testargs_4.alock.fh.n_len ||
		args->testargs_4.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm4_test_exi: caller %s fh %s\n",
		args->testargs_4.alock.caller_name,
		netobj_to_str(&args->testargs_4.alock.fh)));
	fhlen = args->testargs_4.alock.fh.n_len;
	fh = (fhandle_t *)args->testargs_4.alock.fh.n_bytes;
	if (fhlen && fh && (fhlen == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

/* ARGSUSED */
int
proc_nlm4_test(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct in_addr *ipaddr = &req->rq_xprt->xp_raddr.sin_addr;
	int error;
	fhandle_t *fhp;
	nlm4_testargs *argp = &args->testargs_4;
	nlm4_testres *resp = &res->testres_4;
	struct flock fl;

	ASSERT(!args->testargs_4.alock.fh.n_len ||
		args->testargs_4.alock.fh.n_bytes);
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("proc_nlm4_test(%d): sysid %s offset %lld len %lld type %s\n",
			req->rq_proc, inet_ntoa(*ipaddr),
			argp->alock.l_offset, argp->alock.l_len,
			argp->exclusive ? "F_WRLCK" : "F_RDLCK"));
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
		bzero(&resp->stat.nlm4_testrply_u, sizeof(resp->stat.nlm4_testrply_u));
	} else {
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = ipaddr->s_addr;
		fl.l_pid = argp->alock.svid;
		/*
		 * The file handle is an NFS3 file handle.  Such file handles
		 * are variable in length.
		 */
		fhp = (fhandle_t *)argp->alock.fh.n_bytes;
		if (fhp && (argp->alock.fh.n_len == NFS_FHSIZE)) {
			error = local_test(exi, fhp, &fl);
			if (error) {
				resp->stat.stat = errno_to_nlm4(error);
			} else if (fl.l_type == F_UNLCK) {
				resp->stat.stat = NLM4_GRANTED;
			} else {
				resp->stat.stat = NLM4_DENIED;
				resp->stat.nlm4_testrply_u.holder.svid = fl.l_pid;
				resp->stat.nlm4_testrply_u.holder.l_offset = fl.l_start;
				resp->stat.nlm4_testrply_u.holder.l_len = fl.l_len;
				resp->stat.nlm4_testrply_u.holder.exclusive =
					(fl.l_type == F_WRLCK);
				resp->stat.nlm4_testrply_u.holder.oh.n_len = 0;
				resp->stat.nlm4_testrply_u.holder.oh.n_bytes = NULL;
			}
		} else {
			cmn_err(CE_WARN,
				"NLMPROC_TEST: invalid file handle, len %d, sysid %s\n",
				argp->alock.fh.n_len, inet_ntoa(*ipaddr));
			resp->stat.stat = NLM4_FAILED;
		}
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_REPLY,
		printf("proc_nlm4_test(%d): reply sysid %s stat %s offset %lld "
			"len %lld type %s\n", req->rq_proc,
			inet_ntoa(*ipaddr), nlm4stats_to_str(resp->stat.stat),
			resp->stat.nlm4_testrply_u.holder.l_offset,
			resp->stat.nlm4_testrply_u.holder.l_len,
			resp->stat.nlm4_testrply_u.holder.exclusive ? "F_WRLCK" :
			"F_RDLCK"));
	*reply_status = resp->stat.stat;
	return(0);
}

/*
 * NLMPROC_LOCK and NLM4PROC_NM_LOCK
 */
/* ARGSUSED */
int
nlm4_lock_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh = NULL;
	int status = 1;
	int fhlen;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->lockargs_4.alock.fh.n_len ||
		args->lockargs_4.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm4_lock_exi: caller %s fh %s\n",
		args->lockargs_4.alock.caller_name,
		netobj_to_str(&args->lockargs_4.alock.fh)));
	fhlen = args->lockargs_4.alock.fh.n_len;
	fh = (fhandle_t *)args->lockargs_4.alock.fh.n_bytes;
	if (fhlen && fh && (fhlen == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

int
proc_nlm4_lock(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	bool_t retrans;
	struct in_addr ipaddr = req->rq_xprt->xp_raddr.sin_addr;
	char *name = NULL;
	int namelen = 0;
	fhandle_t *fhp;
	int error = 0;
	nlm4_lockargs *argp = &args->lockargs_4;
	nlm4_res *resp = &res->statres_4;
	struct flock fl;
	union nlm_args_u grantargs;
	union nlm_res_u grantres;
	extern int fork(void *uap, rval_t *rvp);
	rval_t rv;
	int vers = req->rq_vers;
	int callback_proc;
	int timeo;
	long cookie;
	void *res_wait = NULL;
	int res_timeo = nlm_granted_timeout;
	nlm4_stats callback_status;
	vnode_t *vp = NULL;

	ASSERT(!args->lockargs_4.alock.fh.n_len ||
		args->lockargs_4.alock.fh.n_bytes);
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_LOCK,
		printf("proc_nlm4_lock(%d): sysid %s offset %lld len %lld type %s %s\n",
			req->rq_proc, inet_ntoa(ipaddr),
			argp->alock.l_offset, argp->alock.l_len,
			argp->exclusive ? "F_WRLCK" : "F_RDLCK",
			argp->reclaim ? "reclaim" : ""));
	/*
	 * copy the cookie to the response
	 * we'll need it regardless of what happens below
	 */
	resp->cookie = argp->cookie;
	fhp = (fhandle_t *)argp->alock.fh.n_bytes;
	/*
	 * If we're still in the grace period, deny this lock unless it is a
	 * reclaim.
	 */
	if (in_grace_period && !argp->reclaim) {
		resp->stat.stat = NLM4_DENIED_GRACE_PERIOD;
		error = 0;
	} else if (fhp && (argp->alock.fh.n_len == NFS_FHSIZE)) {
		if (req->rq_proc != NLMPROC_NM_LOCK) {
			/*
			 * Encode the client name and the IP address to which
			 * it sent the request into a single name string.  The
			 * name is encoded as <client name>,<IP address> where
			 * <client name> is the caller name sent by the client
			 * and <IP address> is the IP address to which the
			 * client sent the RPC request.  The length of this
			 * string will be the length of the client name plus
			 * 15 for the largest possible IP address plus 1 for
			 * the separator (a ':') plus 1 for the NULL
			 * terminator.
			 * The purpose of this encoding is to enable statd
			 * to do notification for multiple interfaces
			 * without having to query for the configured
			 * interfaces and send crash notifications for
			 * all.  This is only done on the server side as
			 * the server could be receiving requests on any
			 * configured interface.
			 * Detect NULL client names so we don't panic the system.
			 * Also detect client names of 0 length.
			 */
			if (argp->alock.caller_name &&
				(namelen = strlen(argp->alock.caller_name))) {
					/*
					 * Do not record the name for requests comming in over
					 * loopback.
					 */
					if (nfs_srcaddr(req->rq_xprt)->s_addr != INADDR_LOOPBACK) {
						namelen += 17;
						name = NLM_KMEM_ALLOC(namelen, KM_SLEEP);
						(void)sprintf(name, "%s:%s", argp->alock.caller_name,
							inet_ntoa(*nfs_srcaddr(req->rq_xprt)));
						error = record_remote_name(name);
						NLM_KMEM_FREE(name, namelen);
					}
			} else {
				error = EINVAL;
			}
			if (error) {
				cmn_err(CE_WARN,
					"!Network lock manager: bad client name \"%s\" from %s "
					"port %d",
					argp->alock.caller_name ? argp->alock.caller_name : "NULL",
					inet_ntoa(req->rq_xprt->xp_raddr.sin_addr),
					req->rq_xprt->xp_raddr.sin_port);
				*reply_status = resp->stat.stat = NLM4_FAILED;
				return(0);
			}
		}
		/*
		 * The file handle is an NFS3 file handle.
		 */
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = ipaddr.s_addr;
		fl.l_pid = argp->alock.svid;
		/*
		 * Get a vnode reference from the file handle and attempt
		 * the lock.  The vnode reference must be released by this
		 * thread or by the forked child.
		 */
		error = local_fhtovp(exi, fhp, &vp);
		if (!error)
			error = local_lock_vp(vp, &fl, 0);
		switch (error) {
			case EACCES:
			case EAGAIN:
				ASSERT(vp);
				if (argp->block) {
					/*
					 * limit the number of blocked lockd processes
					 */
					if (atomicAddInt(&lockd_procs, 1) > max_lockd_procs) {
						atomicAddInt(&lockd_procs, -1);
						resp->stat.stat = NLM4_DENIED_NOLOCKS;
						error = 0;
						break;
					}
					/*
					 * fill in the flock structure again from
					 * the test args in case it was modified in
					 * local_lock processing
					 */
					fl.l_type = argp->exclusive ? F_WRLCK : F_RDLCK;
					fl.l_whence = 0;
					fl.l_start = (off_t)argp->alock.l_offset;
					fl.l_len = (off_t)argp->alock.l_len;
					fl.l_sysid = ipaddr.s_addr;
					fl.l_pid = argp->alock.svid;
					/*
					 * This is a blocking request, so we must call local_lock
					 * again, telling it to block.  First, set up the args
					 * for the grant callback, copying the data from the
					 * lock args.  We must copy because the args will get
					 * freed or reused.
					 */
					bzero(&grantargs, sizeof(grantargs));
					bzero(&grantres, sizeof(grantres));
					grantargs.testargs_4.exclusive = fl.l_type == F_WRLCK;
					grantargs.testargs_4.alock.svid = fl.l_pid;
					grantargs.testargs_4.alock.l_offset = fl.l_start;
					grantargs.testargs_4.alock.l_len = fl.l_len;
					grantargs.testargs_4.cookie.n_len = sizeof(cookie);
					grantargs.testargs_4.cookie.n_bytes = (char *)&cookie;
					cookie = atomicAddLong(&Cookie, 1);
					dup_netobj(&argp->alock.fh, &grantargs.testargs_4.alock.fh);
					dup_netobj(&argp->alock.oh, &grantargs.testargs_4.alock.oh);
					if (argp->alock.caller_name) {
						namelen = strlen(argp->alock.caller_name) + 1;
						grantargs.testargs_4.alock.caller_name =
							NLM_KMEM_ALLOC(namelen, KM_SLEEP);
						bcopy(argp->alock.caller_name,
							grantargs.testargs_4.alock.caller_name, namelen);
					}
					/*
					 * set the callback proc
					 * according to the XNFS specification, the asynchronous
					 * callback must be used if an asynchronous lock call
					 * was used
					 */
					if (req->rq_proc == NLMPROC_LOCK_MSG) {
						callback_proc = NLMPROC_GRANTED_MSG;
						timeo = 0;
						retrans = TRUE;
					} else {
						callback_proc = NLMPROC_GRANTED;
						timeo = nlm_granted_timeout;
						retrans = FALSE;
					}
					/*
					 * Reset error to 0 so we don't return an incorrect
					 * error.
					 */
					error = 0;
					/*
					 * Create a new process.
					 * Do not create as a sys process.  The child should be
					 * killable.
					 * The parent will return and reply normally.
					 * The child will block and make an NLMPROC_GRANTED
					 * callback to the client.
					 */
					error = fork(NULL, &rv);
					if (!error) {
						if (rv.r_val1 != 0) {
							vp = NULL; /* child is responsible */
							resp->stat.stat = NLM4_BLOCKED;
							NLM_DEBUG(NLMDEBUG_BLOCKED,
								printf("proc_nlm4_lock: reply nlm_blocked, "
									"offset %lld len %lld client %s pid "
									"%d\n", fl.l_start, fl.l_len,
									argp->alock.caller_name, fl.l_pid));
						} else {
							NLM_DEBUG(NLMDEBUG_PROCS,
								printf("proc_nlm4_lock: new process for "
									"client proc %d\n", fl.l_pid));
							/*
							 * This process will need to exit when it
							 * is through.  It will also need to send a
							 * granted message to the client.
							 * After this point, the child process must
							 * not reference args or res or any data they
							 * might point to.
							 * Also, the child must not return.
							 */
							error = local_lock_vp(vp, &fl, 1);
							NLM_DEBUG(NLMDEBUG_ERROR,
								if (error && (error != EINTR) &&
									(error != EBUSY))
									printf("proc_nlm4_lock: lock error %d\n",
										error));
							grantres.statres_4.stat.stat = errno_to_nlm4(error);
							if (grantres.statres_4.stat.stat == NLM4_GRANTED) {
								NLM_DEBUG(NLMDEBUG_BLOCKED,
									printf("proc_nlm4_lock: proc %lld "
										"granting, offset %lld len %lld "
										"client %s pid %d\n", get_thread_id(),
										fl.l_start, fl.l_len,
										grantargs.testargs_4.alock.caller_name,
										fl.l_pid));
								if (!timeo) {
									res_wait = get_nlm_response_wait(cookie,
										&ipaddr);
								}
retransmit:
								error = nlm_callback(vers, callback_proc,
									&ipaddr, xdr_nlm4_testargs,
									(caddr_t)&grantargs, xdr_nlm4_res,
									(caddr_t)&grantres, retrans, timeo);
								switch (error) {
									case 0:
										if (timeo) {
											callback_status =
												grantres.statres_4.stat.stat;
										} else {
											ASSERT(res_wait);
											error = await_nlm_response(res_wait,
												&ipaddr, NULL, &callback_status,
												res_timeo);
											switch (error) {
												case 0:
													break;
												default:
													NLM_DEBUG(NLMDEBUG_ERROR,
														printf("proc_nlm4_lock:"
														" await_nlm_response "
														"error %d\n", error));
												case EINTR:
													break;
												case ETIMEDOUT:
													NLM_DEBUG(NLMDEBUG_TRACE,
														printf("proc_nlm4_lock:"
															" wait timed out, "
															"retransmitting "
															"request\n"));
													res_timeo = lm_backoff(
														res_timeo);
													retrans = TRUE;
													goto retransmit;
											}
										}
										if (callback_status != NLM4_GRANTED) {
											NLM_DEBUG(NLMDEBUG_TRACE |
												NLMDEBUG_GRANTED,
												printf("NLM4 lock grant denied, status %d\n",
												callback_status));
											fl.l_type = F_UNLCK;
											error = local_unlock_vp(vp, &fl, 0);
											if (error) {
												NLM_DEBUG(NLMDEBUG_ERROR,
													printf("unable to unlock after denied NLM4 grant, error %d\n",
													error));
											}
										}
										break;
									case ETIMEDOUT:
										/*
										 * handle cases where getting the port
										 * number times out
										 */
										retrans = TRUE;
										goto retransmit;
									case EINTR:
									case ENOENT:
										error = 0;
										break;
									default:
										cmn_err(CE_WARN,
											"Blocked NLM %d lock request "
											"grant error, client %s: "
											"error %d\n", vers,
											inet_ntoa(ipaddr), error);
								}
								if (res_wait) {
									release_nlm_wait(res_wait);
								}
							} else {
								switch (error) {
									case EBUSY:		/* lock request pending */
									case EINTR:		/* cancelled lock */
									case ESTALE:	/* stale file handle */
										break;
									default:
										cmn_err(CE_WARN,
											"Blocked NLM4 %d lock request "
											"not granted, client %s: %s "
											"error %d\n", vers,
											inet_ntoa(ipaddr),
											nlm4stats_to_str(
											grantres.statres_4.stat.stat),
											error);
								}
							}
							/*
							 * No matter what, the data allocated above for
							 * the grant callback must be freed.
							 */
							free_netobj(&grantargs.testargs_4.alock.fh);
							free_netobj(&grantargs.testargs_4.alock.oh);
							NLM_KMEM_FREE(
								grantargs.testargs_4.alock.caller_name,
								namelen);
							NLM_DEBUG(NLMDEBUG_PROCS,
								printf("proc_nlm4_lock: process for "
									"client proc %d exiting\n", fl.l_pid));
							VN_RELE(vp);
							atomicAddInt(&lockd_procs, -1);
							exit(CLD_EXITED, 0);
						}
					} else {
						/*
						 * No process could be created to block on the
						 * lock.  This requires an nlm_denied_nolocks
						 * reply.
						 */
						NLM_DEBUG(NLMDEBUG_ERROR,
							printf("proc_nlm4_lock: fork error %d\n", error));
						error = 0;
						resp->stat.stat = NLM4_DENIED_NOLOCKS;
						/*
						 * Make sure we free the grant callback data.
						 */
						free_netobj(&grantargs.testargs_4.alock.fh);
						free_netobj(&grantargs.testargs_4.alock.oh);
						NLM_KMEM_FREE(grantargs.testargs_4.alock.caller_name,
							namelen);
					}
				} else {
					resp->stat.stat = errno_to_nlm4(error);
					error = 0;
				}
				break;
			default:
				resp->stat.stat = errno_to_nlm4(error);
				error = 0;
		}
		if (vp)
			VN_RELE(vp);
	} else {
		cmn_err(CE_WARN,
			"NLMPROC_LOCK: invalid file handle, len %d, sysid %s\n",
			argp->alock.fh.n_len, inet_ntoa(ipaddr));
		resp->stat.stat = NLM4_FAILED;
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_REPLY | NLMDEBUG_LOCK,
		printf("proc_nlm4_lock(%d): sysid %s error %d stat %s\n",
			req->rq_proc, inet_ntoa(ipaddr), error,
			nlm4stats_to_str(resp->stat.stat)));
	*reply_status = resp->stat.stat;
	return(error);
}

/* ARGSUSED */
int
nlm4_unlock_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh = NULL;
	int status = 1;
	int fhlen;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->unlockargs_4.alock.fh.n_len ||
		args->unlockargs_4.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm4_unlock_exi: caller %s fh %s\n",
		args->unlockargs_4.alock.caller_name,
		netobj_to_str(&args->unlockargs_4.alock.fh)));
	fhlen = args->unlockargs_4.alock.fh.n_len;
	fh = (fhandle_t *)args->unlockargs_4.alock.fh.n_bytes;
	if (fhlen && fh && (fhlen == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

/* ARGSUSED */
int
proc_nlm4_unlock(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct in_addr *ipaddr = &req->rq_xprt->xp_raddr.sin_addr;
	fhandle_t *fhp;
	nlm4_unlockargs *argp = &args->unlockargs_4;
	nlm4_res *resp = &res->statres_4;
	struct flock fl;

	ASSERT(!args->unlockargs_4.alock.fh.n_len ||
		args->unlockargs_4.alock.fh.n_bytes);
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE,
		printf("proc_nlm4_unlock(%d): sysid %s offset %lld len %lld\n",
			req->rq_proc, inet_ntoa(*ipaddr),
			argp->alock.l_offset, argp->alock.l_len));
	/*
	 * copy the cookie to the response
	 * we'll need it regardless of what happens below
	 */
	resp->cookie = argp->cookie;
	fhp = (fhandle_t *)argp->alock.fh.n_bytes;
	if (fhp && (argp->alock.fh.n_len == NFS_FHSIZE)) {
		/*
		 * fill in the flock structure from the test args
		 */
		fl.l_type = F_UNLCK;
		fl.l_whence = 0;
		fl.l_start = (off_t)argp->alock.l_offset;
		fl.l_len = (off_t)argp->alock.l_len;
		fl.l_sysid = ipaddr->s_addr;
		fl.l_pid = argp->alock.svid;
		resp->stat.stat = errno_to_nlm4(local_unlock(exi, fhp, &fl, 0));
	} else {
		cmn_err(CE_WARN,
			"NLMPROC_UNLOCK: invalid file handle, len %d, sysid %s\n",
			argp->alock.fh.n_len, inet_ntoa(*ipaddr));
		resp->stat.stat = NLM4_FAILED;
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_REPLY,
		printf("proc_nlm4_unlock(%d): reply sysid %s stat %s\n",
			req->rq_proc, inet_ntoa(*ipaddr),
			nlm4stats_to_str(resp->stat.stat)));
	*reply_status = resp->stat.stat;
	return(0);
}

/* ARGSUSED */
int
nlm4_cancel_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh = NULL;
	int status = 1;
	int fhlen;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->cancelargs_4.alock.fh.n_len ||
		args->cancelargs_4.alock.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm4_cancel_exi: caller %s fh %s\n",
		args->cancelargs_4.alock.caller_name,
		netobj_to_str(&args->cancelargs_4.alock.fh)));
	fhlen = args->cancelargs_4.alock.fh.n_len;
	fh = (fhandle_t *)args->cancelargs_4.alock.fh.n_bytes;
	if (fhlen && fh && (fhlen == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

/* ARGSUSED */
int
proc_nlm4_cancel(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	struct in_addr *ipaddr = &req->rq_xprt->xp_raddr.sin_addr;
	fhandle_t *fhp;
	nlm4_cancargs *argp = &args->cancelargs_4;
	nlm4_res *resp = &res->statres_4;
	struct flock fl;

	ASSERT(!args->cancelargs_4.alock.fh.n_len ||
		args->cancelargs_4.alock.fh.n_bytes);
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CANCEL,
		printf("proc_nlm4_cancel(%d): sysid %s offset %lld len %lld\n",
			req->rq_proc, inet_ntoa(*ipaddr),
			argp->alock.l_offset, argp->alock.l_len));
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
	} else {
		fhp = (fhandle_t *)argp->alock.fh.n_bytes;
		if (fhp && (argp->alock.fh.n_len == NFS_FHSIZE)) {
			/*
			 * fill in the flock structure from the test args
			 */
			fl.l_type = F_UNLCK;
			fl.l_whence = 0;
			fl.l_start = (off_t)argp->alock.l_offset;
			fl.l_len = (off_t)argp->alock.l_len;
			fl.l_sysid = ipaddr->s_addr;
			fl.l_pid = argp->alock.svid;
			resp->stat.stat = errno_to_nlm4(local_unlock(exi, fhp, &fl, 1));
		} else {
			cmn_err(CE_WARN,
				"NLMPROC_UNLOCK: invalid file handle, len %d, sysid %s\n",
				argp->alock.fh.n_len, inet_ntoa(*ipaddr));
			resp->stat.stat = NLM4_FAILED;
		}
	}
	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_REPLY | NLMDEBUG_CANCEL,
		printf("proc_nlm4_cancel(%d): reply sysid %s stat %s\n",
			req->rq_proc, inet_ntoa(*ipaddr),
			nlm4stats_to_str(resp->stat.stat)));
	*reply_status = resp->stat.stat;
	return(0);
}

/*
 * This is used for all versions.  The argument and results structures are
 * identical for NLM_VERSX and NLM4_VERS.
 */
int
proc_nlm4_share(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	nlm4_shareargs *argp = &args->shareargs_4;
	nlm4_shareres *resp = &res->shareres_4;

	ASSERT(!argp->share.fh.n_len || argp->share.fh.n_bytes);
	ASSERT(!argp->cookie.n_len || argp->cookie.n_bytes);

	if (argp->share.fh.n_len && argp->share.fh.n_bytes) {
		resp->cookie = argp->cookie;
		if (in_grace_period && !(argp->reclaim)) {
			resp->stat = NLM4_DENIED_GRACE_PERIOD;
		} else {
			proc_nlm_share(req, argp, resp, exi);
			/*
		 	 * Translate to the old status codes 
			 * for the older versions.
		 	 */
			if (req->rq_vers != NLM4_VERS) {
				resp->stat = (nlm4_stats)nlm4stats_to_nlm[resp->stat];
			}
		}
	} else {
		/* Empty file handle */
		resp->stat = NLM4_FAILED;
	}
	*reply_status = resp->stat;
	return(0);
}

int
nlm4_share_exi(struct svc_req *req, union nlm_args_u *args,
	struct exportinfo **exi)
{
	fhandle_t *fh = NULL;
	int status = 1;
	int fhlen;

	ASSERT(exi);
	ASSERT(args);
	ASSERT(!args->shareargs_4.share.fh.n_len ||
		args->shareargs_4.share.fh.n_bytes);
	NLM_DEBUG(NLMDEBUG_TRACE, printf("nlm4_cancel_exi: caller %s fh %s\n",
		args->shareargs_4.share.caller_name,
		netobj_to_str(&args->shareargs_4.share.fh)));
	switch (req->rq_vers) {
		case NLM_VERSX:
			fhlen = args->shareargs_1.share.fh.n_len;
			fh = (fhandle_t *)args->shareargs_1.share.fh.n_bytes;
			break;
		case NLM4_VERS:
			fhlen = args->shareargs_4.share.fh.n_len;
			fh = (fhandle_t *)args->shareargs_4.share.fh.n_bytes;
			break;
		case NLM_VERS:
		default:
			return(0);
	}
	if (fhlen && fh && (fhlen == NFS_FHSIZE)) {
		*exi = findexport(&fh->fh_fsid, (struct fid *) &fh->fh_xlen);
		if (*exi == NULL) {
			status = 0;
		}
	} else {
		status = 0;
	}
	return(status);
}

/* ARGSUSED */
int
nlm4_free_all(struct svc_req *req, union nlm_args_u *args,
	union nlm_res_u *res, struct exportinfo *exi, nlm4_stats *reply_status)
{
	destroy_client_shares(req, args->notify_4.name);
	return(0);
}

/*
 * function to unlock export entries
 * this is a function so that it can be stubbed and lockd installed without
 * the nfs server
 */
void
export_unlock(void)
{
	EXPORTED_MRUNLOCK();
}

void
nlmsvc_init(void)
{
	mutex_init(&VnodeTableLock, MUTEX_DEFAULT, "Vnode table lock");
	spinlock_init(&addrhash_lock, "address hash lock");
}
