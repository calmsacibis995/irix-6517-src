/*
 * Directory Name Lookup Cache.
 *
 * Copyright 1992-1993, Silicon Graphics, Inc.
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

#ident	"$Revision: 1.36 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/dnlc.h>
#include <sys/kmem.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/sema.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <string.h>
#include <sys/atomic_ops.h>

/*
 * Directory name lookup cache.
 * Based on code originally done by Robert Elz at Melbourne.
 * Merged with MP soft-reference cache by Brendan Eich at SGI.
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where the vp refers to the
 * directory containing the name.
 *
 * For simplicity (and economy of storage), names longer than
 * some (small) maximum length are not cached; they occur
 * infrequently in any case, and are almost never of interest.
 */

/*
 * Hash bucket of name cache entries.
 * One multi-reader lock for each chain.
 */
typedef struct nc_hash_s {
	ncache_t *hash_next;
	mrlock_t hash_lock;
#if (DEBUG_PN_HASH || DEBUG)
	int	hash_size;
	int	hash_avg_size;
#endif
} nc_hash_t;
#ifdef MP
#pragma set type attribute nc_hash_t align=128
#endif

#if (DEBUG_PN_HASH || DEBUG)
#define HASH_SIZE_SAMPLES		1024
#define nc_hash_avg_size(nch)	((nch)->hash_avg_size / HASH_SIZE_SAMPLES)
#define nc_hash_size_dec(nch) \
	{ (nch)->hash_size--; \
	  (nch)->hash_avg_size -= nc_hash_avg_size(nch); \
	  (nch)->hash_avg_size += (nch)->hash_size; }
#define nc_hash_size_inc(nch) \
	{ (nch)->hash_size++; \
	  (nch)->hash_avg_size -= nc_hash_avg_size(nch); \
	  (nch)->hash_avg_size += (nch)->hash_size; }
#else
#define nc_hash_size_dec(nch)
#define nc_hash_size_inc(nch)
#endif

/*
 * LRU list of cache entries for aging.
 * This is just a structure for the header.
 */
typedef struct nc_lru_s {
	ncache_t 	*hash_next;	/* unused in LRU header */
	ncache_t 	**hash_prevp;
	ncache_t 	*lru_next;	/* LRU chain */
	ncache_t 	*lru_prev;
	lock_t		lru_lock;
} nc_lru_t;
#ifdef MP
#pragma set type attribute nc_lru_t align=128
#endif

#define LRU_LOCK(lrup)	mutex_spinlock(&(lrup)->lru_lock)
#define LRU_UNLOCK(l,s)	mutex_spinunlock(&(l)->lru_lock, s)
#ifdef DNLC_PRINTF
# define LRU_TRYLOCK(l)	mutex_spintrylock(&(l)->lru_lock)
#endif

/*
 * dnlc global data
 */
extern int 	ncsize;			/* defined in mtune/kernel */
extern int 	nclrus;			/* defined in mtune/kernel */

static u_int 		nc_hash_mask;	/* hash mask */
static u_int 		nc_hash_shift;	/* hash mask shift */
static u_int 		nc_lru_mask;	/* lru mask */
static ncache_t 	*ncache;	/* name cache entry */
static nc_hash_t	*nc_hash;	/* hash list of name cache entries */
static nc_lru_t		*nc_lrup;	/* lru lists of entries for aging */

static int _nchash(u_long, __psunsigned_t);

/*
 * Hash macros.
 */
#define	NC_HASH_PN(hash, vp)	_nchash(hash, (__psunsigned_t)vp)
#define	NC_HASH(name, len, vp)	NC_HASH_PN(pn_hash(name,len), vp)

#define	NULL_HASH(ncp)		((ncp)->hash_next = 0, \
				 (ncp)->hash_prevp = &(ncp)->hash_next)

#ifdef MP
#define VP_LRU_INDEX(vp)	((((__psunsigned_t)(vp))>>8) & nc_lru_mask)
#define	VP_TO_LRU_LIST(vp)	&nc_lrup[VP_LRU_INDEX(vp)]
#define	NC_LRU_LIST(ncp)	&nc_lrup[(ncp - ncache) & nc_lru_mask]
#else
#define VP_LRU_INDEX(vp)	0
#define	VP_TO_LRU_LIST(vp)	&nc_lrup[0]
#define	NC_LRU_LIST(ncp)	&nc_lrup[0]
#endif

/* #define DNLC_PRINTF 1 for more debugging */
#ifdef DNLC_PRINTF
int dnlc_looks, dnlc_waits, dnlc_enthit, dnlc_entmiss, 
    dnlc_spins, dnlc_nopromo;
