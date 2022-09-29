/*
 * os/mtext.c
 *
 * 	Virtual file system for shared text pages modified for 
 *	processor workarounds
 *
 * Copyright 1996, Silicon Graphics, Inc.
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

#ident	"$Revision: 1.15 $"

#ifdef _MTEXT_VFS

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/idbgentry.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/pfdat.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pvnode.h>
#include <sys/immu.h>
#include <sys/proc.h>
#include <sys/pda.h>
#include <sys/buf.h>
#include <sys/schedctl.h>
#include <sys/runq.h>
#include <sys/tuneable.h>
#include <sys/anon.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <sys/cred.h>
#include <sys/fs_subr.h>

#include "as/region.h"
#include "as/as_private.h"
#include "vm/anon_mgr.h"

#if defined(DEBUG) && (! defined(DEBUG_MTEXT))
#define DEBUG_MTEXT 1
#endif

#include <sys/mtext.h>

#ifdef DEBUG_MTEXT
#define MTEXT_CHECK(mp) \
              ASSERT_ALWAYS(mp->magic == MTEXT_MAGIC) 
#define MTEXT_CHECK_BHV(bdp) \
              ASSERT_ALWAYS(BHV_TO_MTEXT(bdp)->magic == MTEXT_MAGIC) 
#define MTEXT_CHECK_SET(mp) \
              { mp->magic = MTEXT_MAGIC; }
#define MTEXT_CHECK_CLEAR(mp) \
              { mp->magic = 0; }
#else /* DEBUG_MTEXT */
#define MTEXT_CHECK(mp) 
#define MTEXT_CHECK_BHV(mp)
#define MTEXT_CHECK_SET(mp) 
#define MTEXT_CHECK_CLEAR(mp) 
#endif

#define REALVP_DCL(bhp,vp,rvp) \
	vnode_t *vp = BHV_TO_VNODE(bhp); \
	vnode_t *rvp = BHV_TO_MTEXT(bhp)->real_vnode

#define REALVP_DCL1(vp,rvp) \
	vnode_t *rvp = VNODE_TO_MTEXT(vp)->real_vnode

#define REALVP_DCL2(bhp,rvp) \
	vnode_t *rvp = BHV_TO_MTEXT(bhp)->real_vnode

#define REALVP_CHECK(rvp) \
      { if (rvp == NULL) return(ENXIO); }

/*
 *  mtext is used to provide dummy vnodes for modified text pages which
 *  are used in implementing processor workarounds.  Such text pages can
 *  still be shared, which avoids memory bloat when such pages are in
 *  commonly-used processes (compared to simply modifying a private copy
 *  for each process).
 */

#define MTEXT_HASH_SIZE 37

STATIC mtextnode_t *mtexthash[MTEXT_HASH_SIZE];

STATIC int mtext_hash(fid_t *);
#define MTEXT_HASH(fp) (mtext_hash(fp))
STATIC int mtext_fidcmp(fid_t *, fid_t *);

STATIC mutex_t	mtextlock;

#define BITS_TO_BITMAP_LENGTH(x) (((((int) (x)) + ((sizeof(int) * NBBY) - 1)) / \
				   (sizeof(int) * NBBY)) * sizeof(int))

/* vnode operations jump table, filled in by mtext_init_vnodeops() */
extern vnodeops_t mtextnode_vnodeops;

void
mtext_init(void)
{
	int i;

	/* init hash table */
	for (i = 0; i < MTEXT_HASH_SIZE; i++)
		mtexthash[i] = NULL;
	
	/* initialize the mutex protecting the mtext hashtable */
	mutex_init(&mtextlock,MUTEX_DEFAULT,"mtext");
}


STATIC void
_mtext_remove_hash(int i, mtextnode_t *mp)
{
	mtextnode_t *np;

	np = mtexthash[i];
	if (np == mp)
		mtexthash[i] = mp->next;
	else {
		while (np != NULL) {
			if (np->next == mp) {
				np->next = mp->next;
				break;
			}
			np = np->next;
		}
	}
}

STATIC void
mtext_remove_hash(mtextnode_t *mp)
{
	mutex_lock(&mtextlock,PZERO);
	_mtext_remove_hash(mp->hash_index,mp);
	mutex_unlock(&mtextlock);
}


/*
 * Probes for a proxy vnode associated with the given vnode (vp).
 * If a proxy vnode does not exist for vp, a new proxy vnode will be
 * allocated.  In either case, the found or created proxy vnode
 * is returned in *nvpp.
 *
 * If the  MTEXTF_PROBE flag is specified, no proxy vnode will be
 * created if it does not already exist.
 *
 * returns 0 on success
 */
