/*
 * Copyright (c) 1992, Sun Microsystems, Inc.
 * All rights reserved.
#pragma ident   "@(#)cachefs_cnode.c 1.75     94/04/21 SMI"
 */


#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/uio.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>
#include <sys/buf.h>
#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include "cachefs_fs.h"
#include <sys/buf.h>
#include <sys/pfdat.h>
#include <sys/dnlc.h>
#include <sys/kthread.h>
#include <ksys/vproc.h>

extern vnodeops_t cachefs_vnodeops;

/*
 * Functions for cnode management.
 */

void
cachefs_addfidhash(struct cnode *cp)
{
	struct fscache *fscp = C_TO_FSCACHE(cp);
	u_int hash;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_addfidhash: ENTER cp 0x%p\n", cp));

	ASSERT(VALID_ADDR(cp));
	ASSERT((cp->c_frontfid.fid_len > 0) &&
		(cp->c_frontfid.fid_len <= MAXFIDSZ));
	ASSERT(!cfind_by_fid(&cp->c_frontfid, fscp));
	hash = fidhash(&cp->c_frontfid, CNODE_BUCKET_SIZE);
	cp->c_hash_by_fid = fscp->fs_cnode_by_fid[hash];
	fscp->fs_cnode_by_fid[hash] = cp;
#ifdef DEBUG
	cp->c_infidhash = 1;
#endif /* DEBUG */
	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_addfidhash: EXIT\n"));
}

void
cachefs_remfidhash(struct cnode *cp)
{
	struct cnode **headpp;
	struct fscache *fscp = C_TO_FSCACHE(cp);
	int found = 0;
	u_int hash;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_remfidhash: ENTER cp 0x%p\n", cp));

	ASSERT(VALID_ADDR(cp));
	ASSERT(cp->c_frontfid.fid_len || (!cp->c_hash_by_fid && !cp->c_infidhash));
	if (cp->c_frontfid.fid_len) {
		ASSERT(cp->c_frontfid.fid_len <= MAXFIDSZ);
		hash = fidhash(&cp->c_frontfid, CNODE_BUCKET_SIZE);
		for (headpp = &fscp->fs_cnode_by_fid[hash];
			*headpp != NULL; headpp = &(*headpp)->c_hash_by_fid) {
			if (*headpp == cp) {
				*headpp = cp->c_hash_by_fid;
				cp->c_hash_by_fid = NULL;
				found++;
				fscp->fs_hashvers++;
				break;
			}
		}
		CFS_DEBUG(CFSDEBUG_CNODE, if (!found)
			printf("cachefs_remfidhash: cnode 0x%p not found in fid hash\n",
				cp));
	}
#ifdef DEBUG
	cp->c_infidhash = 0;
#endif /* DEBUG */

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_remfidhash: EXIT\n"));
}

/*
 * Add a cnode to the hash queues.
 */
void
cachefs_addhash(struct cnode *cp)
{
	struct fscache *fscp = C_TO_FSCACHE(cp);
	u_int hash = fidhash(cp->c_backfid, CNODE_BUCKET_SIZE);

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_addhash: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp->c_backfid));
	ASSERT(VALID_ADDR(cp));
	ASSERT(cp->c_hash == NULL);
	ASSERT(!cp->c_inhash);
	ASSERT(!cfind(cp->c_backfid, fscp));
	cp->c_hash = fscp->fs_cnode[hash];
	fscp->fs_cnode[hash] = cp;
	if (cp->c_frontfid.fid_len) {
		cachefs_addfidhash(cp);
		ASSERT(cp->c_infidhash);
#ifdef DEBUG
	} else {
		ASSERT(!C_CACHING(cp));
#endif /* DEBUG */
	}
	fscp->fs_hashvers++;
#ifdef DEBUG
	cp->c_inhash = 1;
#endif /* DEBUG */

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_addhash: EXIT\n"));
}

/*
 * Remove a cnode from the hash queues
 */
void
cachefs_remhash(struct cnode *cp)
{
	struct cnode **headpp;
	struct fscache *fscp = C_TO_FSCACHE(cp);
	int found = 0;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_remhash: ENTER cp 0x%p\n", cp));

	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cp->c_backfid));
	ASSERT(cp->c_backfid->fid_len <= MAXFIDSZ);
	for (headpp = &fscp->fs_cnode[fidhash(cp->c_backfid, CNODE_BUCKET_SIZE)];
		*headpp != NULL; headpp = &(*headpp)->c_hash) {
		if (*headpp == cp) {
			ASSERT(cp->c_inhash);
			*headpp = cp->c_hash;
			cp->c_hash = NULL;
			found++;
			fscp->fs_hashvers++;
#ifdef DEBUG
			cp->c_inhash = 0;
#endif /* DEBUG */
			break;
		}
	}
	cachefs_remfidhash(cp);
	ASSERT(!cp->c_inhash && !cp->c_infidhash);
	ASSERT((cp->c_flags & CN_DESTROY) || !cfind(cp->c_backfid, fscp));
	ASSERT(!cp->c_frontfid.fid_len ||
		(cfind_by_fid(&cp->c_frontfid, fscp) != cp));

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_remhash: EXIT\n"));
}

/*
 * Search for the cnode on the hash queues hanging off the fscp struct.
 * On success, returns 0 and *cpp is set to the cnode.
 */
cnode_t *
cfind(fid_t *key, struct fscache *fscp)
{
	struct cnode *head;
#ifdef DEBUG
	int found = 0;
#endif
	cnode_t *cp = NULL;
	u_int hv;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cfind: ENTER fscp 0x%p\n", fscp));

	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(key));
	ASSERT(key->fid_len <= MAXFIDSZ);
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cfind, current_pid(), key, fscp);
	hv = fidhash(key, CNODE_BUCKET_SIZE);
	for (head = fscp->fs_cnode[hv]; head != NULL; head = head->c_hash) {
		/*
		 * cnodes marked CN_DESTROY may be in the hash.  They are removed
		 * by fscache_sync.
		 */
		if ( !(head ->c_flags & CN_DESTROY) &&
			FID_MATCH( head->c_backfid, key ) ) {
#ifdef DEBUG
				CFS_DEBUG(CFSDEBUG_CNODE,
					if (found)
						printf("cfind: duplicate cnodes cp 0x%p head 0x%p\n",
							cp, head));
				ASSERT(found == 0);
				found = 1;
#endif
				cp = head;
#ifndef DEBUG
				break;
#endif
		}
	}

	ASSERT(!found || cp);
	ASSERT(found || !cp);
	ASSERT(!found || cp->c_inhash);
	ASSERT(!cp || !(cp ->c_flags & CN_DESTROY));
	CFS_DEBUG(CFSDEBUG_CNODE, printf("cfind: EXIT cp 0x%p\n", cp));
	return ( cp );
}