#endif

static void		dnlc_rm(ncache_t *);
static ncache_t		*dnlc_search(vnode_t *, char *, int, nc_hash_t *,
				     cred_t *);

static void __inline	nc_inshash(ncache_t *, nc_hash_t *);
static void __inline	nc_rmhash(ncache_t *);
static void __inline	nc_inslru(ncache_t *, ncache_t *);
static void __inline	nc_rmlru(ncache_t *);
extern void		qprintf(char *f, ...);

static int
_nchash(u_long b, __psunsigned_t d)
{
	unsigned int shift;
	u_long hval;

	/*
	 * __psunsigned and u_long are both
	 * (independently) either 32 or 64 bits.
	 */
	shift = nc_hash_shift;

	/* top bit(s) are never intersting. */

	d >>= 8;		/* divide by ~sizeof(vnode_t) */
#if _MIPS_SIM == _ABI64
	b += (d & 0x7fffffff);	/* 512 GB of address should be enough! */
#else
	b += (d & 0x7fffff);	/* remove top bit */
#endif
	/*
	 * Fold by the size of the name-cache hash bucket.
	 */
	for (hval = 0; b; b >>= shift) {
		hval ^= b;
	}
	return (int)(hval & nc_hash_mask);
}

/*
 * Initialize the directory cache.
 * Put all the entries on the LRU chain and clear out the hash links.
 */
void
dnlc_init(void)
{
	register ncache_t *ncp;
	register nc_lru_t *lrup;
	register int i;
	register int b;

	i = ncsize / 8;
	/*
	 * minimum hashes is 16 
	 */
	for (b = 4; (1 << b) < i; b++)
		;
	nc_hash_mask = (1 << b) -1;
	nc_hash_shift = b;

	nc_hash = kmem_zalloc((nc_hash_mask+1) * sizeof *nc_hash, KM_SLEEP);
	for (i = 0; i <= nc_hash_mask; i++) {
		mrlock_init(&(nc_hash[i].hash_lock), MRLOCK_DEFAULT, 
			    "nc_lock", i);
#ifdef DEBUG
		/* First approximation at average size -- time will tell. */
		nc_hash[i].hash_avg_size = HASH_SIZE_SAMPLES *
			ncsize / (nc_hash_mask+1);
#endif
	}

#ifdef MP
	/*
	 * Allocate lru lists.  The number of lru lists must always
	 * be a power of two.
	 */
	if (nclrus) {
		/*
		 * Round nclrus up or down to power of two.
		 */
		i = numcpus;
		while (i & (i - 1))
			i++;

		nc_lru_mask = nclrus;
		if (nc_lru_mask > i)
			nc_lru_mask = i;
		else
			while (nc_lru_mask & (nc_lru_mask-1))
				nc_lru_mask++;
	} else {
		/*
		 * calc power-of-two <= numcpus
		 */
		i = numcpus;
		while (i & (i-1))
			i--;
		/*
		 * Round up if closer to next power of two.
		 */
		if (numcpus >= (i * 3 / 2))
			i <<= 1;

		/*
		 * First approximation.  Target is to have a power-of-two
		 * number of lrus that is <= numcpus and which contain
		 * between XXX and YYY entries per lru list.
		 * XXX	Should min and max lru sizes be configurable?
		 */
		nc_lru_mask = i / 2;

		while ((nc_lru_mask > 1) && ((ncsize/nc_lru_mask) < 392))
			nc_lru_mask /= 2;
		while ((nc_lru_mask < i) && ((ncsize/nc_lru_mask) > 8192))
			nc_lru_mask *= 2;
	}

	nc_lrup = kmem_zalloc(nc_lru_mask * sizeof *nc_lrup, KM_SLEEP);
	nc_lru_mask--;
#else
	nc_lrup = kmem_zalloc(sizeof *nc_lrup, KM_SLEEP);
	nc_lru_mask = 0;
#endif

	for (i = 0; i <= nc_lru_mask; i++) {
		init_spinlock(&(nc_lrup[i].lru_lock), "nc_lru", i);
		nc_lrup[i].lru_next =
		nc_lrup[i].lru_prev = (ncache_t *) &nc_lrup[i];
	}

	ncache = kmem_zalloc(ncsize * sizeof *ncache, KM_SLEEP);

	/*
	 * name-cache members are permanently assigned to particular
	 * lru-lists.  The lru list of a particular name cached is
	 * based on the in-core vnode address.  The current vnodes
	 * are allocated from 256-byte zones, and the hashing code
	 * just divides by 256, so they should be pretty well distributed.
	 */
	for (ncp = ncache, i = 0; i < ncsize; ncp++, i++) {
		lrup = &nc_lrup[i & nc_lru_mask];
		nc_inslru(ncp, (ncache_t *)lrup);
		NULL_HASH(ncp);
	}
}

