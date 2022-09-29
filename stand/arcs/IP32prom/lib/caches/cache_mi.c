
/**************************************************************************
 *                                                                        *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Generalized cache operations:
 *
 * This file implements a generalized notion of cache operations.
 *
 *   1) It generalizes cache operations to operate on a range of addresses,
 *      not just a single cell.  Thus, the range to be operated on is
 *      specified as a (addr,length) pair, not simply an address.  This
 *      iteration over a range is performed by the "cacheLoop" routine.
 *      No alignment restrictions are assumed for these generalized cache
 *      operations.
 *
 *   2) The mechanics of the cache operation are abstracted out of the
 *      cacheLoop routine. The cacheLoop routine breaks down the request
 *      into a series of L1 and L2 "logical cache op's".
 *
 *   3) At cache configuration time, the configuration code initializes
 *      data structures used both by this module and by external clients.
 *      A mapping from logical cache op's to machine dependent cache
 *      op's is made at configuration time, no machine dependencies are
 *      resolved when the actual cache operation routines are called.
 *
 * Machine dependent code:
 *
 *   The actual machine dependent and generally requiring access to
 *   machine registers, are located in rxxxxx_cache.s
 */

#include <caches.h>
#include <stdio.h>
#include <assert.h>
#include <sys/cpu.h>
#include <sys/sbd.h>

#if defined(C0_IMP_R10000)
#undef  C0_IMP_R10000
#endif
#define IMPOSSIBLE              0
#define C0_IMP_R10000		0x9
#define	C0_IMPMASK		0xff00
#define	C0_IMPSHIFT		8
#define	R10KPIBLK		64
#define	R10KPDBLK		32
#define	R10KPCACH		0x8000
#define	R10KPCSET		0x4000
#define	R10K_PI_SHIFT		29
#define	R10K_PI_MASK		7
#define	R10K_PD_SHIFT		26
#define	R10K_PD_MASK		7
#define	R10K_SS_SHIFT		16
#define	R10K_SS_MASK		7
#define	R10K_SB_SHIFT		13
#define	R10K_SB_MASK 		1

extern unsigned long Read_C0_PRId(void);

/*
 * Cache configuration data
 *
 * The following public cache configuation data is used/maintained by this
 * module:
 *
 *    _icache_size	Size of L1 I cache in bytes
 *    _dcache_size      Size of L1 D cache in bytes
 *    _sidcache_size    Size of L2 I/D cache in bytes
 *    _icache_linesize  Block size of L1 I cache in bytes
 *    _dcache_linesize  Block size of L1 D cache in bytes
 *    _icache_linemask  _icache_linesize-1
 *    _dcache_linemask  _dcache_linesize-1
 *    _scache_linesize  Block size of L2 I/D cache in bytes
 *    _scache_linemask  _scache_linesize-1
 *    _r4600sc_sidcache_size  Size of add-on external cache ala' Indy 4600SC.
 *    _two_set_pcaches  Non-zero if 2way primary, value == dcache_size/2.
 *
 * (Cache block sizes are constrained to be powers of two)
 */

/* Caches size  */
SIZE_t _icache_size;
SIZE_t _dcache_size;
SIZE_t _sidcache_size;
SIZE_t _r4600sc_sidcache_size;

/* Cache line size. */
SIZE_t _icache_linesize;
SIZE_t _dcache_linesize;
SIZE_t _scache_linesize;

/* Cache line mask. */
ADDR_t _icache_linemask;
ADDR_t _dcache_linemask;
ADDR_t _scache_linemask;

/* Sets in cache.   */
SIZE_t _two_set_pcaches;
SIZE_t _two_set_icaches;
SIZE_t _two_set_scaches;

/* Type of secondary cache. */
CACH_t _scache_type;
CACH_t cachewrback = 1;

/* Low level cache ops table.   */
cop lcoptab[3][10], _flushAllCache, _validCacheAddr;

/*
 * macro for access low level logical cache ops through
 * lcoptab.
 */
#if !defined(CacheOP)
#define CacheOP(CACHE,CODE,ADDR) (lcoptab[CACHE][CODE])(ADDR);
#endif

#undef  DEBUG
#define DEBUG 1

#undef  DEBUG_CACH
/* #define DEBUG_CACH 1  */
#if defined(DEBUG_CACH)
void __testcache_haha(ADDR_t, int);
#endif

/* #undef  DEBUGR10K    */
/* #define DEBUGR10K 1  */
#if defined(DEBUGR10K)
SIZE_t scache_set_size;
SIZE_t RetriveSETsize(void);
void   SaveSETsize(uint32_t);
void __cacheram_test(void);
void __dataram_test(void);
void print_cache_config(void);
void IP32processorTCI(void);
int  jumper_off(void);
#endif

/**************
 * config_cache
 *-------------
 * Do run time cache configuration and setup all those cache 
 * related variables.
 */
