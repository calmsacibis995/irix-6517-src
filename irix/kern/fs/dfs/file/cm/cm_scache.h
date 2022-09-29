/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_scache.h,v $
 * Revision 65.9  1998/09/23 14:34:23  bdr
 * Added the SC_MEMORY_MAPPED flag used to detect when a file has been
 * memory mapped.
 *
 * Revision 65.8  1998/08/24  16:39:31  lmc
 * Added the SC_STARTED_FLUSHING_ON and SC_STARTED_FLUSHING_OFF macros.
 *
 * Revision 65.7  1998/08/14  01:09:50  lmc
 * Add the SC_STARTED_FLUSHING flag to prevent a thread from
 * flushing within a flush, which happens when revoking tokens.
 *
 * Revision 65.6  1998/07/24  17:36:24  lmc
 * Added a flag so we would queue only 1 background thread for
 * returning tokens.  This also picks up some rw lock counters
 * for a rw locking fix.
 *
 * Revision 65.5  1998/07/02  19:17:20  lmc
 * Add a field to keep the thread owning the R/W lock.
 *
 * Revision 65.4  1998/04/02  19:43:49  bdr
 * Changed the cm_scahce structure to make the scanCookie element a afs_hyper_t
 *
 * Revision 65.3  1998/03/19  23:47:24  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.2  1998/03/02  22:27:31  bdr
 * Initial cache manager changes.
 *
 * Revision 65.1  1997/10/20  19:17:25  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.105.1  1996/10/02  17:12:35  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:55  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 *
 *	$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_scache.h,v 65.9 1998/09/23 14:34:23 bdr Exp $
 */

#ifndef	TRANSARC_CM_SCACHE_H
#define TRANSARC_CM_SCACHE_H

#include <dcedfs/xvfs_vnode.h>

#ifdef AFS_OSF_ENV
#define SCACHE_PUBLIC_POOL /* should a public vnode pool be used? */
#endif /* AFS_OSF_ENV */

/* locking rules are described in cm_scache.c */

/*
 * Status cache related macros
 */
#define	SC_HASHSIZE	128		/* stat cache hash table size */
#define	SC_CPSIZE	2		/* Num of user ids and acls on obj */


/* 
 * Some standard Vnode related attributes.
 * Note that we track modtime and change time both locally and at
 * server, since these are version numbers are well as times, and
 * if we've made a modifcation locally, the time has changed but
 * the version number hasn't.
 */
struct cm_attrs {
  long ModFlags;			/* bitmap of modified attributes */
#ifdef SGIMIPS
  afs_hyper_t Length;			/* length, in bytes */
#else  /* SGIMIPS */
  long Length;				/* length, in bytes */
#endif /* SGIMIPS */
  long serverAccessTime;		/* atime (in sec) recorded in server */
  afs_hyper_t DataVersion;		/* data version # of this file */
  afsTimeval ModTime;		        /* modification time */
  afsTimeval ChangeTime;		/* inode change time */
  afsTimeval AccessTime;		/* inode access time */
  afsTimeval ServerChangeTime;		/* change time at server */
  long Owner;				/* owner */
  long Group;				/* group owner? */
  long BlocksUsed;			/* 1K blocks used */
  unsigned short Mode;			/* unix access mode ( skipping type) */
  unsigned short LinkCount;		/* hard link count */
};

/* how far out of sync access time can drift */
#define CM_ACCESSSKEW		3600	/* one hour */

/* flags for setting locks in functions */
#define CM_LOCK_SCACHE_ENTRY	1	/* lock the scache entry for me */
#define CM_LOCK_DCACHE_ENTRY	2	/* lock the dcache entry */
#define CM_LOCK_SCACHE_LOCK	4	/* lock the global scache lock */
#define CM_LOCK_DCACHE_LOCK	8	/* lock the global dcache lock */

/* flags for merge status */
#define CM_MERGE_STORING	1	/* storing status in this call */
#define CM_MERGE_CHACL		2	/* changing ACL-related info */

/* 
 * cm_attrs ModFlags states: Items marked with (X) shouldn't be changed locally:
 * represent status at server with special semantic consistency requirements.
 */