/*
 * Add a name to the directory cache.  This takes the behavior
 * to be returned from a dnlc_lookup call.  This is better than returning
 * the vnode the behavior belongs to, because the caller would almost
 * always have to immediately look up the behavior anyway.  We can
 * easily get from the behavior to the vnode, and we keep it in the
 * dnlc for use in identifying the vnode a behavior is associated
 * with in the purge and remove calls.
 */
void
dnlc_enter(register vnode_t *dp, register char *name, bhv_desc_t *bdp,
	   cred_t *cred)
{
	register u_int namlen;
	ncfastdata_t fd;

	fd.name = name;
	fd.namlen = namlen = strlen(name);
	fd.offset = 0;
	fd.hash = &nc_hash[NC_HASH(name, namlen, dp)];
	dnlc_enter_fast(dp, &fd, bdp, cred);
}

void
dnlc_enter_fast(vnode_t *dp, ncfastdata_t *fd, bhv_desc_t *bdp,
		struct cred *cred)
{
	register u_int namlen;
	register char *name;
	register nc_hash_t *nch;
	register ncache_t *ncp;
	nc_lru_t *lrup;
	vnode_t *vp;
	struct cred *ocred;
	int s;

	if ((namlen = fd->namlen) > NC_NAMLEN) {
		NCSTATS.long_enter++;
		return;
	}
	name = fd->name;
	nch = fd->hash;
	vp = BHV_TO_VNODE(bdp);
	lrup = VP_TO_LRU_LIST(vp);

	/*
	 * Get a least recently used cache struct
	 * which we can "easily" remove from its hash chain.
	 * We keep trying until we get one NOT on any chain, or
	 * that we can lock on the first try.
	 * Note that list is not null-terminated -- it ends with
	 * nc_lru_t header.
	 */
	s = LRU_LOCK(lrup);
	for (ncp = lrup->lru_next;
	     ncp != (ncache_t *)lrup;
	     ncp = ncp->lru_next) {
		ASSERT(((ncp - ncache) & nc_lru_mask) == lrup - nc_lrup);
		if (ncp->hash_chain) {
			if (!mrtryupdate(&ncp->hash_chain->hash_lock))
				continue;
			nc_rmhash(ncp);
			nc_hash_size_dec(ncp->hash_chain);
			mrunlock(&ncp->hash_chain->hash_lock);
		}
		nc_rmlru(ncp);
		break;
	}
	LRU_UNLOCK(lrup, s);

	if (ncp == (ncache_t *)lrup) {
		return;			/* could not find any! */
	}
#ifdef DNLC_PRINTF
	if (mrtryupdate(&nch->hash_lock)) {
		atomicAddInt(&dnlc_enthit, 1);
	} else {
		mrupdate(&nch->hash_lock);
		atomicAddInt(&dnlc_entmiss, 1);
	}
#else
	mrupdate(&nch->hash_lock);
#endif	
	/*
	 * We could have dups, if there were multiple CPUs
	 * doing the above concurrently
	 */
	if (dnlc_search(dp, name, namlen, nch, cred) != NULL) {
		NCSTATS.dbl_enters++;
		s = LRU_LOCK(lrup);
		ncp->hash_chain = NULL;
		nc_inslru(ncp, (ncache_t *)lrup);
		LRU_UNLOCK(lrup, s);
		mrunlock(&nch->hash_lock);
		return;
	}

	/*
	 * Hold the cred for the entering user and
	 * fill in cache info.
	 */
	ncp->dp = dp;
	ncp->dp_cap = dp->v_namecap;
	ncp->bdp = bdp;
	ncp->vp = vp;
	ncp->vp_cap = vp->v_namecap;
	ncp->namlen = (u_char)namlen;
	bcopy(name, ncp->name, namlen);
	ncp->offset = fd->offset;
	ocred = ncp->cred;
	ncp->cred = cred;
	if (cred)
		crhold(cred);
	/*
	 * Insert in hash chain, LRU (with lock).
	 */
	s = LRU_LOCK(lrup);
	ncp->hash_chain = nch;
	nc_inshash(ncp, nch);
	nc_inslru(ncp, lrup->lru_prev);
	LRU_UNLOCK(lrup, s);
	nc_hash_size_inc(nch);
	mrunlock(&nch->hash_lock);
	NCSTATS.enters++;

	/*
	 * Drop hold on old cred (if we had one).  We delay this until
	 * after dropping the mrlock to conserve lock time.
	 */
	if (ocred != NULL)
		crfree(ocred);

