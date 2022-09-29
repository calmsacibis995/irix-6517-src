/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: cm_vcache.h,v $
 * Revision 65.2  1998/03/02 22:27:49  bdr
 * Initial cache manager changes.
 *
 * Revision 65.1  1997/10/20  19:17:27  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.76.1  1996/10/02  17:13:13  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:06:04  damon]
 *
 * $EndLog$
 */
/* Copyright (C) 1989, 1994 Transarc Corporation - All rights reserved */

#ifndef _CM_VCACHE_INCL_ENV__
#define _CM_VCACHE_INCL_ENV__ 1

extern long cm_cacheDirty, cm_maxCacheDirty;
extern int cm_diskless;

#define CM_DCACHE_TYPE_NFS	1
#define CM_DCACHE_TYPE_UFS	2
#define CM_DCACHE_TYPE_MEM	3
#define CM_DCACHE_TYPE_EPI	4
#define CM_MAXCACHETYPES	4	/* a reasonable number, no doubt */

/*
 * Functions exported by a cache type 
 */
struct cm_cacheOps {
    char *(*open)();
    int (*truncate)();
    int (*rdwr)();
    int (*close)();
#ifdef SGIMIPS
    int (*quick_trunc)();
#endif /* SGIMIPS */
};


/* 
 * Arguments passed by cmd 
 */
struct cm_cacheParams {
    long cacheSCaches;
    long cacheFiles;
    long cacheBlocks;
    long cacheDCaches;
    long cacheVolumes;
    long chunkSize;
    long setTimeFlag;
    long memCacheFlag;
    long spare1;
    long spare2;
};

extern void cm_InitMemCache _TAKES((long, long));
extern int cm_RegisterCacheType _TAKES((struct cm_cacheOps *));
extern void cm_SetCacheType _TAKES((int));
#ifdef AFS_VFSCACHE
extern char *cm_CFileOpen _TAKES((struct fid *));
#else
extern char *cm_CFileOpen _TAKES((long));
#endif /* AFS_VFSCACHE */
extern int cm_CFileTruncate _TAKES((char *, long));
extern int cm_CFileClose _TAKES((char *afile));
extern int cm_CFileRW _TAKES((char *, enum uio_rw, char *, long, long,
			      osi_cred_t *));
extern int cm_CFileRDWR _TAKES((char *, enum uio_rw, struct uio *, osi_cred_t *));
#ifdef SGIMIPS
extern int cm_MemCacheQuickTrunc _TAKES((struct fid *, long));
#endif /* SGIMIPS */

#endif

