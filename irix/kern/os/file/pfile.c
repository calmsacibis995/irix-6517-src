/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Id: pfile.c,v 1.9 1997/09/10 16:11:45 henseler Exp $"
#include <sys/param.h>
#include <sys/systm.h>
#include <ksys/vfile.h>
#include <sys/vnode.h>
#include <ksys/vfile.h>
#include "pfile_private.h"
#include <ksys/cell.h>
#include <sys/atomic_ops.h>

/*
 * The routines in this file implement the physical file (pfile) object.
 *
 * The pfile object contains the following file information:
 *	pf_offset - current file offset
 * 	pf_offlock - (cell) mutex protecting pf_offset
 *	pf_bd - behavior stuff
 *
 * For single cell (not distributed) usage, the file offset is managed
 * only by the pfile layer.  For distributed access, the file offset is
 * managed by the DC and DS layer.
 *
 * The open file flags live in the vfile object, and are managed by the
 * pfile layer (single cell) or by the DC/DS layer (multicell).
 */

extern void pfile_setflags (bhv_desc_t * bdp, int am, int om);
extern void pfile_getoffset (bhv_desc_t * bdp, off_t *offp);
extern void pfile_setoffset (bhv_desc_t * bdp, off_t off);
extern void pfile_getoffset_locked (bhv_desc_t * bdp, int lock, off_t *offp);
extern void pfile_setoffset_locked (bhv_desc_t * bdp, int lock, off_t off);
extern void pfile_teardown (bhv_desc_t * bdp);
extern int pfile_close (bhv_desc_t * bdp, int);

vfileops_t  pfile_ops = {
	BHV_IDENTITY_INIT_POSITION(VFILE_POSITION_PFILE),
	pfile_setflags,
	pfile_getoffset,
	pfile_setoffset,
	pfile_getoffset_locked,
	pfile_setoffset_locked,
	pfile_teardown,
	pfile_close
};

struct zone	*pfile_zone;

#if DEBUG
int pfile_outstanding;
#endif

void
pfile_init()
{
	pfile_zone = kmem_zone_init(sizeof(struct pfile),
		"pfile");
}

void 
pfile_setflags (
	bhv_desc_t * bdp, 
	int am, 
	int om)
{
	vfile_t	*vfp = BHV_TO_VFILE(bdp);
	int s;

	s = VFLOCK(vfp);
	vfp->vf_flag &= am;
	vfp->vf_flag |= om;
	VFUNLOCK (vfp, s);
}

void 
pfile_getoffset (
	bhv_desc_t * bdp, 
	off_t *offp)
{
	pfile_t	*pfp = BHV_TO_PFILE(bdp);

	*offp = pfp->pf_offset;
}

void 
pfile_setoffset (
	bhv_desc_t * bdp, 
	off_t off)
{
	pfile_t	*pfp = BHV_TO_PFILE(bdp);

	if (off != VFILE_NO_OFFSET)
		pfp->pf_offset = off;
}

void 
pfile_getoffset_locked (
	bhv_desc_t * bdp, 
	int lock, 
	off_t *offp)
{
	pfile_t	*pfp = BHV_TO_PFILE(bdp);

	if (lock) {
		mutex_lock(&pfp->pf_offlock, PRIBIO);
	}
	*offp = pfp->pf_offset;
}

void 
pfile_setoffset_locked (
	bhv_desc_t * bdp, 
	int lock, 
	off_t off)
{
	pfile_t	*pfp = BHV_TO_PFILE(bdp);

	if (off != VFILE_NO_OFFSET)
		pfp->pf_offset = off;
	if (lock) {
		mutex_unlock(&pfp->pf_offlock);
	}
}

void 
pfile_teardown (
	bhv_desc_t * bdp)
{
	pfile_t	*pfp = BHV_TO_PFILE(bdp);

	ASSERT (!mutex_owned(&pfp->pf_offlock));
	mutex_destroy(&pfp->pf_offlock);
	if (bdp->bd_vobj) {
		vfile_t	*vfp = BHV_TO_VFILE(bdp);
		bhv_remove(&vfp->vf_bh, &pfp->pf_bd);
	}
	kmem_zone_free (pfile_zone, pfp);

#if DEBUG
	atomicAddInt(&pfile_outstanding, -1);
#endif

}

/*
 * Called from vfile_close when file is closed.
 *
 * Decrement the reference count if either it's currently 1 or this
 * is the second call from vfile_close.
 *
 * The flag arg is one of
 *	VFILE_FIRSTCLOSE - first of the two calls from vfile_close
 *	VFILE_SECONDCLOSE - second of the two calls from vfile_close
 */
int
pfile_close (
	bhv_desc_t * bdp,
	int	flag)
{
	vfile_t  *vfp  = BHV_TO_VFILE(bdp);
	int		rval;
	int		s;

	s = VFLOCK(vfp);
	ASSERT(vfp->vf_count > 0);
	if (vfp->vf_count == 1) {
		vfp->vf_count--;
		rval = VFILE_ISLASTCLOSE;
	} else if (flag == VFILE_SECONDCLOSE) {
		vfp->vf_count--;
		rval = 0;
	} else {
		rval = 0;
	}
	VFUNLOCK(vfp, s);

	return(rval);
}

void 
pfile_create (
	vfile_t *vfp)
{
	pfile_t *pfp;

	pfp = (pfile_t *)kmem_zone_alloc (pfile_zone, KM_SLEEP);
	ASSERT (pfp != NULL);

	mutex_init(&pfp->pf_offlock, MUTEX_DEFAULT, "foffset");
	pfp->pf_offset = 0;

	bhv_desc_init(&pfp->pf_bd, pfp, vfp, &pfile_ops);
	bhv_insert_initial(&vfp->vf_bh, &pfp->pf_bd);

#if DEBUG
	atomicAddInt(&pfile_outstanding, 1);
#endif

}

void 
pfile_target_migrate(
	vfile_t *vfp,
	off_t offset)
{
	pfile_t *pfp;

	pfp = (pfile_t *)kmem_zone_alloc (pfile_zone, KM_SLEEP);
	ASSERT (pfp != NULL);

	mutex_init(&pfp->pf_offlock, MUTEX_DEFAULT, "foffset");
	pfp->pf_offset = 0;

	bhv_desc_init(&pfp->pf_bd, pfp, vfp, &pfile_ops);
	bhv_insert(&vfp->vf_bh, &pfp->pf_bd);

	pfile_setoffset (&pfp->pf_bd, offset);

#if DEBUG
	atomicAddInt(&pfile_outstanding, 1);
#endif

}