cnode_t *
cfind_by_fid(fid_t *fidp, fscache_t *fscp)
{
	struct cnode *head;
	u_int hv;
	cnode_t *cp = NULL;
#ifdef DEBUG
	int found = 0;
#endif

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cfind_by_fid: ENTER fidp 0x%p fscp 0x%p\n", fidp, fscp));

	ASSERT(VALID_ADDR(fscp));
	ASSERT(VALID_ADDR(fidp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cfind_by_fid, current_pid(), fidp, fscp);
	for (hv = fidhash(fidp, CNODE_BUCKET_SIZE),
		head = fscp->fs_cnode_by_fid[hv]; head != NULL;
		head = head->c_hash_by_fid) {
			/*
			 * cnodes marked CN_DESTROY may be in the hash.  They are removed
			 * by fscache_sync.
			 */
			if ( !(head ->c_flags & CN_DESTROY) &&
				FID_MATCH( &head->c_frontfid, fidp ) ) {
#ifdef DEBUG
					CFS_DEBUG(CFSDEBUG_CNODE,
						if (found)
							printf("cfind_by_fid: duplicate cnodes cp 0x%p "
								"head 0x%p\n", cp, head));
					ASSERT(found == 0);
					found = 1;
#endif
					cp = head;
#ifndef DEBUG
					break;
#endif
			}
	}

	ASSERT(!found || cp);
	ASSERT(found || !cp);
	ASSERT(!found || cp->c_inhash);
#ifdef DEBUG
	if (cp) {
		ASSERT(!(cp ->c_flags & CN_DESTROY));
		ASSERT(cp->c_inhash);
		ASSERT(C_TO_FSCACHE(cp) == fscp);
		ASSERT(cfind(cp->c_backfid, fscp) == cp);
	}
#endif
	CFS_DEBUG(CFSDEBUG_CNODE, printf("cfind_by_fid: EXIT cp 0x%p\n", cp));
	return ( cp );
}

void
cachefs_cnode_free( struct cnode *cp )
{
	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_cnode_free: ENTER cp 0x%p\n", cp));
	ASSERT(VALID_ADDR(cp));
	ASSERT(cp->c_hash == NULL);
	ASSERT(cp->c_hash_by_fid == NULL);
	ASSERT(!cp->c_inhash);
	ASSERT(!cp->c_infidhash);
	ASSERT(!cp->c_nio);
	ASSERT(!(cp->c_ioflags & (CIO_ASYNCWRITE | CIO_ASYNCREAD)));
	ASSERT(cp->c_vnode == NULL);
	ASSERT(cp->c_flags & CN_DESTROY);
	ASSERT(!cp->c_frontfid.fid_len || !C_TO_FSCACHE(cp) ||
		(cfind_by_fid(&cp->c_frontfid, C_TO_FSCACHE(cp)) != cp));
	ASSERT(cp->c_refcnt == 0);
	ASSERT(!(cp->c_flags & CN_UPDATED));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_cnode_free,
		current_pid(), cp, 0);
	/* release front and back files */
	if (cp->c_cred != NULL) {
		crfree(cp->c_cred);
	}
	if (cp->c_dcp) {
		CN_RELE(cp->c_dcp);
		cp->c_dcp = NULL;
	}
	if (cp->c_frontdirvp)
		VN_RELE(cp->c_frontdirvp);
	if (cp->c_frontvp)
		VN_RELE(cp->c_frontvp);
	if (cp->c_backvp) {
		VN_RELE(cp->c_backvp);
	}
	if ( cp->c_fileheader ) {
		CACHEFS_RELEASE_FILEHEADER(&cp->c_frontfid, cp->c_fileheader);
	} else if (cp->c_backfid) {
		CACHEFS_ZONE_FREE(Cachefs_fid_zone, cp->c_backfid);
	}
	if (cp->c_fscache) {
		CFS_DEBUG(CFSDEBUG_CNODE,
			printf("cachefs_cnode_free: fscache_rele cp 0x%p fscache 0x%p\n",
				cp, cp->c_fscache));
		fscache_rele(cp->c_fscache);
	}
	sv_destroy( &cp->c_iosv );
	spinlock_destroy( &cp->c_iolock );
	rw_destroy( &cp->c_rwlock );
	spinlock_destroy( &cp->c_statelock );
	sv_destroy( &cp->c_popwait_sv );
#if defined(DEBUG) && defined(_CNODE_TRACE)
	spinlock_destroy( &cp->c_trace.ct_lock );
#endif /* DEBUG && _CNODE_TRACE */
	bzero((caddr_t)cp, sizeof (cnode_t));
	CACHEFS_ZONE_FREE(Cachefs_cnode_zone, cp);
	(void) cachefs_cnode_cnt(-1);
	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_cnode_free: EXIT\n"));
}

static int
cachefs_get_fids( cnode_t *cp )
{
	int error = 0;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_get_fids: ENTER cp 0x%p\n", cp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_get_fids, current_pid(),
		cp, 0);
	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(cp->c_frontvp));
	VOP_FID2( cp->c_frontvp, &cp->c_frontfid, error );
	if ( !error ) {
		if (cp->c_frontdirvp) {
			ASSERT(VALID_ADDR(cp->c_frontdirvp));
			VOP_FID2( cp->c_frontdirvp, &cp->c_frontdirfid, error );
			if ( error ) {
				cp->c_frontfid.fid_len = 0;
				CACHEFS_STATS->cs_fronterror++;
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("cachefs_get_fids(line %d): error getting "
						"front dir file ID, cnode 0x%p: %d\n", __LINE__,
						cp, error));
			}
		}
	} else {
		CACHEFS_STATS->cs_fronterror++;
		CFS_DEBUG(CFSDEBUG_ERROR,
			printf("cachefs_get_fids(line %d): error getting "
				"front file ID, cnode 0x%p: %d\n", __LINE__, cp, error));
	}
	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_get_fids: EXIT, error = %d\n", error));
	return(error);
}

/*
 * Validate the allocation map in the file header against the attibutes from
 * the back file and also validate the size of the front file against the
 * allocation map and the back file attributes.
 */