void
config_cache(void)
{
  int cpuType = (Read_C0_PRId() & C0_IMPMASK) >>C0_IMPSHIFT ;

  /* Zero the cache size variables */
  _icache_size = _dcache_size = _sidcache_size =
  _r4600sc_sidcache_size = 0;

  /* Zero the cache line size variables.    */
  _icache_linesize = _dcache_linesize = _scache_linesize = 0x0; 

  /* Zero the cache line mask variables.    */
  _icache_linemask = _dcache_linemask = _scache_linemask = 0x0;

  /* Zero the cache set variables.  */
  _two_set_pcaches = _two_set_icaches = _two_set_scaches = 0x0;
  _scache_type = 0;

  switch (cpuType) {
  case C0_IMP_R10000:
    r10000_cache_config();
#if defined(DEBUGR10K)
    if (jumper_off()) {
        SaveSETsize(_two_set_scaches);
        scache_set_size = _two_set_scaches;
        __dataram_test();
        __cacheram_test();
    }
#endif
    break ;
    
  case C0_IMP_TRITON:
    r5000_cache_config();
    break ;
    
  case C0_IMP_R4700:
  case C0_IMP_R4650:
  case C0_IMP_R4600:
    r4600_cache_config();
    break ;
    
  case C0_IMP_R4400:
    r4400_cache_config();
    break ;
    
  default:
    printf("Not supported CPU type. PRid<impl>=0x%x\n", cpuType);
    assert(IMPOSSIBLE);
    break;
  }

  /* Force cachewrback to 1 */
  cachewrback = 1;
}

#if defined(DEBUG)
/**************
 * config_cache
 *-------------
 * Do run time cache configuration and setup all those cache 
 * related variables.
 */
void
print_cache_config(void)
{
  int indx1, indx2;

  printf ("_icache_size     = 0x%-8x, _dcache_size           = 0x%-8x\n",
          _icache_size, _dcache_size);
  printf ("_icache_linesize = 0x%-8x, _dcache_linesize       = 0x%-8x\n",
          _icache_linesize, _dcache_linesize);
  printf ("_two_set_pcaches = 0x%-8x, _two_set_icaches       = 0x%-8x\n",
          _two_set_pcaches, _two_set_icaches);
  printf ("_sidcache_size   = 0x%-8x, _r4600sc_sidcache_size = 0x%-8x\n",
          _sidcache_size, _r4600sc_sidcache_size);
  printf ("_scache_linesize = 0x%08x\n", _scache_linesize);
  printf ("_two_set_scaches = 0x%08x\n", _two_set_scaches);
  printf ("_scache_type     = 0x%08x\n", _scache_type);
  printf ("cachewrback      = 0x%08x\n", cachewrback);

  for (indx1 = 0; indx1 < 3; indx1 += 1) {
    printf ("Logical Cache Table row[%d]\n", indx1);
    for (indx2 = 0; indx2 < 10; indx2 += 1)
      printf ("   lcoptab[%d][%d] = 0x%08x\n", indx1, indx2,
              lcoptab[indx1][indx2]);
    printf ("\n");
  }

  printf ("_flushAllCache  =  0x%08x\n", _flushAllCache);
  printf ("_validCacheAddr =  0x%08x\n", _validCacheAddr);
}
#endif

#if 0
typedef unsigned long       uint32_t;
typedef unsigned long long  uint64_t;
#endif

int     CacheValid(char*);

/***************************
 * __dcache_inval(addr, len)
 * -------------------------
 * invalidate data cache for range of physical addr to addr+len-1
 *
 * We use the HIT_INVALIDATE cache op. Therefore, this requires
 * that 'addr'..'len' be the actual addresses which we want to
 * invalidate in the caches.
 * addr/len are not necessarily cache line aligned. Any lines
 * partially covered by addr/len are written out with the
 * KSEG1 base address.
 * Invalidate lines in both the primary and secondary cache
 * (if there is one).
 */
#pragma weak clean_cache = __dcache_inval
void
__dcache_inval(char* Addr, int len)
{
  ADDR_t        Ledge, Tedge;
  register char *addr = (char*)(((uint32_t)Addr&0x5fffffff)|0x80000000);
  register char *lineaddr, *endaddr;

  /*
   * make sure address is cached address, we assume the address given
   * is cached address, i.e. either mapped in TLBs as cached or KSEG0
   * based address.
   */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    Ledge = (ADDR_t)addr & ~_scache_linemask;
    endaddr = (char*)(((ADDR_t)addr + (ADDR_t)len) & ~_scache_linemask);
    Tedge = (ADDR_t)endaddr + _scache_linesize;
  } else {
    Ledge = (ADDR_t)addr & ~_dcache_linemask;
    endaddr = (char*)(((ADDR_t)addr + (ADDR_t)len) & ~_dcache_linemask);
    Tedge = (ADDR_t)endaddr + _dcache_linesize;
  }
  
  /* Taking care of the leading edge    */
  if (CacheValid(addr))
    while ((char*)Ledge < addr) {
      *(volatile char*)((uint32_t)Ledge|K1BASE) = *(char*)Ledge;
      Ledge += 1;
    }

  /* Taking care of the tailing edge    */
  if (CacheValid(endaddr)) {
    Ledge = (ADDR_t)addr + len;
    while ((char*)Ledge < (char*)Tedge) {
      *(volatile char*)(Ledge|K1BASE) = *(char*)Ledge;
      Ledge += 1;
    }
  }
  
  /* Now is the easier part, invalidate the whole range */
  endaddr  = (char*)(((uint32_t)addr + len) & ~_dcache_linemask);
  lineaddr = (char*)((uint32_t)addr & ~_dcache_linemask);

  /* Invalidate primary cache first.    */
  do {
    CacheOP(PDCACHE, HIT_INVAL, (ADDR_t)lineaddr);
    lineaddr += _dcache_linesize;
  } while (lineaddr <= endaddr);
  
  /* See we have a secondary cache to invalidate.   */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    endaddr  = (char*)(((uint32_t)addr + len) & ~_scache_linemask);
    lineaddr = (char*)((uint32_t)addr & ~_scache_linemask);
    do {
      CacheOP(SIDCACHE, HIT_INVAL, (ADDR_t)lineaddr);
      lineaddr += _scache_linesize;
    } while (lineaddr <= endaddr);
  }
  /* Done   */
  return;
}
  