#define CM_MODMASK	(~0)	/* mask to see if any useful flags are set */
#define CM_MODLENGTH	1	/* modified the length field locally */
#define CM_MODDV	2	/* modified data version locally (X) */
#define CM_MODMOD	4	/* modified mod time locally */
#define CM_MODCHANGE	8	/* modified change time locally */
#define CM_MODACCESS	0x10	/* modified access time locally */
#define CM_MODOWNER	0x20	/* modified owner locally */
#define CM_MODGROUP	0x40	/* modified group locally */
#define CM_MODMODE	0x80	/* modified mode locally */
#define CM_MODLINK	0x100	/* modified link count locally (X) */
#define CM_MODTRUNCPOS	0x200	/* truncpos has valid truncate position */
#define CM_MODMODEXACT	0x400	/* modtime should be set exactly */
#define CM_MODACCESSEXACT 0x800	/* access time explicitly set */

#define cm_tcmp(a,b) ((unsigned long) (a).sec < (unsigned long) (b).sec? -1 : ((unsigned long) (a).sec > (unsigned long) (b).sec? 1 : \
    ((unsigned long)(a).usec < (unsigned long) (b).usec? -1 : ((unsigned long)(a).usec > (unsigned long)(b).usec? 1 : 0))))


/*	
 * Represents name in a virtual directory enumeration; name is kludged onto the
 * end. Fields marked as "NA" are not valid in alias entries. These entries are
 * locked by their parent directory's scache llock lock.
 */
struct cm_vdirent {
    struct squeue ageq;         /* age queue for GC    */
    struct cm_vdirent *next;	/* next guy in the dir */
    long inode;			/* the virtual vnode # associated with (NA) */
    long errCode;		/* when there's no vp to return */
    struct cm_scache *vp;	/* vnode pointed to by this vdirent */
    struct cm_scache *pvp;	/* parent vnode pointer (NA) */
    u_long timeout;		/* time this info is obsolete */
    short states;		/* states bits */
    char name[1];		/* name of this element */
};

/* 
 * cm_vdirent states bits
 */
#define CM_VDIR_GOODNAME	2	/* bind status of name is known, and good */
#define CM_VDIR_BADNAME		4	/* bind status of name is known, and bad */
#define CM_VDIR_FIRSTINBLK	8	/* first vdirent in a new block */
#define CM_VDIR_BUSY		0x10	/* vdir is under construction */
#define CM_VDIR_WAITING		0x20	/* someone waiting on construction */


/*
 * cm locking structure (for Sys-V type locking)
 */
struct cm_lockf {
    int type;
#ifdef SGIMIPS
    afs_hyper_t startPos, endPos;
#else  /* SGIMIPS */
    long startPos, endPos;
#endif /* SGIMIPS */
    long sysid;
    long pid;
};

/* 
 * cm_lockf's type bits
 */
#define CM_READLOCK		1
#define CM_WRITELOCK		2
#define CM_UNLOCK		4


struct cm_localFid {  /* Fid without a cell ID */
    afs_hyper_t Volume;
    unsigned long Vnode;
    unsigned long Unique;
};

/*
 * A token list holds all of the tokens relevant to the scache
 * entry on whose list a token resides. 
 */
struct cm_tokenList {
    struct squeue q;			/* circular list of tokens */
    long refCount;			/* # of users of this entry */
    long flags;				/* various flags */
    struct cm_server *serverp;		/* server granting token, if this is
					 * not a freely granted token; otherwise
					 * this is null.  Valid only when entry
					 * is in scp's tokenList.
					 */
    afs_token_t token;		        /* a token for this file */
    struct cm_localFid tokenFid;	/* token belonging to this fid */
};

/*
 * cm_tokenList flags bits
 */
#define CM_TOKENLIST_RETURNL	1	/* return lock token when done */
#define CM_TOKENLIST_RETURNO	2	/* return open token when done */
#define CM_TOKENLIST_DELETED	4	/* free this entry when refcount is 0 */
#define CM_TOKENLIST_ASYNC	8	/* not granted yet */
#define CM_TOKENLIST_ASYNCWAIT	0x10	/* someone's waiting for async grant */
#define CM_TOKENLIST_REVALIDATE	0x20	/* token needs to be revalidated
					 * by TSR
					 */


/*
 * A chunk offset list records the offset of the first directory entry in dir
 * chunk. A dir offset entry keeps track of the server cookie value for the 
 * start of a chunk, where 0 is assumed to be the value for the first chunk. We
 * need to keep track of this, since while chunk 1 may start at our byte 65536,
 * this may have no correlation to the server file system's readdir opaque cook
 * ie at the start of that chunk.
 */