int
valid_allocmap( cnode_t *cp )
{
	int ent;
	int error = 0;
	int valid = 1;
	off_t lastoff;
	vattr_t *fap = NULL;
	fileheader_t *fhp = cp->c_fileheader;
	allocmap_t *amp;
	u_short entries;

	ASSERT(VALID_ADDR(cp));
	if (!C_CACHING(cp)) {
		return(1);
	}
	ASSERT(VALID_ADDR(fhp));
	if (fhp->fh_metadata.md_state & MD_POPULATED) {
		ASSERT(VALID_ADDR(cp->c_attr));
		amp = fhp->fh_allocmap;
		entries = fhp->fh_metadata.md_allocents;
		CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)valid_allocmap,
			current_pid(), amp, entries);
		if (!cp->c_frontfid.fid_len) {
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf("valid_allocmap: file populated but no front file\n"));
			valid = 0;
		} else if (!(cp->c_flags & CN_UPDATED)) {
			fap = (vattr_t *)CACHEFS_ZONE_ZALLOC(Cachefs_attr_zone,
				KM_SLEEP);
			fap->va_mask = AT_SIZE;
			error = FRONT_VOP_GETATTR(cp, fap, 0, sys_cred);
			if (!error) {
				switch (CTOV(cp)->v_type) {
					case VDIR:
						/*
						 * If the directory data has been populated, the
						 * allocation map must contain exactly one entry.
						 */
						if ((entries != 1) || (amp->am_start_off != (off_t)0) ||
							(BTOBB(amp->am_size) != BTOBB(fap->va_size -
							(off_t)DATA_START(C_TO_FSCACHE(cp))))) {
								CFS_DEBUG(CFSDEBUG_FILEHEADER,
									printf("valid_allocmap: invalid front dir "
										"file size : %d should be %d\n",
										(int)fap->va_size,
										(int)(fhp->fh_allocmap[0].am_size +
										(off_t)DATA_START(C_TO_FSCACHE(cp)))));
								valid = 0;
						}
						break;
					case VLNK:
						if (fhp->fh_metadata.md_state & MD_NOALLOCMAP) {
							if ((entries > sizeof(fhp->fh_allocmap)) ||
								(fap->va_size != (off_t)DATA_START(
								C_TO_FSCACHE(cp)))) {
									CFS_DEBUG(CFSDEBUG_FILEHEADER,
										printf("valid_allocmap: invalid front "
											"file size (VLNK): entries %d "
											"(%d max), front file size %d\n",
											entries,
											sizeof(fhp->fh_metadata.md_allocents),
											(int)fap->va_size));
									valid = 0;
							}
						} else if (cp->c_size !=
							(fap->va_size - (off_t)DATA_START(
							C_TO_FSCACHE(cp)))) {
								CFS_DEBUG(CFSDEBUG_FILEHEADER,
									printf("valid_allocmap: invalid front file "
										"size (VLNK): %d should be %d\n",
										fap->va_size -
										(off_t)DATA_START(C_TO_FSCACHE(cp)),
										cp->c_size));
							valid = 0;
						} else if ((entries != 1) || (amp->am_start_off != 0) ||
							(amp->am_size != cp->c_size)) {
								CFS_DEBUG(CFSDEBUG_FILEHEADER,
									printf("valid_allocmap: invalid map "
										"entry 0 (VLNK): %d entries, "
										"am_start_off %d, am_size %d, "
										"c_size %d\n", entries,
										(int)amp->am_start_off,
										(int)amp->am_size, (int)cp->c_size));
								valid = 0;
								break;
						}
						break;
					case VNON:
						valid = 0;
						CFS_DEBUG(CFSDEBUG_FILEHEADER,
							printf("valid_allocmap: invalid file type\n"));
						break;
					case VREG:
					default:
						if (BTOBB(cp->c_attr->ca_size) !=
							BTOBB(fap->va_size -
							(off_t)DATA_START(C_TO_FSCACHE(cp)))) {
								CFS_DEBUG(CFSDEBUG_FILEHEADER,
									printf("valid_allocmap: invalid front file "
										"size\n"));
								valid = 0;
						} else {
							for ( lastoff = (off_t)0; entries;
								lastoff = amp->am_start_off + amp->am_size,
								entries--, amp++ ) {
									if ((amp->am_size == (off_t)0) ||
										(amp->am_start_off < (off_t)0) ||
										(lastoff && (amp->am_start_off <=
										lastoff)) ||
										(amp->am_start_off + amp->am_size - 1 >
										cp->c_attr->ca_size)) {
											CFS_DEBUG(CFSDEBUG_FILEHEADER,
												printf("valid_allocmap: "
													"invalid map entry %d, "
													"cnode 0x%p\n", 
													(int)(fhp->fh_metadata.md_allocents - entries),
													cp));
											valid = 0;
											break;
									}
							}
						}
				}
			} else {
				CACHEFS_STATS->cs_fronterror++;
				CFS_DEBUG(CFSDEBUG_ERROR,
					printf("valid_allocmap(line %d): error getting "
						"front file attributes, vnode 0x%p: %d\n", __LINE__,
						cp, error));
				valid = 0;
			}
		}
		if (fap) {
			CACHEFS_ZONE_FREE(Cachefs_attr_zone, fap);
		}
	} else if (CTOV(cp)->v_type == VREG) {
		if (fhp->fh_metadata.md_allocents > ALLOCMAP_SIZE) {
			valid = 0;
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf("valid_allocmap: bad entry count %d\n",
					fhp->fh_metadata.md_allocents));
		} else {
			for (ent = 0; ent < fhp->fh_metadata.md_allocents - 1; ent++) {
				if ((fhp->fh_allocmap[ent].am_start_off +
					fhp->fh_allocmap[ent].am_size) >
					fhp->fh_allocmap[ent + 1].am_start_off) {
						valid = 0;
						CFS_DEBUG(CFSDEBUG_FILEHEADER, printf(
							"valid_allocmap: bad allocation map, entry %d\n",
							ent));
						break;
				}
			}
		}
	}
	return(valid);
}

static int
all_zero(caddr_t addr, int size)
{
	int i;

	ASSERT(addr);
	for (i = 0; i < size; addr++, i++) {
		if (*addr != 0) {
			return(0);
		}
	}
	return(1);
}

int
valid_file_header(fileheader_t *fhp, fid_t *backfid)
{
	int valid = 1;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("valid_file_header: ENTER fhp 0x%p\n", fhp));
	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)valid_file_header, current_pid(),
		fhp, 0);
	ASSERT(VALID_ADDR(fhp));
	ASSERT(!backfid || VALID_ADDR(backfid));
	ASSERT(!backfid || (backfid->fid_len <= MAXFIDSZ));
	if (!VALID_MD_STATE(fhp->fh_metadata.md_state)) {
		valid = 0;
		CFS_DEBUG(CFSDEBUG_FILEHEADER,
			printf( "valid_file_header: bad state 0x%x\n",
				(int)fhp->fh_metadata.md_state));
	} else if (!(fhp->fh_metadata.md_state & MD_MDVALID)) {
		if (fhp->fh_metadata.md_state != MD_INIT) {
			valid = 0;
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf( "valid_file_header: MD_MDVALID not set\n"));
		} else if (fhp->fh_metadata.md_allocents != 0) {
			valid = 0;
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf(
					"valid_file_header: bad allocents value for MD_INIT: %d\n",
					fhp->fh_metadata.md_allocents));
		} else if (!all_zero((caddr_t)&fhp->fh_metadata.md_backfid,
			sizeof(fid_t))) {
				valid = 0;
				CFS_DEBUG(CFSDEBUG_FILEHEADER,
					printf( "valid_file_header: bad initial back file ID\n"));
		} else if (!all_zero((caddr_t)&fhp->fh_metadata.md_token,
			sizeof(ctoken_t))) {
				valid = 0;
				CFS_DEBUG(CFSDEBUG_FILEHEADER,
					printf( "valid_file_header: bad initial token\n"));
		}
	} else if (!(fhp->fh_metadata.md_state & MD_NOALLOCMAP) &&
		(fhp->fh_metadata.md_allocents > ALLOCMAP_SIZE)) {
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf(
					"valid_file_header: invalid allocation map entry count\n"));
			valid = 0;
	} else if ((fhp->fh_metadata.md_state & MD_NOALLOCMAP) &&
		(fhp->fh_metadata.md_allocents > sizeof(fhp->fh_allocmap))){
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf(
					"valid_file_header: invalid allocation map data length\n"));
			valid = 0;
	} else if (!(fhp->fh_metadata.md_state & MD_INIT)) {
		if (fhp->fh_metadata.md_backfid.fid_len == 0) {
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf("valid_file_header: empty back file ID\n"));
			valid = 0;
		} else if (fhp->fh_metadata.md_backfid.fid_len > MAXFIDSZ) {
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf("valid_file_header: bad back file ID length %d\n",
					fhp->fh_metadata.md_backfid.fid_len));
			valid = 0;
		} else if (fhp->fh_metadata.md_attributes.ca_type == VNON) {
			CFS_DEBUG(CFSDEBUG_FILEHEADER,
				printf("valid_file_header: invalid attribute type %d\n",
					(int)fhp->fh_metadata.md_attributes.ca_type));
			valid = 0;
		} else if (backfid &&
			!FID_MATCH(backfid, &fhp->fh_metadata.md_backfid)) {
				CFS_DEBUG(CFSDEBUG_FILEHEADER,
					printf("valid_file_header: back FID mismatch\n"));
				valid = 0;
		}
	}
	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("valid_file_header: EXIT, %s\n", valid ? "valid" : "invalid"));
	return(valid);
}