/***********************
 * __dcache_wb(addr,len)
 * ---------------------
 * write back data from the primary data and secondary cache.
 * use hit writeback cache op to writeback data caches.
 */
void
__dcache_wb(char* Addr, int len)
{
  register char *addr, *lineaddr, *endaddr;
  register int  addr_type = ((int)Addr >> 29) & 0x3;

  /*
   * Return if its uncached address.
   */
  if (addr_type == 5) {
    printf ("__cache_wb_inval was called with uncached address 0x%-8x\n",
            Addr);
    return;
  } else
    addr = (char*)(((uint32_t)Addr&0x5fffffff)|0x80000000);

  /*
   * make sure address is cached address, we assume the address given
   * is cached address, i.e. either mapped in TLBs as cached or KSEG0
   * based address.
   * Since hit writeback will not cause the inconsistence between
   * memory/scache/dcache, we simply choose the largest block
   * size (the _scache_linesize) 
   */
  
  /* Write back the given range of data from caches to memory   */
  endaddr  = (char*)(((uint32_t)addr + len) & ~_dcache_linemask);
  lineaddr = (char*)((uint32_t)addr & ~_dcache_linemask);

  /* Write data back to 2nd cache - if there is one.    */
  do {
    CacheOP(PDCACHE, HIT_WRITEBACK, (ADDR_t)lineaddr);
    lineaddr += _dcache_linesize;
  } while (lineaddr <= endaddr);
  
  /* Write data back to memory - if there is a 2nd cache.       */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    endaddr  = (char*)(((uint32_t)addr + len) & ~_scache_linemask);
    lineaddr = (char*)((uint32_t)addr & ~_scache_linemask);
    do {
      CacheOP(SIDCACHE, HIT_WRITEBACK, (ADDR_t)lineaddr);
      lineaddr += _scache_linesize;
    } while (lineaddr <= endaddr);
  }

  /* Done   */
  return;
}    
  

/******************************
 * __dcache_wb_inval(addr, len)
 * ----------------------------
 * writeback-invalidate data cache for range of
 * physical addr to addr+len-1
 */
#pragma weak pdcache_wb_inval = __dcache_wb_inval
void
__dcache_wb_inval(char* Addr, int len)
{
  register char *addr, *lineaddr, *endaddr;
  register int  addr_type = ((int)Addr >> 29) & 0x3;

  /*
   * Return if its uncached address.
   */
  if (addr_type == 5) {
    printf ("__cache_wb_inval was called with uncached address 0x%-8x\n",
            Addr);
    return;
  } else
    addr = (char*)(((uint32_t)Addr&0x5fffffff)|0x80000000);
  
  /*
   * make sure address is cached address, we assume the address given
   * is cached address, i.e. either mapped in TLBs as cached or KSEG0
   * based address.
   * Since hit writeback will not cause the inconsistence between
   * memory/scache/dcache, we simply choose the largest block
   * size (the _scache_linesize) 
   */
  
  /* Write back the given range of data from caches to memory   */
  endaddr  = (char*)(((uint32_t)addr + len) & ~_dcache_linemask);
  lineaddr = (char*)((uint32_t)addr & ~_dcache_linemask);

  /* Write data back to 2nd cache - if there is one.    */
  do {
    CacheOP(PDCACHE, HIT_WB_INVAL, (ADDR_t)lineaddr);
    lineaddr += _dcache_linesize;
  } while (lineaddr <= endaddr);
  
  /* Write data back to memory - if there is a 2nd cache.       */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    endaddr  = (char*)(((uint32_t)addr + len) & ~_scache_linemask);
    lineaddr = (char*)((uint32_t)addr & ~_scache_linemask);
    do {
      CacheOP(SIDCACHE, HIT_WB_INVAL, (ADDR_t)lineaddr);
      lineaddr += _scache_linesize;
    } while (lineaddr <= endaddr);
  }
  
  /* Done   */
  return;
}    


/***************************
 * __icache_inval(addr, len)
 * -------------------------
 * invalidate instruction cache for range of physical addr to addr+len-1
 *
 * We use the HIT_INVALIDATE cache op. Therefore, this requires
 * that 'addr'..'len' be the actual addresses which we want to
 * invalidate in the caches. (i.e. won't work for user virtual
 * addresses that the kernel converts to K0 before calling.)
 * addr/len are not necessarily cache line aligned.
 * Invalidate lines in both the primary and secondary cache.
 *
 */