struct cm_cookieList {
    struct cm_cookieList *next;		/* next cookie in list */
    long chunk;				/* chunk# whose start cookie is cookie*/
#ifdef SGIMIPS
    afs_hyper_t cookie;			/* cookie value of this chunk's start */
#else  /* SGIMIPS */
    long cookie;			/* cookie value of this chunk's start */
#endif /* SGIMIPS */
};


#ifdef SCACHE_PUBLIC_POOL
/*
 * Header for public pool systems. Contains the few fields that
 * cache manager needs to reference.
 */
struct cm_head {
	long		v_count;		/* reference count of users */
	enum vtype	v_type;			/* vnode type */
	struct vnode *	v_vnode;		/* back pointer to vnode */
};

#endif /* SCACHE_PUBLIC_POOL */
/* 
 * Status cache entry for all remote objects (files, symlink, directories, 
 * mount points, volume roots, etc.) 
 */
struct cm_scache {
#ifdef SGIMIPS
    struct bhv_desc cm_bhv_desc;        /* inode behavior descriptor*/
    vnode_t     *vp;                    /* pointer to vnode */
#else  /* SGIMIPS */
#ifdef SCACHE_PUBLIC_POOL
    struct cm_head v;			/* stub of vnode for public pools */
#else
    struct vnode v;			/* Has reference count in v.v_count */
#endif /* SCACHE_PUBLIC_POOL */
#endif /* SGIMIPS */
    struct squeue lruq;			/* LRU queue */
    struct cm_scache *next;		/* Hash next pointer */
    afsFid fid;			        /* Entry's Unique Fid */
    struct cm_attrs m;		    	/* General status attrs fields */
    struct lock_data llock;		/* The low-level lock on the scache */
#ifdef AFS_AIX31_VM
    struct lock_data pvmlock;		/* Around aix's vm calls */
#endif
#ifdef AFS_SUNOS5_ENV
    krwlock_t rwlock;			/* serializes write requests */
#endif
#ifdef SGIMIPS
    mrlock_t	rwlock;			/* Read/write lock */
    long 	rwl_owner;		/* Last or current owner 
						of read/write lock */
    int		rwl_count;		/* Number of writers recursively
						locked, 1 or 0 */
    int		rl_count;		/* Number of readers locked
						with write lock. 1 or 0 */
    int		rw_demote;		/* We promoted the lock, demote
					   when unlocking for write. 1 or 0*/
#endif /* SGIMIPS */
    union {
	afsFid *parentFidp;	        /* parent dir for the root volume */
	afsFid *rootFidp;	        /* root vol dir for the mount point */
    } pfid;
    char *linkDatap;			/* Link data for a symlink. */
    struct cm_volume *volp;		/* volume pointer for this object */
    osi_cred_t *writerCredp;		/* last writer */
#ifdef AFS_SUNOS5_ENV
    osi_cred_t *NFSCredp;		/* NFS credential */
#endif
    struct cm_vdirent *vdirList;	/* list of virtual dir elts */
    struct cm_vdirent *parentVDIR;	/* parent vdir,if any(for path eval) */
    afs_hyper_t cookieDV;		        /* data version of dir cookie list */
    struct squeue tokenList;		/* token list for this entry */
    struct squeue lockList;		/* list of flocks held on this vnode */
    struct cm_cookieList *cookieList;	/* cookie list for dirs */
    struct cm_ds {			/* hints for readdir scan */
	long scanOffset;		/* chunk off. we expect to read next */
#ifdef SGIMIPS
	afs_hyper_t scanCookie;		/* cookie pos we expect to read next */
#else  /* SGIMIPS */
	long scanCookie;		/* cookie pos we expect to read next */
#endif /* SGIMIPS */
	long scanChunk;			/* applicable chunk (-1 == invalid) */
    } ds;

    /*
    Support for bypassing prefetch during random I/O operations.
    Keep track of the next expected unix page. Set the seq access
    flag for seq access and clear it for random access. If the
    current page doesn't equal the expected page, then we have
    random access. Only need to look at the first two pages. As
    soon as we know, set the set_prefetch flag to avoid looking
    at more pages. Also keep track of next chunk to prefetch. We
    support prefetching an arbitrary number of chunks for high
    performance sequential read.
    */

    struct seq_hint
    {
    	u_long next_page;	/* next expected unix page */
	short seq_access_flag;	/* set for seq access */
	short set_prefetch;	/* set for known access type */
    	long next_chunk;	/* use for hot prefetch */
    }
    seq_hint;


