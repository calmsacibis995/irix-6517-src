#ifndef __SYS_MTEXT_H__
#define __SYS_MTEXT_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sys/mtext.h
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

#ident	"$Revision: 1.4 $"

#define CVT_CHECK_MASK  0xFFFF003F
#define CVT_D_L_ID	0x46A00021
#define CVT_S_L_ID	0x46A00020
#define INTCVT_INC	0x00400000
#define INTCVT_D_L_ID	(CVT_D_L_ID + INTCVT_INC)
#define INTCVT_S_L_ID	(CVT_S_L_ID + INTCVT_INC)

#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

#include <sys/vnode.h>

#define VNODE_POSITION_MTEXT	(VNODE_POSITION_BASE+8)

#ifdef _MTEXT_VFS

/*
 * The mtextnode is the equivilent of an inode in other filesystems; it
 * is information associated with a specific implementation layer of a
 * specific vnode.  mtextnode structures are associated with proxy vnodes
 * that are built by the mtext filesystem, and which cover a real vnode.
 */
#ifdef DEBUG_MTEXT
#define MTEXT_MAGIC 0xfadf1327
#endif

typedef struct mtextnode {
	struct vnode	*real_vnode;	/* pointer to vnode for real file (held) */
					/* (NULL when mtextnode is inactive) */
	char		*clean_map;	/* bit map of known clean pages */
					/* (NULL if all pages are clean) */
	size_t		clean_map_size; /* size in bits (one bit per page) */
	off_t		text_base;	/* offset of text segment in file */
	size_t		text_size;	/* size of text segment in bytes */
	struct mtextnode *next;		/* next mtextnode on this hash chain */
	cnt_t		map_count;	/* count of mappings */
	timespec_t	real_mtime;	/* va_mtime for real vnode */
	off_t		real_size;	/* va_size for real vnode */
	bhv_desc_t	mt_bhv_desc;	/* behavior descriptor	*/
	int		hash_index;	/* mtexthash index */
	fid_t		real_fid;	/* file handle for real file */
#ifdef DEBUG_MTEXT
	int		magic;		/* bug catcher */
#endif 
} mtextnode_t;

#define BHV_TO_MTEXT(bhv) ((mtextnode_t *) (BHV_PDATA(bhv)))
#define MTEXT_TO_BHV(mp) (& ((mp)->mt_bhv_desc))
#define VNODE_TO_MTEXT(vp) BHV_TO_MTEXT(VNODE_TO_FIRST_BHV(vp))
#define MTEXT_TO_VNODE(mp) BHV_TO_VNODE(MTEXT_TO_BHV(mp))

/*
 *  flags for mtext_probe calls
 */
#define MTEXTF_PROBE	0x0001		/* probe only (do not create) */
#define MTEXTF_ALL_CLEAN 0x00002	/* all pages are known clean  */

#ifdef _KERNEL

extern vnodeops_t mtextnode_vnodeops;

extern void mtext_init(void);

extern int mtext_probe(struct vnode *,	/* real vnode for which to probe */
		struct vnode **,	/* receives mtext vnode		 */
		off_t,			/* file offset of text section	 */
		size_t,			/* size of text section		 */
		int);			/* flags (MTEXTF_*)		 */
extern int mtext_offset_is_clean(struct vnode *,off_t);
extern struct vnode *mtext_real_vnode(struct vnode *);

#define IS_MTEXT_VNODE(vp) (((vnodeops_t *) (vp)->v_fops) == &mtextnode_vnodeops)

extern int mtext_check_range(
		caddr_t, 	/* kernel address of instructions  */
	        size_t,		/* length of instructions in bytes */
		size_t *);	/* starting offset on input and    */
				/* next match 			   */
	/* returns 0 if no match, 1 on a match */

extern void mtext_fixup_inst(
		caddr_t, 	/* kernel address of instructions  */
		size_t *); 	/* starting offset on input and    */
				/* incremented by one instruction  */
				/* on return 			   */

extern int mtext_map_real_vnode(
	    	vnode_t *vp,	/* mtext vnode 	     		*/
		pas_t	*pas,
		ppas_t	*ppas,
		caddr_t vaddr);	/* virtual address to remap 	*/

extern int mtext_execmap_vnode(
		vnode_t *vp,	/* real vnode			*/
		caddr_t vaddr, 
	       	size_t len,
		size_t zfodlen,
		off_t  offset,
		int prot,
		int flags,
		vasid_t vasid,
		int ckpt);

#endif /* _KERNEL */

#endif /* _MTEXT_VFS */

#endif /* C || C++ */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_MTEXT_H__ */
