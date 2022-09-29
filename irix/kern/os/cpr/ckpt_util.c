/*
 * ckpt_util.c
 *
 * 	Utility routunes for checkpoint/restart
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

#ident "$Revision: 1.43 $"

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/ckpt.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/fdt.h>
#include <sys/kmem.h>
#include <sys/sat.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/cred.h>
#include <sys/uuid.h>
#include <sys/uthread.h>
#include <ksys/vfile.h>
#include <sys/vfs.h>
#include <sys/handle.h>


typedef struct ckpt_lookup {
#ifdef DEBUG_CKPT_LOOKUP
#define	LOOKUP_MAGIC	0xdeadbeef
	unsigned long	ckpt_magic;
#endif
	struct fid	   ckpt_pdfid;		/* parent dir file id */
	struct ckpt_lookup *ckpt_next;		/* next chain */
} ckpt_lookup_t;

#define FID_SIZE(fidp)  (sizeof(struct fid) - MAXFIDSZ + (fidp)->fid_len)
#define FID_MATCH(fp1, fp2) \
        (((fp1)->fid_len == (fp2)->fid_len) && \
        (bcmp((caddr_t)(fp1)->fid_data, (caddr_t)(fp2)->fid_data, \
                (fp1)->fid_len) == 0))

static zone_t	*ckpt_lookup_zone;

#ifdef DEBUG_CKPT_LOOKUP
#define	VALID_ADDR(x)	(IS_KSEG0(x)||IS_KSEG2(x))

struct {
	int	total_alloc;
	int	total_refcnt;
	int	current_alloc;
	int	current_refcnt;
	int	max_index;
	int	num_retries;
} ckpt_stat;
#endif

void
ckpt_init(void)
{
	ckpt_lookup_zone = kmem_zone_init(sizeof(ckpt_lookup_t), "ckptlookups");
}

/*
 * open_by_fid
 */
ckpt_vp_open(vnode_t *vp, int filemode, int ckpt, int *rvp)
{
	int error;
	vfile_t *fp;
	int fd;

	if (error = vfile_alloc(filemode&FMASK, &fp, &fd)) {
		return error;
	}
	if ((filemode & FWRITE) == 0)
		_SAT_PN_BOOK(SAT_OPEN_RO, curuthread);
	else
		_SAT_PN_BOOK(SAT_OPEN, curuthread);

	/* hack for usema */
	curuthread->ut_openfp = fp;

	if (error = vp_open(vp, filemode, get_current_cred()))
		vfile_alloc_undo(fd, fp);
	else {
		fp->vf_ckpt = ckpt;
		vfile_ready(fp, vp);
		*rvp = fd;
	}
	_SAT_OPEN(*rvp, filemode&FCREAT, filemode, error);

	return error;
}
/*
 * 	Checkpoint memory mapping support
 */

/*
 * ckpt_fid
 * 
 * Used to create a fid for a vnode. Uses VOP_FID2 rather than VOP_FID 
 * to avoid repetitive calls to the kernel memory allocator. (The checkpoint 
 * code will allocate space for a data structure that includes a fid and the 
 * VOP_FID() interface allocates memory for the fid.)
 *
 * VOP_FID2 also handles xfs files with very large ino #s:
 */

static int
ckpt_fid(vnode_t *vp, struct fid *fidp)
{
	int error;
	fid_t *tmpfid;

	VOP_FID2(vp, fidp, error);

	if (error && (error == ENOSYS)) {
		/*
		 * Not all fs's implement VOP_FID2 (xfs, efs and nfs do)...fall back
		 * and try VOP_FID
		 */
		VOP_FID(vp, &tmpfid, error);

		if (!error) {
			bcopy(tmpfid, fidp, FID_SIZE(tmpfid));
			freefid(tmpfid);
		}
	}
	return error;
}

ckpt_handle_t
ckpt_lookup(vnode_t *dvp)
{
	ckpt_lookup_t	*lookup;

	lookup = (ckpt_lookup_t *)kmem_zone_alloc(ckpt_lookup_zone, KM_SLEEP);

	if (ckpt_fid(dvp, &lookup->ckpt_pdfid)) {
		kmem_zone_free(ckpt_lookup_zone, lookup);
		return (NULL);
	}
	lookup->ckpt_next = NULL;
#ifdef DEBUG_CKPT_LOOKUP
	lookup->ckpt_magic = LOOKUP_MAGIC;
	atomicAddInt(&ckpt_stat.total_alloc, 1);
	atomicAddInt(&ckpt_stat.current_alloc, 1);
	atomicAddInt(&ckpt_stat.total_refcnt, 1);
	atomicAddInt(&ckpt_stat.current_refcnt, 1);
#endif
	return ((ckpt_handle_t)lookup);
}