#pragma weak picache_wb_inval = __icache_inval
void
__icache_inval(char* Addr, int len)
{
  register char *addr, *lineaddr, *endaddr;
  register int  addr_type = ((int)Addr >> 29) & 0x3;

  /*
   * Return if its uncached address.
   */
  if (addr_type == 5) {
    printf ("__cache_wb_inval was called with uncached address 0x%-8x\n",
            Addr);
    return;
  } else
    addr = (char*)(((uint32_t)Addr&0x5fffffff)|0x80000000);
  
  /*
   * make sure address is cached address, we assume the address given
   * is cached address, i.e. either mapped in TLBs as cached or KSEG0
   * based address.
   * Since hit writeback will not cause the inconsistence between
   * memory/scache/dcache, we simply choose the largest block
   * size (the _scache_linesize) 
   */
  endaddr  = (char*)(((uint32_t)addr + len) & ~_icache_linemask);
  lineaddr = (char*)((uint32_t)addr & ~_icache_linemask);
  do {
    CacheOP(PICACHE, HIT_WB_INVAL, (ADDR_t)lineaddr);
    lineaddr += _icache_linesize;
  } while (lineaddr <= endaddr);

  /*
   * Now see if we have a secondary cache.
   */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    endaddr  = (char*)(((uint32_t)addr + len) & ~_scache_linemask);
    lineaddr = (char*)((uint32_t)addr & ~_scache_linemask);
    do {
      CacheOP(SIDCACHE, HIT_WB_INVAL, (ADDR_t)lineaddr);
      lineaddr += _scache_linesize;
    } while (lineaddr <= endaddr);
  }

  /* Done   */
  return;
}    


/*****************************
 * __cache_wb_inval(addr, len)
 * ---------------------------
 * Uses the INDEX_INVALIDATE and INDEX_WRITEBACK_INVALIDATE
 * cacheops. Should be called with K0 addresses, to avoid
 * the tlb translation (and tlb miss fault) overhead.
 *
 * Since we are shooting down 'live' lines in the 2nd cache, these
 * lines have to be written back to memory before invalidating.
 * Since we are invalidating lines in the secondary cache which
 * will correspond to primary data cache lines, the primary data
 * cache has to be invalidated also (a line cannot be valid in
 * the primary but invalid in secondary.)
 * We have to invalidate the primary instruction cache for the
 * same reason.
 *
 * Since we are using index (writeback) invalidate cache ops on
 * the primary caches, and since the address we are given generally
 * has nothing to do with the virtual address at which the data may
 * have been used (otherwise we would use the 'hit' operations), we
 * perform the cache op at both indices in the primary caches. (Assumes
 * 4K page size and 8K caches.) This is necessary since the address
 * is basically meaningless.
 *
 * XXX We assume that this routine is not part of the stuff user would
 *
 * NOTE:  We'll have to invalidate both set if its true two set caches
 *        (Being taking care of by the low level logical cache ops).
 */
#pragma weak clear_cache = __cache_wb_inval
void
__cache_wb_inval(char* Addr, int len)
{
  volatile char *addr, *lineaddr, *endaddr;
  register int  addr_type = ((int)Addr >> 29) & 0x3;

  /*
   * Return if its uncached address.
   */
  if (addr_type == 5) {
    printf ("__cache_wb_inval was called with uncached address 0x%-8x\n",
            Addr);
    return;
  } else
    addr = (char*)(((uint32_t)Addr&0x5fffffff)|0x80000000);
  
  /*
   * We don't really care about the edges, its writeback invalidate, 
   * any dirty data will get write back to memory, nothing will go
   * wrong, simply align the target address to the largest cache block
   * size (usually is the 2nd cache) and flush it.
   */
  
  /* Do instruction cache first.  */
  endaddr  = (char*)(((uint32_t)addr + len) & ~_icache_linemask);
  lineaddr = (char*)((uint32_t)addr & ~_icache_linemask);
  do {
    CacheOP(PICACHE, IDX_INVAL, (ADDR_t)lineaddr);
    lineaddr += _icache_linesize;
  } while (lineaddr <= endaddr);
  
  /* Do data cache.               */
  endaddr  = (char*)(((uint32_t)addr + len) & ~_dcache_linemask);
  lineaddr = (char*)((uint32_t)addr & ~_dcache_linemask);
  do {
    CacheOP(PDCACHE, IDX_WB_INVAL, (ADDR_t)lineaddr);
    lineaddr += _dcache_linesize;
  } while (lineaddr <= endaddr);

  /* For the true set associated write back 2nd cache this is for it */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    endaddr  = (char*)(((uint32_t)addr + len) & ~_scache_linemask);
    lineaddr = (char*)((uint32_t)addr & ~_scache_linemask);
    do {
      CacheOP(SIDCACHE, IDX_WB_INVAL, (ADDR_t)lineaddr);
      lineaddr += _scache_linesize;
    } while (lineaddr <= endaddr);
  }

  /* Done   */
  return;
}

/****************
 * FlushAllCaches
 * --------------
 * For every supported platform the low level cache ops should
 * a flush function and will initialize the flushallcaches function
 * pointer.
 */
#pragma weak FlushAllCaches = flush_cache
void
flush_cache(void)
{
  _flushAllCache(0) ;
  return ;
}


/************
 * CacheValid
 * ----------
 * See if there is a valid cache line for the given virtual 
 * address.
 */
int
CacheValid(char* ADDR)
{
  return _validCacheAddr((ADDR_t)ADDR);
}


#if defined(DEBUG_CACH)
/******************
 * __testcache_haha
 */