int
mtext_probe(struct vnode *vp,	/* real vnode for which to probe */
	    struct vnode **nvpp, /* receives mtext vnode 	 */
	    off_t textp,	/* offset to text section	 */
	    size_t tsize,	/* size of text section		 */
	    int flags)		/* flags (MTEXTF_*)		 */
{
	int	i;		/* hash bucket for vnode	*/
	mtextnode_t *np;
	mtextnode_t *tp;
	vnode_t	*nvp;
	vmap_t vmap;
	struct vattr va;
	fid_t	vfid;

	*nvpp = NULL;

	/*
	 * Go through the hash table of mtextnodes to see if this vnode
	 * is already mapped.  If it is, we will use the existing vnode.
	 * Otherwise, we will allocate a new proxy vnode with vn_alloc, and
	 * set up an mtextnode structure associated with it.
	 */
again:
	/* get vnode size and modification time for later verification */
	va.va_mask = AT_SIZE | AT_MTIME;
	VOP_GETATTR(vp, &va, ATTR_LAZY, get_current_cred(), i);
	if (i)
		return(i);
	VOP_FID2(vp, &vfid, i);
	if (i && (i == ENOSYS)) {
		fid_t *tmpfid;
		/*
		 * Not all fs's implement VOP_FID2 (xfs, efs and nfs do)...fall back
		 * and try VOP_FID
		 */
		VOP_FID(vp, &tmpfid, i);

		if (!i) {
			if (tmpfid->fid_len > MAXFIDSZ)
				tmpfid->fid_len = MAXFIDSZ;
			bcopy(tmpfid, &vfid, (sizeof(struct fid) - MAXFIDSZ + tmpfid->fid_len));
			freefid(tmpfid);
		}
	}
	if (i)
		return(i);

	i = MTEXT_HASH(&vfid);
	mutex_lock(&mtextlock,PZERO);
	for (np = mtexthash[i];
	     np != NULL;
	     np = np->next) {
		MTEXT_CHECK(np);
		if (np->text_size != tsize ||
		    np->text_base != textp) 
			continue;
		if (np->real_vnode == vp ||
			! mtext_fidcmp(&vfid,&np->real_fid)) {
			nvp = MTEXT_TO_VNODE(np);
			VMAP(nvp,vmap);

			/* verify that real vnode has not been modified */
			if ((va.va_mtime.tv_sec != np->real_mtime.tv_sec) ||
			    (va.va_mtime.tv_nsec != np->real_mtime.tv_nsec) ||
			    (va.va_size != np->real_size)) {
				/* mtext vnode is obsolete: unlink */
				_mtext_remove_hash(i,np);
				mutex_unlock(&mtextlock);
				goto again;
			}

			mutex_unlock(&mtextlock);

			if (! (nvp = vn_get(nvp,&vmap,0)))
				goto again;

			mutex_lock(&mtextlock,PZERO);
			if (np->real_vnode == NULL) {
				VN_HOLD(vp);
				np->real_vnode = vp;
			}
			mutex_unlock(&mtextlock);
			*nvpp = nvp;	/* return the found vnode */
			return(0);	/* success */
		}
	}
	mutex_unlock(&mtextlock);

	/*
	 * If probe only, just return failure at this point.
	 */
	if (flags & MTEXTF_PROBE)
		return(ENOENT);
	
	/*
	 * At this point, we have determined that we have no appropriate mtext
	 * vnode to return to the caller.  We will allocate a new mtext vnode,
	 * associate a new mtextnode structure with it.
	 */
	np = (mtextnode_t *) kmem_zalloc(sizeof(mtextnode_t),KM_SLEEP);
	np->clean_map_size = btoc(tsize);
	if (! (flags & MTEXTF_ALL_CLEAN))
		np->clean_map = (char *)
	    		kmem_zalloc(BITS_TO_BITMAP_LENGTH(np->clean_map_size),
				    KM_SLEEP);
	np->text_size = tsize;
	np->text_base = textp;
	np->real_vnode = vp;
	np->real_size = va.va_size;
	np->real_mtime = va.va_mtime;
	np->hash_index = i;
	np->real_fid = vfid;
	MTEXT_CHECK_SET(np);
	VN_HOLD(vp);

	/*
	 * Allocate the mtext vnode
	 */
	nvp = vn_alloc(vp->v_vfsp,vp->v_type,vp->v_rdev);
	bhv_desc_init(&(np->mt_bhv_desc), np, nvp, &mtextnode_vnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(nvp), &(np->mt_bhv_desc));

	/*
	 * Add the new mtext vnode to our hash table.  Before we do the
	 * actual insertion, we make sure that nobody has beat us to
	 * it while we dropped the mutex above.  If they have, we just
	 * delete the new proxy we just allocated, and return the one
	 * found in the hash table.
	 */
	mutex_lock(&mtextlock,PZERO);
	for (tp = mtexthash[i];
	     tp != NULL;
	     tp = tp->next) {
		MTEXT_CHECK(tp);
		if (tp->real_vnode == vp) {
			/* lost the race--free extra mtextvnode */
			nvp = MTEXT_TO_VNODE(tp);
			VMAP(nvp,vmap);
			mutex_unlock(&mtextlock);
			nvp = vn_get(nvp,&vmap,0);
			
			vn_free(MTEXT_TO_VNODE(np));
			VN_RELE(vp);
			if (np->clean_map != NULL)
				kmem_free((void *) np->clean_map,
					  BITS_TO_BITMAP_LENGTH(np->clean_map_size));
			MTEXT_CHECK_CLEAR(np);
			kmem_free((void *) np,sizeof(mtextnode_t));
			if (! nvp) 
				goto again;
			*nvpp = nvp;
			return(0);
		}
	}

	/*
	 * Add this new proxy vnode to the hash bucket.
	 */
	np->next = mtexthash[i];
	mtexthash[i] = np;
	mutex_unlock(&mtextlock);
	*nvpp = nvp;
	return(0);
}



/*
 * Returns 1 if the page containing the file offset given is
 * known to be clean.  Returns 0 otherwise.
 */
int
mtext_offset_is_clean(
	struct vnode *vp,	/* proxy vnode				*/
	off_t filep		/* file-relative offset of page to check*/
	)
{
	mtextnode_t *mp;

	if (! IS_MTEXT_VNODE(vp))
		return(1);
	mp = VNODE_TO_MTEXT(vp);
	if (mp->clean_map == NULL ||
	    filep < mp->text_base ||
	    filep >= (mp->text_base + ctob(mp->clean_map_size)))
		return(1);

	return(btst(mp->clean_map,btoct((filep - mp->text_base))));
}


STATIC void
mtext_offset_set_clean(
	vnode_t *vp,
	off_t filep		/* offset of page to mark clean		*/
	)
{
	mtextnode_t *mp;

	if (! IS_MTEXT_VNODE(vp))
		return;
	mp = VNODE_TO_MTEXT(vp);
	if (mp->clean_map == NULL ||
	    filep < mp->text_base ||
	    filep >= (mp->text_base + ctob(mp->clean_map_size)))
		return;

	bset(mp->clean_map,btoct(filep-mp->text_base));
}


