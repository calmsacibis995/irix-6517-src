/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_dcache.h,v $
 * Revision 65.4  1998/04/02 19:43:39  bdr
 * Changed prototype for cm_ComputeOffsetInfo to match the new arg types.
 *
 * Revision 65.3  1998/03/19  23:47:23  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.2  1998/03/02  22:27:09  bdr
 * Initial cache manager changes.
 *
 * Revision 65.1  1997/10/20  19:17:23  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.86.1  1996/10/02  17:07:41  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:48  damon]
 *
 * $EndLog$
*/
/*
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_dcache.h,v 65.4 1998/04/02 19:43:39 bdr Exp $ */

#ifndef	_CM_DCACHEH_
#define _CM_DCACHEH_

/*
 * General dcache module related constants
 */
#define	DC_DEFCACHEDCACHES	100	/* Default # of dcache cache entries */
#define	DC_MINCACHEDCACHES	10	/* Minimum cached dcache entries */
#define DC_CHASHSIZE		1024	/* dchashTable hash table size */
#define DC_VHASHSIZE		1024	/* dvhashTable hash table size */
#define	DC_NULLIDX		-1	/* null index definition */
#define	DC_FHMAGIC		0x7635fab8 /*magic number for CacheInfo file */

/*
 * AFS Data cache definitions: Each entry describes a Unix file on the local 
 * disk that is is serving as a cached copy of all or part of a afs file. 
 * Entries live in circular queues for each hash table slot. Which queue is 
 * this thing in?  Good question. A struct dcache entry is in the freeDSlot 
 * queue when not associated with a cache slot. Otherwise, it is in the DLRU 
 * queue.  The freeDSlot queue uses the lruq.next field as its "next" pointer.
 *
 * Cache entries in the DLRU queue are either associated with afs files, in 
 * which case they are hashed by hvNextp and hcNextp pointers, or they are in 
 * the freeDCList  and are not associated with any file.  This last list uses 
 * the hvNextp pointer for its "next" pointer.
 *
 * Note that the DLRU and freeDSlot queues are *memory* queues, while the 
 * hvNextp/hcNextp hash lists and the freeDCList all go through the disk. Think
 * of these queues as lower-level queues caching a number of the disk entries.
 */

/* 
 * flags in cm_indexFlags array 
 */
#define	DC_IFEVERUSED	1	/* index entry has >= 1 byte of data */
#define	DC_IFFREE	2	/* index entry in freeDCList */
#define	DC_IFDATAMOD	4	/* file needs to be written out */
#define	DC_IFFLAG	8	/* utility flag */
#define DC_IFDIRTYPAGES	0x10	/* chunk may be backing writable pages */
#define	DC_IFENTRYMOD	0x20	/* has entry itself been modified? */
#define DC_IFFLUSHING	0x40	/* entry being flushed now */
#define DC_IFANYPAGES	0x80	/* if any pages are backed by this chunk */
#ifdef AFS_SUNOS5_ENV
#define DC_IFPPLOCKED	0x100	/* if locked pages waiting for a uiomove */
#endif

/*	
 * CacheItems file has a header of type struct cm_fheader (keep aligned properly) 
 */
struct cm_fheader {
    long magic;			/* Magic number for CacheItems File */
    long firstCSize;		/* Information about the first and ... */
    long otherCSize;		/* other chunk sizes is kept here */
    long spare;		       	/* as the name implies... */
};

/*	
 * kept on disk and in dcache entries 
 */
struct cm_fcache {
    short hvNextp;		/* Next in vnode hash table, or freeDCList */
    short hcNextp;		/* Next index in [fid, chunk] hash table */
    afsFid fid;		        /* Fid for this file */
    long modTime;		/* last time this entry was modified */
    afs_hyper_t versionNo;	        /* Associated data version number */
    afs_hyper_t tokenID;           /* Token associated with this chunk */
    long chunk;			/* Relative chunk number */
#ifdef AFS_VFSCACHE
    struct fid handle;		/* handle for this chunk */
#else
    long inode;			/* Unix inode for this chunk */
#endif /* AFS_VFSCACHE */
    long chunkBytes;
    long startDirty;		/* first byte dirty in chunk */
    long endDirty;		/* last byte dirty in chunk + 1 */
    unsigned short cacheBlocks;	/* 1K cache blocks held by this chunk */
    char states;		/* Has this chunk been modified? */
};

/* 
 *  cm_fcache states bits 
 */