	vn_trace_entry(vp, "dnlc_enter_vp", (inst_t *)__return_address);
}

/*
 * Look up a name in the directory name cache.  This returns the behavior
 * that was given with the dnlc_enter call.  This is better than returning
 * the vnode the behavior belongs to, because the caller would almost
 * always have to immediately look up the behavior anyway.
 *
 * The flags passed in are passed unchanged to vn_get().
 */
bhv_desc_t *
dnlc_lookup(vnode_t *dp, register char *name, cred_t *cred, uint vn_flags)
{
	ncfastdata_t fd;

	return dnlc_lookup_fast(dp, name, NULL, &fd, cred, vn_flags);
}

bhv_desc_t *
dnlc_lookup_fast(vnode_t *dp, char *name, struct pathname *pnp,
		 ncfastdata_t *fd, struct cred *cred, uint vn_flags)
{
	register u_int namlen;
	u_short flags;
	register nc_hash_t *nch;
	register ncache_t *ncp;
	register bhv_desc_t *bdp;
	nc_lru_t *lrup;
	vnode_t *vp;
	vmap_t vmap;
	int lru_lock_s;

	if (pnp) {
		namlen = pnp->pn_complen;
		flags = pnp->pn_flags;
		nch = &nc_hash[NC_HASH_PN(pnp->pn_hash, dp)];
	} else {
		namlen = strlen(name);
		if (namlen == 1 && *name == '.')
			flags = PN_ISDOT;
		else if (namlen == 2 && name[0] == '.' && name[1] == '.')
			flags = PN_ISDOTDOT;
		else
			flags = 0;
		nch = &nc_hash[NC_HASH(name, namlen, dp)];
	}

	/*
	 * Always return fastdata results.
	 */
	fd->name = name;
	fd->namlen = namlen;
	fd->flags = flags;
	fd->hash = nch;
	fd->vnowait = 0;
	if (namlen > NC_NAMLEN) {
		NCSTATS.long_look++;
		return NULL;
	}

#ifdef DNLC_PRINTF
	if (atomicAddInt(&dnlc_looks, 1) == 9999) {
		printf("waits=%d, enter hits=%d waits=%d nopromo=%d",
		       dnlc_waits, dnlc_enthit, dnlc_entmiss, dnlc_nopromo);
		if (pnp) {
			printf(" pnp %d cred %s bucket=%d\n", 
			       pnp->pn_hash, cred ? "Y" : "N",
			       NC_HASH_PN(pnp->pn_hash, dp));
		} else {
			printf(" cred %s bucket=%d dp=%X len=%d\n", 
			       cred ? "Y" : "N",
			       NC_HASH(name, namlen, dp), dp, namlen);
		}
		dnlc_looks = 0;
		dnlc_waits = 0;
		dnlc_enthit = 0;
		dnlc_entmiss = 0;
		dnlc_spins = 0;
		dnlc_nopromo = 0;
	}
	if (!mrtryaccess(&nch->hash_lock)) {
		atomicAddInt(&dnlc_waits, 1);
		mraccess(&nch->hash_lock);
	}
#else
	mraccess(&nch->hash_lock);
#endif	
	/*
	 * Search the dnlc.
	 */
	if ((ncp = dnlc_search(dp, name, namlen, nch, cred)) == NULL) {
		mrunlock(&nch->hash_lock);
		NCSTATS.misses++;
		return NULL;
	}

	/*
	 * Race handling with vnode reclamation.
	 *
	 * We must ensure that dnlc_purge_vp (on behalf of vnode reclamation)
	 * hasn't been called since the time that dnlc_search succeeded.
	 * Thus, while holding the vnode lru locks reverify that the
	 * capability number matchs (dnlc_purge_vp gets the lru lock of
	 * the purging vnode before bumping the capability number).
	 * 
	 * Also while holding the lru lock snapshot the VMAP structure.  
	 * If we didn't do this then it's possible that the vnode would
	 * get reallocated (vn_reclaim'd and then vn_alloc'd) to another 
	 * use (with the v_number bumped), we then snapshot VMAP, and
	 * then vn_get erroneously finds a match.
	 *
	 * Note that name removal (via dnlc_remove) and vnode shaking
	 * (via dnlc_remove_vp) are synchronized with the hash chain
	 * lock that we're currently holding.
	 */
	lrup = NC_LRU_LIST(ncp);
	ASSERT(((ncp - ncache) & nc_lru_mask) == lrup - nc_lrup);

	/*
	 * Acquire the LRU lock to move the entry to the end of the LRU list.
	 */
#ifdef DNLC_PRINTF
	lru_lock_s = LRU_TRYLOCK(lrup);
	if (lru_lock_s == 0) {
		atomicAddInt(&dnlc_spins, 1);
		lru_lock_s = LRU_LOCK(lrup);
	}
#else
	lru_lock_s = LRU_LOCK(lrup);
#endif

	vp = ncp->vp;
	if (ncp->vp_cap != vp->v_namecap) {
		LRU_UNLOCK(lrup, lru_lock_s);
		mrunlock(&nch->hash_lock);
		NCSTATS.misses++;
		return NULL;
	}

	vn_trace_entry(vp, "dnlc_lookup_fast", (inst_t *)__return_address);
	VMAP(vp, vmap);

	/*
	 * Move to the end of the LRU list.
	 */
	nc_rmlru(ncp);
	nc_inslru(ncp, lrup->lru_prev);

	LRU_UNLOCK(lrup, lru_lock_s);

	fd->offset = ncp->offset;
	bdp = ncp->bdp;

	/*
	 * If not at the head of the hash chain, move forward so 
	 * it will be found earlier if looked up again.
	 * Optimization, so do NOT be forceful.
	 */
	if (ncp->hash_prevp != &nch->hash_next) {
		if (mrislocked_update(&nch->hash_lock) ||
		    mrtrypromote(&nch->hash_lock)) {
			nc_rmhash(ncp);
			nc_inshash(ncp, nch);
#ifdef DNLC_PRINTF
		} else {
			atomicAddInt(&dnlc_nopromo, 1);
#endif
		}
	}

	mrunlock(&nch->hash_lock);

	if (!(vp = vn_get(vp, &vmap, vn_flags))) {
		if (vmap.v_id == -2)
			fd->vnowait = 1;
		return NULL;
	}

	NCSTATS.hits++;
	vn_trace_entry(vp, "dnlc_lookup_fast:x", (inst_t *)__return_address);

	return bdp;
}

