/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident	"$Revision: 1.12 $"

#include <sys/types.h>
#include <limits.h>
#include <sys/fs_subr.h>
#include <sys/idbgentry.h>
#include <sys/systm.h>
#include <sys/atomic_ops.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/vsocket.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <ksys/cell/handle.h>
#include <ksys/kqueue.h>
#include <ksys/cell.h>
#include <ksys/cell/relocation.h>
#include "./dsvsock.h"
#include "./vsock.h"

/*
 * Server side hash table for vsockets.  This allows us to
 * store and retrieve vsockets by identifier.
 */
extern char *makesname(char *, const char *, long);

/* tuneables */			/* XXX make these master.d */
#define	VSOCK_I	64
int	vsocki =  VSOCK_I;
int	vsock_max = (64 * VSOCK_I);


struct	vsockinfo	vsock_info;
/*
 * basic lookup table - vsockets are hashed by 'id'
 */
typedef struct vsockentry {
	kqueue_t	kq_queue;
	vsock_handle_t	kq_handle;
	vsock_t		*kq_vsock;
	int		refcnt;
} vsockent_t;
typedef struct vsocktable {
	kqueue_t	vsockt_queue;
	mrlock_t	vsockt_lock;
} vsocktab_t;

/*
 * These are all cell global.
 */

vsocktab_t		*vsocktab;	/* The lookup hash table */
service_t       	vsock_service_id;
struct zone 		*dsvsock_zone;
struct zone		*dcvsock_zone;
struct vsock_statistics vsock_statistics;
service_t		dssock_service_id;
static vsock_gen_t	vsock_nextid;	/* Next free id to allocate */
struct zone		*vsockent_zone;

#define VSOCKTAB_LOCK(vq, mode)  { \
				ASSERT((vq) != 0); \
				mrlock(&(vq)->vsockt_lock, (mode), PZERO); \
}
#define VSOCKTAB_UNLOCK(vq)	mrunlock(&(vq)->vsockt_lock)

extern obj_relocation_if_t vsock_obj_iface;

void
vsock_hinit ()
{
	int		i;
	vsocktab_t	*vs;

	vsock_info.vsock_max = vsock_max;
	vsock_info.vsock_i = vsocki;
	vsock_statistics.vsocktab_total = 0;
	vsock_statistics.vsocktab_lookups = 0;

	/*
	 * Allocate and initialize the vsocket hash table
	 */
	vsocktab = (vsocktab_t *)kern_malloc(sizeof(vsocktab_t) * vsocki);
	ASSERT(vsocktab != 0);
	for (i = 0; i < vsocki; i++) {
		vs = &vsocktab[i];
		kqueue_init(&vs->vsockt_queue);
		mrlock_init(&vs->vsockt_lock, MRLOCK_DEFAULT, "vsockt", cellid());
	}
	vsock_nextid = 1;
	
	vsockent_zone = kmem_zone_init (sizeof (vsockent_t), "vsockentry");

	SERVICE_MAKE(dssock_service_id, CELL_GOLDEN, SVC_VSOCK);
	SERVICE_MAKE(vsock_service_id, cellid(), SVC_VSOCK);
	obj_service_register(vsock_service_id, NULL, &vsock_obj_iface); 
	dcsock_initialize();
	dssock_initialize();
}

vsock_gen_t
vsock_newid ()
{

	return (atomicAddInt((int *)(&vsock_nextid), 1));
}

/*
 * Decrement the queue reference count and if zero remove from the
 * queue.
 */

/* ARGSUSED */
int
vsock_hremove (
	vsock_t		*vso,		/* might be bhv_desc_t */
	vsock_handle_t	*handle)
{
	vsockent_t	*vse;
	vsocktab_t	*vq;
	kqueue_t	*kq;
	vsock_gen_t	id = handle->vs_ident;
	int		refcnt;

	vq = &vsocktab[id%vsock_info.vsock_i];
	VSOCKTAB_LOCK(vq, MR_UPDATE);
	kq = &(vq->vsockt_queue);
	for (vse = (vsockent_t *)kqueue_first(kq);
	     vse != (vsockent_t *)kqueue_end(kq);
	     vse = (vsockent_t *)kqueue_next(&vse->kq_queue)) {
		if (VSOCK_HANDLE_EQU(*handle, vse->kq_handle)) {
		    refcnt = atomicAddInt (&vse->refcnt, -1);
		    ASSERT(refcnt >= 0);
		    if (refcnt == 0) {
			kqueue_remove(&(vse->kq_queue));
			kqueue_null(&(vse->kq_queue));
			kmem_zone_free(vsockent_zone, vse);
			vsock_statistics.vsocktab_total--;
		    }
		    VSOCKTAB_UNLOCK(vq);
		    return (refcnt ? 0 : 1);
		}
	}
	ASSERT(0);
	return(0);
}

int
vsock_drop_ref (
	vsock_t		*vso)
{
	int		refcnt;

	refcnt = atomicAddInt (&((vsockent_t *)vso->vs_vsent)->refcnt, -1);
	ASSERT(refcnt);
	return(0);
}