STATIC void
mtext_find_next_clean_range(mtextnode_t *mp,
			    off_t c_off,
			    size_t c_len,
			    off_t *n_offp,
			    size_t *n_lenp)
{
	size_t	i;
	size_t	icnt;
	size_t	t;
	off_t	text_end = (mp->text_base + mp->text_size);
	
	if (mp->clean_map == NULL ||
	    c_len == 0 ||
	    (c_off >= text_end) ||
	    ((c_off  + c_len) <= mp->text_base))
		goto done;

	if (c_off < mp->text_base) {
		i = 0;
		icnt = btoct(mp->text_base - c_off);
	} else {
		i = btoct(c_off - mp->text_base);
		if ((c_off + c_len) < text_end)
			t = btoc(c_len - (c_off - mp->text_base));
		else
			t = (mp->clean_map_size - i);
		t = bftstclr(mp->clean_map,i,t);
		i += t;
		c_off += ctob(t);
		c_len -= ctob(t);
		icnt = 0;
	}
	if (i < mp->clean_map_size) {
		t = bftstset(mp->clean_map,i,(mp->clean_map_size - i));
		i += t;
		icnt += t;
	}
	if (! ((i >= mp->clean_map_size) &&
	       ((c_off + c_len) > text_end)))
		c_len = ctob(icnt);

done:	    
	*n_offp = c_off;
	*n_lenp = c_len;
}



struct vnode *
mtext_real_vnode(struct vnode *vp)
{
	if (! IS_MTEXT_VNODE(vp))
		return(vp);
	return(VNODE_TO_MTEXT(vp)->real_vnode);
}


int
mtext_check_range(caddr_t buf, 	/* kernel address of instructions  */
	          size_t len,	/* length of instructions in bytes */
		  size_t *offp)	/* starting offset on input and    */
				/* next match 			   */
	/* returns 0 if no match, 1 on a match */
{
	int	i;
	int	ilimit;
	inst_t	ti;
#define ibuf ((inst_t *) buf)

	for (i = ((*offp) / sizeof(inst_t)), ilimit = len / sizeof(inst_t);
	     i < ilimit;
	     i++) {
		ti = ibuf[i] & CVT_CHECK_MASK;
		if (ti == CVT_D_L_ID ||
		    ti == CVT_S_L_ID) {
			*offp = (i * sizeof(inst_t));
			return(1);
		}
	}
	*offp = (i * sizeof(inst_t));
	return(0);
#undef ibuf
}


void
mtext_fixup_inst(caddr_t buf,  	/* kernel address of instructions  */
		  size_t *offp)	/* starting offset on input and    */
				/* incremented by one instruction  */
				/* on return 			   */
{
	*((inst_t *) (buf + *offp)) += INTCVT_INC;
	(*offp) += sizeof(inst_t);
}


/*
 *	mtext_execmap_vnode
 *
 *	execmap the given vnode, directly for known clean pages
 *	and via an mtextnode for other pages
 */

int
mtext_execmap_vnode(
		vnode_t *vp,	/* real vnode containing section*/
		caddr_t vaddr, 	/* virtual address to map to	*/
	       	size_t len,	/* length of section to map	*/
		size_t zfodlen,
		off_t  offset,	/* file offset of section to map*/
		int prot,	/* PROT_xxx flags		*/
		int flags,	/* MAP_xxx flags		*/
		vasid_t vasid,	/* virtual address space ID	*/
		int ckpt)
{
	vnode_t	*mvp;		/* mtext proxy vnode for the map	*/
	mtextnode_t *mp;	/* mtextnode associated with mvp	*/
	off_t	c_off;		/* current offset in mapping loop	*/
	size_t	c_len;		/* current length in mapping loop	*/
	off_t	n_off;		/* next offset in mapping loop		*/
	size_t	n_len;		/* next length in mapping loop		*/
	caddr_t	c_vaddr;	/* current virt address in mapping loop	*/
	caddr_t	base_vaddr;	/* base virtual address of first page	*/
	size_t	c_zfodlen;
	off_t	e_off;		/* ending offset of map	*/
	int	error;

	error = mtext_probe(vp, &mvp, offset, len, 0);
	if (error)
		return(error);
	ASSERT(mvp != NULL);
	mp = VNODE_TO_MTEXT(mvp);

	if (! ((poff(vaddr) == poff(offset)) && len)) {
		/*
		 * len is 0, or poff(vaddr) != poff(offset)
		 * Map this section normally using the mtext vnode
		 */
		error = execmap(mvp,vaddr,len,zfodlen,offset,prot,flags,vasid,ckpt);
					/* execmap will call loadreg(), which */
					/* will fixup the instructions        */
		VN_RELE(mvp);
		return(error);
	}
	
	/*
	 * Get the base address of the first page we are mapping to
	 */
	base_vaddr = vaddr - poff(vaddr);
	if (poff(vaddr) > 0) {
		/*
		 * destination vaddr was not page aligned; adjust the offset
		 * and length so we can do page-by-page mapping.
		 */
		offset -= poff(vaddr);		/* adjust offset to beginning of page */
		len += (size_t) poff(vaddr);	/* adjust len to make up for poff(vaddr)*/
	}

	/*
	 * Here is the main mapping loop.  We go through each range in the section
	 * to be mapped, looking for clean ranges that can be mapped directly to
	 * the real vnode.  For non-clean ranges, we map to the mtext proxy vnode.
	 */
	e_off = offset + len;			/* ending offset */
	for (c_off = offset;
	     c_off < e_off;) {
		/*
		 * Get the next known clean range
		 */
		mtext_find_next_clean_range(mp,		/* the mtextnode_t	*/
					    c_off,	/* current offset	*/
					    e_off - c_off, /* remaining length	*/
					    &n_off,	/* OUT: next clean off	*/
					    &n_len);	/* OUT: next clean len	*/
		/*
		 * If n_len is zero, there are no more known clean ranges.
		 * Set next offset to the ending offset so the loop will end.
		 */
		if (n_len == 0)
			n_off = e_off;
		
		/*
		 * If the offset of the next known clean range is greater than that of
		 * the current offset, the range between the current range and the
		 * clean range might be dirty, so has to be mapped through the
		 * mtext proxy vnode.
		 */
		if (n_off > c_off) {
			c_vaddr = base_vaddr + (c_off - offset);
							/* vaddr of this range	*/
			c_len = (n_off - c_off);	/* length of this range	*/
			c_zfodlen = (((c_off + c_len) == e_off)
				     ? zfodlen : 0);

			/*
			 * Map this range through the mtext proxy vnode, since it may
			 * have bad instructions
			 */
			error = execmap(mvp,c_vaddr,c_len,c_zfodlen,
					c_off, prot, flags, vasid, ckpt);
			if (error)
				break;
			c_off += c_len;	/* advance to next range	*/
		}

		/*
		 * At this point, we have mapped any dirty range through the mtext
		 * proxy vnode.  If mtext_find_next_clean_range() found a clean
		 * range following it, we map that through the real vnode here.
		 */
		if (n_len > 0) {
			/* clean range was found */
			c_vaddr = base_vaddr + (n_off - offset);
							/* vaddr of this range	*/
			c_zfodlen = (((n_off + n_len) == e_off)
				     ? zfodlen : 0);

			/*
			 * Map this range through the real vnode, since we know
			 * that it is clean.
			 */
			error = execmap(vp,c_vaddr,n_len,c_zfodlen,
					n_off, prot, flags, vasid, ckpt);
			if (error)
				break;
			c_off = n_off + n_len;	/* advance to next range	*/
		} /* if (n_len > 0) */
	} /* for( <all_ranges> ) */


	VN_RELE(mvp);
	return(error);
}