/*
 * Find the name of a vnode.  Used by idbg.c.
 * Note 1) no locking; 2) use of static.
 */
char *
dnlc_v2name(struct vnode *vp)
{
	static	char name[NC_NAMLEN+1];
	ncache_t *ncp, *ncpend;
	int step;

	ncpend = ncache + ncsize;
	step = nc_lru_mask + 1;

	for (ncp = ncache + VP_LRU_INDEX(vp); ncp < ncpend; ncp += step) {
		if (ncp->vp == vp &&
		    ncp->vp_cap == vp->v_namecap &&
		    ncp->dp &&
		    ncp->namlen > 0) {
			bcopy(ncp->name, name, ncp->namlen);
			name[ncp->namlen] = 0;
			return (name);
		}
	}
	return ("?");
}

/*
 * Find the name of a vnode.  Called with the vnode held (indirectly)
 * in the case of the buffer cache, so the vnode won't be going anywhere,
 * so it's safe to dereference vp.
 * We don't traverse the lru list because we'd be holding it for a very
 * long time (average lru-list-size/2).  Instead we find what looks like
 * an entry for vp, get the particulars, calculate which hash list it
 * would be on, then try to find it on the hash list.  [The hash list
 * should be much shorter than the lru list.]
 */
void
dnlc_vname(struct vnode *vp, char *name)
{
	ncache_t *ncp, *next, *ncpend;
	struct vnode *dp;
	nc_hash_t *nch;
	int step, namlen;

	ncpend = ncache + ncsize;
	step = nc_lru_mask + 1;

	for (ncp = ncache + VP_LRU_INDEX(vp); ncp < ncpend; ncp += step) {
		if (ncp->vp != vp ||
		    ncp->vp_cap != vp->v_namecap ||
		    !(namlen = ncp->namlen))
			continue;

		dp = ncp->dp;
		ASSERT(namlen <= NC_NAMLEN);
		bcopy(ncp->name, name, namlen);
		name[namlen] = '\0';

		nch = &nc_hash[NC_HASH(name, namlen, dp)];

		mraccess(&nch->hash_lock);
		for (next = nch->hash_next; next; next = next->hash_next) {
			if (next != ncp)
				continue;
			if (ncp->vp == vp &&
			    ncp->vp_cap == vp->v_namecap &&
			    namlen == ncp->namlen &&
			    bcmp(ncp->name, name, namlen) == 0) {
				mrunlock(&nch->hash_lock);
				return;
			}
			break;
		}
		mrunlock(&nch->hash_lock);
	}

	*name = '?';
	*(name+1) = '\0';
}

