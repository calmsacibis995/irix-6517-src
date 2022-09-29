#include "types.h"
#include <sys/param.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ksignal.h>
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
#include <sys/idbg.h>
#include <sys/idbgentry.h>

#include "xdr.h"
#include "auth.h"
#include "clnt.h"
#include "svc.h"

#include "lockmgr.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "nlm_rpc.h"
#include "nlm_debug.h"
#include "lockd_impl.h"

#define MAXSTR	128

#ifdef NLMDEBUG
int nlm_debug = NLMDEBUG_ERROR | NLMDEBUG_NOPROG | NLMDEBUG_CANTSEND |
	NLMDEBUG_NOPROC | NLMDEBUG_VERSERR;
#else /* NLMDEBUG */
int nlm_debug = 0;
#endif /* NLMDEBUG */

static char str[MAXSTR];

char *
nlmstats_to_str(nlm_stats stat)
{
	char *strp = str;

	switch (stat) {
		case nlm_granted:
			strp = "nlm_granted";
			break;
		case nlm_denied:
			strp = "nlm_denied";
			break;
		case nlm_denied_nolocks:
			strp = "nlm_denied_nolocks";
			break;
		case nlm_blocked:
			strp = "nlm_blocked";
			break;
		case nlm_denied_grace_period:
			strp = "nlm_denied_grace_period";
			break;
		case nlm_deadlck:
			strp = "nlm_deadlck";
			break;
		default:
			sprintf(strp, "invalid stat (%d)", (int)stat);
	}
	return(strp);
}

char *
nlm4stats_to_str(nlm4_stats stat)
{
	char *strp = str;

	switch (stat) {
		case NLM4_GRANTED:
			strp = "NLM4_GRANTED";
			break;
		case NLM4_DENIED:
			strp = "NLM4_DENIED";
			break;
		case NLM4_DENIED_NOLOCKS:
			strp = "NLM4_DENIED_NOLOCKS";
			break;
		case NLM4_BLOCKED:
			strp = "NLM4_BLOCKED";
			break;
		case NLM4_DENIED_GRACE_PERIOD:
			strp = "NLM4_DENIED_GRACE_PERIOD";
			break;
		case NLM4_DEADLCK:
			strp = "NLM4_DEADLCK";
			break;
		case NLM4_ROFS:
			strp = "NLM4_ROFS";
			break;
		case NLM4_STALE_FH:
			strp = "NLM4_STALE_FH";
			break;
		case NLM4_FBIG:
			strp = "NLM4_FBIG";
			break;
		case NLM4_FAILED:
			strp = "NLM4_FAILED";
			break;
		default:
			sprintf(strp, "invalid stat (%d)", (int)stat);
	}
	return(strp);
}

char *
locktype_to_str(ushort lktype)
{
	char *strp = str;

	switch (lktype) {
		case F_RDLCK:
			strp = "F_RDLCK";
			break;
		case F_WRLCK:
			strp = "F_WRLCK";
			break;
		case F_UNLCK:
			strp = "F_UNLCK";
			break;
		default:
			sprintf(strp, "invalid type (%d)", (int)lktype);
	}
	return(strp);
}

char *
lockcmd_to_str(int cmd)
{
	char *strp = str;

	switch (cmd) {
		case F_GETLK:
			strp = "F_GETLK";
			break;
		case F_SETLK:
			strp = "F_SETLK";
			break;
		case F_SETLKW:
			strp = "F_SETLKW";
			break;
		case F_SETBSDLK:
			strp = "F_SETBSDLK";
			break;
		case F_SETBSDLKW:
			strp = "F_SETBSDLKW";
			break;
		default:
			sprintf(strp, "invalid cmd (%d)", cmd);
	}
	return(strp);
}