    afs_hyper_t bulkstatdiroffset;

    long anyAccess;			/* System:AnyUser's access to this.*/
    long storeFailTime;			/* time store first failed */
    struct cm_aclent *randomACL;	/* random ACL cache */
    u_long kaExpiration;	        /* when its keep-alive expires */
    long dirRevokes;			/* counter bumped on every dir data token revoke;
					 * used by dir caching code to handle races.
					 */
    long modChunks;			/* count of dirty chunks in dcache */
#ifdef SGIMIPS
    afs_hyper_t truncPos;		/* last position truncated to */
#else  /* SGIMIPS */
    long truncPos;			/* last position truncated to */
#endif /* SGIMIPS */
    long states;			/* state bits */
    long devno;				/* device #, if special dev. vnode */
    short opens;			/* # of opens, reads or writes. */
    short writers;			/* # of open/writers */
    short readers;			/* # of open/reads */
    short shareds;			/* # of open/shared (and execs) */
    short exclusives;			/* # of open/exclusives */
    short storeCount;			/* # of stores w/o SC_STORING set */
    short fetchCount;			/* # of chunks being fetched */
    short activeRevokes;		/* # of active revokes now executing */

    /* use cm_scachelock to protect this field */

    short installStates;		/* state bits for bulkstat */
    /*
     * Notes about asyncStatus
     *
     * It is unsigned so that ESTALE won't look negative.
     *
     * To modify it, one must hold both the scache entry's llock AND
     * the global cm_scachelock, write-locked.  To read it, a read lock on
     * either of those locks is sufficient.
     */
    u_char  asyncStatus; 		/* repository for rpc error */
    u_char  storeFailStatus;		/* repository for soft rpc error */
};

/* 
 * scache states bits 
 */
#define	SC_STATD	1		/* has this file ever been stat'd? */
#define	SC_RO		2		/* is it on a read-only volume */
#define	SC_MVALID	4		/* is the mount point info valid? */
/* 0x08 used to be SC_DATAMOD */
#ifdef AFS_SUNOS5_ENV
#define SC_PPLOCKED	0x10		/* someone has locked pages */
#define SC_PPLWAITING	0x20		/* waiting for PPLOCKED to clear */
#endif
#define	SC_MOUNTPOINT	0x40		/* Mount point for a volume */
#define	SC_VOLROOT	0x80		/* Root of volume entry */
#define	SC_RETURNOPEN	0x100		/* Return open tokens on close */
#define	SC_VDIR		0x200		/* A virtual BIND directory */
#define	SC_CELLROOT	0x400		/* this dir is a cell root. */
#define SC_SPECDEV	0x800		/* this vnode is a spec. dev, socket or fifo */
#define SC_STOREFAILED	0x1000		/* failed to store the data */
#define SC_STORING	0x2000		/* storing status back */
#define SC_FETCHING	0x4000		/* fetching status now */
#define SC_WAITING	0x8000		/* waiting for fetch or store flag */
#define SC_REVOKEWAIT	0x10000		/* waiting for active revokes to drop */
#define SC_HASPAGES	0x20000		/* has VM pages (RO or RW) to flush */
/* looks like 0x40000 is unused */
#define SC_MARKINGBAD	0x80000		/* in markbadscache, don't recurse */
#define SC_BADSPECDEV	0x100000	/* device but maj/min not supported on this arch */
#define SC_STOREINVAL	0x200000	/* SC_STORING action is invalidated */
#define SC_RETURNREF	0x400000	/* return token when dropping last ref */
#define SC_LENINVAL	0x800000	/* file's real length > 2^31-1 */
#ifdef SGIMIPS
#define SC_STARTED_RETURNREF 0x01000000	/* background for return token 
					 * when dropping last ref started*/
#define SC_STARTED_FLUSHING 0x02000000	/* Someone has started a flush on
					 * this scp, don't start another one. */
#define SC_MEMORY_MAPPED 0x04000000	/* File has been memory mapped */
#endif /* SGIMIPS */

/* flags for scache installStates */

#define BULKSTAT_INSTALLEDBY_BULKSTAT	1 /* installed by bulkstat */
#define BULKSTAT_SEEN			2 /* hit by lookup */

/* scache statistics for bulkstat */

struct sc_stats
{
    /* entered by bulkstat, hit by lookup */