#if (DEBUG_PN_HASH || DEBUG)
static void
idbg_ncache_hash(void)
{
	int i, sz, avg;
	long tot = 0;
	long totavg = 0;
	int szovflow = 0;
	int avgovflow = 0;
	int avgs[32];
	int sizes[32];
	nc_hash_t *nch;
	int largest = 0;

	for (i = 0; i < 32; i++) {
		avgs[i] = 0;
		sizes[i] = 0;
	}

	for (i = 0; i <= nc_hash_mask; i++) {
		sz = nc_hash[i].hash_size;
		tot += sz;
		if (sz < 32)
			sizes[sz]++;
		else
			szovflow++;

		if (sz > largest) {
			nch = &nc_hash[i];
			largest = sz;
		}

		avg = nc_hash_avg_size(&nc_hash[i]);
		totavg += avg;
		if (avg < 32)
			avgs[avg]++;
		else
			avgovflow++;
	}

	qprintf(" namecache hash list avg sizes:\n ");
	for (i = 0; i < 32; i++) {
		if (avgs[i])
			qprintf(" %d at %d;", avgs[i], i);
	}
	if (avgovflow)
		qprintf(" %d over 31;", avgovflow);

	qprintf("\n namecache hash list actual sizes:\n ");
	for (i = 0; i < 32; i++) {
		if (sizes[i])
			qprintf(" %d at %d;", sizes[i], i);
	}
	if (szovflow)
		qprintf(" %d over 31;", szovflow);
	if (largest)
		qprintf(" largest %d, header 0x%x nch [%d]",
			largest, nch, nch - &nc_hash[0]);

	qprintf("\n %d (of %d) entries hashed, avg hash size %d\n",
		tot, ncsize, tot / (nc_hash_mask+1));
}
#endif

void
idbg_ncache(__psint_t n)
{
	char name[NC_NAMLEN+1];
	ncache_t *ncp;

	if (n < 0) {
		ncp = (ncache_t *)n;
	} else if (n < ncsize) {
		ncp = ncache + n;
		qprintf("printing ncache entry at 0x%x\n", ncp);
	} else {
		qprintf("ncache entry %d out of range [0-%d]\n", n, ncsize);
#if (DEBUG_PN_HASH || DEBUG)
		idbg_ncache_hash();
#endif
		return;
	}

	bcopy(ncp->name, name, ncp->namlen);
	name[ncp->namlen] = 0;

	if (ncp->vp)
		qprintf("  name %s vp 0x%x dp 0x%x bdp 0x%x cap 0x%x %s\n", 
			name, ncp->vp, ncp->dp, ncp->bdp, ncp->vp_cap,
			(ncp->vp_cap == ncp->vp->v_namecap &&
			 ncp->dp &&
			 ncp->dp_cap == ncp->dp->v_namecap) ?
			"VALID" : "INVALID");
	else
		qprintf("  name %s [%s] dp 0x%x dp cap 0x%x %s\n", 
			name, ncp->namlen ? "NEG" : "EMPTY",
			ncp->dp, ncp->dp_cap,
			(ncp->dp && ncp->dp_cap == ncp->dp->v_namecap) ?
			"VALID" : "INVALID");
	qprintf(
	"  lru_n 0x%x lru_p 0x%x hsh_next 0x%x hsh_prev 0x%x hsh_chn 0x%x\n", 
		ncp->lru_next, ncp->lru_prev,
		ncp->hash_next, ncp->hash_prevp, ncp->hash_chain);
}

/*
 * Dump everything in the dnlc, or search for entry matching a 
 * particular behavior descriptor (bdp).  For idbg.c.
 */
void
idbg_dnlc(__psint_t n)
{
	static	char name[NC_NAMLEN+1];
	ncache_t *ncp;
	nc_lru_t *lrup;
	bhv_desc_t *bdp;
	int i, all;

	if (n == -1L) {
		all = 1;
		qprintf("DNLC contents\n");
	} else {
		all = 0;
		bdp = (bhv_desc_t *)n;
		qprintf("Searching DNLC for entry with bdp 0x%x\n", bdp);
	}
	
	for (lrup = nc_lrup, i = 0; i <= nc_lru_mask; i++, lrup++) {
		qprintf(" DNLC LRU %d:\n", i);

		for (ncp = lrup->lru_next;
		     ncp != (ncache_t *) lrup;
		     ncp = ncp->lru_next) {
			if (ncp->namlen == 0 ||
			    !ncp->dp ||
			    (!all && bdp != ncp->bdp))
				continue;

			bcopy(ncp->name, name, ncp->namlen);
			name[ncp->namlen] = 0;
			qprintf(
			" name %s vp 0x%x bdp 0x%x cap 0x%x %s\n", 
				name, ncp->vp, ncp->bdp, ncp->vp_cap,
				(ncp->vp_cap == ncp->vp->v_namecap &&
				 ncp->dp_cap == ncp->dp->v_namecap) ?
				"VALID" : "INVALID");
		}
	}
}