/*
 * 	mtext_forced_read_page
 *
 *	Read the underlying page, and copy it to a virtual page, even
 * 	if it is clean.
 */

STATIC int
mtext_forced_read_page(vnode_t *vp,	/* mtext vnode 			*/
		  off_t fileoffset)	/* file offset			*/
{
	int	error;
	iovec_t	aiov;
	uio_t	auio;
 	pfd_t	*pfd;
	caddr_t	ibuf;
	size_t	i_off;
	size_t	i_off_last;
	pfd_t	*pfd2;
	caddr_t	ibuf2;
	int	did_fixup;
	flid_t	flid;
	REALVP_DCL1(vp,rvp);

	REALVP_CHECK(rvp);

	get_current_flid(&flid);

	bzero((void *) &auio,sizeof(auio));
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = fileoffset - poff(fileoffset);
	auio.uio_segflg = UIO_NOSPACE;
	aiov.iov_base = (caddr_t) 0xdeadbeefL;
	auio.uio_resid = aiov.iov_len = NBPP;
	VOP_READ(vp, &auio, 0, get_current_cred(), &flid, error);

	if (error != 0)
		return(error);

	pfd = pfind((void *) vp,btoct(fileoffset),0);
	if (pfd != NULL)
		return(0);

	pfd = pfind((void *) rvp,btoct(fileoffset),VM_ATTACH);
	if (pfd == NULL)
		return(0); /* someone already stole the page; try again */

	ibuf = page_mapin(pfd,0,colorof(fileoffset));

	/* copy, patch, and hash the page */
	while ((pfd2 = (
#ifdef _VCE_AVOIDANCE
			vce_avoidance
			? pagealloc(colorof(fileoffset),VM_VACOLOR|VM_MVOK)
			:
#endif /* _VCE_AVOIDANCE */			
			pagealloc(0,VM_MVOK))) == NULL) {
		setsxbrk();
	}
	COLOR_VALIDATION(pfd2,colorof(fileoffset),0,0);
	ibuf2 = page_mapin(pfd2,VM_VACOLOR,colorof(fileoffset));
	i_off = 0;
	i_off_last = 0;
	did_fixup = 0;
	while (mtext_check_range(ibuf,NBPP,&i_off)) {
		bcopy(ibuf + i_off_last,ibuf2 + i_off_last,
		      (i_off - i_off_last) + sizeof(inst_t));
		mtext_fixup_inst(ibuf2,
				 &i_off);
		did_fixup = 1;
		i_off_last = i_off;
	}
	if (i_off > i_off_last)
		bcopy(ibuf + i_off_last,ibuf2 + i_off_last,
		      (i_off - i_off_last));
	if (! did_fixup)
		mtext_offset_set_clean(vp,fileoffset);
	page_mapout(ibuf);
	pagefree(pfd);
	page_mapout(ibuf2);
	pagedone(pfd2);
	pfd2 = vnode_pinsert_try(vp,pfd2,btoct(fileoffset));
	pagefree(pfd2);
	return(0);
}

/*
 *	mtext_map_real_vnode
 *
 *	Map real vnode in place of mapping of modified virtual vnode
 *	in current process, for locations known to be clean in the
 *	region which includes vaddr, for virtual vnode vp.  If vaddr
 *	is not mapped by vp, simply return.
 */