void
idbg_vsockhandle(char *cmd, vsock_handle_t *handle)
{
	qprintf ("(%s) 0x%x: ", cmd, handle);
	qprintf (" service 0x%x objid 0x%x ident %d\n",
		handle->vs_objhand.h_service, handle->vs_objhand.h_objid,
		handle->vs_ident);
}

void
idbg_vsockent(vsockent_t *vse)
{
	qprintf ("vse 0x%x: ", vse);
	qprintf (" queue 0x%lx kq_vsockid 0x%lx kq_vsock 0x%lx\n",
		vse->kq_queue, vse->kq_handle, vse->kq_vsock);
}

void
idbg_vsockent1(char *cmd, vsockent_t *vse)
{
	qprintf ("(%s) ", cmd);
	idbg_vsockent(vse);
}

/*
 * Look for a vsocktab_t structure with the given id in a hash table
 * queue.    The caller must have the hash table entry locked. The
 * vsocktab_t returned is referenced
 */
void
vsock_qlookup(
	vsocktab_t	*vq,
	vsock_handle_t	*handle,
	vsock_t		**vso)
{
	vsockent_t	*vse;
	kqueue_t *kq;

	/* Get the hash queue */
	kq = &vq->vsockt_queue;
	/*
	 * Step through the hash queue looking for the given id
	 */
	vsock_statistics.vsocktab_lookups++;
	for (vse = (vsockent_t *)kqueue_first(kq);
	     vse != (vsockent_t *)kqueue_end(kq);
	     vse = (vsockent_t *)kqueue_next(&vse->kq_queue)) {
		if (VSOCK_HANDLE_EQU(*handle, vse->kq_handle)) {
		    *vso = vse->kq_vsock;
		    return;
		}
	}
	/*
	 * Can't find it, return NULL
	 */
	*vso = NULL;
}

/*
 * Lookup a vsocket
 */
void
vsock_lookup_id (
	vsock_handle_t	*handle,
	vsock_t		**vso)
{
	vsocktab_t	*vq;
	vsock_t		*vso1 = NULL;
	vsock_gen_t	id = handle->vs_ident;

	/*
	 * Look for the id in the appropriate hash queue.
	 * Returns with the vsock referenced (or NULL)
	 */
	vq = &vsocktab[id%vsock_info.vsock_i];
	VSOCKTAB_LOCK(vq, MR_ACCESS);
	vsock_qlookup(vq, handle, &vso1);
	if (vso1) {
		atomicAddInt (&((vsockent_t *)vso1->vs_vsent)->refcnt, 1);
	}
	VSOCKTAB_UNLOCK(vq);
	*vso = vso1;
}

/*
 * Enter a socket in the local list
 */
void
vsock_enter_id (
	vsock_handle_t	*handle,
	vsock_t		*vso)
{
	vsocktab_t	*vq;
	vsockent_t	*vsi;
	vsock_gen_t	id = handle->vs_ident;

	vsi = (vsockent_t *) kmem_zone_zalloc(vsockent_zone, KM_SLEEP);
	bcopy(handle, &vsi->kq_handle, sizeof (vsock_handle_t));

	vq = &vsocktab[id%vsock_info.vsock_i];
	vsi->kq_vsock = vso;
	vso->vs_vsent = (void *)vsi;

	VSOCKTAB_LOCK(vq, MR_UPDATE);
#if DEBUG
	{
	vsock_t		*vso1;
	vsock_qlookup(vq, handle, &vso1);
	ASSERT(vso1 == NULL);
	}
#endif
	vsi->refcnt = 1;
	kqueue_enter(&vq->vsockt_queue, &vsi->kq_queue);
	VSOCKTAB_UNLOCK(vq);

	vsock_statistics.vsocktab_total++;
}

/* ARGSUSED */
void
idbg_vsockt(__psint_t x)
{
	kqueue_t	*kq;
	vsocktab_t	*vs;
	vsockent_t	*vse;
	int		i;

	qprintf ("idbg_vsockt(%d)\n", x);
	if (!vsocktab) {
		printf ("Table not initialised\n");
		return;
	}
	if (x == -1) x = 0;
	qprintf ("vsocktab cell %d 0x%lx\n", x, vsocktab);
	qprintf("Dumping vsock hash table for cell %d\n", cellid());
	for (i = 0; i < vsock_info.vsock_i; i++) {
		vs = &vsocktab[i];
		kq = &(vs->vsockt_queue);
		/*
		 * for every element on this hash queue
		 */
		if (kq->kq_next != kq) {
			qprintf ("table[%2d] kq 0x%lx ",
				i, kq);
			qprintf ("0x%lx 0x%lx", kq->kq_next, kq->kq_prev);
			qprintf ("\n");
		}
		for (vse = (vsockent_t *)kqueue_first(kq);
		    vse != (vsockent_t *)kqueue_end(kq);
		    vse = (vsockent_t *)kqueue_next(&vse->kq_queue)) {
			qprintf ("  vse 0x%lx handle 0x%x 0x%x %d kq_vsock 0x%lx\n",
				vse, vse->kq_handle.vs_objhand.h_service,
				vse->kq_handle.vs_objhand.h_objid,
				vse->kq_handle.vs_ident, vse->kq_vsock);
		}
	}
}

void
idbg_vsockt1(char *cmt, __psint_t x)
{
	printf ("%s  ", cmt);
	idbg_vsockt(x);
}