/*
 * Remove an entry in the directory name cache.
 */
void
dnlc_remove(vnode_t *dp, register char *name)
{
	register int namlen = strlen(name);
	register nc_hash_t *nch;
	register ncache_t *ncp;

	if (namlen > NC_NAMLEN)
		return;

	nch = &nc_hash[NC_HASH(name, namlen, dp)];
	mrupdate(&nch->hash_lock);
	while (ncp = dnlc_search(dp, name, namlen, nch, ANYCRED))
		dnlc_rm(ncp);
	mrunlock(&nch->hash_lock);
	NCSTATS.removes++;
}

void
dnlc_remove_fast(vnode_t *dp, ncfastdata_t *fd)
{
	register int namlen;
	register char *name;
	register nc_hash_t *nch;
	register ncache_t *ncp;

	if ((namlen = fd->namlen) > NC_NAMLEN)
		return;

	name = fd->name;
	nch = fd->hash;
	mrupdate(&nch->hash_lock);
	while (ncp = dnlc_search(dp, name, namlen, nch, ANYCRED))
		dnlc_rm(ncp);
	mrunlock(&nch->hash_lock);
	NCSTATS.removes++;
}

/*
 * The given vnode is about to be deallocated, so remove
 * any reference to it.
 */
void
dnlc_remove_vp(vnode_t *vp)
{
	ncache_t *ncp, *ncq;
	int bucket;
	nc_hash_t *nhp;

	vn_trace_entry(vp, "dnlc_remove_vp", (inst_t *)__return_address);

	/*
	 * At the time dnlc_remove_vp is called, there's little
	 * likelyhood that there's any references to it cached
	 * in the dnlc.  With that in mind, we'll search the hash
	 * buckets in access mode and only bother to upgrade if
	 * we see any matches. --yohn
	 */
	for (bucket = 0; bucket <= nc_hash_mask; bucket++) {
		nhp = &nc_hash[bucket];
		mraccess(&nhp->hash_lock);
		for (ncp = nhp->hash_next; ncp; ncp = ncq) {
			ncq = ncp->hash_next;
			if (ncp->dp == vp || ncp->vp == vp) {
				if (!mrtrypromote(&nhp->hash_lock)) {
					mrunlock(&nhp->hash_lock);
					mrupdate(&nhp->hash_lock);
				}
				for (ncp = nhp->hash_next; ncp; ncp = ncq) {
					ncq = ncp->hash_next;
					if (ncp->dp == vp || ncp->vp == vp) {
						NCSTATS.purges++;
						dnlc_rm(ncp);
					}
				}
				break;
			}
		}
		mrunlock(&nhp->hash_lock);
	}
}

/*
 * "Purge" any cache entries referencing a vnode.
 * Actually just invalidates by incrementing
 * the capability number. Subsequent lookups get stale hits.
 */
void
dnlc_purge_vp(register vnode_t *vp)
{
	nc_lru_t *lrup = VP_TO_LRU_LIST(vp);
	int s;

	vn_trace_entry(vp, "dnlc_purge_vp", (inst_t *)__return_address);

	/*
	 * This field is now 64-bits and just won't ever wrap.
	 */
	s = LRU_LOCK(lrup);
	++vp->v_namecap;
	ASSERT(vp->v_namecap);
	LRU_UNLOCK(lrup, s);

	/* NCSTATS.purges++; */
}

/*
 * Purge cache entries referencing a vfsp.  Caller supplies a count
 * of entries to purge; up to that many will be removed.  A count of
 * zero indicates that all such entries should be purged.  Returns
 * the number of entries that were purged.
 */
/* ARGSUSED */
int
dnlc_purge_vfsp(register vfs_t *vfsp, register int count)
{
	register ncache_t *ncp, *ncq;
	register int n = 0, bucket;
	nc_hash_t *nhp;

	for (bucket = 0; bucket <= nc_hash_mask; bucket++) {
		nhp = &nc_hash[bucket];
		mrupdate(&nhp->hash_lock);
		for (ncp = nhp->hash_next; ncp; ncp = ncq) {
			ncq = ncp->hash_next;
			if (ncp->dp && (ncp->dp->v_vfsp == vfsp) ||
			    ncp->vp && (ncp->vp->v_vfsp == vfsp)) {
				NCSTATS.vfs_purges++;
				dnlc_rm(ncp);
				if (++n == count) {
					mrunlock(&nhp->hash_lock);
					return (n);
				}
			}
		}
		mrunlock(&nhp->hash_lock);
	}
	return (n);
}