#ifdef DEBUG
/*
 * A valid cnode supplied to cachefs_initcnode adheres to the following
 * contitions.
 */
#define GOOD_CNODE(cp) \
	(!(cp)->c_flags && !(cp)->c_hash && !(cp)->c_hash_by_fid && \
		!(cp)->c_frontvp && !(cp)->c_frontdirvp && \
		!(cp)->c_backvp && VALID_ADDR((cp)->c_vnode) && \
		!(cp)->c_frontfid.fid_len && !(cp)->c_frontdirfid.fid_len && \
		!(cp)->c_backfid && !(cp)->c_fileheader && !(cp)->c_attr && \
		!(cp)->c_size && !(cp)->c_token && !(cp)->c_error && \
		!(cp)->c_nio && !(cp)->c_ioflags && !(cp)->c_cred && \
		!(cp)->c_fscache && !(cp)->c_inhash && !(cp)->c_infidhash && \
		!(cp)->c_written)
#endif /* DEBUG */

/*
 * We have to initialize the cnode contents. Fill in the contents from the
 * cache (attrcache file), from the info passed in, whatever it takes.
 */
/*
 * It is assumed that this function is only called when a cnode has just
 * been allocated from kernel memory.  Thus, no vnode will be associated
 * with the cnode upon entry to this function.  Violation of this assumption
 * by the caller will lead to memory leakage.
 * It is assumed that the caller did not initialize any part of the data
 * structure.
 */
static int
cachefs_initcnode(
	cnode_t *cp,
	cnode_t *dp,
	fid_t *backfid,
	struct fscache *fscp,
	fileheader_t *fhp,
	vnode_t *frontvp,
	vnode_t *frontdirvp,
	vnode_t *backvp,
	int flag,
	vattr_t *vap,
	cred_t *cr)
{
	int error = 0;
	vattr_t *attrs = NULL;
	vattr_t *chkattr = vap;
	vnode_t *realvp = NULL;
	int mandlock;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_initcnode: ENTER  cp 0x%p fhp 0x%p\n", cp, fhp));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_initcnode, current_pid(),
		cp, dp);
	ASSERT(VALID_ADDR(cp));
	ASSERT(!dp || VALID_ADDR(dp));
	ASSERT(VALID_ADDR(backfid));
	ASSERT(VALID_ADDR(fscp));
	ASSERT(!fhp || VALID_ADDR(fhp));
	ASSERT(!frontvp || VALID_ADDR(frontvp));
	ASSERT(!frontdirvp || VALID_ADDR(frontdirvp));
	ASSERT(!backvp || VALID_ADDR(backvp));
	ASSERT(!backvp || (backvp->v_count >= 1));
	ASSERT(!vap || VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	ASSERT(backvp || fhp);
	ASSERT(GOOD_CNODE(cp));
	ASSERT(!issplhi(getsr()));
	/*
	 * Initialize the locks, etc. first.
	 */
	rw_init(&cp->c_rwlock, "cnode serialize lock", RW_DEFAULT, NULL);
	spinlock_init(&cp->c_statelock, "cnode state lock");
	spinlock_init(&cp->c_iolock, "cnode IO lock");
	sv_init(&cp->c_iosv, SV_DEFAULT, "Async IO CV");
	sv_init(&cp->c_popwait_sv, SV_DEFAULT, "popwait");
#if defined(DEBUG) && defined (_CNODE_TRACE)
	/*
	 * If we are debugging, initiaize the trace buffer lock.
	 */
	spinlock_init(&cp->c_trace.ct_lock, "cnode trace lock");
#endif /* DEBUG && _CNODE_TRACE */
	/*
	 * point the cnode back to its fscache
	 * this is somewhat redundant as the vfs structure also points to this
	 * it is necessary, however, since cachefs_reclaim will set c_vnode to
	 * null but not necessarily throw away the cnode
	 */
	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_initcnode: fscache_hold cp 0x%p fscache 0x%p\n", cp,
			fscp));
	fscache_hold(fscp);
	cp->c_fscache = fscp;
	cp->c_flags = flag;
	if (cp->c_flags & CN_NOCACHE) {
		CACHEFS_STATS->cs_nocache++;
	}
	if (!vap) {
		if (backvp) {
			if (!fhp || (fhp->fh_metadata.md_state & MD_INIT)) {
				attrs = (vattr_t *)CACHEFS_ZONE_ALLOC(Cachefs_attr_zone,
					KM_SLEEP);
				attrs->va_mask = AT_ALL;
				NET_VOP_GETATTR_NOSTAT(backvp, attrs, 0, cr,
					C_ISFS_NONBLOCK(fscp) ? BACKOP_NONBLOCK : BACKOP_BLOCK,
					error);
				if (error) {
					CFS_DEBUG(CFSDEBUG_ERROR,
						printf("cachefs_initcnode: getattr vp 0x%p: %d\n",
							backvp, error));
					goto out;
				}
				chkattr = vap = attrs;
				ASSERT(vap->va_type != VNON);
			} else {
				vap = NULL;
				ASSERT(fhp->fh_metadata.md_attributes.ca_type != VNON);
				ASSERT(fhp->fh_metadata.md_attributes.ca_type ==
					backvp->v_type);
			}
		} else {
			ASSERT(fhp);
			vap = NULL;
			ASSERT(fhp->fh_metadata.md_attributes.ca_type != VNON);
		}
#ifdef DEBUG
	} else {
		ASSERT(vap->va_type != VNON);
#endif
	}
#ifdef DEBUG
	if (flag & CN_UPDATED) {
		ASSERT(fhp);
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_initcnode,
			cp->c_flags, fhp->fh_metadata.md_allocents);
	}
	if (flag & CN_NOCACHE) {
		CNODE_TRACE(CNTRACE_NOCACHE, cp, cachefs_initcnode, (int)cp->c_flags,
			0);
	}
