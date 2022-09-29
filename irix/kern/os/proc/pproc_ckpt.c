/*
 * pproc_ckpt.c
 *
 * 	Routines to support checkpoint/restart ops on proc
 *
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident  "$Revision: 1.3 $"

#ifdef CKPT

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/kmem.h>
#include <sys/ckpt.h>
#include <sys/avl.h>
#include "pproc_private.h"
#include "pproc.h"
#ifdef DEBUG
#include <ksys/vshm.h>
#endif

/*
** Shared Memory Support
*/
#define	CKPT_SHMCHUNK	16

typedef struct ckpt_shm {
	avlnode_t cs_avlnode;		/* lookup */
	int	  cs_base;		/* base shmid */
	uint	  cs_shmid[CKPT_SHMCHUNK]; /* list of shmid entries */
} ckpt_shm_t;

static __psunsigned_t
ckpt_avl_start(avlnode_t *np)
{
	return ((__psunsigned_t ) (((ckpt_shm_t *)np)->cs_base));
}
static __psunsigned_t
ckpt_avl_end(avlnode_t *np)
{
	return ((__psunsigned_t ) (((ckpt_shm_t *)np)->cs_base)+CKPT_SHMCHUNK);
}

static
avlops_t ckpt_avlops = {
	ckpt_avl_start,
	ckpt_avl_end
};
/*
 * Offset shmid value by 1 so that 0 is not valid
 */
/*
 * ckpt_shmget
 *
 * Called from shmget to log receipt of an shmid
 */
void
pproc_ckpt_shmget(bhv_desc_t *bhv, int shmid)
{
	proc_t *p;
	ckpt_shm_t	*entry;
	int		normal;			/* normalized shmid */
	uint		ckpt_shmid;
	extern		shmid_base;
	extern		shmmni;
	
	p = BHV_TO_PROC(bhv);
	/*
	 * Bring shmid down into range, offset shmid by 1 so 0 isn't valid
	 */
	normal = (shmid - shmid_base) % shmmni;
	ckpt_shmid = (uint)shmid + 1;

	mrupdate(&p->p_ckptlck);		/* for threaded procs */

	if (p->p_ckptshm == NULL) {
		p->p_ckptshm = (avltree_desc_t *)kmem_alloc(sizeof(avltree_desc_t), KM_SLEEP);
		avl_init_tree(p->p_ckptshm, &ckpt_avlops);
	}
	entry = (ckpt_shm_t *)avl_findrange(p->p_ckptshm, normal);
	if (entry == NULL) {
		/*
		 * Need a new one
		 */
		entry = (ckpt_shm_t *)kmem_zalloc(sizeof(ckpt_shm_t), KM_SLEEP);
		entry->cs_base = normal & ~(CKPT_SHMCHUNK-1);
		avl_insert(p->p_ckptshm, (avlnode_t *)entry);
	}
	/*
	 * It's possible to be overwriting a previous value. In that case, the
	 * shmid value stored there *must* have been removed
	 */
#ifdef DEBUG
	{
	int prev;
	vshm_t *vshm;
	prev = entry->cs_shmid[normal % CKPT_SHMCHUNK];
	if ((prev != 0)&&(prev != ckpt_shmid)) {
		ASSERT(vshm_lookup_id(prev - 1, &vshm) != 0);
	}
	}
#endif
	entry->cs_shmid[normal % CKPT_SHMCHUNK] = ckpt_shmid;
	
	mrunlock(&p->p_ckptlck);
}

static int
ckpt_get_shmcnt(proc_t *p)
{
	ckpt_shm_t *ckpt_shm;
	int i;
	int count = 0;

	if (p->p_ckptshm == NULL)
		return (0);

	mraccess(&p->p_ckptlck);

	ckpt_shm = (ckpt_shm_t *)p->p_ckptshm->avl_firstino;

	while (ckpt_shm) {
		for (i = 0; i < CKPT_SHMCHUNK; i++) {
			if (ckpt_shm->cs_shmid[i] != 0)
				count++;
		}
		ckpt_shm = (ckpt_shm_t *)ckpt_shm->cs_avlnode.avl_nextino;
	}
	mrunlock(&p->p_ckptlck);

	return (count);
}

static int
ckpt_get_shmlist(proc_t *p, ckpt_shmlist_t *arg)
{
	int count = arg->count;
	int *shmlist = arg->list;
	ckpt_shm_t *ckpt_shm;
	int i;

	if (p->p_ckptshm == NULL)
		return (0);

	mraccess(&p->p_ckptlck);

	ckpt_shm = (ckpt_shm_t *)p->p_ckptshm->avl_firstino;

	while (ckpt_shm && count) {

		for (i = 0; (i < CKPT_SHMCHUNK)&&(count); i++ ) {
			if (ckpt_shm->cs_shmid[i] != 0) {
				*shmlist++ = ckpt_shm->cs_shmid[i] - 1;
				count--;
			}
		}
		ckpt_shm = (ckpt_shm_t *)ckpt_shm->cs_avlnode.avl_nextino;
	}
	mrunlock(&p->p_ckptlck);

	return (0);
}
		
/*
 * Retrieve checkpoint info
 */
int
pproc_get_ckpt(bhv_desc_t *bhv, int code, void *arg)
{
	proc_t *p;

	p = BHV_TO_PROC(bhv);

	switch (code) {
	case CKPT_SHMCNT:
		*((int *)arg) = ckpt_get_shmcnt(p);
		break;

	case CKPT_SHMLIST:
		return (ckpt_get_shmlist(p, (ckpt_shmlist_t *)arg));

	default:
		return (-1);
	}
	return (0);
}
/*
 * Fork processing for ckpt 
 *
 * Right now, only thing to do is copy parents shmid list to child
 */
void
ckpt_fork(proc_t *pp, proc_t *cp)
{
	ckpt_shm_t	*pckpt;
	ckpt_shm_t	*cckpt;

	mraccess(&pp->p_ckptlck);	/* can't race in child proc */

	if (pp->p_ckptshm == NULL) {
		mrunlock(&pp->p_ckptlck);
		return;
	}
	cp->p_ckptshm = (avltree_desc_t *)kmem_alloc(sizeof(avltree_desc_t), KM_SLEEP);
	avl_init_tree(cp->p_ckptshm, &ckpt_avlops);

	pckpt = (ckpt_shm_t *)pp->p_ckptshm->avl_firstino;

	while (pckpt) {

		cckpt = (ckpt_shm_t *)kmem_zalloc(sizeof(ckpt_shm_t), KM_SLEEP);

		cckpt->cs_base = pckpt->cs_base;
		bcopy(pckpt->cs_shmid, cckpt->cs_shmid, CKPT_SHMCHUNK);

		avl_insert(cp->p_ckptshm, (avlnode_t *)cckpt);

		pckpt = (ckpt_shm_t *)pckpt->cs_avlnode.avl_nextino;
	}
	mrunlock(&pp->p_ckptlck);
}

/*
 * Exit processing for ckpt
 *
 * Free shmid list
 *
 * Always single-threaded when get here
 */
void
ckpt_exit(proc_t *p)
{
	ckpt_shm_t	*entry;

	if (p->p_ckptshm == NULL)
		return;

	while (entry = (ckpt_shm_t *)p->p_ckptshm->avl_firstino) {
		avl_delete(p->p_ckptshm, (avlnode_t *)entry);
		kmem_free(entry, sizeof(ckpt_shm_t));
	}
	kmem_free(p->p_ckptshm, sizeof(avltree_desc_t));
}

#endif /* CKPT */
