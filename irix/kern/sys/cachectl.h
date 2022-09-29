#ifndef	__SYS_CACHECTL_H__
#define	__SYS_CACHECTL_H__
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* --------------------------------------------------- */
/* | Copyright (c) 1986 MIPS Computer Systems, Inc.  | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */

#ident "$Revision: 1.11 $"

/*
 * cachectl.h -- defines for MIPS cache control system calls
 */

/*
 * Options for cacheflush system call
 */
#define	ICACHE	0x1		/* flush i cache */
#define	DCACHE	0x2		/* flush d cache */
#define	BCACHE	(ICACHE|DCACHE)	/* flush both caches */

/*
 * Options for cachectl system call
 */
#define	CACHEABLE	0	/* make page(s) cacheable */
#define	UNCACHEABLE	1	/* make page(s) uncacheable */

/* The IP17 tracks the number of errors in tags and data for each of the
 * caches, and provides a syssgi() call to allow cmd/ecc to fetch them
 * for display. */
#define ECC_ERR_TYPES	8
enum ecc_err_types { PI_DERRS = 0, PI_TERRS, PD_DERRS, PD_TERRS, 
		     SC_DERRS, SC_TERRS, SYSAD_ERRS, NO_ERROR };

/*
 * cache synchronization support
 */
typedef int (*cache_synch_handler_t)(void *,int);
extern cache_synch_handler_t cache_synch_handler(void);

#ifndef _KERNEL

#ifdef __cplusplus
extern "C" {
#endif

extern int	cachectl(void *, int, int);
extern int	cacheflush(void *, int, int);
extern int	_flush_cache(char *, int, int);

#ifdef __cplusplus
}
#endif

#endif /* _KERNEL */
#endif /* __SYS_CACHECTL_H__ */