void
__testcache_haha(ADDR_t Addr, int len)
{
  ADDR_t            Ledge, Tedge;
  register char     *addr = (char*)(((uint32_t)Addr&0x5fffffff)|0x80000000);
  register char     *lineaddr, *endaddr;
  
  /* register int      tmp; */
  /*
   * make sure address is cached address, we assume the address given
   * is cached address, i.e. either mapped in TLBs as cached or KSEG0
   * based address.
   */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    Ledge = (ADDR_t)addr & ~_scache_linemask;
    endaddr = (char*)(((ADDR_t)addr + (ADDR_t)len) & ~_scache_linemask);
    Tedge = (ADDR_t)endaddr + _scache_linesize;
  } else {
    Ledge = (ADDR_t)addr & ~_dcache_linemask;
    endaddr = (char*)(((ADDR_t)addr + (ADDR_t)len) & ~_dcache_linemask);
    Tedge = (ADDR_t)endaddr + _dcache_linesize;
  }

  printf ("  Addr  = 0x%08x addr  = 0x%08x len = 0x%-x\n",
          Addr, addr, len);
  printf ("  Ledge   = 0x%08x Tedge = 0x%08x\n", Ledge, Tedge);
  printf ("  endaddr = 0x%08x\n", endaddr);
  
  /* Taking care of the leading edge    */
  /* tmp = CacheValid(addr);            */
  if (CacheValid(addr)) {
    printf ("  Leading Edge -\n");
    while ((char*)Ledge < addr) {
      printf ("     Ledge = 0x%08x addr = 0x%08x\n", Ledge, addr);
      Ledge += 1;
    }
  }
  
  /* Taking care of the tailing edge    */
  /* tmp = CacheValid(endaddr);         */
  if (CacheValid(endaddr)) {
    Ledge = (ADDR_t)addr + len;
    printf ("  Tailing Edge Ledge = 0x%08x\n", Ledge);
    while ((char*)Ledge < (char*)Tedge) {
      printf ("     Tedge = 0x%08x Ledge = 0x%08x\n", Tedge, Ledge);
      Ledge += 1;
    }
  }
  
  /* Now is the easier part, invalidate the whole range */
   endaddr  = (char*)(((uint32_t)addr + len) & ~_dcache_linemask);
  lineaddr = (char*)((uint32_t)addr & ~_dcache_linemask);

  printf ("  endaddr = 0x%08x\n", endaddr);
  
  /* Invalidate primary cache first.    */
  do {
    printf ("    primary lineaddr = 0x%08x\n", lineaddr);
    lineaddr += _dcache_linesize;
  } while (lineaddr <= endaddr);
  
  /* See we have a secondary cache to invalidate.   */
  if ((_sidcache_size != 0) && (_scache_type != FAKE2ND)) {
    endaddr  = (char*)(((uint32_t)addr + len) & ~_scache_linemask);
    lineaddr = (char*)((uint32_t)addr & ~_scache_linemask);
    printf ("  endaddr = 0x%08x\n", endaddr);
    do {
      printf ("    secondary lineaddr = 0x%08x\n", lineaddr);
      lineaddr += _scache_linesize;
    } while (lineaddr <= endaddr);
  }
  /* Done   */
  return;
}
#endif
#undef DEBUG_CACH

#if defined(DEBUGR10K)
#define ADDRESSTST 1
#define WALKING01  1
extern  void Pinvalid(int);
extern  void ClearTagHiLo(void);
extern  void InitPTag(uint32_t,uint32_t,int);
extern  void InitTag(uint32_t,uint32_t,int);
extern  void LoadTag(int);
extern  void LoadPTag(int);
extern  void InitData(uint32_t,uint32_t,int);
extern  void LoadData(int);
extern  uint32_t readTagLo(void);
extern  uint32_t readTagHi(void);

/******************
 * __cacheram_test 
 */