int
mtext_map_real_vnode(
	vnode_t *mvp,	/* mtext vnode 	     		*/
	pas_t *pas,	/* phys address space		*/
	ppas_t *ppas,	/* private phys address space	*/
	caddr_t vaddr)	/* virtual address to remap 	*/
{
	reg_t	*rp;		/* reg_t backing the pregion	*/
	preg_t	*prp;		/* pregion_t for this vaddr	*/
	uchar_t	prp_maxprot;	/* pregion's maxprot field	*/
	mprot_t	prp_prot;	/* pregion's attrs.attr_prot	*/

	int	retval = 0;	/* return code			*/
	off_t	c_off;		/* offset of page in file	*/
	vnode_t *real_vp;	/* the real vnode		*/
	mtextnode_t *mp;	/* the mtextnode_t associated	*/
	int	map_flags = 0;
	int	force_read = 0;
#ifdef CKPT
	int	rp_ckpt;
#endif
	
	as_deletespace_t asd;
	as_addspace_t 		asa;	/* args for VAS_ADDSPACE()	*/
	as_addspaceres_t 	asar;	/* return from VAS_ADDSPACE()	*/
	vasid_t	vasid;

	ASSERT( IS_MTEXT_VNODE(mvp) );
	mp 	= VNODE_TO_MTEXT(mvp);
	real_vp = mp->real_vnode;	/* get the real vnode		*/
	vaddr 	-= poff(vaddr);		/* go to beginning of page 	*/
	/*
	 * Lock down the address space of this process
	 */
	if (! mrtryupdate(&pas->pas_aspacelock) )	{
		mraccess(&pas->pas_aspacelock);
		force_read = 1;
	}

	/*
	 * Get the pregion and region in which this address sits, and copy out
	 * fields, since we will be removing this region shortly.
	 */
	if ((prp = findpreg(pas, ppas, vaddr)) == NULL)
		goto out;
	rp = prp->p_reg;
	if (rp == NULL ||		/* no reg_t		*/
	    rp->r_flags & RG_PHYS ||	/* physical region	*/
	    rp->r_vnode != mvp)		/* wrong vnode		*/
		goto out;	/* returns success	*/

	prp_prot = prp->p_attrs.attr_prot;
	prp_maxprot = prp->p_maxprots;
#ifdef CKPT
	rp_ckpt = rp->r_ckptinfo;
#endif
	
	/*
	 * convert the region flags to mapping flags
	 */
	if (rp->r_flags & RG_AUTOGROW)
		map_flags |= MAP_AUTOGROW;
	if (rp->r_flags & RG_AUTORESRV)
		map_flags |= MAP_AUTORESRV;
	if (rp->r_flags & RG_TEXT)
		map_flags |= MAP_TEXT;
	if (prp->p_flags & PF_NOSHARE)
		map_flags |= MAP_LOCAL;
	if (rp->r_flags & (RG_CW | RG_ANON))
		map_flags |= MAP_PRIVATE;
	if (rp->r_type == RT_MAPFILE)
		map_flags |= MAP_SHARED;
	if (prp->p_flags & PF_PRIMARY)
		map_flags |= MAP_PRIMARY;

	c_off = rp->r_fileoff + ctob(prp->p_offset) + (vaddr - prp->p_regva);

	if (force_read) {
		/* must create an actual page, even though it is clean, */
		/* to avoid a deadlock.					*/
		retval = mtext_forced_read_page(mvp,c_off);
		goto out;
	}


	/*
	 * At this point, we are all set to remove the old region (that is
	 * mapping the mtext proxy vnode), and replace it with a new
	 * region mapping the actual vnode.
	 */
	as_lookup_current(&vasid);	/* grab our address space id	*/

	/*
	 * Replace the existing mapping
	 */
	asd.as_op = AS_DEL_MUNMAP;
	asd.as_munmap_start = vaddr;
	asd.as_munmap_len = NBPP;
	asd.as_munmap_flags = DEL_MUNMAP_NOLOCK;
	(void) VAS_DELETESPACE(vasid, &asd, NULL);

	asa.as_op 		= AS_ADD_EXEC;
	asa.as_addr 		= vaddr;
	asa.as_length		= NBPP;
	asa.as_prot		= prp_prot;
	asa.as_maxprot		= prp_maxprot;
	asa.as_exec_flags	= map_flags;
	asa.as_exec_off		= c_off;
	asa.as_exec_vp		= real_vp;
	asa.as_exec_zfodlen	= 0;
#ifdef CKPT
	asa.as_exec_ckpt	= rp_ckpt;
#endif /* CKPT */
	retval = VAS_ADDSPACE(vasid, &asa, &asar);
	
out:
	mrunlock(&pas->pas_aspacelock);
	return(retval);
}			     

/*
 * vnodeops for mtext dummy vnodes
 */

