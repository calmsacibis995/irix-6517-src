/* cacheLib.h - cache library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02p,19mar95,dvs  removed #ifdef TRON - tron no longer supported.
02o,08jun94,tpr  added branch cache used by MC68060 cpu.
02n,02dec93,pme  Added Am29K family support.
02m,09jun93,hdn  added support for I80X86
02l,19oct92,jcf  reordered xxLibInit params.  made CACHE_TYPE v5.0 compatible.
02k,29sep92,jwt  merged cacheLibInit(), cacheReset(), and cacheModeSet().
02j,22sep92,rrr  added support for c++
02i,20aug92,wmd  added #include for i960.
02h,13aug92,rdc  removed instances of cacheLib functions and CACHE_FUNCS
	 	 structures and conditional compilation of corresponding
		 IMPORTS.
02g,30jul92,dnw  added cacheLib functions and CACHE_FUNCS structures.
02f,15jul92,jwt  added more CACHE_MODEs.
02e,09jul92,jwt  added virtual-to-physical and physical-to-virtual stuff.
02d,07jul92,ajm  added support for mips
02c,06jul92,jwt  cleaned up cacheDrvXxx() macros by stressing pointers.
02b,04jul92,jcf  cleaned up.
02a,03jul92,jwt  cache library header for 5.1; moved '040 stuff to arch/mc68k.
01k,16jun92,jwt  made safe for assembly language files.
01j,26may92,rrr  the tree shuffle
                  -changed includes to have absolute path from h/
01i,20jan92,shl  added cache68kLib.h.
01h,09jan92,jwt  created cacheSPARCLib.h;
                 cleaned up ANSI compiler warning messages.
01g,04oct91,rrr  passed through the ansification filter
                  -fixed #else and #endif
                  -changed copyright notice
01f,27aug91,shl  added scope type for MC68040 caches.
01e,19jul91,gae  renamed architecture specific include file to be xx<arch>.h.
01d,23jan91,jwt  added SPARC cache commands.
01c,01mar91,hdn  added TRON related stuff.
01b,05oct90,shl  added copyright notice; made #endif ANSI style.
01a,15jul90,jcf  written.
*/

#ifndef __INCcacheLibh
#define __INCcacheLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "limits.h"

/* Cache types */

#define	_INSTRUCTION_CACHE 	0	/* Instruction Cache(s) */
#define	_DATA_CACHE		1	/* Data Cache(s) */
#define	_BRANCH_CACHE		2	/* Branch Cache(s) */

/* Cache mode soft bit masks */

#define	CACHE_DISABLED		0x00	/* No cache or disabled */

#define	CACHE_WRITETHROUGH	0x01	/* Write-through Mode */
#define	CACHE_COPYBACK		0x02	/* Copyback Mode */
#define	CACHE_WRITEALLOCATE	0x04	/* Write Allocate Mode */
#define	CACHE_NO_WRITEALLOCATE	0x08
#define	CACHE_SNOOP_ENABLE	0x10	/* Bus Snooping */
#define	CACHE_SNOOP_DISABLE	0x20
#define	CACHE_BURST_ENABLE	0x40	/* Cache Burst Cycles */
#define	CACHE_BURST_DISABLE	0x80

/* Errno values */

#define S_cacheLib_INVALID_CACHE	(M_cacheLib | 1)

/* Value for "entire cache" in length arguments */

#define	ENTIRE_CACHE		ULONG_MAX


#ifndef	_ASMLANGUAGE

typedef enum				/* CACHE_TYPE */
    {
#if (CPU == MC68060)
    BRANCH_CACHE      = _BRANCH_CACHE,
#endif /* (CPU == MC68060) */
    INSTRUCTION_CACHE = _INSTRUCTION_CACHE,
    DATA_CACHE        = _DATA_CACHE
    } CACHE_TYPE;

typedef	UINT	CACHE_MODE;		/* CACHE_MODE */