/*
 * Obliterate a cache entry.
 * Must be called with hash chain locked for update.
 * Gets the lru lock.
 */
static void
dnlc_rm(register ncache_t *ncp)
{
	nc_lru_t *lrup = NC_LRU_LIST(ncp);
	struct cred *cred;
	int s;

	ASSERT(ncp->hash_chain == NULL || \
	       ismrlocked(&ncp->hash_chain->hash_lock, MR_UPDATE));

	/*
	 * Remove from LRU, hash chains.
	 */
	s = LRU_LOCK(lrup);
	nc_rmlru(ncp);
	nc_rmhash(ncp);
	nc_hash_size_dec(ncp->hash_chain);
	/*
	 * Remove ref on vnodes and cred.
	 */
	ncp->dp = NULL;
	ncp->vp = NULL;
	cred = ncp->cred;
	ncp->cred = NOCRED;

	/*
	 * Insert at head of LRU list (first to grab).
	 */
	nc_inslru(ncp, (ncache_t *)lrup);
	
	/*
	 * And make a dummy hash chain.
	 */
	NULL_HASH(ncp);
	ncp->hash_chain = NULL;
	LRU_UNLOCK(lrup, s);

	/*
	 * Put this off till after we've released lru_lock -- it
	 * can take a while, and could sleep on cellular systems.
	 */
	if (cred != NOCRED)
		crfree(cred);
}

/*
 * Utility routine to search for a cache entry.
 * Must be called with the hash chain already locked.
 */
static ncache_t *
dnlc_search(
	register vnode_t *dp,
	register char *name,
	register int namlen,
	nc_hash_t *nhp,
	cred_t *cred)
{
	register ncache_t *ncp;

	NCSTATS.searches++;
retry:
	for (ncp = nhp->hash_next; ncp; ncp = ncp->hash_next) {
		if (ncp->dp == dp && ncp->namlen == namlen
		  && (cred == ANYCRED || ncp->cred == cred)
		  && *ncp->name == *name	/* fast chk 1st chr */
		  && bcmp(ncp->name+1, name+1, namlen-1) == 0) {
			if (ncp->dp_cap != dp->v_namecap ||
			    ncp->vp_cap != ncp->vp->v_namecap) {
				NCSTATS.stale_hits++;
				if (mrislocked_update(&nhp->hash_lock) ||
				    mrtrypromote(&nhp->hash_lock)) {
					dnlc_rm(ncp);
					goto retry;
				} else
					continue;  /* only held for ACCESS */
			}
			vn_trace_entry(ncp->vp, "dnlc_search hit", 
				       (inst_t *)__return_address);
			return ncp;
		}
		NCSTATS.steps++;
	}
	return NULL;
}

/*
 * Name cache hash list insertion and deletion routines.
 */
static void __inline
nc_inshash(register ncache_t *ncp, register nc_hash_t *hp)
{
	ncache_t *ncq;

	if (ncq = hp->hash_next)
		ncq->hash_prevp = &ncp->hash_next;
	ncp->hash_next = ncq;
	ncp->hash_prevp = &hp->hash_next;
	hp->hash_next = ncp;
}

static void __inline
nc_rmhash(register ncache_t *ncp)
{
	ncache_t *ncq;

	if (ncq = ncp->hash_next)
		ncq->hash_prevp = ncp->hash_prevp;
	*ncp->hash_prevp = ncq;
}

/*
 * Insert into LRU list.
 */
static void __inline
nc_inslru(register ncache_t *ncp2, register ncache_t *ncp1)
{
	register ncache_t *ncp3 = ncp1->lru_next;

	ASSERT(ncp2->lru_next == NULL);
	ASSERT(ncp2->lru_prev == NULL);
	ASSERT(ncp3->lru_prev == ncp1);
	ASSERT(ncp1->lru_prev->lru_next == ncp1);

	ncp1->lru_next = ncp2;
	ncp2->lru_next = ncp3;
	ncp3->lru_prev = ncp2;
	ncp2->lru_prev = ncp1;
}

/*
 * Remove from LRU list.
 */
static void __inline
nc_rmlru(register ncache_t *ncp)
{
	register ncache_t *next = ncp->lru_next;
	register ncache_t *prev = ncp->lru_prev;

	ASSERT(next->lru_prev == ncp);
	ASSERT(prev->lru_next == ncp);

	prev->lru_next = next;
	next->lru_prev = prev;
#ifdef DEBUG
	ncp->lru_next = ncp->lru_prev = NULL;
#endif
}