STATIC int
mtextnode_read	(
	bhv_desc_t *bdp,	/* behavior wrapping vnode to read	*/
	struct uio *uiop,	/* user i/o info			*/
	int ioflag,		/* */
	struct cred *cr,	/* user credentials			*/
	struct flid *flidp
	)
{
	int	error;		/* return code					*/
	off_t	fileoffset;	/* base offset for read; same as uio->offset	*/
	size_t	filelen;	/* length of read				*/
	int 	i;		/* loop iterator				*/
	off_t	c_off;		/* current offset during page traversal		*/
	pfd_t	*pfd;		/* original page as read from the filesystem	*/
	caddr_t	ibuf;		/* in-memory copy of original page contents	*/
	
	size_t	i_off;
	size_t	i_off_last;
	
	pfd_t	*pfd2;		/* newly allocated page if we need to modify	*/
	caddr_t	ibuf2;		/* in-memory copy of new page contents		*/

	mtextnode_t *mp =	/* mtext-specific information re: this vnode	*/
		BHV_TO_MTEXT(bdp);
	
	struct iovec tiov;	/* temp copy of first iovec (uiop->uio_iov[0]) 	*/
	struct iovec *base_iov; /* address of first iovec in uio array		*/
	int	ntiov;		/* number of iovecs in uio array 		*/
	
	size_t	i_len;		/* length ofXXX ??? */
	inst_t	tinst;		/* temp instruction pointer if UIO_USERSPACE 	*/

	REALVP_DCL(bdp,vp,rvp);

	MTEXT_CHECK(mp);
	REALVP_CHECK(rvp);
	
	fileoffset = uiop->uio_offset;	/* save off beginning offset of read	*/

	/*
	 * add up the total number of bytes to read from all iovecs
	 */
	filelen = 0;
	for (i = 0; i < uiop->uio_iovcnt; i++) 
		filelen += uiop->uio_iov[i].iov_len;
        ntiov = uiop->uio_iovcnt;	/* number of iovecs in request		*/
	tiov = uiop->uio_iov[0];	/* structure copy the first iovec	*/
	base_iov = uiop->uio_iov;	/* pointer to first iovec		*/

	VOP_READ(rvp, uiop, ioflag, cr, flidp, error);
	if (error != 0)	{
		return(error);	/* propogate underlying fs error code	*/
	}

	/*
	 * Switch out based on address space of the read
	 */
	switch (uiop->uio_segflg) {
	case UIO_NOSPACE:
		/*
		 * UIO_NOSPACE is specified when the vm manager wants to page in
		 * a region of a file.  In this case, we ignore any user buffers in the
		 * uio structure passed to us and instead focus on the actual pages
		 * that the underlying filesystem will have allocated.
		 *
		 * The basic loop here traverses the range requested by the read,
		 * and checks each page individually for problem cvt instructions.
		 * If a page is found to be clean, it is marked as such so that it
		 * will not have to be checked on any subsequent reads.  If a page is 
		 * found that does, however, have one or more problem instructions, we:
		 *	- allocate a new page with pagealloc();
		 *	- copy the original page's contents to the new page;
		 *	- modify the problem instructions in the new page such that they 
		 *	  throw an Unimplemented exception that is handled in softfp;
		 *	- remove the original page from the vnode's page cache (pcache)
		 *	  with premove();
		 *	- insert the new (modified) page into the vnode's pcache with pinsert().
		 */
		for (c_off = fileoffset; c_off < fileoffset + filelen; c_off += NBPP) {
			if (mtext_offset_is_clean(vp,c_off)) 
				continue; /* someone else already checked the page */

			pfd = pfind((void *) vp,btoct(c_off),0);
			if (pfd != NULL)
				continue; /* someone else already patched the page */

			pfd = pfind((void *) mp->real_vnode,btoct(c_off),VM_ATTACH);
			if (pfd == NULL) 
				continue; /* someone stole the real page */
			ibuf = page_mapin(pfd,0,colorof(c_off));
			i_off = 0;
			if (! mtext_check_range(ibuf,NBPP,&i_off)) {
				mtext_offset_set_clean(vp,c_off);
				page_mapout(ibuf);
				pagefree(pfd);
				continue;
			}
			/* copy, patch, and hash the page */
			while ((pfd2 = (
#ifdef _VCE_AVOIDANCE
				vce_avoidance
				    ? pagealloc(colorof(c_off),VM_VACOLOR|VM_MVOK)
				    :
#endif /* _VCE_AVOIDANCE */			
				pagealloc(0,VM_MVOK))) == NULL) {
				setsxbrk();
			}
			COLOR_VALIDATION(pfd2,colorof(c_off),0,0);
			ibuf2 = page_mapin(pfd2,VM_VACOLOR,colorof(c_off));
			i_off = 0;
			i_off_last = 0;
			while (mtext_check_range(ibuf,NBPP,&i_off)) {
				bcopy(ibuf + i_off_last,ibuf2 + i_off_last,
				      (i_off - i_off_last) + sizeof(inst_t));
				mtext_fixup_inst(ibuf2,
						 &i_off);
				i_off_last = i_off;
			}
			if (i_off > i_off_last)
				bcopy(ibuf + i_off_last,ibuf2 + i_off_last,
				      (i_off - i_off_last));
			page_mapout(ibuf);
			pagefree(pfd);
			page_mapout(ibuf2);
			pagedone(pfd2);
			pfd2 = vnode_pinsert_try(vp,pfd2,btoct(c_off));
			pagefree(pfd2);
		}
		break;
	case UIO_USERSPACE:
	case UIO_USERISPACE:
		/*
		 * In this case, the pages were read into a user buffer instead of
		 * just being paged in as they are for the UIO_NOSPACE case above.
		 * here, we check the buffer that the underlying filesystem copied
		 * into, and look for our favorite instruction.  If we find it,
		 * we modify the user's buffer.
		 */
		if (ntiov != 1 ||
		    (((__psunsigned_t) tiov.iov_base) & (sizeof(inst_t) - 1)) != 0 ||
		    (tiov.iov_len & (sizeof(inst_t) -1)) != 0 ||
		    (uiop->uio_iov[0].iov_len & (sizeof(inst_t) - 1)) != 0)
			break;
		i_len = (tiov.iov_len - base_iov->iov_len);
		
		
		/*
		 * loop through the user's buffer and check for bad instructions.
		 */
		for (i_off = 0; i_off < i_len; i_off += sizeof(inst_t)) {
			tinst = (inst_t) fuiword(((char *)tiov.iov_base) + i_off);
			if (((__int32_t) tinst) == -1)
				continue;
			i_off_last = 0;
			if (mtext_check_range((char *) &tinst,
					      sizeof(inst_t),
					      &i_off_last)) {
				mtext_fixup_inst((char *) &tinst,
						 &i_off_last);
				suiword(((char *) tiov.iov_base) + i_off,
					tinst);
			}
		}
		break;
	case UIO_SYSSPACE:
	default:
		break;
	}

	return(error);
}

STATIC int
mtextnode_write(bdp, uiop, ioflag, cr, fl)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	struct uio *uiop;
	int ioflag;
	struct cred *cr;
	struct flid *fl;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_WRITE(rvp, uiop, ioflag, cr, fl, error);
	return(error);
}

STATIC int
mtextnode_ioctl(
	bhv_desc_t *bdp,	/* behavior wrapping vnode to read	*/
	int cmd,
	void *arg,
	int flag,
	struct cred *cr,
	int *rvalp,
	struct vopbd *vbds)
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_IOCTL(rvp, cmd, arg, flag, cr, rvalp, vbds, error);
	return(error);
}

STATIC int
mtextnode_setfl(bdp, oflags, nflags, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	int oflags;
	int nflags;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_SETFL(rvp, oflags, nflags, cr, error);
	return(error);
}

STATIC int
mtextnode_getattr(bdp, vap, flags, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	struct vattr *vap;
	int flags;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_GETATTR(rvp, vap, flags, cr, error);
	return(error);
}