void
__cacheram_test(void)
{
  int       idx, idx2, tmp;
  uint32_t  TagLo, TagHi, ETagLo, ETagHi, i;
  uint32_t  ZER0;

  printf ("\n**** SECONDARY CACHE TAG RAM TEST ****\n");

#if defined(WALKING01)
  /*
   * [1] Walking 1 through TagLo/TagHi for secondary cache tags.
   */
  IP32processorTCI();
  scache_set_size = RetriveSETsize();
  printf (" Walking 1 test\n");
  TagLo = TagHi = 1;
  for (idx=0; idx < 32; idx+=1) {
    ETagLo = TagLo & 0xffffcdff;
    ETagHi = TagHi & 0x8000000f;
    InitTag(ETagLo,ETagHi,0x80000000);
    ClearTagHiLo();
    LoadTag(0x80000000);
    if (ETagLo != (i = readTagLo())) {
      printf ("  Error: Expected TagLo = 0x%08x\n"
              "         Observed TagLo = 0x%08x\n", ETagLo, i);
    }
    if (ETagHi != (i = readTagHi())) {
      printf ("  Error: Expected TagHi = 0x%08x\n"
              "         Observed TagHi = 0x%08x\n", ETagHi, i);
    }
    TagLo = TagLo << 1;
    TagHi = TagHi << 1;
  }
  
  /*
   * [2] Walking 0 through TagLo/TagHi for secondary cache tags.
   */
  IP32processorTCI();
  scache_set_size = RetriveSETsize();
  printf (" Walking 0 test\n");
  TagLo = TagHi = 1;
  for (idx=0; idx < 32; idx+=1) {
    ETagLo = (~TagLo) & 0xffffcdff;
    ETagHi = (~TagHi) & 0x8000000f;
    InitTag(ETagLo,ETagHi,0x80000000);
    ClearTagHiLo();
    LoadTag(0x80000000);
    if (ETagLo != (i = readTagLo())) {
      printf ("  Error: Expected TagLo = 0x%08x\n"
              "         Observed TagLo = 0x%08x\n", ETagLo, i);
    }
    if (ETagHi != (i = readTagHi())) {
      printf ("  Error: Expected TagHi = 0x%08x\n"
              "         Observed TagHi = 0x%08x\n", ETagHi, i);
    }
    TagLo = TagLo << 1;
    TagHi = TagHi << 1;
  }
#endif
#if defined(ADDRESSTST)
  /*
   * [3] Walking address test for TAG ram
   */
  IP32processorTCI();
#if 0
    printf ("Write 0x00000001 to Tag<0x80000000>\n");
    InitTag(0x1,0x0,0x80000000);
    
    printf ("Write 0xfffffffe to Tag<0x80000001>\n");
    InitTag(0xfffffffe,0x0,0x80000001);

    ClearTagHiLo();
    LoadTag(0x80000000);
    printf ("Read from Tag<0x80000000> TagHi=0x%08x TagLo=0x%08x\n",
            readTagHi(), readTagLo());
    ClearTagHiLo();
    LoadTag(0x80000001);
    printf ("Read from Tag<0x80000001> TagHi=0x%08x TagLo=0x%08x\n",
            readTagHi(), readTagLo());
    while(1);
#endif
    
#if 1
  for (idx=0; idx < scache_set_size; idx+=_scache_linesize) {
    ETagLo = idx & 0xffffcdff;
    ETagHi = 0;
    if (idx == 0) ZER0 = ETagLo;
    InitTag(ETagLo,ETagHi,(0x80000000|idx));
    ClearTagHiLo();
    LoadTag(0x80000000);
    if ((i = readTagLo()) != ZER0) {
      printf ("For set 0 Tag<0x80000000> and set 0 Tag<0x%08x>\n",
              (0x80000000|idx));
      printf ("Tag<0x80000000> Value changed after write 0x%08x to Tag<0x%08x>\n"
              "   Expected= 0x%08x, Observed= 0x%08x\n", ETagLo,
              (0x80000000|idx), ZER0, i);
      while(1);
    }
  }
  for (idx=0; idx < scache_set_size; idx+=_scache_linesize) {
    ETagLo = idx & 0xffffcdff;
    ETagHi = 0;
    InitTag(ETagLo,ETagHi,(0x80000001|idx));
    ClearTagHiLo();
    LoadTag(0x80000000);
    if ((i = readTagLo()) != ZER0) {
      printf ("For set 0 Tag<0x80000000> and set 1 Tag<0x%08x>\n", (0x80000001|idx));
      printf ("Tag<0x80000000> Value changed after write 0x%08x to Tag<0x%08x>\n"
              "   Expected= 0x%08x, Observed= 0x%08x\n", ETagLo,
              (0x80000001|idx), ZER0, i);
      while(1);
    }
  }
#endif
  
  printf (" Walking address test\n");
  /* Set 0  */
  printf ("  Initialize Set 0\n");
  for (idx=0; idx < scache_set_size; idx+=_scache_linesize) {
    ETagLo = idx & 0xffffcdff;
    ETagHi = 0;
    InitTag(ETagLo,ETagHi,(0x80000000|idx));
  }
  /* Set 1 */
  printf ("  Initialize Set 1\n");
  for (idx=0; idx < scache_set_size; idx+=_scache_linesize) {
    ETagLo = (~idx) & 0xffffcdff;
    ETagHi = 0;
    InitTag(ETagLo,ETagHi,(0x80000001|idx));
  }
  /* Check Set 0 */
  printf ("  Verify Set 0\n");
  for (idx=0; idx < scache_set_size; idx+=_scache_linesize) {
    ETagLo = idx & 0xffffcdff;
    ETagHi = 0;
    ClearTagHiLo();
    LoadTag((0x80000000|idx));
    if (ETagLo != (i = readTagLo())) {
      printf ("  Error: Expected TagLo = 0x%08x\n"
              "         Observed TagLo = 0x%08x\n", ETagLo, i);
    }
    if (ETagHi != (i = readTagHi())) {
      printf ("  Error: Expected TagHi = 0x%08x\n"
              "         Observed TagHi = 0x%08x\n", ETagHi, i);
    }
  }
  /* Check Set 1 */
  printf ("  Verify Set 1\n");
  for (idx=0; idx < scache_set_size; idx+=_scache_linesize) {
    ETagLo = (~idx) & 0xffffcdff;
    ETagHi = 0;
    ClearTagHiLo();
    LoadTag((0x80000001|idx));
    if ((int)ETagLo != (tmp = readTagLo())) {
      printf ("  Error: Expected TagLo = 0x%08x\n"
              "         Observed TagLo = 0x%08x\n", ETagLo, tmp);
    }
    if ((int)ETagHi != (tmp = readTagHi())) {
      printf ("  Error: Expected TagHi = 0x%08x\n"
              "         Observed TagHi = 0x%08x\n", ETagHi, tmp);
    }
  }
#endif
  
#if defined(WALKING01)
  /*
   * [4] Walking 1 through the lower 64 bits of the 128 bit bus.
   */
  IP32processorTCI();
  printf ("\n**** SECONDARY CACHE DATA RAM TEST ****\n");

  printf ("  Data Ram walking 1 test - lower 64 bits\n");
  TagLo = 1;
  TagHi = 0;
  for (idx = 0; idx < 2; idx += 1) {
    for (idx2 = 0; idx2 < 32; idx2 += 1) {
      ETagLo = TagLo;
      ETagHi = TagHi;
      InitData(TagLo,TagHi,0x80000000);
      LoadData(0x80000000);
      if (TagLo != (i = readTagLo())) {
        printf ("  Error: Expected BIT<31:0>(TagLo) = 0x%08x\n"
                "         Observed BIT<31:0>(TagLo) = 0x%08x\n",
                TagLo, i);
      }
      if (TagHi != (i = readTagHi())) {
        printf ("  Error: Expected BIT<63:32>(TagHi) = 0x%08x\n"
                "         Observed BIT<63:32>(TagHi) = 0x%08x\n",
                TagHi, i);
      }
      LoadData(0x80000008);
      if ((i = readTagLo()) != 0x0) {
        printf ("  Error: Expected BIT<95:64>(TagLo) = 0x0\n"
                "         Observed BIT<95:64>(TagLo) = 0x%08x\n", i);
      }
      if ((i = readTagHi()) != 0x0) {
        printf ("  Error: Expected BIT<127:96>(TagHi) = 0x0\n"
                "         Observed BIT<127:96>(TagHi) = 0x%08x\n", i);
      }
      TagLo = TagLo << 1;
      TagHi = TagHi << 1;
      printf ("         test data = 0x%08x%08x\n", ETagHi, ETagLo);
    }
    TagLo = 0;
    TagHi = 1;
  }

  /*
   * [5] Walking 0
   */
  IP32processorTCI();
  printf ("  Data Ram walking 0 test - lower 64 bits\n");
  TagLo = 1;
  TagHi = 0;
  for (idx = 0; idx < 2; idx += 1) {
    for (idx2 = 0; idx2 < 32; idx2 += 1) {
      ETagLo = ~TagLo;
      ETagHi = ~TagHi;
      InitData(ETagLo,ETagHi,0x80000000);
      LoadData(0x80000000);
      if (ETagLo != (i = readTagLo())) {
        printf ("  Error: Expected BIT<31:0>(TagLo) = 0x%08x\n"
                "         Observed BIT<31:0>(TagLo) = 0x%08x\n",
                ETagLo, i);
      }
      if (ETagHi != (i = readTagHi())) {
        printf ("  Error: Expected BIT<63:32>(TagHi) = 0x%08x\n"
                "         Observed BIT<63:32>(TagHi) = 0x%08x\n",
                ETagHi, i);
      }
      LoadData(0x80000008);
      if ((i = readTagLo()) != 0x0) {
        printf ("  Error: Expected BIT<95:64>(TagLo) = 0x0\n"
                "         Observed BIT<95:64>(TagLo) = 0x%08x\n", i);
      }
      if ((i = readTagHi()) != 0x0) {
        printf ("  Error: Expected BIT<127:96>(TagHi) = 0x0\n"
                "         Observed BIT<127:96>(TagHi) = 0x%08x\n", i);
      }
      TagLo = TagLo << 1;
      TagHi = TagHi << 1;
      printf ("         test data = 0x%08x%08x\n", ETagHi, ETagLo);
    }
    TagLo = 0;
    TagHi = 1;
  }
#endif
#if defined(ADDRESSTST)
  /*
   * [6] Walking address test for data ram.
   */
  IP32processorTCI();
  scache_set_size = RetriveSETsize();
  printf ("  Data Ram walking address test - lower 64 bits\n");
  printf ("       - Initialize data ram to its address\n");
  printf ("         scache_set_size = 0x%08x\n", scache_set_size);
  printf ("         address =           ");
  for (idx = 0; idx < scache_set_size; idx += 16) {
    TagLo = idx;
    TagHi = ~TagLo;
    InitData(TagLo,TagHi,(0x80000000|idx));
    TagLo += 1;
    TagHi = ~TagLo;
    InitData(TagLo,TagHi,(0x80000001|idx));
    if ((idx & 0xfff) == 0x0) printf ("\b\b\b\b\b\b\b\b\b\b0x%08x",idx);
  }
  scache_set_size = RetriveSETsize();
  printf ("\n       - Check addressability\n");
  printf ("           scache_set_size = 0x%08x\n",scache_set_size);
  printf ("           address =           ");
  for (idx2 = 0; idx2 < 2; idx2 += 1) {
    for (idx = 0; idx < scache_set_size; idx += 16) {
      TagLo = idx + idx2;
      TagHi = ~TagLo;
      ClearTagHiLo();
      LoadData(0x80000000|TagLo);
      if (TagLo != (i = readTagLo())) {
        printf ("  Error: Address  = 0x%08x\n"
                "         Expected BIT<31:0>(TagLo)  = 0x%08x\n"
                "         Observed BIT<31:0>(TagLo)  = 0x%08x\n"
                "         Cache set %d data ram addr = 0x%08x\n",
                (0x80000000|TagLo), TagLo, i, idx2, idx);
      }
      if (TagHi != (i = readTagHi())) {
        printf ("  Error: Address = 0x%08x\n"
                "         Expected BIT<63:32>(TagHi)  = 0x%08x\n"
                "         Observed BIT<63:32>(TagHi)  = 0x%08x\n",
                "         Cache set %d data ram addr  = 0x%08x\n",
                (0x80000000|TagLo),TagHi, i, idx2, idx);
      }
      LoadData(0x80000008|TagLo);
      if ((i = readTagLo()) != 0x0) {
        printf ("  Error: Address = 0x%08x\n"
                "         Expected BIT<95:64>(TagLo)  = 0x0\n"
                "         Observed BIT<95:64>(TagLo)  = 0x%08x\n"
                "         Cache set %d data ram addr  = 0x%08x\n",
                (0x80000008|TagLo), i, idx2, idx);
      }
      if ((i = readTagHi()) != 0x0) {
        printf ("  Error: Address = 0x%08x\n"
                "         Expected BIT<127:64>(TagHi)  = 0x0\n"
                "         Observed BIT<127:64>(TagHi)  = 0x%08x\n",
                "         Cache set %d data ram addr   = 0x%08x\n",
                (0x80000008|TagLo), i, idx2, idx);
      }
      if ((TagLo & 0xfff) == 0x0) printf ("\b\b\b\b\b\b\b\b\b\b0x%08x",TagLo);
    }
  }
  printf ("\n");
#endif  
}

