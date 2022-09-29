/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_vcache.c,v 65.8 1998/06/22 15:02:39 bdr Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright (C) 1996, 1990 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>			/* Should be always first */
#include <cm.h>				/* Cm-based standard headers */
#include <cm_vcache.h>
#include <cm_stats.h>
#ifdef SGIMIPS
#include <dcedfs/osi_ufs.h>
#endif /* SGIMIPS */

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_vcache.c,v 65.8 1998/06/22 15:02:39 bdr Exp $")

/*
 * This file contains the virtual cache operations; these are not vnode-like in 
 * that the functions to call are embedded here. Enough is enough, as they say. We
 * should virtualize them sometime, perhaps, if there really are a lot of different
 * types of cache files that people want to insinuate into AFS 4.
 */

/* generic cache type info */
static int cm_maxCacheType=0;
int cm_diskless = 0;
static int cm_cacheType = -1;
static struct cm_cacheOps *cm_cacheTypes[CM_MAXCACHETYPES];

#ifdef SGIMIPS
int cm_RegisterCacheType(register struct cm_cacheOps *aops) 
#else  /* SGIMIPS */
int cm_RegisterCacheType(aops)
    register struct cm_cacheOps *aops; 
#endif /* SGIMIPS */
{
    register int slot;

    if (cm_maxCacheType >= CM_MAXCACHETYPES-1)
	return -1;
    cm_cacheTypes[slot = (cm_maxCacheType++)] = aops;
    return slot;
}


#ifdef SGIMIPS
void cm_SetCacheType(register int aslot)
#else  /* SGIMIPS */
void cm_SetCacheType(aslot)
    register int aslot; 
#endif /* SGIMIPS */
{
    if (aslot >= 0 && aslot < cm_maxCacheType) 
	cm_cacheType = aslot;
    else 
	panic("bogus setcachetype");
}

#ifdef AFS_VFSCACHE
#ifdef SGIMIPS
char *cm_CFileOpen(struct fid * handle)
#else  /* SGIMIPS */
char *cm_CFileOpen(handle)
struct fid * handle;
#endif /* SGIMIPS */
{
    /* return a generic file structure */
    return (*(cm_cacheTypes[cm_cacheType]->open))(handle);
}
#else
char *cm_CFileOpen(ainode)
    register long ainode; 
{
    return (*(cm_cacheTypes[cm_cacheType]->open))(ainode);
}
#endif /* AFS_VFSCACHE */


#ifdef SGIMIPS
int cm_CFileTruncate(
    register char *afile, 
    register long asize)
#else  /* SGIMIPS */
int cm_CFileTruncate(afile, asize)
    register long asize;
    register char *afile; 
#endif /* SGIMIPS */
{
    return (*(cm_cacheTypes[cm_cacheType]->truncate))(afile, asize);
}

#ifdef SGIMIPS
int
cm_CFileQuickTrunc(
  struct fid * handle,
  long asize)
{
    /* return a generic file structure */
    return (*(cm_cacheTypes[cm_cacheType]->quick_trunc))(handle, asize);
}
#endif /* SGIMIPS */


#ifdef SGIMIPS
int cm_CFileClose(register char *afile)
#else  /* SGIMIPS */
int cm_CFileClose(afile)
    register char *afile; 
#endif /* SGIMIPS */
{
    return (*(cm_cacheTypes[cm_cacheType]->close))(afile);
}


#ifdef SGIMIPS
int cm_CFileRW(
    char *afile,
    enum uio_rw arw,
    char *abase,
    long aoffset,
    long alen,
    osi_cred_t *acredp) 
#else  /* SGIMIPS */
int cm_CFileRW(afile, arw, abase, aoffset, alen, acredp)
    char *afile;
    enum uio_rw arw;
    char *abase;
    long aoffset;
    long alen;
    osi_cred_t *acredp; 
#endif /* SGIMIPS */
{
    struct uio tuio;
    struct iovec tiovec;

    /* 
     * Setup base UIO structure pointing to one io vector 
     */
    osi_InitUIO(&tuio);
    tuio.osi_uio_iov = &tiovec;
    tuio.osi_uio_iovcnt = 1;
    tuio.osi_uio_offset = aoffset;
#ifdef SGIMIPS
    tuio.osi_uio_limit = osi_getufilelimit();
#endif /* SGIMIPS */
    tuio.osi_uio_resid = alen;
    tuio.osi_uio_seg = OSI_UIOSYS;	/* kernel space for this one */
#ifndef AFS_OSF_ENV
    tuio.osi_uio_fmode = (arw == UIO_READ ? FREAD : FWRITE);
#endif
    tiovec.iov_base = abase;
    tiovec.iov_len = alen;

    if (cm_CFileRDWR(afile, arw, &tuio, acredp))
	return -1;	/* something went wrong */

#ifdef CM_ENABLE_COUNTERS
    if (arw == UIO_READ)
	CM_BUMP_COUNTER_BY(dataCacheBytesReadFromCache, (alen - tuio.osi_uio_resid));
    else
	CM_BUMP_COUNTER_BY(dataCacheBytesWrittenToCache, (alen - tuio.osi_uio_resid));
#endif /* CM_ENABLE_COUNTERS */

    /* 	
     * Otherwise, return the # of bytes actually read or written 
     */

#ifdef SGIMIPS
    return ((int)(alen - tuio.osi_uio_resid));
#else  /* SGIMIPS */
    return (alen - tuio.osi_uio_resid);
#endif /* SGIMIPS */
}


/* 
 * arw is one of UIO_READ or UIO_WRITE, auio is a uio structure, acredp is the
 * required credential structure, and afile is the generic file object.
 */
#ifdef SGIMIPS
int cm_CFileRDWR(
    char *afile,
    enum uio_rw arw,
    struct uio *auio,
    osi_cred_t *acredp) 
#else  /* SGIMIPS */
int cm_CFileRDWR(afile, arw, auio, acredp)
    char *afile;
    enum uio_rw arw;
    struct uio *auio;
    osi_cred_t *acredp; 
#endif /* SGIMIPS */
{
    return (*(cm_cacheTypes[cm_cacheType]->rdwr))(afile, arw, auio, acredp);
}