STATIC int
mtextnode_setattr(bdp, vap, flags, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	struct vattr *vap;
	int flags;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_SETATTR(rvp, vap, flags, cr, error);
	return(error);
}

STATIC int
mtextnode_access(bdp, mode, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	int mode;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_ACCESS(rvp, mode, cr, error);
	return(error);
}

/* ARGSUSED */
STATIC int
mtextnode_inactive(bhv_desc_t *bdp,struct cred *cr)
{
	mtextnode_t *mp =    /* mtext-specific information re: this vnode    */
		BHV_TO_MTEXT(bdp);
	REALVP_DCL2(bdp,rvp);

	MTEXT_CHECK_BHV(bdp);
	if (rvp != NULL) {
		mp->real_vnode = NULL;
		VN_RELE(rvp);
	}

	return(VN_INACTIVE_CACHE);
}

STATIC int
mtextnode_fid(bdp, fidpp)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	struct fid **fidpp;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_FID(rvp, fidpp, error);
	return(error);
}

STATIC int
mtextnode_fid2(bdp, fidp)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	struct fid *fidp;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_FID2(rvp, fidp, error);
	return(error);
}

/* ARGSUSED */
STATIC void
mtextnode_rwlock(bdp, write_lock)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	vrwlock_t write_lock;
{
	REALVP_DCL2(bdp,rvp);

	if (rvp != NULL)
		VOP_RWLOCK(rvp, write_lock);
}

/* ARGSUSED */
STATIC void
mtextnode_rwunlock(bdp, write_lock)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	vrwlock_t write_lock;
{
	REALVP_DCL2(bdp,rvp);

	if (rvp != NULL)
		VOP_RWUNLOCK(rvp, write_lock);
}

STATIC int
mtextnode_seek(bdp, ooff, noffp)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	off_t ooff;
	off_t *noffp;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_SEEK(rvp, ooff, noffp,error);
	return(error);
}

STATIC int
mtextnode_cmp(
	bhv_desc_t *bdp,
	vnode_t *vp2
	)
{
	int	error;
	REALVP_DCL(bdp,vp1,rvp);

	REALVP_CHECK(rvp);

	if (vp1 == vp2)
		return 1;
	if (rvp == vp2)
		return 1;

	VOP_CMP(vp1,vp2,error);
	return(error);
}

struct flock;

STATIC int
mtextnode_frlock(bdp, cmd, bfp, flag, offset, vrwlock, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	int cmd;
	struct flock *bfp;
	int flag;
	off_t offset;
	vrwlock_t vrwlock;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_FRLOCK(rvp, cmd, bfp, flag, offset, vrwlock, cr, error);
	return(error);
}

STATIC int
mtextnode_realvp(bdp, vpp)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	register vnode_t **vpp;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_REALVP(rvp, vpp,error);
	return(error);
}

STATIC int
mtextnode_bmap(bdp, offset, count, rw, cr, iex, nex)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	off_t offset;
	ssize_t count;
	int rw;
	struct cred *cr;
	struct bmapval *iex;
	int *nex;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_BMAP(rvp, offset, count, rw, cr, iex, nex, error);
	return(error);
}

struct buf;

STATIC void
mtextnode_strategy(bdp, bp)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	struct buf *bp;
{
	REALVP_DCL2(bdp,rvp);

	MTEXT_CHECK_BHV(bdp);
	if (rvp == NULL) {
		bp->b_error = ENXIO;
		biodone(bp);
		return;
	}

	VOP_STRATEGY(rvp, bp);
}

/* ARGSUSED */
STATIC int
mtextnode_map(bdp, off, len, prot, flags, cr, nvp)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	off_t off;
	size_t len;
	mprot_t prot;
	u_int flags;
	struct cred *cr;
	vnode_t **nvp;
{
	int	error;
	struct vattr va;
	REALVP_DCL(bdp,vp,rvp);

	REALVP_CHECK(rvp);
	
	va.va_mask = AT_SIZE | AT_MODE;
	VOP_GETATTR(rvp, &va, ATTR_LAZY,
		    cr, error);
	if (error)
		return(error);
	if (vp->v_type != VREG)
		return ENODEV;
	if ((__scint_t)off < 0 || (__scint_t)(off + len) < 0)
		return EINVAL;
	if (len == 0)
		return ENXIO;
	return(0);
}


/* ARGSUSED */
STATIC int
mtextnode_addmap(bdp, op, vt, pgno, off, len, prot, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	vaddmap_t	op;
	vhandl_t	*vt;
	pgno_t *pgno;
	off_t off;
	size_t len;
	mprot_t	prot;
	struct cred *cr;
{
	MTEXT_CHECK_BHV(bdp);
	BHV_TO_MTEXT(bdp)->map_count += btoc(len);
	return(0);
}


/* ARGSUSED */
STATIC int
mtextnode_delmap(bdp, vt, len, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	struct __vhandl_s	*vt;
	size_t		len;
	cred_t		*cr;
{
	MTEXT_CHECK_BHV(bdp);
	BHV_TO_MTEXT(bdp)->map_count -= btoc(len);
	return(0);
}

STATIC int
mtextnode_poll(
	bhv_desc_t *bdp,	/* behavior wrapping vnode to read	*/
	short events,
	int anyyet,
	short *reventsp,
	struct pollhead **phpp,
	unsigned int *genp)
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_POLL(rvp, events, anyyet, reventsp, phpp, genp,error);
	return(error);
}

STATIC int
mtextnode_dump(bdp, addr, bn, count)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	caddr_t addr;
	daddr_t bn;
	u_int count;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_DUMP(rvp, addr, bn, count,error);
	return(error);
}

STATIC int
mtextnode_pathconf(bdp, cmd, valp, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	int cmd;
	long *valp;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_PATHCONF(rvp, cmd, valp, cr, error);
	return(error);
}

STATIC int
mtextnode_allocstore(bdp, off, len, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	off_t off;
	size_t len;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_ALLOCSTORE(rvp, off, len, cr, error);
	return(error);
}