/****************
 * __dataram_test 
 */
void
__dataram_test(void)
{
  int       idx, idx1, idx2;
  uint32_t  TagLo, TagHi, ETagLo, ETagHi, i;

  printf ("Setting up cache and memory for cache test\n");
  
  IP32processorTCI();
  
  printf ("Initialize memory 0xa2000000 to increment data pattern\n");
  idx2 = 0x00010203;
  for (idx1 = 0; idx1 < 0x40; idx1 += 4) {
    *(uint32_t*)(0xa2000000|idx1) = idx2 ;
    idx2 += 0x04040404;
  }
    
  printf ("Initialize memory 0xa1000000 to decrement data pattern\n");
  idx2 = 0x00010203;
  for (idx1 = 0; idx1 < 0x40; idx1 += 4) {
    *(uint32_t*)(0xa1000000|idx1) = ~idx2 ;
    idx2 += 0x04040404;
  }

  printf ("Initialize memory 0xa0800000 to 5a5a5a5a pattern\n");
  for (idx1 = 0; idx1 < 0x40; idx1 += 4) {
    *(uint32_t*)(0xa0800000|idx1) = 0x5a5a5a5a;
  }
    
  printf ("Initialize memory 0xa0400000 to deadbeef  data pattern\n");
  for (idx1 = 0; idx1 < 0x40; idx1 += 4) {
    *(uint32_t*)(0xa0400000|idx1) = 0xdeadbeef;
  }

  printf ("[01] Read    0x80400000 - 0x%08x\n",
          *(uint32_t*)0x80400000);
  
  printf ("[02] Modify  0x80400000 with 0x03020100\n");
  *(uint32_t*)0x80400000 = 0x03020100;
  
  printf ("[03] Read    0x80800000 - 0x%08x\n",
          *(uint32_t*)0x80800000);
  
  printf ("[04] Modify  0x80800000 with 0xccddeeff\n");
  *(uint32_t*)0x80800000 = 0xccddeeff;
  
  printf ("[05] Read    0x81000000 - 0x%08x\n",
          *(uint32_t*)0x81000000);
  

  printf ("[06] Read    0xa0800000 - 0x%08x\n",
          *(uint32_t*)0xa0800000);
  
  printf ("[06] Read    0xa0400000 - 0x%08x\n",
          *(uint32_t*)0xa0400000);
  

    printf ("*************  T5 CACHE TEST   *************\n");
    TagLo = 0xdeadbeef;
    TagHi = 0xbeefdead;
    InitData(TagLo,TagHi,0x80000040);
    printf ("Write TagHi=0x%08x TagLo=0x%08x to 0x80000040\n",
            TagHi, TagLo);
    
    TagLo = 0x5a5a5a5a;
    TagHi = 0xa5a5a5a5;
    InitData(TagLo,TagHi,0x80000041);
    printf ("Write TagHi=0x%08x TagLo=0x%08x to 0x80000041\n",
            TagHi, TagLo);

    ClearTagHiLo();
    LoadData(0x80000040);
    printf ("Read From 0x800000040 TagHi=0x%08x TagLo=0x%08x\n",
            readTagHi(), readTagLo());
    
    ClearTagHiLo();
    LoadData(0x80000048);
    printf ("Read From 0x800000048 TagHi=0x%08x TagLo=0x%08x\n",
            readTagHi(), readTagLo());
    
    ClearTagHiLo();
    LoadData(0x80000041);
    printf ("Read From 0x800000041 TagHi=0x%08x TagLo=0x%08x\n",
            readTagHi(), readTagLo());
    
    ClearTagHiLo();
    LoadData(0x80000049);
    printf ("Read From 0x800000049 TagHi=0x%08x TagLo=0x%08x\n",
            readTagHi(), readTagLo());

    InitData(0,0,0x80000000);
    InitData(0,0,0x80000010);
    InitData(0,0,0x80000020);
    InitData(0,0,0x80000030);
    InitTag(0xc00,0,0x80000000);
    InitPTag(0xc0,0x40000000,0x80000000);
    InitPTag(0xc0,0x40000000,0x80000020);
    for (idx=0;idx < 64;idx+=1)
      *(char*)(0x80000000|idx) = idx;
    printf ("Push data out to secondary cache.\n");
    
    Pinvalid(0x80000000);
    Pinvalid(0x80000020);
    
    LoadPTag(0x80000000);
    printf (" Primary cache data Tag 0x80000000\n"
            " TagHi = 0x%08x\n"
            " TagLo = 0x%08x\n", readTagHi(), readTagLo());
    LoadPTag(0x80000020);
    printf (" Primary cache data Tag 0x80000020\n"
            " TagHi = 0x%08x\n"
            " TagLo = 0x%08x\n", readTagHi(), readTagLo());
    
    printf ("Read it back from secondary cache.\n");
    for (idx=0;idx<64;idx+=4)
      printf ("word<%d>=0x%08x\n", idx,*(uint32_t*)(0x80000000|idx));

}
#endif
#undef DEBUGR10K

/* END OF FILE */