    unsigned long statusCacheBulkstatSeen;

    /* entered by bulkstat, never referrenced */

    unsigned long statusCacheBulkstatNotseen;

    /* entered by bulkstat */

    unsigned long statusCacheBulkstatEntered;
}
;

#ifdef SGIMIPS
/*
 *   Macros for setting SC_STARTED_FLUSHING flag.
 *	If its already set, return 1.
 */
#define SC_STARTED_FLUSHING_ON(scp,rc)		\
    rc=0;					\
    lock_ObtainWrite(&(scp)->llock);		\
    if ((scp)->states & SC_STARTED_FLUSHING) {	\
	rc=1;  					\
    } else {					\
        (scp)->states |= SC_STARTED_FLUSHING;   \
    }						\
    lock_ReleaseWrite(&(scp)->llock);

#define SC_STARTED_FLUSHING_OFF(scp)	\
    lock_ObtainWrite(&(scp)->llock);	\
    (scp)->states &= ~SC_STARTED_FLUSHING; \
    lock_ReleaseWrite(&(scp)->llock);
#endif /* SGIMIPS */


/*
 * Quick and dirty manipulation of the vnode's reference count; not to be
 * confused with CM_HOLD and CM_RELE, defined below.  The following macros
 * examine and manipulate the vnode's reference count without any locking.
 * So far, we have gotten away with this, even on Solaris, where it is supposed
 * to be necessary to hold the vnode's v_lock.  If this works, it is for the
 * following reasons:
 * - CM_RC assumes that cm_scachelock is held, and so the reference count
 *   will not be raised by any of the usual ops, such as VFS_VGET, VFS_ROOT,
 *   VOP_LOOKUP, or VOP_CREATE.  If the reference count is low, there are no
 *   references, and the only references until cm_scachelock is dropped will
 *   be from the fsflush daemon.
 * - If CM_RC finds a high reference count, this result is only considered to
 *   be advisory.
 * - Even cm_scachelock is not sufficient to allow CM_RAISERC and CM_LOWERRC
 *   to be safely used if there is any possibility that the fsflush daemon
 *   will fiddle with the reference count.  These macros are called only from
 *   cm_FlushSCache, in which it is assumed that all dirty pages were already
 *   cleaned (by cm_close), and so the fsflush daemon will not get involved.
 * These macros are potentially platform-dependent, but to date, we are using
 * the same definitions for all platforms.
 */
#ifdef SGIMIPS
#define CM_RC(scp)              ((scp)->vp->v_count)
#define CM_SETRC(scp, n)        (scp)->vp->v_count = (n)
#define CM_RAISERC(scp)         (scp)->vp->v_count++
#define CM_LOWERRC(scp)         (scp)->vp->v_count--

#define SCTOV(scp)      ((scp)->vp)
/*#define VTOSC(vp)     ((struct cm_scache *) VNODE_TO_FIRST_BHV(vp)) */
#define VTOSC(vp, scp) \
{                       \
        bhv_desc_t *bdp; \
        bdp = bhv_lookup_unlocked(VN_BHV_HEAD(vp),xvfs_ops); \
        ASSERT(bdp != NULL); \
        scp = (struct cm_scache *)bdp; \
}
#else  /* SGIMIPS */
#define CM_RC(scp)		((scp)->v.v_count)
#define CM_SETRC(scp, n)	(scp)->v.v_count = (n)
#define CM_RAISERC(scp)		(scp)->v.v_count++
#define CM_LOWERRC(scp)		(scp)->v.v_count--
#endif /* SGIMIPS */


#ifndef SCACHE_PUBLIC_POOL
#ifndef SGIMIPS 
#define SCTOV(scp) ((struct vnode *) (scp)) 	/* convert scache to vnode; cheat
						 * based on vnode being first field
						 */

#define VTOSC(vp) ((struct cm_scache *) (vp))  /* convert vnode to scache; currently cheats */
#endif /* !SGIMIPS */

/*
 * Increment, Decrement Reference counts on cm_scache
 */
#define CM_HOLD(scp)		OSI_VN_HOLD(SCTOV(scp))
#define CM_RELE(scp)		OSI_VN_RELE(SCTOV(scp))
#else
#define SCTOV(scp) ((scp)->v.v_vnode)		/* convert scache to vnode */
#define VTOSC(vp)  (*(struct cm_scache **)(vp)->v_data) /* convert vnode to scache */
/*
 * Increment, Decrement Reference counts on cm_scache
 */