typedef	struct				/* Cache Routine Pointers */
    {
    FUNCPTR	enableRtn;		/* cacheEnable() */
    FUNCPTR	disableRtn;		/* cacheDisable() */
    FUNCPTR	lockRtn;		/* cacheLock() */
    FUNCPTR	unlockRtn;		/* cacheUnlock() */
    FUNCPTR	flushRtn;		/* cacheFlush() */
    FUNCPTR	invalidateRtn;		/* cacheInvalidate() */
    FUNCPTR	clearRtn;		/* cacheClear() */

    FUNCPTR	textUpdateRtn;		/* cacheTextUpdate() */
    FUNCPTR	pipeFlushRtn;		/* cachePipeFlush() */
    FUNCPTR	dmaMallocRtn;		/* cacheDmaMalloc() */
    FUNCPTR	dmaFreeRtn;		/* cacheDmaFree() */
    FUNCPTR	dmaVirtToPhysRtn;	/* virtual-to-Physical Translation */
    FUNCPTR	dmaPhysToVirtRtn;	/* physical-to-Virtual Translation */
    } CACHE_LIB;

/* Cache flush and invalidate support for general use and drivers */

typedef	struct				/* Driver Cache Routine Pointers */
    {
    FUNCPTR	flushRtn;		/* cacheFlush() */
    FUNCPTR	invalidateRtn;		/* cacheInvalidate() */
    FUNCPTR	virtToPhysRtn;		/* Virtual-to-Physical Translation */
    FUNCPTR	physToVirtRtn;		/* Physical-to-Virtual Translation */
    } CACHE_FUNCS;


/* Cache macros */

#define	CACHE_TEXT_UPDATE(adrs, bytes)	\
        ((cacheLib.textUpdateRtn == NULL) ? OK :	\
	 (cacheLib.textUpdateRtn) ((adrs), (bytes)))

#define	CACHE_PIPE_FLUSH()	\
        ((cacheLib.pipeFlushRtn == NULL) ? OK :	\
	 (cacheLib.pipeFlushRtn) ())


#define	CACHE_DRV_FLUSH(pFuncs, adrs, bytes)	\
        (((pFuncs)->flushRtn == NULL) ? OK :	\
         ((pFuncs)->flushRtn) (DATA_CACHE, (adrs), (bytes)))

#define	CACHE_DRV_INVALIDATE(pFuncs, adrs, bytes)	\
        (((pFuncs)->invalidateRtn == NULL) ? OK :	\
         ((pFuncs)->invalidateRtn) (DATA_CACHE, (adrs), (bytes)))

#define	CACHE_DRV_VIRT_TO_PHYS(pFuncs, adrs)	\
        (((pFuncs)->virtToPhysRtn == NULL) ? (void *) (adrs) :	\
	 (void *) (((pFuncs)->virtToPhysRtn) (adrs)))

#define	CACHE_DRV_PHYS_TO_VIRT(pFuncs, adrs)	\
        (((pFuncs)->physToVirtRtn == NULL) ? (void *) (adrs) :	\
	 ((void *) ((pFuncs)->physToVirtRtn) (adrs)))

#define	CACHE_DRV_IS_WRITE_COHERENT(pFuncs)	\
	((pFuncs)->flushRtn == NULL)

#define	CACHE_DRV_IS_READ_COHERENT(pFuncs)	\
	((pFuncs)->invalidateRtn == NULL)


#define	CACHE_DMA_FLUSH(adrs, bytes)		\
	CACHE_DRV_FLUSH (&cacheDmaFuncs, (adrs), (bytes))

#define	CACHE_DMA_INVALIDATE(adrs, bytes)	\
	CACHE_DRV_INVALIDATE (&cacheDmaFuncs, (adrs), (bytes))

#define	CACHE_DMA_VIRT_TO_PHYS(adrs)	\
	CACHE_DRV_VIRT_TO_PHYS (&cacheDmaFuncs, (adrs))

#define	CACHE_DMA_PHYS_TO_VIRT(adrs)	\
	CACHE_DRV_PHYS_TO_VIRT (&cacheDmaFuncs, (adrs))

#define	CACHE_DMA_IS_WRITE_COHERENT()	\
	CACHE_DRV_IS_WRITE_COHERENT (&cacheDmaFuncs)

#define	CACHE_DMA_IS_READ_COHERENT()	\
	CACHE_DRV_IS_READ_COHERENT (&cacheDmaFuncs)


#define	CACHE_USER_FLUSH(adrs, bytes)	\
	CACHE_DRV_FLUSH (&cacheUserFuncs, (adrs), (bytes))

#define	CACHE_USER_INVALIDATE(adrs, bytes)	\
	CACHE_DRV_INVALIDATE (&cacheUserFuncs, (adrs), (bytes))