void
ckpt_lookup_free(ckpt_handle_t lookup)
{
#ifdef DEBUG_CKPT_LOOKUP
	ASSERT(((ckpt_lookup_t *)lookup)->ckpt_magic == LOOKUP_MAGIC);
#endif

	kmem_zone_free(ckpt_lookup_zone, lookup);
#ifdef DEBUG_CKPT_LOOKUP
	atomicAddInt(&ckpt_stat.current_refcnt, -1);
	atomicAddInt(&ckpt_stat.current_alloc, -1);
#endif
}
/*
 * Entries are added to the vnode list only if they are unique.  That is,
 * if the vnode is looked up in a directory it has not been looked up in before.
 *
 * Entries are removed only when vnode is deallocated or recycled.
 */
int
ckpt_lookup_add(vnode_t *vp, ckpt_handle_t handle)
{
	ckpt_lookup_t *new, *lookup, **insert;
	int i;

#ifdef DEBUG_CKPT_LOOKUP
	ASSERT(VALID_ADDR(vp));
#endif
	ASSERT(handle);
	/*
	 * Don't track directories and weird cases
	 */
	switch (vp->v_type) {
	case VNON:
	case VLNK:
	case VDIR:
	case VBAD:
	case VSOCK:
#ifdef DEBUG_CKPT_LOOKUP
		atomicAddInt(&ckpt_stat.current_alloc, -1);
		atomicAddInt(&ckpt_stat.current_refcnt, -1);
#endif
		kmem_zone_free(ckpt_lookup_zone, handle);
		return (-1);
	}
	/*
	 * since entries are never removed (while vnode is active)
	 * take a look for a dup without holding lock and remember
	 * last entry if no dup found
	 */
	new = (ckpt_lookup_t *)handle;

#ifdef DEBUG_CKPT_LOOKUP
	ASSERT(new->ckpt_magic == LOOKUP_MAGIC);
#endif
	ASSERT(new->ckpt_next == NULL);
	ASSERT(vp->v_count > 0);
retry:
	for (insert = (ckpt_lookup_t **)&vp->v_ckpt, lookup = vp->v_ckpt, i = 0;
	     lookup;
	     insert = &lookup->ckpt_next, lookup = lookup->ckpt_next, i++) {

		if (FID_MATCH(&lookup->ckpt_pdfid, &new->ckpt_pdfid)) {
#ifdef DEBUG_CKPT_LOOKUP
			atomicAddInt(&ckpt_stat.current_alloc, -1);
#endif
			kmem_zone_free(ckpt_lookup_zone, new);
			return (i);
		}
	}
	/*
	 * Note that no entries can be removed...only added!
	 *
	 * we could be racing with another inserter, so do atomic compare
	 * and swap on tail pointer
	 *
	 * If thngs are ever changed to remove entries on active vnodes,
	 * this won't work anymore
	 */
	if (compare_and_swap_ptr((void **)insert, NULL, (void *)new) == 0) {
		/*
		 * lost race, try again
		 */
#ifdef DEBUG_CKPT_LOOKUP
		atomicAddInt(&ckpt_stat.num_retries, -1);
#endif
		goto retry;
	}
#ifdef DEBUG_CKPT_LOOKUP
	if (i == 0) {
		ASSERT(insert == (ckpt_lookup_t **)&vp->v_ckpt);
	} else {
		ASSERT(insert != (ckpt_lookup_t **)&vp->v_ckpt);
	}
	if (i > ckpt_stat.max_index)
		ckpt_stat.max_index = i;
#endif
	return (i);
}
/*
 * Get the checkpoint info at the "index" entry on the chpt list.
 * Note, since no entries are removed for active vnodes, we don't need
 * to lock this.
 */
fid_t *
ckpt_lookup_get(vnode_t *vp, int index)
{
	ckpt_lookup_t *lookup;

	ASSERT(vp->v_count > 0);

	if (index < 0)
		return NULL;

	lookup = vp->v_ckpt;

	while (--index >= 0 && lookup)
		lookup = lookup->ckpt_next;

	if (!lookup) {
		ASSERT(index == -1);
		return NULL;
	}
	return (&lookup->ckpt_pdfid);
}
/*
 * Called when a vnode is being freerd or recycled.
 * This is the only time that entries are taken off the chekpoint list
 */
void
ckpt_vnode_free(vnode_t *vp)
{
	ckpt_lookup_t *lookup;

	ASSERT(vp->v_count == 0);

	while (lookup = (ckpt_lookup_t *)vp->v_ckpt) {
		vp->v_ckpt = ((ckpt_lookup_t *)vp->v_ckpt)->ckpt_next;
		kmem_zone_free(ckpt_lookup_zone, lookup);
#ifdef DEBUG_CKPT_LOOKUP
		atomicAddInt(&ckpt_stat.current_alloc, -1);
		atomicAddInt(&ckpt_stat.current_refcnt, -1);
#endif
	}
}

void
ckpt_setfd(int fd, int info)
{
	struct vfile *fp;

	if (info == -1)
		return;

	if (getf(fd, &fp))
		/* error */
		return;

	ASSERT(fp->vf_ckpt == -1);

	fp->vf_ckpt = info;
}
