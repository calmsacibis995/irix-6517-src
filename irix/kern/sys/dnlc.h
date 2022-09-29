/*
 * Directory Name Lookup Cache.
 *
 * Copyright 1992, Silicon Graphics, Inc.
 * All Rights Reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ifndef _FS_DNLC_H	/* wrapper symbol for kernel use */
#define _FS_DNLC_H	/* subject to change without notice */

/*#ident	"@(#)uts-3b2:fs/dnlc.h	1.4"*/
#ident	"$Revision: 1.12 $"

/*
 * This structure describes the elements in the cache of recently
 * looked up names.
 */

#define	NC_NAMLEN	31	/* maximum name segment length we bother with */

struct bhv_desc;
struct cred;
struct vnode;
struct nc_hash_s;

typedef struct ncache_s {
	struct ncache_s *hash_next; 	/* hash chain, MUST BE FIRST */
	struct ncache_s **hash_prevp;	/* pointer to previous hash_next */
	struct ncache_s *lru_next; 	/* LRU chain */
	struct ncache_s *lru_prev;
	struct nc_hash_s *hash_chain;	/* NULL if not on any */
	struct vnode *vp;		/* vnode the name refers to */
	struct bhv_desc *bdp;		/* behavior of the referred vnode */
	struct vnode *dp;		/* vnode of parent of name */
	struct cred *cred;		/* credentials */
	uint64_t vp_cap;		/* vnode pointer capability */
	uint64_t dp_cap;		/* parent directory capability */
	u_char namlen;			/* length of name */
	char name[NC_NAMLEN];		/* segment name */
	off_t offset;			/* offset in directory if fast */
} ncache_t;

/*
 * Stats on usefulness of name cache.
 */
typedef struct ncstats {
	u_int	hits;		/* hits that we can really use */
	u_int	misses;		/* cache misses */
	u_int	enters;		/* number of enters done */
	u_int	dbl_enters;	/* number of enters tried when already cached */
	u_int	long_enter;	/* long names tried to enter */
	u_int	long_look;	/* long names tried to look up */
	u_int	lru_empty;	/* LRU list empty */
	u_int	purges;		/* number of purges of cache */
	u_int	vfs_purges;	/* number of filesystem purges */
	u_int	removes;	/* number of removals by name */
	u_int	searches;	/* number of hash lookups */
	u_int	stale_hits;	/* hits that found old vnode stamp */
	u_int	steps;		/* hash chain steps for all searches */
	u_int	neg_enters;	/* number of negative entries entered */
	u_int	neg_hits;	/* hits of negative entries */
	u_int	pad;		/* allocated for future expansion */
} ncstats_t;

#define	ANYCRED	((struct cred *)(__psint_t)-1)
#define	NOCRED	((struct cred *)(__psint_t)0)

#ifdef _KERNEL
/*
 * External routines.
 */
struct pathname;
struct vfs;

typedef struct ncfastdata {
	char	*name;		/* name being operated upon */
	u_short	namlen;		/* name length, precomputed by lookuppn */
	u_char	flags;		/* name flags -- see pathname.h */
	u_char	vnowait;	/* lookup failed due to vn_get nowait */
	off_t	offset;		/* offset in directory if invariant */
	void	*hash;		/* hash bucket pointer hint */
} ncfastdata_t;

void		dnlc_enter(struct vnode *, char *, struct bhv_desc *,
			   struct cred *);
void		dnlc_enter_fast(struct vnode *, struct ncfastdata *,
				struct bhv_desc *, struct cred *);
void		dnlc_init(void);
struct bhv_desc	*dnlc_lookup(struct vnode *, char *, struct cred *, uint);
struct bhv_desc	*dnlc_lookup_fast(struct vnode *, char *, struct pathname *,
				  struct ncfastdata *, struct cred *, uint);
void		dnlc_purge_vp(struct vnode *);
int		dnlc_purge_vfsp(struct vfs *, int);
void		dnlc_remove(struct vnode *, char *);
void		dnlc_remove_fast(struct vnode *, struct ncfastdata *);
void		dnlc_remove_vp(struct vnode *);

#endif	/* _KERNEL */
#endif	/* _FS_DNLC_H */