#endif /* DEBUG */
	if (dp) {
		CN_HOLD(dp);
		cp->c_dcp = dp;
	}
	/*
	 * Complete the initialization of the vnode
	 */
	ASSERT(attrs || vap || fhp);
	ASSERT(cp->c_vnode);
	if (attrs) {
		cp->c_type = cp->c_vnode->v_type = attrs->va_type;
	} else if (vap) {
		cp->c_type = cp->c_vnode->v_type = vap->va_type;
	} else {
		cp->c_type = cp->c_vnode->v_type =
			fhp->fh_metadata.md_attributes.ca_type;
	}
	/*
	 * If there is a back vnode, put it into the cnode and hold it.
	 * Make sure we have the NFS vnode not some other one.
	 */
	if (backvp) {
		VOP_REALVP(backvp, &realvp, error);
		if (!error) {
			ASSERT(realvp);
			backvp = realvp;
		}
		VN_HOLD( backvp );
		cp->c_backvp = backvp;
		error = 0;
	}
	/*
	 * If there is a back vnode, put it into the cnode and hold it.  Also
	 * check for a front directory vnode.  There should not be a front dir
	 * vp if there is no front file vp.
	 */
	if (frontvp) {
		VN_HOLD(frontvp);
		cp->c_frontvp = frontvp;
		ASSERT(frontvp->v_count > 1);
		if (frontdirvp) {
			VN_HOLD(frontdirvp);
			cp->c_frontdirvp = frontdirvp;
			ASSERT(frontdirvp->v_count > 1);
		}
		/*
		 * Get the file IDs for the front file and front directory if there
		 * is one.
		 */
		error = cachefs_get_fids( cp );
		if (error) {
			goto out;
		}
	}
	cp->c_fileheader = fhp;
	CNODE_TRACE(CNTRACE_FILEHEADER, cp, cachefs_initcnode, fhp, 0);
	ASSERT(fhp || vap);
	if (!fhp) {
		cp->c_flags |= CN_NOCACHE;
		cp->c_size = vap->va_size;
		cp->c_attr = NULL;
		cp->c_token = NULL;
		cp->c_backfid = (fid_t *)CACHEFS_ZONE_ALLOC(Cachefs_fid_zone, KM_SLEEP);
		cp->c_backfid->fid_len = backfid->fid_len;
		ASSERT(backfid->fid_len <= MAXFIDSZ);
		bcopy( (void *)backfid->fid_data, (void *)cp->c_backfid->fid_data,
			backfid->fid_len );
	} else if (fhp->fh_metadata.md_state & MD_INIT) {
		cp->c_attr = &fhp->fh_metadata.md_attributes;
		cp->c_backfid = &fhp->fh_metadata.md_backfid;
		cp->c_token = &fhp->fh_metadata.md_token;
		/*
		 * Fill in the back file ID.
		 */
		fhp->fh_metadata.md_backfid.fid_len = backfid->fid_len;
		bcopy( (void *)backfid->fid_data,
			(void *)&fhp->fh_metadata.md_backfid.fid_data,
			fhp->fh_metadata.md_backfid.fid_len );
		error = CFSOP_INIT_COBJECT(fscp, cp, vap, cr);
		ASSERT(!issplhi(getsr()));
		if (error) {
			goto out;
		}
		cp->c_fileheader->fh_metadata.md_state &= ~MD_INIT;
		cp->c_fileheader->fh_metadata.md_state |= MD_MDVALID;
		cp->c_flags |= CN_UPDATED;
		CNODE_TRACE(CNTRACE_UPDATE, cp, (void *)cachefs_initcnode,
			cp->c_flags, cp->c_fileheader->fh_metadata.md_allocents);
		ASSERT(cp->c_frontvp);
		ASSERT(cachefs_zone_validate((caddr_t)cp->c_fileheader,
			FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));
		ASSERT(!backvp || (cp->c_attr->ca_type == backvp->v_type));
		ASSERT(cp->c_backfid->fid_len <= MAXFIDSZ);
		ASSERT(valid_file_header(cp->c_fileheader, NULL));
	} else {
		ASSERT(frontvp);
		/*
		 * If a file header has been supplied, then the front file existed.
		 * Assume that the supplied file header is valid.
		 */
		ASSERT(valid_file_header(cp->c_fileheader, NULL));
		/*
		 * Initialize the cached object using the attributes in the
		 * file header.  We initialize from the file header so that
		 * CFSOP_CHECK_COBJECT can validate the file contents based
		 * upon the attributes from the file header.
		 */
		cp->c_attr = &fhp->fh_metadata.md_attributes;
		cp->c_backfid = &fhp->fh_metadata.md_backfid;
		cp->c_token = &fhp->fh_metadata.md_token;
		error = CFSOP_INIT_COBJECT(fscp, cp, NULL, cr);
		ASSERT(!issplhi(getsr()));
		if (error) {
			goto out;
		}
		ASSERT(!backvp || (cp->c_attr->ca_type == backvp->v_type));
		ASSERT(cp->c_backfid->fid_len <= MAXFIDSZ);
		ASSERT(valid_file_header(cp->c_fileheader, NULL));
	}
	/*
	 * Check the object to make sure it is current.  Always do this when
	 * creating a new cnode.  If the attributes were supplied by the caller,
	 * use those (vap), otherwise, pass attrs to CFSOP_CHECK_COBJECT.  If
	 * attrs is NULL, no attributes have been retrieved from the back file.
	 * chkattr points to the attributes originally supplied by the caller
	 * or to the attributes gotten above.
	 * This cnode is not in the hash yet, so no other process can have
	 * access to it.  Thus, we use the lock mode NOLOCK to indicate that
	 * if invalidation is necessary, c_rwlock need not be acuired.
	 */
	error = CFSOP_CHECK_COBJECT(fscp, cp, chkattr, cr, NOLOCK);
	ASSERT(!issplhi(getsr()));
	if (error) {
		goto out;
	}
	/*
	 * it is assumed that vn_alloc will always succeed
	 * assert that this is the case
	 */
	ASSERT(cp->c_hash == NULL);

out:
	if (error) {
		if (cp->c_backvp) {
			VN_RELE(cp->c_backvp);
			cp->c_backvp = NULL;
		}
		if (cp->c_frontvp) {
			VN_RELE(cp->c_frontvp);
			cp->c_frontvp = NULL;
		}
		if (cp->c_frontdirvp) {
			VN_RELE(cp->c_frontdirvp);
			cp->c_frontdirvp = NULL;
		}
		if (cp->c_vnode) {
			vn_free(cp->c_vnode);
			cp->c_vnode = NULL;
		}
		cp->c_flags = 0;
	}
	if (attrs) {
		CACHEFS_ZONE_FREE(Cachefs_attr_zone, attrs);
	}
	ASSERT(error || !cp->c_fileheader ||
		valid_file_header(cp->c_fileheader, cp->c_backfid));
	ASSERT(!issplhi(getsr()));
	ASSERT(!fhp || cachefs_zone_validate((caddr_t)fhp,
		FILEHEADER_BLOCK_SIZE(fscp->fs_cache)));

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("cachefs_initcnode: EXIT error %d\n", error));
	return (error);
}

#ifdef DEBUG
/*
 * check that the file header in the cnode matches the one in the front
 * file
 */
void
validate_fileheader(cnode_t *cp, char *file, int line)
{
	fileheader_t *fhp;
	int match = 1;
	int error = 0;
	int ospl;

	ASSERT(VALID_ADDR(cp));
	ASSERT(VALID_ADDR(file));
	if (cp->c_frontvp && C_CACHING(cp) &&
	!(cp->c_flags & (CN_UPDATED | CN_UPDATE_PENDING))) {
		ASSERT(cp->c_fileheader);
		error = cachefs_read_file_header(cp->c_frontvp, &fhp,
			cp->c_attr ? cp->c_attr->ca_type : VNON,
			C_TO_FSCACHE(cp)->fs_cache);
		if (!error) {
			ospl = mutex_spinlock(&cp->c_statelock);
			if (!(cp->c_flags & (CN_UPDATED | CN_UPDATE_PENDING))) {
				if (fhp->fh_metadata.md_state !=
					cp->c_fileheader->fh_metadata.md_state) {
						printf("validate_fileheader: (file %s, line %d) fhp "
							"0x%p cp 0x%p state mismatch\n", file, line,
							fhp, cp);
						match = 0;
				} else if (fhp->fh_metadata.md_allocents !=
					cp->c_fileheader->fh_metadata.md_allocents) {
						printf("validate_fileheader: (file %s, line %d) fhp "
							"0x%p cp 0x%p allocents mismatch\n", file, line,
							fhp, cp);
						match = 0;
				} else if (bcmp(&fhp->fh_metadata.md_attributes,
					&cp->c_fileheader->fh_metadata.md_attributes,
					sizeof(vattr_t)) != 0) {
						printf("validate_fileheader: (file %s, line %d) fhp "
							"0x%p cp 0x%p attribute mismatch\n", file, line,
							fhp, cp);
						match = 0;
				} else if (bcmp(&fhp->fh_metadata.md_backfid,
					&cp->c_fileheader->fh_metadata.md_backfid,
					sizeof(fid_t)) != 0) {
						printf("validate_fileheader: (file %s, line %s) fhp "
							"0x%p cp 0x%p back FID mismatch\n", file, line,
							fhp, cp);
						match = 0;
				} else if (bcmp(fhp->fh_allocmap, cp->c_fileheader->fh_allocmap,
					sizeof(allocmap_t)) != 0) {
						printf("validate_fileheader: (file %s, line %d) fhp "
							"0x%p cp 0x%p alloc map mismatch\n", file, line,
							fhp, cp);
						match = 0;
				}
				ASSERT(match);
			}
			mutex_spinunlock(&cp->c_statelock, ospl);
		} else {
			printf("validate_fileheader: (file %s, line %d) fhp 0x%p cp "
				"0x%p error %d\n", file, line, fhp, cp, error);
		}
		ASSERT(!(fhp->fh_metadata.md_state & MD_INIT));
		CACHEFS_RELEASE_FILEHEADER(&cp->c_frontfid, fhp);
	}
}
#endif