#define	DC_DONLINE	1	/* had data token since dv matched */
#define	DC_DWRITING	8   	/* file being written (used for cache validation) */


/* 
 * kept in memory 
 */
struct cm_dcache {
    struct squeue lruq;		/* Free queue for in-memory images */
    short refCount;		/* Associated reference count. */
    short index;		/* The index in the CacheInfo file*/
#ifdef SGIMIPS
    afs_hyper_t validPos;	/* number of valid bytes during fetch */
#else  /* SGIMIPS */
    long validPos;		/* number of valid bytes during fetch */
#endif /* SGIMIPS */
    struct lock_data llock;	/* lock for dcache entry */
    short states;		/* more state bits */
    struct cm_fcache f;		/* disk image */
};

/*     
 * cm_dcache states bits
 */
#define	DC_DFNEXTSTARTED 1	/* next chunk has been prefetched already */
#define	DC_DFFETCHING	4	/* file is currently being fetched */
#define	DC_DFWAITING	8	/* someone waiting for file */
#define	DC_DFFETCHREQ	0x10	/* waiting for DFFetching to go on */
#define DC_DFFETCHINVAL	0x20	/* invalidate currently fetching chunk */
#define DC_DFSTORING	0x80	/* storing this chunk */
#define	DC_DFSTOREINVAL 0x100	/* the store action for this chunk is invalid */


/*
 * The following structure is specifically used for NCS RPC to transfer a
 * huge chunk of data via pipe.
 * Note: This data structure is referenced in cm_subr.c and cm_dcache.c
 */

#ifdef SGIMIPS
struct pipeDState {
  char *fileP;			/* file we're using */
  unsigned32 offset;			/* start (and current) offset */
  unsigned32 scode;			/* error code */
  char *bufp;			/* pointer to data buffer */
  unsigned32 bsize;			/* buffer size */
  unsigned32 nbytes;			/* bytes to transfer */
  struct cm_dcache *dcp;	/* dcp for chunk */
};
#else  /* SGIMIPS */
struct pipeDState {
  char *fileP;			/* file we're using */
  long offset;			/* start (and current) offset */
  long scode;			/* error code */
  char *bufp;			/* pointer to data buffer */
  long bsize;			/* buffer size */
  long nbytes;			/* bytes to transfer */
  struct cm_dcache *dcp;	/* dcp for chunk */
};
#endif /* SGIMIPS */

/* 
 * Macro to mark a dcache entry as bad; must set DC_IFENTRYMOD flag
 * after using this function, too.
 */
#define DC_INVALID(x)	((x)->f.fid.Unique = 0)

/* 
 * Dcache Hashing related macros
 */
#define	DC_CHASH(v, c) \
    ((((v)->Vnode+(AFS_hgetlo((v)->Volume) << 2)+(v)->Unique+(c))) \
     & (DC_CHASHSIZE-1))
#define	DC_VHASH(v)    \
    ((((v)->Vnode+(AFS_hgetlo((v)->Volume) << 2)+(v)->Unique)) \
     & (DC_VHASHSIZE-1))

/* flags for controlling cm_GetDLock */
#define CM_DLOCK_READ		0
#define CM_DLOCK_WRITE		1
#define CM_DLOCK_FETCHOK	2

/* flags for controlling cm_GetDownD */
#define CM_GETDOWND_NEEDSPACE		1	/* this GC must free space */
#define	CM_GETDOWND_ONLYDISCARDS	2	/* only GC discarded guys */
#define CM_GETDOWND_ONLYGC		4	/* garbage collection only 
						 * from the cm_TruncateDaemon.
						 * Try not to block */

/* flags for controlling cm_StabilizeDCache */
#define CM_DSTAB_MAKEIDLE	0	/* not fetching chunk or storing chunk */
#define CM_DSTAB_FETCHOK	1	/* fetching chunks ok */

/* flags for controlling cm_StoreDCache and cm_StoreAllSegments */
#define CM_STORED_ASYNC		1	/* async (don't store status) */

#define CM_STORED_FSYNC		2	/* do fsync at server */

/* Disk cache reservation unit, MUST be a power of 2 >= 1024 */
#ifdef AFS_AIX31_ENV
#define CM_DISK_CACHE_UNIT 4096
#else
#define CM_DISK_CACHE_UNIT 1024
#endif

/* 
 * Exported by cm_dcache.c 
 */