#define CM_HOLD(scp)	((scp)->v.v_count++)
#define CM_RELE(scp)	(osi_assert((scp)->v.v_count>0), (scp)->v.v_count--)
extern struct vnode *cm_get_vnode(struct cm_scache *);
#endif /* SCACHE_PUBLIC_POOL */

/*
 * Some "internal" scache struct defines
 */
#define segid		v.v_gnode->gn_seg /* segment ID for AIX 3.1 VM */
#define	rootDotDot	pfid.parentFidp	/* ".." fid of a volume */
#define	mountRoot	pfid.rootFidp	/* root of volume for mount point */


/* 
 * don't hash on the cell, our callback-breaking code sometimes fails to
 * compute the cell correctly, and only scans one hash bucket
 */
#define	SC_HASH(fid) ((AFS_hgetlo((fid)->Volume) + (fid)->Vnode) \
		      & (SC_HASHSIZE-1))

/*
 * do lots of address arithmetic to go from lruq to the base of the scache 
 * structure. Don't move struct vnode, since we think of a struct scache as a 
 * specialization of a struct vnode 
 */
#define	SC_QTOS(e)	((struct cm_scache *)(((char *) (e)) - (((char *) \
			(&(((struct cm_scache *)(e))->lruq))) - ((char *)(e)))))

/* flags for cm_GetSLock */
#define CM_SLOCK_READ		0	/* need to read status */
#define CM_SLOCK_WRITE		1	/* need to update status */
#define CM_SLOCK_WRDATAONLY	2	/* need to update data chunk only */

/*
 * Deciding what to do with an error in storing back to the server
 *
 * Perhaps CM_ERROR_TRANSIENT and CM_ERROR_TIME_LIMITED should include
 * EIO and/or ENFILE?
 */
#ifdef DFS_EDQUOT_MISSING
#define CM_ERROR_TRANSIENT(e) ((e) == ETIMEDOUT || (e) == ENOSPC \
				|| (e) == EINTR)
#else
#define CM_ERROR_TRANSIENT(e) ((e) == ETIMEDOUT || (e) == ENOSPC \
				|| (e) == EDQUOT || (e) == EINTR)
#endif
#define CM_ERROR_NOT_CLOSE_LIMITED(e) ((e) == ETIMEDOUT)
#define CM_ERROR_TIME_LIMITED(e) ((e) == ETIMEDOUT || (e) == EINTR)
/* 
 * Exported by cm_scache.c 
 */
extern struct cm_scache *cm_shashTable[SC_HASHSIZE];
extern struct lock_data cm_scachelock;
extern struct squeue SLRU;
extern struct cm_scache *freeSCList, *Initial_freeSCList;
extern long cm_scount;
extern struct sc_stats sc_stats;

extern struct cm_scache *cm_NewSCache _TAKES((afsFid *, struct cm_volume *));
extern int cm_GetSCache _TAKES((afsFid *, struct cm_scache **, struct cm_rrequest *));
extern struct cm_scache *cm_FindSCache _TAKES((afsFid *));
#if CM_MAXFIDSZ<24
extern struct cm_scache *cm_NFSFindSCache _TAKES((afsFid *));
#endif
extern struct cm_scache *cm_GetRootSCache _TAKES((afsFid *, struct cm_rrequest *));
extern int cm_MarkTime _TAKES((struct cm_scache *, long, osi_cred_t *,
			       long, int));
extern void cm_PutSCache _TAKES((struct cm_scache *));
extern void cm_HoldSCache _TAKES((struct cm_scache *));
extern int cm_StoreSCache _TAKES((struct cm_scache *, afsStoreStatus *,
				 int, struct cm_rrequest *));
extern long cm_SyncSCache _TAKES((struct cm_scache *));
extern void cm_ResetAccessCache _TAKES((long));
extern void cm_MarkBadSCache _TAKES((struct cm_scache *, long));
extern int cm_NeedStatusStore _TAKES((struct cm_scache *));
extern void cm_ScanStatus _TAKES((struct cm_scache *, afsStoreStatus *));
extern int cm_IdleSCaches _TAKES((void));
extern void cm_ClearScan _TAKES((struct cm_scache *));

/* 
 * Exported by cm_vdirent.c 
 */
extern void cm_FreeAllVDirs _TAKES((struct cm_scache *));

#endif /* TRANSARC_CM_SCACHE_H */