static char Hexdig[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

char *
netobj_to_str(netobj *obj)
{
	static char objstr[MAX_NETOBJ_SZ * 2 + 1];
	int i;
	char *cp;

	if (!obj->n_bytes || !obj->n_len) {
		return("nullobj");
	}
	for (cp = &objstr[0], i = 0; i < obj->n_len; i++) {
		*cp = Hexdig[(obj->n_bytes[i] >> 4) & 0xf];
		cp++;
		*cp = Hexdig[obj->n_bytes[i] & 0xf];
		cp++;
	}
	*cp = '\0';
	return(objstr);
}

char *
nlmproc_to_str(int vers, int proc)
{
	char *strp = "NULL";

	switch (vers) {
		case NLM_VERS:
		case NLM_VERSX:
			switch (proc) {
				case 0:
					strp = "NULLPROC";
					break;
				case NLM_TEST:
					strp = "NLM_TEST";
					break;
				case NLM_LOCK:
					strp = "NLM_LOCK";
					break;
				case NLM_CANCEL:
					strp = "NLM_CANCEL";
					break;
				case NLM_UNLOCK:
					strp = "NLM_UNLOCK";
					break;
				case NLM_GRANTED:
					strp = "NLM_GRANTED";
					break;
				case NLM_TEST_MSG:
					strp = "NLM_TEST_MSG";
					break;
				case NLM_LOCK_MSG:
					strp = "NLM_LOCK_MSG";
					break;
				case NLM_CANCEL_MSG:
					strp = "NLM_CANCEL_MSG";
					break;
				case NLM_UNLOCK_MSG:
					strp = "NLM_UNLOCK_MSG";
					break;
				case NLM_GRANTED_MSG:
					strp = "NLM_GRANTED_MSG";
					break;
				case NLM_TEST_RES:
					strp = "NLM_TEST_RES";
					break;
				case NLM_LOCK_RES:
					strp = "NLM_LOCK_RES";
					break;
				case NLM_CANCEL_RES:
					strp = "NLM_CANCEL_RES";
					break;
				case NLM_UNLOCK_RES:
					strp = "NLM_UNLOCK_RES";
					break;
				case NLM_GRANTED_RES:
					strp = "NLM_GRANTED_RES";
					break;
				case NLM_SHARE:
					strp = "NLM_SHARE";
					break;
				case NLM_UNSHARE:
					strp = "NLM_UNSHARE";
					break;
				case NLM_NM_LOCK:
					strp = "NLM_NM_LOCK";
					break;
				case NLM_FREE_ALL:
					strp = "NLM_FREE_ALL";
					break;
				default:
					strp = "BADPROC";
			}
			break;
		case NLM4_VERS:
			switch (proc) {
				case NLMPROC_NULL:
					strp = "NLMPROC_NULL";
					break;
				case NLMPROC_TEST:
					strp = "NLMPROC_TEST";
					break;
				case NLMPROC_LOCK:
					strp = "NLMPROC_LOCK";
					break;
				case NLMPROC_CANCEL:
					strp = "NLMPROC_CANCEL";
					break;
				case NLMPROC_UNLOCK:
					strp = "NLMPROC_UNLOCK";
					break;
				case NLMPROC_GRANTED:
					strp = "NLMPROC_GRANTED";
					break;
				case NLMPROC_TEST_MSG:
					strp = "NLMPROC_TEST_MSG";
					break;
				case NLMPROC_LOCK_MSG:
					strp = "NLMPROC_LOCK_MSG";
					break;
				case NLMPROC_CANCEL_MSG:
					strp = "NLMPROC_CANCEL_MSG";
					break;
				case NLMPROC_UNLOCK_MSG:
					strp = "NLMPROC_UNLOCK_MSG";
					break;
				case NLMPROC_GRANTED_MSG:
					strp = "NLMPROC_GRANTED_MSG";
					break;
				case NLMPROC_TEST_RES:
					strp = "NLMPROC_TEST_RES";
					break;
				case NLMPROC_LOCK_RES:
					strp = "NLMPROC_LOCK_RES";
					break;
				case NLMPROC_CANCEL_RES:
					strp = "NLMPROC_CANCEL_RES";
					break;
				case NLMPROC_UNLOCK_RES:
					strp = "NLMPROC_UNLOCK_RES";
					break;
				case NLMPROC_GRANTED_RES:
					strp = "NLMPROC_GRANTED_RES";
					break;
				case NLMPROC_SHARE:
					strp = "NLMPROC_SHARE";
					break;
				case NLMPROC_UNSHARE:
					strp = "NLMPROC_UNSHARE";
					break;
				case NLMPROC_NM_LOCK:
					strp = "NLMPROC_NM_LOCK";
					break;
				case NLMPROC_FREE_ALL:
					strp = "NLMPROC_FREE_ALL";
					break;
				default:
					strp = "BADPROC";
			}
			break;
		default:
			strp = "BADVERS";
	}
	return(strp);
}

#ifdef DEBUG
int nlm_mem_usage = 0;
int nlm_zone_usage = 0;
struct km_wrap *NLM_memlist = NULL;

lock_t nlm_kmem_lock;

void *
nlm_kmem_alloc(void *(*func)(size_t, int), int size, int flag)
{
	caddr_t mp = NULL;
	struct km_wrap *kwp;
	int n = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
	int allocsize = n + (2 * WRAPSIZE);
	int old_spl;

	ASSERT(size != 0);
	ASSERT(n >= size);
	ASSERT(n < (size + sizeof(void *)));
	mp = (func)(allocsize, flag);
	if (mp == NULL) {
		return (NULL);
	}
	old_spl = mutex_spinlock( &nlm_kmem_lock );
	/*LINTED alignment okay*/
	kwp = (struct km_wrap *)mp;
	kwp->kw_size = allocsize;
	kwp->kw_req = size;
	kwp->kw_caller = (void *)__return_address;
	kwp->kw_prev = NULL;
	kwp->kw_next = NLM_memlist;
	if (NLM_memlist) {
		NLM_memlist->kw_prev = kwp;
	}
	NLM_memlist = kwp;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *) ((__psunsigned_t)mp + WRAPSIZE + n);
	kwp = (struct km_wrap *)kwp->kw_other;
	ASSERT(!((__psunsigned_t)kwp & (sizeof(void *) - 1)));
	ASSERT((int)(((__psunsigned_t)kwp + (__psunsigned_t)WRAPSIZE) -
		(__psunsigned_t)mp) == allocsize);
	kwp->kw_size = allocsize;
	kwp->kw_req = size;
	/*LINTED alignment okay*/
	kwp->kw_other = (struct km_wrap *)mp;
	kwp->kw_caller = kwp->kw_prev = kwp->kw_next = NULL;

	ASSERT(nlm_mem_usage >= 0);
	nlm_mem_usage += allocsize;
	mutex_spinunlock( &nlm_kmem_lock, old_spl );

	ASSERT(VALID_ADDR((__psunsigned_t)mp + (__psunsigned_t)WRAPSIZE));
	ASSERT(!(((__psunsigned_t)mp + (__psunsigned_t)WRAPSIZE) &
		(__psunsigned_t)(sizeof(void *) - 1)));

	return (mp + (__psunsigned_t)WRAPSIZE);
}

