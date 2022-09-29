/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 *      Copyright (C) 1996, 1993 Transarc Corporation
 *      All rights reserved.
 */
/*
 * HISTORY
 * $Log: cm_stats.h,v $
 * Revision 65.1  1997/10/20 19:17:26  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.25.1  1996/10/02  17:12:49  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:05:59  damon]
 *
 * $EndLog$
 */

/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_stats.h,v 65.1 1997/10/20 19:17:26 jdoak Exp $ */

#ifndef	TRANSARC_CM_STATS_H_
#define	TRANSARC_CM_STATS_H_
/*
 * This file describes the structure for a set of counters that have been
 * added to the cache manager.  All counters are cumulative.
 */

/*
 * Status cache counters.  These count the number of entries in the CM status
 * cache, the number of hits and misses on status cache lookups, and the number
 * of times an entry is flushed from the cache.  Flushes essentially counts
 * the number of calls to FlushSCache() where flushing will actually occur.
 *
 *    unsigned long	statusCacheEntries;
 *    unsigned long	statusCacheHits;
 *    unsigned long	statusCacheMisses;
 *    unsigned long 	statusCacheFlushes;
 *
 * Data cache counters.  These count the number of entries in the CM data
 * cache, the number of hits and misses on data cache chunk lookups, the
 * number of times an entry is recycles, the number of bytes read from the 
 * cache, the number of bytes read off the wire, the number of bytes written
 * to the cache, and the number of bytes written to the wire.  Recycles 
 * essentially counts the number of calls to GetDownDSlot().
 *
 *    unsigned long	dataCacheEntries;
 *    unsigned long	dataCacheHits;
 *    unsigned long	dataCacheMisses;
 *    unsigned long	dataCacheRecycles;
 *
 *    unsigned long	dataCacheBytesReadFromCache;
 *    unsigned long	dataCacheBytesReadFromWire;
 *    unsigned long	dataCacheBytesWrittenToCache;
 *    unsigned long	dataCacheBytesWrittenToWire;
 *
 * Directory name cache counters.   This cache keeps information on directory
 * names that have been looked up, including non-existent directories.  These
 * counters keep track of the hits and misses for the cache.
 *
 *    unsigned long	nameCacheHits;
 *    unsigned long	nameCacheMisses;
 *
 * Fileset cache.  This cache maintains the mapping between a fileset and
 * the server where the fileset is located.  The cache is updated when a new
 * mountpoint is crossed.  The cache is invalidated when a 'cm checkf' is done
 * or when the fileset is moved.
 *
 *    unsigned long	filesetCacheInvalidations;
 *    unsigned long	filesetCacheUpdates;
 *
 * Status token counters.  These counters keep track of the number of status tokens
 * that are acquired, released, and revoked.  Note that the token counts will not "balance".
 * This is because a status token acquired or released may contain both types of
 * status token but the read or write part may be revoked independently.  In this 
 * respect, these counters are useful for determining the frequency of token
 * acquisition rather than in checking for token leaks.
 *
 *    unsigned long	statusTokensAcquired;
 *    unsigned long	statusTokensReleased;
 *    unsigned long	statusTokensRevoked;
 *
 * Data token counters.  These counters keep track of the number of data tokens 
 * that are acquired, released, and revoked.  Note that the token counts will not "balance".
 * This is because a data token acquired or released may contain both types of
 * data token but the read or write part may be revoked independently.  In this 
 * respect, these counters are useful for determining the frequency of token
 * acquisition rather than in checking for token leaks.
 *
 *    unsigned long	dataTokensAcquired;
 *    unsigned long	dataTokensReleased;
 *    unsigned long	dataTokensRevoked;
 *
 * Binding handle counters.  The cache manager maintains a pool of binding handles
 * The cache manager will create up to three binding handles per user per server,
 * then will wait for a binding handle to become available.  These counters measure
 * the total number of binding handles created and the number of times the cache
 * manager waits for a handle.
 *
 *    unsigned long	bindingHandlesCreated;
 *    unsigned long	bindingHandleWaits;
 *
 */

#ifdef CM_ENABLE_COUNTERS
#define CM_BUMP_COUNTER(member) cm_stats.member++
#define CM_BUMP_COUNTER_BY(member,delta) cm_stats.member += (delta)

struct cm_stats {
    /* status cache counters */
    unsigned long	statusCacheEntries;
    unsigned long	statusCacheHits;
    unsigned long	statusCacheMisses;
    unsigned long 	statusCacheFlushes;

    /* data cache counters */
    unsigned long	dataCacheEntries;
    unsigned long	dataCacheHits;
    unsigned long	dataCacheMisses;
    unsigned long	dataCacheRecycles;

    unsigned long	dataCacheBytesReadFromCache;
    unsigned long	dataCacheBytesReadFromWire;
    unsigned long	dataCacheBytesWrittenToCache;
    unsigned long	dataCacheBytesWrittenToWire;

    /* directory name lookup cache counters */
    unsigned long	nameCacheHits;
    unsigned long	nameCacheMisses;

    /* fileset cache counters */
    unsigned long	filesetCacheInvalidations;	/* because of TSR */
    unsigned long	filesetCacheUpdates;

    /* status token counters */
    unsigned long	statusTokensAcquired;
    unsigned long	statusTokensReleased;
    unsigned long	statusTokensRevoked;

    /* data token counters */
    unsigned long	dataTokensAcquired;
    unsigned long	dataTokensReleased;
    unsigned long	dataTokensRevoked;

    /* connection counters */
    unsigned long	bindingHandlesCreated;
    unsigned long	bindingHandleWaits;		/* # of times a binding handle awaited  */

    /* spares */
    unsigned long	spare0;
    unsigned long	spare1;
    unsigned long	spare2;
    unsigned long	spare3;
    unsigned long	spare4;
    unsigned long	spare5;
    unsigned long	spare6;
    unsigned long	spare7;
    unsigned long	spare8;
    unsigned long	spare9;
    unsigned long	spare10;
    unsigned long	spare11;
    unsigned long	spare12;
    unsigned long	spare13;
    unsigned long	spare14;
    unsigned long	spare15;
    unsigned long	spare16;
    unsigned long	spare17;
    unsigned long	spare18;
    unsigned long	spare19;
    unsigned long	spare20;
    unsigned long	spare21;
    unsigned long	spare22;
    unsigned long	spare23;
    unsigned long	spare24;
    unsigned long	spare25;
    unsigned long	spare26;
    unsigned long	spare27;
    unsigned long	spare28;
    unsigned long	spare29;
    unsigned long	spare30;
    unsigned long	spare31;
};

extern struct cm_stats cm_stats;
#else /* CM_ENABLE_COUNTERS */
#define CM_BUMP_COUNTER(member) /* nothing */
#define CM_BUMP_COUNTER_BY(member,delta) /* nothing */
#endif /* CM_ENABLE_COUNTERS */
#endif	/* TRANSARC_CM_STATS_H_ */