extern short cm_dvhashTable[DC_VHASHSIZE], cm_dchashTable[DC_CHASHSIZE];
extern struct lock_data cm_dcachelock;
extern int cm_GDDInProgress;
extern void cm_CacheTruncateDaemon(void);
extern void cm_MaybeWakeupTruncateDaemon(void);
extern struct lock_data cm_getdcachelock;
extern short freeDCList;
extern long freeDCCount,  *cm_indexTimes,  cm_indexCounter, cm_discardedChunks;
extern struct squeue DLRU;
extern struct cm_dcache *freeDSList, *Initial_freeDSList, **cm_indexTable;
extern long freeDSCount;
extern short *cm_indexFlags;

#ifdef SGIMIPS
extern struct cm_dcache *cm_GetDCache _TAKES((struct cm_scache *, afs_hyper_t));
#else  /* SGIMIPS */
extern struct cm_dcache *cm_GetDCache _TAKES((struct cm_scache *, long));
#endif /* SGIMIPS */
extern int cm_GetNDCaches _TAKES((struct cm_scache *, long, long));
#ifdef SGIMIPS
extern struct cm_dcache *cm_FindDCache _TAKES((struct cm_scache *,afs_hyper_t));
extern struct cm_dcache *cm_FindDCacheNL _TAKES((struct cm_scache *, 
			afs_hyper_t));
#else  /* SGIMIPS */
extern struct cm_dcache *cm_FindDCache _TAKES((struct cm_scache *, long));
extern struct cm_dcache *cm_FindDCacheNL _TAKES((struct cm_scache *, long));
#endif /* SGIMIPS */
extern struct cm_dcache *cm_GetDSlot _TAKES((long, struct cm_dcache *));
extern void cm_PutDCache _TAKES((struct cm_dcache *));
extern int cm_WriteDCache _TAKES((struct cm_dcache *, int));
extern void cm_InvalidateOneSegment _TAKES((struct cm_scache *,
					    struct cm_dcache *, int));
extern void cm_InvalidateAllSegments _TAKES ((struct cm_scache *, int));
#define CM_INVAL_SEGS_MAKESTABLE	0
#define CM_INVAL_SEGS_MARKINGBAD	1
#define CM_INVAL_SEGS_DATAONLY	2
extern void cm_GetDownD _TAKES((int,int));
extern void cm_FlushDCache _TAKES((struct cm_dcache *));
extern int cm_StoreDCache _TAKES((struct cm_scache *, struct cm_dcache *, int,
				  struct cm_rrequest *));
extern int cm_StoreAllSegments _TAKES((struct cm_scache *, int, struct cm_rrequest *));
#ifdef SGIMIPS
extern int cm_TruncateAllSegments _TAKES((struct cm_scache *, afs_hyper_t, struct cm_rrequest *));
#else  /* SGIMIPS */
extern int cm_TruncateAllSegments _TAKES((struct cm_scache *, long, struct cm_rrequest *));
#endif /* SGIMIPS */
extern void cm_TryToSmush _TAKES((struct cm_scache *, int));
extern void cm_UpdateDCacheOnLineState _TAKES((struct cm_scache *, 
	afs_token_t *, afs_token_t *));

extern void cm_SetDataMod _TAKES((struct cm_scache *, struct cm_dcache *,
				 osi_cred_t *));
extern void cm_SetEntryMod _TAKES((struct cm_dcache *));
extern void cm_StabilizeDCache _TAKES((struct cm_scache *,
				      struct cm_dcache *,
				      int));
extern int cm_SyncDCache _TAKES((struct cm_scache *, int, osi_cred_t *));
extern void cm_ConsiderGivingUp _TAKES((struct cm_scache *));
extern void cm_ClearOnlineState _TAKES((struct cm_scache *));
extern long cm_GetDOnLine _TAKES((struct cm_scache *, struct cm_dcache *,
				  afs_token_t *,  struct cm_rrequest *));
#ifdef SGIMIPS
extern void cm_ComputeOffsetInfo _TAKES((struct cm_scache *,
					 struct cm_dcache *, afs_hyper_t,
					 long *, long *));
#else  /* SGIMIPS */
extern void cm_ComputeOffsetInfo _TAKES((struct cm_scache *,
					 struct cm_dcache *, long,
					 long *, long *));
#endif /* SGIMIPS */
extern void cm_SetChunkDirtyRange _TAKES((struct cm_dcache *, long, long));

extern long cm_GetSafeCacheSize _TAKES((long));

/* some distinct errors guaranteed not to conflict with errno values */
#define CM_ERR_DIRSYNC		2001	/* unknown cookie offset for chunk */

#endif /* _CM_DCACHEH_ */