/*
 * makecachefsnode() called from lookup (and create). It finds a cnode if one
 * exists. If one doesn't exist on the hash queues, then it allocates one and
 * fills it in calling c_initcnode().
 */
int
makecachefsnode(
	cnode_t *dp,					/* parent directory cnode */
	fid_t *backfid,					/* file ID for back file */
	struct fscache *fscp,			/* pointer to mounted file system record */
	fileheader_t *fhp,				/* pointer to file header */
									/* assumed to have already been validated */
	vnode_t *frontvp,				/* front file system vnode (cached data) */
	vnode_t *frontdirvp,			/* front file system directory vnode */
									/* non-NULL only for directories */
	vnode_t *backvp,				/* back file system vnode for the file */
									/* to which this cnode will correspond */
	vattr_t *vap,					/* vnode attributes for the new cnode */
									/* this is allowed to be NULL; if it is, */
									/* we will just get it with getattr */
	cred_t *cr,						/* credentials */
	int flags,						/* flags for cnode; normally 0, CN_ROOT */
									/* or CN_NOCACHE */
	struct cnode **cpp )			/* place to which the cnode pointer for */
									/* the new cnode is to be returned */
{
	vnode_t *realvp = NULL;
	int have_fid = 0;
	int check_cnode = 0;
	int validate = (fhp != NULL) && !(flags & CN_NOCACHE);
	cnode_t *newcp = NULL;
	u_int hashvers;
	struct cnode *cp = NULL;
	int error;
	vmap_t vmap;
	vnode_t *vp;
	int ospl;
	fid_t frontfid;
	int match_error;

	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("makecachefsnode: ENTER dcp 0x%p frontvp 0x%p, backvp 0x%p\n",
			dp, frontvp, backvp));

	ASSERT(!(flags & CN_NOCACHE) || !fhp);
	ASSERT(VALID_ADDR(cpp));
	ASSERT(!dp || VALID_ADDR(dp));
	ASSERT(!backfid || VALID_ADDR(backfid));
	ASSERT(VALID_ADDR(fscp));
	ASSERT(!fhp || VALID_ADDR(fhp));
	ASSERT(!frontvp || VALID_ADDR(frontvp));
	ASSERT(!frontdirvp || VALID_ADDR(frontdirvp));
	ASSERT(!backvp || VALID_ADDR(backvp));
	ASSERT(!vap || VALID_ADDR(vap));
	ASSERT(VALID_ADDR(cr));
	ASSERT(!frontvp || (frontvp->v_count > 0));
	ASSERT(!frontdirvp || (frontdirvp->v_count > 0));
	ASSERT(!backvp || (backvp->v_count > 0));
	ASSERT(!issplhi(getsr()));

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)makecachefsnode, current_pid(),
		dp ? CTOV(dp) : NULL, backfid);
	CACHEFS_STATS->cs_makecnode++;
restart:
	match_error = 0;
	error = 0;
	cp = NULL;
	if (fhp && !valid_file_header(fhp, backfid)) {
		CACHEFS_STATS->cs_badheader.cbh_data++;
		return(ESTALE);
	}

	mutex_enter( &fscp->fs_cnodelock );

	if (!backfid) {
		if (frontvp) {
			VOP_FID2(frontvp, &frontfid, error);
			if (!error) {
				have_fid = 1;
				/*
				 * Look up the cnode by front file ID.  If one is
				 * found, activate it.
				 */
				if (cp = cfind_by_fid(&frontfid, fscp)) {
					CFS_DEBUG(CFSDEBUG_CNODE,
						printf("makecachefsnode: cfind_by_fid 0x%p\n", cp));
					goto activate;
				}
			}
		}
		mutex_exit( &fscp->fs_cnodelock );
		return(ENOENT);
	}