int
nlm_kmem_validate(caddr_t mp, int size)
{
	struct km_wrap *front_kwp;
	struct km_wrap *back_kwp;
	int n = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
	int freesize = n + (2 * WRAPSIZE);

	ASSERT(n < (size + sizeof(void *)));
	ASSERT(VALID_ADDR(mp));
	ASSERT(VALID_ADDR((u_long)mp + (u_long)n));
	/*LINTED alignment okay*/
	front_kwp = (struct km_wrap *)(mp - (__psunsigned_t)WRAPSIZE);
	/*LINTED alignment okay*/
	back_kwp = (struct km_wrap *)((caddr_t)mp + n);
	ASSERT(VALID_ADDR(front_kwp));
	ASSERT(VALID_ADDR(back_kwp));

	return(!((__psunsigned_t)front_kwp & (sizeof(void *) - 1)) &&
		!((__psunsigned_t)back_kwp & (sizeof(void *) - 1)) &&
		(front_kwp->kw_req == size) &&
		(front_kwp->kw_other == back_kwp) &&
		(front_kwp->kw_size == freesize) &&
		(back_kwp->kw_req == size) &&
		(back_kwp->kw_other == front_kwp) &&
		(back_kwp->kw_size == freesize) &&
		(back_kwp->kw_caller == NULL) &&
		(back_kwp->kw_prev == NULL) &&
		(back_kwp->kw_next == NULL));
}

/* ARGSUSED */
int
nlm_zone_validate(caddr_t mp, int size)
{
#ifdef NLM_DEBUG_ZONES
	return (nlm_kmem_validate(mp, size));
#else
	return(1);
#endif
}

