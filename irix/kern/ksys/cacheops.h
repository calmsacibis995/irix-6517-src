/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1994 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __KSYS_CACHEOPS_H__
#define __KSYS_CACHEOPS_H__

extern void     __cache_wb_inval(void *, int);
extern void     __icache_inval(void *, int);
extern void     __dcache_wb_inval(void *, int);
extern void     __dcache_wb(void *, int);
extern void     __dcache_inval(void *, int);
extern void 	__cache_hwb_inval_pfn(__psunsigned_t, int, pfn_t);

extern uint	getcachesz(cpuid_t cpunum);

#if IP19 || SN0
extern int	pdcache_size;
extern int	__pdcache_wbinval(void *, int);
extern void     __picache_inval(void *, int);
#endif
#if IP25
extern void     __cache_exclusive(void *, int);
#endif

#if SN0
extern void	__prefetch_load(void *, int);
extern void 	__prefetch_store(void *, int);
#endif	/* SN0 */

#if IP32
extern void	__vm_dcache_inval(void *, int);
extern void	__vm_dcache_wb_inval(void *addr, int);
#endif
#endif /* __KSYS_CACHEOPS_H__ */