lookagain:
	if ( !(cp = cfind(backfid, fscp)) ) {

		/*
		 * We will need to create a new cnode.
		 * If the cnode is to be cached (CN_NOCACHE is not set in flags) and
		 * no front vnode has been supplied, return ENOENT.
		 */
		if ( !(flags & CN_NOCACHE) && !frontvp && !backvp ) {
			CACHEFS_STATS->cs_nocnode++;
			error = ENOENT;
			cp = NULL;
		} else {
			if (!newcp) {
				CACHEFS_STATS->cs_newcnodes++;
				/*
				 * We must release the cnode mutex here to allocate the vnode.
				 * It is possible that vn_alloc will attempt to reclaim a vnode
				 * for a cachefs file.  cachefs_reclaim acquires fs_cnodelock.
				 */
				hashvers = fscp->fs_hashvers;
				mutex_exit(&fscp->fs_cnodelock);

				/* always create a new one */
				newcp = (struct cnode *)CACHEFS_ZONE_ZALLOC(Cachefs_cnode_zone,
					KM_SLEEP);
				(void) cachefs_cnode_cnt(1);
				newcp->c_vnode = vn_alloc( FSC_TO_VFS(fscp),
							  VNON, FSC_TO_VFS(fscp)->vfs_dev);
				bhv_desc_init(CTOBHV(newcp), newcp,
					      newcp->c_vnode,
					      &cachefs_vnodeops);
				vn_bhv_insert_initial(VN_BHV_HEAD(newcp->c_vnode),
					   CTOBHV(newcp));
				error = cachefs_initcnode( newcp, dp, backfid, fscp, fhp,
					frontvp, frontdirvp, backvp, flags, vap, cr );
				if (error) {
					/*
					 * An error occurred in cnode initializaion.  Make sure the
					 * file header is not freed.  The caller may want it.
					 * Make sure c_backfid is also set to NULL, otherwise,
					 * cachefs_cnode_free will try to free it.  That would be
					 * bad.
					 */
					newcp->c_fileheader = NULL;
					newcp->c_backfid = NULL;
					CNODE_TRACE(CNTRACE_FILEHEADER, newcp, cachefs_initcnode,
						NULL, 0);
					newcp->c_flags |= CN_DESTROY;
					cachefs_cnode_free( newcp );
					newcp = NULL;
					cp = NULL;
					goto out;
				}
				mutex_enter(&fscp->fs_cnodelock);
				if (hashvers != fscp->fs_hashvers) {
					CACHEFS_STATS->cs_cnodelookagain++;
					goto lookagain;
				}
			}
			/*
			 * At this point, we are assured that there is no other cnode
			 * matching the one being created now.
			 */
			cp = newcp;
			newcp = NULL;
			ASSERT(!error);
			/*
			 * No matching cnode was found, so enter the one
			 * we just created.
			 */
			ASSERT(!cfind(cp->c_backfid, C_TO_FSCACHE(cp)));
			cachefs_addhash(cp);
			/*
			 * There is one further set of validations to be
			 * performed on the file header.  The caller cannot
			 * be expected to do these as it may not have all of
			 * the necessary information (i.e., back file type and
			 * back file attributes).  The validation to be
			 * performed is on the allocation map.
			 *
			 * If the allocation map is invalid, simply invalidate
			 * the cached object.
			 *
			 * We validate here because we know that no other
			 * process has created another cnode with this identity.
			 *
			 * No lock need be acquired by cachefs_inval_object
			 * because this is the only process which can have
			 * access to this cnode at this point.
			 */
			if (validate && !valid_allocmap(cp)) {
				cachefs_inval_object(cp, NOLOCK);
			}
			ASSERT(!cp->c_frontvp || (cp->c_frontvp->v_count > 1));
			ASSERT(!cp->c_frontdirvp || (cp->c_frontdirvp->v_count > 1));
			ASSERT(!cp->c_backvp || (cp->c_backvp->v_count > 1));
			CNODE_TRACE(CNTRACE_ACT, cp, (void *)makecachefsnode,
				cp->c_flags, CTOV(cp)->v_count);
			CFS_DEBUG(CFSDEBUG_VALIDATE,
				validate_fileheader(cp, __FILE__, __LINE__));
		}
	} else {
		CFS_DEBUG(CFSDEBUG_CNODE,
			printf("makecachefsnode: found 0x%p\n", cp));
		if (newcp) {
			/*
			 * We found a cnode with a matching back file ID.
			 * We created a cnode above but were racing with
			 * another process in this creation.  The other
			 * process created the cnode first, now we must
			 * free the one we have and use the one we just
			 * found in the hash.
			 */
			ASSERT(newcp != cp);
			ASSERT(!newcp->c_inhash);
			/*
			 * If fhp points to the same data as c_fileheader
			 * in the cnode being destroyed, set fhp to NULL
			 * so that it will not be freed later.  Otherwise,
			 * we want to free both fhp and c_fileheader.
			 */
			if (fhp == newcp->c_fileheader) {
				fhp = NULL;
			}
			/*
			 * The vnode should have no other references, so we
			 * can just use vn_free to free it.
			 */
			vn_bhv_remove( VN_BHV_HEAD(newcp->c_vnode), 
				      CTOBHV(newcp) );
			vn_free( newcp->c_vnode );
			/*
			 * We must set c_vnode to NULL and turn on CN_DESTROY
			 * prior to calling cachefs_cnode_free.  It is not
			 * absolutely necessary except for debugging.  There
			 * are ASSERTs in cachefs_cnode_free which will blow
			 * if the following settings are not made.
			 */
			newcp->c_vnode = NULL;
			newcp->c_flags |= CN_DESTROY;
			cachefs_cnode_free( newcp );
			newcp = NULL;
			CACHEFS_STATS->cs_cnodetoss++;
		}
activate:
		ASSERT(cp);
		/*
		 * We found a cnode with a matching back file ID.
		 */
		ospl = mutex_spinlock( &cp->c_statelock );
		if ((flags & CN_NOCACHE) && !(cp->c_flags & CN_NOCACHE)) {
			CACHEFS_STATS->cs_nocache++;
			CNODE_TRACE(CNTRACE_NOCACHE, cp, makecachefsnode,
				(int)cp->c_flags, 0);
		}
		if (!C_CACHING(cp)) {
			flags &= ~CN_UPDATED;
		}
		cp->c_flags |= flags;
		if (flags & CN_UPDATED) {
			CNODE_TRACE(CNTRACE_UPDATE, cp, makecachefsnode,
				(int)cp->c_flags, 0);
			if (!cp->c_frontvp) {
				mutex_spinunlock( &cp->c_statelock, ospl );
				error = cachefs_getfrontvp(cp);
				if (error) {
					cachefs_nocache(cp);
					error = 0;
				}
				ospl = mutex_spinlock( &cp->c_statelock );
			}
		}
		if ( vp = CTOV(cp) ) {
			VMAP(vp, vmap);
			mutex_spinunlock( &cp->c_statelock, ospl );
			CFS_DEBUG(CFSDEBUG_VALIDATE,
				validate_fileheader(cp, __FILE__, __LINE__));
			/*
			 * Check the file ID of the front file provided by the
			 * caller against that contained in the cnode.
			 */
			if (C_CACHING(cp)) {
				if (have_fid) {
					if (!FID_MATCH(&frontfid, &cp->c_frontfid)) {
						match_error = EEXIST;
					}
				} else if (frontvp) {
					VOP_FID2(frontvp, &frontfid, error);
					if (error) {
						CFS_DEBUG(CFSDEBUG_ERROR,
						printf("makecachefsnode: error %d getting fid for "
							"vnode 0x%p\n", error, frontvp));
						cachefs_nocache(cp);
						error = 0;
					} else {
						if (!FID_MATCH(&frontfid, &cp->c_frontfid)) {
							match_error = EEXIST;
						}
					}
				}
			}
			/*
			 * The back vnode is set here if it had not already been set.
			 */
			if (backvp && !cp->c_backvp) {
				VOP_REALVP(backvp, &realvp, error);
				if (!error) {
					ASSERT(realvp);
					backvp = realvp;
				}
				VN_HOLD(backvp);
				cp->c_backvp = backvp;
				error = 0;
			}
			/*
			 * The vnode pointer in the cnode is non-null.  We've gotten
			 * the vmap for the vnode.  Now, get a reference to the vnode.
			 * The race with VOP_RECLAIM is handled in vn_get.  If NULL is
			 * returned, the vnode has been reclaimed.  The cnode data is
			 * about to be or has already been freed.  We must search the
			 * hash again.
			 */
			mutex_exit(&fscp->fs_cnodelock);
			if ( !(vp = vn_get(vp, &vmap, 0)) ) {
				CACHEFS_STATS->cs_cnoderestart++;
				CACHEFS_STATS->cs_race.cr_reclaim++;
				CFS_DEBUG(CFSDEBUG_CNODE,
					printf("makecachefsnode: NULL vn_get restart, current "
						"cnode 0x%p\n", cp));
				goto restart;
			}
			ospl = mutex_spinlock( &cp->c_statelock );
			ASSERT(cp->c_vnode == vp);
			if (cp->c_flags & CN_DESTROY) {
				mutex_spinunlock( &cp->c_statelock, ospl );
				VN_RELE(vp);
				CACHEFS_STATS->cs_cnoderestart++;
				CACHEFS_STATS->cs_race.cr_destroy++;
				CFS_DEBUG(CFSDEBUG_CNODE,
					printf("makecachefsnode: CN_DESTROY restart, current "
						"cnode 0x%p\n", cp));
				goto restart;
			}
			mutex_spinunlock( &cp->c_statelock, ospl );
			mutex_enter( &fscp->fs_cnodelock );
			ASSERT( vp->v_vfsp->vfs_fstype == cachefsfstyp );
		} else if (!(cp->c_flags & CN_RECLAIM)) {
			if (!(cp->c_flags & CN_VALLOC)) {
				cp->c_flags |= CN_VALLOC;
				mutex_spinunlock( &cp->c_statelock, ospl );
				mutex_exit(&fscp->fs_cnodelock);
				vp = vn_alloc( FSC_TO_VFS(fscp),
					cp->c_type, FSC_TO_VFS(fscp)->vfs_dev);
				ospl = mutex_spinlock( &cp->c_statelock );
				bhv_desc_init(CTOBHV(cp), cp, vp, &cachefs_vnodeops);
				vn_bhv_insert_initial(VN_BHV_HEAD(vp), CTOBHV(cp));
				cp->c_vnode = vp;
				cp->c_flags &= ~CN_VALLOC;
				sv_broadcast(&cp->c_valloc_wait);
				mutex_spinunlock( &cp->c_statelock, ospl );
				mutex_enter( &fscp->fs_cnodelock );
			} else {
				mutex_exit(&fscp->fs_cnodelock);
				sv_wait(&cp->c_valloc_wait, PZERO, &cp->c_statelock, ospl);
				/*
				 * At this point, another process has woken us, indicating
				 * that the vnode allocation has completed.  Since we
				 * had to release fs_cnodelock, we don't know what might
				 * have happened between when we entered sv_wait and now.
				 * We must start over.
				 */
				goto restart;
			}
		} else {
			mutex_spinunlock( &cp->c_statelock, ospl );
			mutex_exit(&fscp->fs_cnodelock);
			CACHEFS_STATS->cs_race.cr_reclaim++;
			CACHEFS_STATS->cs_cnoderestart++;
			CFS_DEBUG(CFSDEBUG_CNODE,
				printf("makecachefsnode: NULL vnode restart, current "
					"cnode 0x%p\n", cp));
			goto restart;
		}
		ASSERT(!(cp->c_flags & CN_RECLAIM));
		CACHEFS_STATS->cs_cnodehit++;
		check_cnode = 1;
		/*
		 * The cnode existed and was not being reclaimed.  We do not
		 * need the file header supplied by the caller.  We free it here
		 * because the only way the caller knows to free it is when an
		 * error is returned.
		 */
		if (fhp && !error) {
			if (frontvp != cp->c_frontvp) {
				if (!have_fid) {
					if (frontvp) {
						VOP_FID2(frontvp, &frontfid, error);
						CACHEFS_RELEASE_FILEHEADER(error ? NULL : &frontfid,
							fhp);
						error = 0;
					} else {
						CACHEFS_RELEASE_FILEHEADER(NULL, fhp);
					}
				} else {
					CACHEFS_RELEASE_FILEHEADER(&frontfid, fhp);
				}
			} else {
				CACHEFS_RELEASE_FILEHEADER(&cp->c_frontfid, fhp);
			}
		}
		/*
		 * Make sure the cnode for the parent has been filled in and
		 * matches the one supplied by the caller.  If it does not
		 * match, the file has been renamed, so update the cnode.
		 */
		if (dp && (cp->c_dcp != dp)) {
			if (cp->c_dcp) {
				CN_RELE(cp->c_dcp);	
			}
			CN_HOLD(dp);
			cp->c_dcp = dp;
		}
		CNODE_TRACE(CNTRACE_ACT, cp, (void *)makecachefsnode, cp->c_flags,
			CTOV(cp)->v_count);
		CFS_DEBUG(CFSDEBUG_VALIDATE,
			validate_fileheader(cp, __FILE__, __LINE__));
	}