void
nlm_kmem_free(caddr_t mp, int size)
{
	struct km_wrap *front_kwp;
	struct km_wrap *back_kwp;
	int n = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);
	int freesize = n + (2 * WRAPSIZE);
	caddr_t p;
	int old_spl;

	ASSERT(nlm_kmem_validate(mp, size));

	/*LINTED alignment okay*/
	front_kwp = (struct km_wrap *)(mp - (__psunsigned_t)WRAPSIZE);
	/*LINTED alignment okay*/
	back_kwp = (struct km_wrap *)((caddr_t)mp + n);

	old_spl = mutex_spinlock( &nlm_kmem_lock );
	if (front_kwp->kw_next) {
		front_kwp->kw_next->kw_prev = front_kwp->kw_prev;
	}
	if (front_kwp->kw_prev) {
		front_kwp->kw_prev->kw_next = front_kwp->kw_next;
	} else {
		NLM_memlist = front_kwp->kw_next;
	}
	nlm_mem_usage -= freesize;
	ASSERT(nlm_mem_usage >= 0);
	ASSERT(!nlm_mem_usage || NLM_memlist);
	mutex_spinunlock( &nlm_kmem_lock, old_spl );

	p = (caddr_t)front_kwp;
	front_kwp->kw_size = back_kwp->kw_size = 0;
	front_kwp->kw_other = back_kwp->kw_other = NULL;
	kmem_free(p, freesize);
}

void *
nlm_zone_alloc(zone_t *zone, int flag)
{
#if !defined(NLM_DEBUG_ZONES)
	void *ptr;
#endif

	ASSERT(VALID_ADDR(zone));

	ASSERT(kmem_zone_unitsize(zone));
	atomicAddInt(&nlm_zone_usage, kmem_zone_unitsize(zone));
	ASSERT(nlm_zone_usage >= kmem_zone_unitsize(zone));
#ifdef NLM_DEBUG_ZONES

	return (nlm_kmem_alloc(kmem_alloc, kmem_zone_unitsize(zone), flag));
#else
	ptr = kmem_zone_zalloc(zone, flag);

	ASSERT((poff((__psint_t)ptr) % kmem_zone_unitsize(zone)) == 0);
	return(ptr);
#endif
}

void *
nlm_zone_zalloc(zone_t *zone, int flag)
{
#if !defined(NLM_DEBUG_ZONES)
	void *ptr;
#endif

	ASSERT(VALID_ADDR(zone));

	ASSERT(kmem_zone_unitsize(zone));
	atomicAddInt(&nlm_zone_usage, kmem_zone_unitsize(zone));
	ASSERT(nlm_zone_usage >= kmem_zone_unitsize(zone));
#ifdef NLM_DEBUG_ZONES

	return (nlm_kmem_alloc(kmem_alloc, kmem_zone_unitsize(zone), flag));
#else
	ptr = kmem_zone_zalloc(zone, flag);
/*

	ASSERT((poff((__psint_t)ptr) % kmem_zone_unitsize(zone))) == 0);
*/
	return(ptr);
#endif
}

void
nlm_zone_free(zone_t *zone, void *ptr)
{
	ASSERT(VALID_ADDR(zone));
	ASSERT(VALID_ADDR(ptr));

	ASSERT(kmem_zone_unitsize(zone));
	atomicAddInt(&nlm_zone_usage, -kmem_zone_unitsize(zone));
	ASSERT(nlm_zone_usage >= 0);
#ifdef NLM_DEBUG_ZONES

	nlm_kmem_free(ptr, kmem_zone_unitsize(zone));
#else
/*

	ASSERT((poff((__psint_t)ptr) % kmem_zone_unitsize(zone))) == 0);
*/
	kmem_zone_free(zone, ptr);
#endif
}

void
nlmdebug_init()
{
	spinlock_init( &nlm_kmem_lock, "NLM kmem lock" );
	idbg_addfunc( "nlmwait", idbg_nlmwait );
	idbg_addfunc( "nwhash", idbg_waithash );
	idbg_addfunc( "nlmdup", idbg_nlmdup );
}
#endif /* DEBUG */