STATIC int
mtextnode_fcntl(
	bhv_desc_t *bdp,	/* behavior wrapping vnode to read	*/
	int cmd,
	void *arg,
	int flag,
	off_t offset,
	struct cred *cr,
	rval_t *rvl)
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_FCNTL(rvp, cmd, arg, flag, offset, cr, rvl, error);
	return(error);
}

/*ARGSUSED*/
STATIC int
mtextnode_reclaim(
	bhv_desc_t *bdp,
	int flag
	)
{
	mtextnode_t 	*mp =	/* the mtextnode info	*/
		BHV_TO_MTEXT(bdp);
	REALVP_DCL(bdp,vp,rvp);

	MTEXT_CHECK(mp);
	VOP_FLUSHINVAL_PAGES(vp, 0, mp->real_size - 1, FI_NONE);
	dnlc_purge_vp(vp);
	mtext_remove_hash(mp);
	
	vn_bhv_remove(VN_BHV_HEAD(vp), &(mp->mt_bhv_desc));

	if (mp->clean_map != NULL) {
		kmem_free((void *) mp->clean_map,
			  BITS_TO_BITMAP_LENGTH(mp->clean_map_size));
		mp->clean_map = NULL;
	}
	MTEXT_CHECK_CLEAR(mp);
	kmem_free((void *) mp,sizeof(mtextnode_t));

	if (rvp != NULL)
		VN_RELE(rvp);
	return 0;
}

STATIC int
mtextnode_attr_get(bdp, attrname, attrvalue, valuelength, flags, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	char *attrname;
	char *attrvalue;
	int *valuelength;
	int flags;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_ATTR_GET(rvp, attrname, attrvalue, valuelength,
					flags, cr, error);
	return(error);
}

STATIC int
mtextnode_attr_set(bdp, attrname, attrvalue, valuelength, flags, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	char *attrname;
	char *attrvalue;
	int valuelength;
	int flags;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_ATTR_SET(rvp, attrname, attrvalue, valuelength,
					flags, cr, error);
	return(error);
}

STATIC int
mtextnode_attr_remove(bdp, attrname, flags, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	char *attrname;
	int flags;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_ATTR_REMOVE(rvp, attrname, flags, cr, error);
	return(error);
}

STATIC int
mtextnode_attr_list(bdp, buffer, bufsize, flags, cursor, cr)
	bhv_desc_t *bdp;	/* behavior wrapping vnode to read	*/
	char *buffer;
	int bufsize;
	int flags;
	struct attrlist_cursor_kern *cursor;
	struct cred *cr;
{
	int	error;
	REALVP_DCL2(bdp,rvp);

	REALVP_CHECK(rvp);

	VOP_ATTR_LIST(rvp, buffer, bufsize, flags, cursor, cr, error);
	return(error);
}

vnodeops_t mtextnode_vnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	(vop_open_t) fs_noerr, /* mtextnode_open, */
	(vop_close_t) fs_nosys, /* mtextnode_close, */
	mtextnode_read,
	mtextnode_write,
	mtextnode_ioctl,
	mtextnode_setfl,
	mtextnode_getattr,
	mtextnode_setattr,
	mtextnode_access,
	(vop_lookup_t) fs_nosys, /* mtextnode_lookup, */
	(vop_create_t) fs_nosys, /* mtextnode_create, */
	(vop_remove_t) fs_nosys, /* mtextnode_remove, */
	(vop_link_t) fs_nosys, /* mtextnode_link, */
	(vop_rename_t) fs_nosys, /* mtextnode_rename, */
	(vop_mkdir_t) fs_nosys, /* mtextnode_mkdir, */
	(vop_rmdir_t) fs_nosys, /* mtextnode_rmdir, */
	(vop_readdir_t) fs_nosys, /* mtextnode_readdir, */
	(vop_symlink_t) fs_nosys, /* mtextnode_symlink, */
	(vop_readlink_t) fs_nosys, /* mtextnode_readlink, */
	(vop_fsync_t) fs_nosys, /* mtextnode_fsync, */
	mtextnode_inactive,
	mtextnode_fid,
	mtextnode_fid2,
	mtextnode_rwlock,
	mtextnode_rwunlock,
	mtextnode_seek,
	mtextnode_cmp,
	mtextnode_frlock,
	mtextnode_realvp,
	mtextnode_bmap,
	mtextnode_strategy,
	mtextnode_map,
	mtextnode_addmap,
	mtextnode_delmap,
	mtextnode_poll,
	mtextnode_dump,
	mtextnode_pathconf,
	mtextnode_allocstore,
	mtextnode_fcntl,
	mtextnode_reclaim,
	mtextnode_attr_get,
	mtextnode_attr_set,
	mtextnode_attr_remove,
	mtextnode_attr_list,
	fs_cover,
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	fs_tosspages,
	fs_flushinval_pages,
	fs_flush_pages,
	fs_invalfree_pages,
	fs_pages_sethole,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};

STATIC int
mtext_hash(fid_t *fp)
{
      u_short *fi;
      int     fl;
      int     fs;

#ifdef DEBUG_MTEXT
      ASSERT_ALWAYS(fp != NULL && (fp->fid_len <= (sizeof(*fp) - sizeof(fp->fid_len))));
#endif
      for (fs = 0, fi = (u_short *) fp->fid_data, fl = (fp->fid_len / sizeof(u_short));
           fl > 0;
           fi++, fl--)
              fs += *fi;
      if (fp->fid_len & 1) 
              fs += fp->fid_data[fp->fid_len - 1];
      return(fs % MTEXT_HASH_SIZE);
}

STATIC int
mtext_fidcmp(fid_t *a, fid_t *b)
{
      return((a->fid_len != b->fid_len) ||
             bcmp((void *) a->fid_data,(void *) b->fid_data,a->fid_len));
}

#endif /* _MTEXT_VFS */

