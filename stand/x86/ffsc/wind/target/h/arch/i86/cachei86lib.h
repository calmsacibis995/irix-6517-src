/* cacheI86Lib.h - I80486 cache library header file */

/* Copyright 1984-1993 Wind River Systems, Inc. */
/*
modification history
--------------------
01b,29may94,hdn  deleted I80486 macro.
01a,15jun93,hdn  created.
*/

#ifndef __INCcacheI86Libh
#define __INCcacheI86Libh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS cacheArchEnable (CACHE_TYPE cache);
extern STATUS cacheArchDisable (CACHE_TYPE cache);
extern STATUS cacheArchLock (CACHE_TYPE cache, void * address, size_t bytes);
extern STATUS cacheArchUnlock (CACHE_TYPE cache,
				void * address, size_t bytes);
extern STATUS cacheArchClear (CACHE_TYPE cache, void * address, size_t bytes);
extern STATUS cacheArchFlush (CACHE_TYPE cache, void * address, size_t bytes);
extern void * cacheArchDmaMalloc (size_t bytes);
extern STATUS cacheArchDmaFree (void * pBuf);
extern STATUS cacheArchTextUpdate (void * address, size_t bytes);
extern STATUS cacheArchClearEntry (CACHE_TYPE cache, void * address);

extern void cache486Reset (void);
extern void cache486Enable (void);
extern void cache486Disable (void);
extern void cache486Lock (void);
extern void cache486Unlock (void);
extern void cache486Clear (void);
extern void cache486Flush (void);

#else

extern STATUS cacheArchEnable ();
extern STATUS cacheArchDisable ();
extern STATUS cacheArchLock ();
extern STATUS cacheArchUnlock ();
extern STATUS cacheArchClear ();
extern STATUS cacheArchFlush ();
extern void * cacheArchDmaMalloc ();
extern STATUS cacheArchDmaFree ();
extern STATUS cacheArchTextUpdate ();
extern STATUS cacheArchClearEntry ();

extern void cache486Reset ();
extern void cache486Enable ();
extern void cache486Disable ();
extern void cache486Lock ();
extern void cache486Unlock ();
extern void cache486Clear ();
extern void cache486Flush ();

#endif /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCcacheI86Libh */