#define	CACHE_USER_IS_WRITE_COHERENT()	\
	CACHE_DRV_IS_WRITE_COHERENT (&cacheUserFuncs)

#define	CACHE_USER_IS_READ_COHERENT()	\
	CACHE_DRV_IS_READ_COHERENT (&cacheUserFuncs)



/* variable declarations */

IMPORT CACHE_LIB	cacheLib;
IMPORT CACHE_FUNCS	cacheNullFuncs;	/* functions for non-cached memory */
IMPORT CACHE_FUNCS	cacheDmaFuncs;	/* functions for dma memory */
IMPORT CACHE_FUNCS	cacheUserFuncs;	/* functions for user memory */

IMPORT FUNCPTR		cacheDmaMallocRtn;
IMPORT FUNCPTR		cacheDmaFreeRtn;
IMPORT CACHE_MODE	cacheDataMode;	/* data cache modes for funcptrs */
IMPORT BOOL		cacheDataEnabled;
IMPORT BOOL		cacheMmuAvailable;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS	cacheLibInit (CACHE_MODE inst, CACHE_MODE data);
extern STATUS	cacheArchLibInit (CACHE_MODE inst, CACHE_MODE data);
extern STATUS	cacheEnable (CACHE_TYPE cache);
extern STATUS	cacheDisable (CACHE_TYPE cache);
extern STATUS	cacheLock (CACHE_TYPE cache, void * adrs, size_t bytes);
extern STATUS	cacheUnlock (CACHE_TYPE cache, void * adrs, size_t bytes);
extern STATUS	cacheFlush (CACHE_TYPE cache, void * adrs, size_t bytes);
extern STATUS	cacheInvalidate (CACHE_TYPE cache, void *adrs, size_t bytes);
extern STATUS	cacheClear (CACHE_TYPE cache, void * adrs, size_t bytes);
extern STATUS	cacheTextUpdate (void * adrs, size_t bytes);
extern STATUS	cachePipeFlush (void);

extern void *	cacheDmaMalloc (size_t bytes);
extern STATUS	cacheDmaFree (void * pBuf);

extern STATUS	cacheDrvFlush (CACHE_FUNCS * pFuncs, void * adrs, size_t bytes);
extern STATUS	cacheDrvInvalidate (CACHE_FUNCS * pFuncs, void * adrs,
				    size_t bytes);
extern void *   cacheDrvVirtToPhys (CACHE_FUNCS * pFuncs, void * adrs);
extern void *   cacheDrvPhysToVirt (CACHE_FUNCS * pFuncs, void * adrs);

extern void	cacheFuncsSet (void);

#else

extern STATUS	cacheLibInit ();
extern STATUS	cacheArchLibInit ();
extern STATUS	cacheEnable ();
extern STATUS	cacheDisable ();
extern STATUS	cacheLock ();
extern STATUS	cacheUnlock ();
extern STATUS	cacheFlush ();
extern STATUS	cacheInvalidate ();
extern STATUS	cacheClear ();
extern STATUS	cacheTextUpdate ();
extern STATUS	cachePipeFlush ();
extern void *	cacheDmaMalloc ();
extern STATUS	cacheDmaFree ();
extern STATUS	cacheDrvFlush ();
extern STATUS	cacheDrvInvalidate ();
extern void *   cacheDrvVirtToPhys ();
extern void *   cacheDrvPhysToVirt ();
extern void	cacheSetFuncs ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

/* Architecture-specific cache headers */

#if	(CPU_FAMILY == MC680X0)
#include "arch/mc68k/cachemc68klib.h"
#endif  /* (CPU_FAMILY == MC680X0) */

#if	(CPU_FAMILY == SPARC)
#include "arch/sparc/cachesparclib.h"
#endif

#if	(CPU_FAMILY == MIPS)
#include "arch/mips/cachemipslib.h"
#endif  /* (CPU_FAMILY == MIPS) */

#if	(CPU_FAMILY == I960)
#include "arch/i960/vxi960lib.h"
#endif  /* CPU_FAMILY == I960 */

#if	(CPU_FAMILY == I80X86)
#include "arch/i86/cachei86lib.h"
#endif  /* CPU_FAMILY == I80X86 */

#if	(CPU_FAMILY == AM29XXX)
#include "arch/am29k/cacheam29klib.h"
#endif  /* CPU_FAMILY == AM29XXX */

#ifdef __cplusplus
}
#endif

#endif /* __INCcacheLibh */