#ifdef DEBUG
	if (!error || (error == EEXIST)) {
		ASSERT(cp);
		ASSERT(C_TO_FSCACHE(cp) == fscp);
		ASSERT(cp->c_frontfid.fid_len || (cp->c_flags & CN_NOCACHE));
		ASSERT(!cp->c_frontvp || (cp->c_frontvp->v_count > 0));
		ASSERT(!cp->c_frontdirvp || (cp->c_frontdirvp->v_count > 0));
		if (cp->c_frontvp && fscp->fs_rootvp &&
			fscp->fs_root->c_frontvp) {
				ASSERT(cp->c_frontvp->v_vfsp ==
					fscp->fs_root->c_frontvp->v_vfsp);
		}
		if (cp->c_backvp) {
			if (fscp->fs_rootvp && fscp->fs_root->c_backvp) {
				ASSERT(cp->c_backvp->v_vfsp ==
					fscp->fs_root->c_backvp->v_vfsp);
			}
			ASSERT(cp->c_backvp->v_count > 0);
			ASSERT(CTOV(cp)->v_type == cp->c_backvp->v_type);
			ASSERT(!fhp || (cp->c_attr->ca_type == cp->c_backvp->v_type));
		}
		ASSERT(!(cp->c_flags & CN_DESTROY));
		ASSERT(!cp->c_fileheader ||
			!(cp->c_fileheader->fh_metadata.md_state & MD_INIT));
		ASSERT(cfind(cp->c_backfid, C_TO_FSCACHE(cp)) == cp);
		ASSERT(!cp->c_fileheader ||
			valid_file_header(cp->c_fileheader, backfid));
		ASSERT(!cp->c_fileheader ||
			find_fileheader_by_fid(&cp->c_frontfid, cp->c_fileheader));
	}
	ASSERT(!issplhi(getsr()));
#endif
	mutex_exit( &fscp->fs_cnodelock );
	if (check_cnode) {
		/*
		 * We check the cached object here, primarily to make sure it
		 * is not stale.  If we get an error, destroy the cnode and
		 * return the error to the caller.  Set CN_DESTROY before
		 * VN_RELE so that cachefs_inactive will see CN_DESTROY.
		 * Do this after releasing fs_cnodelock to prevent a double
		 * trip of the mutex.
		 */
		error = CFSOP_CHECK_COBJECT(fscp, cp, vap, cr, UNLOCKED);
		if (error) {
			ospl = mutex_spinlock( &cp->c_statelock );
			cp->c_flags |= CN_DESTROY;
			mutex_spinunlock( &cp->c_statelock, ospl );
			/*
			 * fs_cnodelock must be released here because VN_RELE may
			 * go through cachefs_inactive, where fs_cnodelock will
			 * be acquired.
			 */
			ASSERT(CTOV(cp));
			VN_RELE(CTOV(cp));
			cp = NULL;
		}
	}
out:
	ASSERT(!(error && (error != EEXIST)) || !cp);
	ASSERT(!(!error || (error == EEXIST)) || cp);
	ASSERT(!newcp);
	*cpp = cp;
	CFS_DEBUG(CFSDEBUG_CNODE,
		printf("makecachefsnode: EXIT cp 0x%p error %d\n", *cpp, error));

	return (error ? error : match_error);
}

/*
 * Adjusts the number of cnodes by the specified amount and
 * returns the new number of cnodes.
 */
int cachefs_cnode_count = 0;
lock_t cachefs_cnode_cnt_lock;

int
cachefs_cnode_cnt(int delta)
{
	int old_spl;

	CACHEFUNC_TRACE(CFTRACE_OTHER, (void *)cachefs_cnode_cnt, current_pid(),
		delta, 0);
	/* grab lock */
	old_spl = mutex_spinlock( &cachefs_cnode_cnt_lock );

	/* adjust count, check for error */
	cachefs_cnode_count += delta;
	ASSERT(cachefs_cnode_count >= 0);

	/* free lock */
	mutex_spinunlock( &cachefs_cnode_cnt_lock, old_spl );

	/* return new value of count */
	return (cachefs_cnode_count);
}
