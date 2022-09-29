/*
 * System call interface to the virtual socket abstraction.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 */

#ident "$Revision: 1.25 $"

#include <sys/types.h>
#include <limits.h>
#include <sys/fs_subr.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/vsocket.h>
#include <sys/socketvar.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/sesmgr.h>
#if CELL_IRIX
#include "cell/dsvsock.h"
#include "cell/vsock.h"
#endif

/*
 * This code implements the basic facilities to allocate and
 * free vsockets.
 */

#include <sys/ktrace.h>

#define bool_t  int
#define  FALSE   0
#define  TRUE    1

struct zone *vsocket_zone;

extern void vsock_hinit(void);

void
vso_initialize()

{
	vsock_hinit();
	vsocket_zone = kmem_zone_init (sizeof (struct vsocket), "vsocket");
	if (vsocket_zone == NULL)
		panic ("vso_initialize");

	/*  Trix uses it's own version of sosend()/soreceive() which
	 *  know about in-line security attributes.
	 */
	if (sesmgr_enabled) {
		lvector.vs_receive = sesmgr_soreceive;
	}
}

void
vsocket_free(struct vsocket *vs)
{
	VSO_UTRACE(UTN('vsoc','free'), vs, __return_address);
	bhv_head_destroy(&(vs->vs_bh));
	VSOCKET_LOCK_DESTROY(vs);
	sv_destroy(&vs->vs_sv);
	kmem_zone_free(vsocket_zone, vs);
}

void
vsocket_hold(struct vsocket *vs)
{
	VSO_UTRACE(UTN('vsoc','hold'), vs, __return_address);
	VSOCKET_LOCK(vs);
	ASSERT (vs->vs_refcnt > 0);
	vs->vs_refcnt++;
	VSOCKET_UNLOCK(vs);
}

void
vsocket_release(struct vsocket *vs)
{
	int		refs;
	bool_t	needwakeup = FALSE;

	ASSERT (vs->vs_refcnt > 0);
	VSOCKET_LOCK(vs);
	refs = --vs->vs_refcnt;

	VSO_UTRACE(UTN('vsoc','rel0'), vs, __return_address);
	switch (refs) {
	case 0:
		VSO_UTRACE(UTN('vsoc','rel1'), vs, __return_address);
		break;
	case 1:
		VSO_UTRACE(UTN('vsoc','rel2'), vs, __return_address);
		/* someone waiting for last reference */
		if (vs->vs_flags & VS_LASTREFWAIT)
			needwakeup = TRUE;
		break;
	default:
		VSO_UTRACE(UTN('vsoc','rel3'), vs, __return_address);
		if (vs->vs_flags & VS_RELEWAIT)
			needwakeup = TRUE;
		break;
	}
	if (needwakeup)
		sv_broadcast(&vs->vs_sv);
	VSOCKET_UNLOCK(vs);
	if (!refs)
		vsocket_free(vs);
}

/*
 * Allocate and initialize (most of) a vsocket.
 */
struct vsocket *
vsocket_alloc()
{
	struct vsocket  *vs;

	vs = (struct vsocket *)kmem_zone_alloc(vsocket_zone, KM_SLEEP);
	ASSERT(vs);
	VSO_UTRACE(UTN('vsoc','allo'), vs, __return_address);

	/*
	 * Initialize the vsocket contents.  type, protocol, domain
	 * initialized by caller.
	 */
	bhv_head_init(&(vs->vs_bh), "vsocket");
	vs->vs_flags = 0;
	VSOCKET_LOCK_INIT(vs);
	sv_init(&vs->vs_sv, SV_DEFAULT, "svwait");
	vs->vs_refcnt = 1;		
	vs->vs_vsent = NULL;
	
	return (vs);
}

/*
 * Allocate storage for a vsocket, and optionally (if 'so' is non-NULL) 
 * attach a socket.
 *
 * This is an internal routine that should be called via the 
 * vsocreate() and vsowrap() macros.
 */
int
vsocreate_internal(
	int dom,
	struct vsocket **avso,
	int type,
	int proto,
	struct socket *so)
{
	struct vsocket *vs;
	vsocket_ops_t *ops = &lvector;
	int error;

	vs = vsocket_alloc();
	if (so) {
		bhv_desc_init(&(so->so_bhv), so, vs, ops);
		bhv_insert_initial(&(vs->vs_bh), &(so->so_bhv));
	} else {

#if CELL_IRIX
		/*
		 * Force request to server cell
		 */
		if (!SERVICE_EQUAL(vsock_service_id, dssock_service_id)) {
			error = dcsock_create (vs, dom, (void *)NULL, type, 
				proto);
		} else {
			error = (ops->vs_create)(vs, dom, (void *)NULL, 
				type, proto);
		}
#else
		error = (ops->vs_create)(vs, dom, (void *)NULL, type, proto);
#endif

                if (error) {
			vsocket_release(vs);
			return error;
		}
	}
	vs->vs_type = type;
	vs->vs_protocol = proto;
	vs->vs_domain = dom;

	*avso = vs;
	return (0);
}

