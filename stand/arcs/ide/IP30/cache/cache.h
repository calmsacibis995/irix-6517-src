#ifndef __IDE_CACHE_H__
#define __IDE_CACHE_H__
/*
 * cache.h 
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */
#ident "ide/IP30/cache/cache.h: $Revision: 1.15 $"

#include "sys/RACER/IP30addrs.h"

/* selects cache(s) for alter.  Note that
 * since the icache tests are separated, the BOTH option refers to
 * the primary data and (combined) secondary. */
#define PRIMARYD	0
#define PRIMARYI	1
#define SECONDARY	2
#define BOTH		3
#define DO_MEMORY	4

/* ide uses between 8MB and 14MB only (6MB): set PHYS_CHECK_LO above it */
#define	PHYS_CHECK_LO	IP30_SCRATCH_MEM

#define	SR_FORCE_ON	(SR_CU0 | SR_CU1 | SR_CE | SR_IEC | SR_KX)
#define	SR_FORCE_OFF	(SR_CU0 | SR_CU1 | SR_IEC | SR_KX)
#define	SR_FORCE_NOERR	(SR_CU0 | SR_CU1 | SR_CE | SR_DE | SR_KX)
#define	SR_NOERR	(SR_CU0 | SR_CU1 | SR_DE | SR_KX)

#if LANGUAGE_C
int calc_r10k_parity(__uint64_t);
#endif

/*
 * C0 CACHE_ERR definitions for R10k
 */

/* Defs for Primary Instruction Cache Errors */
#define CACHERR_IC_CODE 0x0
#define CACHERR_IC_EW   0x20000000
#define CACHERR_IC_D    0x0C000000
#define CACHERR_IC_TA   0x03000000
#define CACHERR_IC_TS   0x00C00000
#define CACHERR_IC_PIDX 0x00003FC0

/* Defs for Primary Data Cache Errors */
#define CACHERR_DC_CODE 0x1
#define CACHERR_DC_EW   0x20000000
#define CACHERR_DC_EE   0x10000000
#define CACHERR_DC_D    0x0C000000
#define CACHERR_DC_TA   0x03000000
#define CACHERR_DC_TS   0x00C00000
#define CACHERR_DC_TM   0x00300000
#define CACHERR_DC_PIDX 0x00003FF8

/* Defs for Seconday Cache Errors */
#define CACHERR_SC_CODE 0x2
#define CACHERR_SC_EW   0x20000000
#define CACHERR_SC_D    0x0C000000
#define CACHERR_SC_TA   0x03000000
#define CACHERR_SC_SIDX 0x007FFFC0

/* Defs for System Interface Errors */
#define CACHERR_SI_CODE 0x3
#define CACHERR_SI_EW   0x20000000
#define CACHERR_SI_EE   0x10000000
#define CACHERR_SI_D    0x0C000000
#define CACHERR_SI_SA   0x02000000
#define CACHERR_SI_SC   0x01000000
#define CACHERR_SI_SR   0x00800000
#define CACHERR_SI_SIDX 0x007FFFC0

/*
 * C0 TAGLO and TAGHI definitions needed for CacheOp Index Load/Store
 */
/* XXX any of these are not in sys/r10k.h? */

/* masks to be used w/ 64 bit quantities */
#define PTAG0 0x0000000ffffff000
#define PTAG1 0x000000f000000000

/* PRIMARY INSTRUCTION CACHE */

/* masks */
#define TAGLO_PI_TP		0x01		/* tag parity */
#define TAGLO_PI_SP		0x04		/* state parity */
#define TAGLO_PI_LRU		0x08
#define TAGLO_PI_PSTATE		0x40
#define TAGLO_PI_PTAG0		0xffffff00
#define TAGHI_PI_PTAG1		0x00000007
/* shifts */
#define TAGLO_PI_PTAG0_SHIFT	4
#define TAGLO_PI_PTAG0_SHIFT_2	8
#define TAGLO_PI_PSTATE_SHIFT	6
#define TAGHI_PI_PTAG1_SHIFT	36

/* PRIMARY DATA CACHE */

/* masks */
#define TAGLO_PD_TP 0x1
#define TAGLO_PD_WAY 0x2
#define TAGLO_PD_SP 0x4
#define TAGLO_PD_LRU 0x8
#define TAGLO_PD_PSTATE 0xC0
#define TAGLO_PD_PSTATE_INVALID   0x0000   /* invalid --> 00 == state 0 */
#define TAGLO_PD_PSTATE_SHARED    0x0040   /* shared --> 01 == state 1*/
#define TAGLO_PD_PSTATE_CLEANEXCL 0x0080   /* clean exclusive --> 10 == state 2 */
#define TAGLO_PD_PSTATE_DIRTYEXCL 0x00C0   /* dirty exclusive --> 11 == state 3 */
#define TAGHI_PD_SM 0xE0000000             /* state modifier */
#define TAGLO_PD_PTAG0 0xffffff00
#define TAGHI_PD_PTAG1 0x00000007
/* shifts */
#define TAGLO_PD_PSTATE_SHIFT 6
#define TAGLO_PD_PTAG0_SHIFT 8
#define TAGHI_PD_PTAG1_SHIFT 24

/* SECONDARY CACHE */

/* masks */
#define TAGLO_SC_ECC		0x7F
#define TAGLO_SC_VINDEX		0x180
#define TAGLO_SC_SSTATE		0xC00
#define TAGLO_SC_SSTATE_INVALID   0x000    /* invalid --> 00 == state 0 */
#define TAGLO_SC_SSTATE_SHARED    0x400    /* shared --> 01 == state 1*/
#define TAGLO_SC_SSTATE_CLEANEXCL 0x800    /* clean exclusive --> 10 == state 2 */
#define TAGLO_SC_SSTATE_DIRTYEXCL 0xC00    /* dirty exclusive --> 11 == state 3 */
#define TAGLO_SC_STAG0		0xFFFFE000
#define TAGHI_SC_STAG1		0x7
#define TAGHI_SC_MRU		0x80000000
/* shifts */
#define TAGLO_SC_VINDEX_SHIFT	7
#define TAGLO_SC_SSTATE_SHIFT	10
#define TAGLO_SC_STAG0_SHIFT	13
#define TAGHI_SC_STAG1_SHIFT	18

#ifndef	LOCORE

#include <sys/types.h>

/* get_tag rolls phys addrs from the taglo register to their correct
 * 31-bit positions before placing them in the tag_info.physaddr
 * field for easy comparision.  The below masks return the valid portion
 * of the address (depending upon which cache the tag is from).
 */
#define	PINFOADDRMASK	0xFFFFF000
#define	SINFOADDRMASK	0xFFFE0000
/* top bit if phys addrs in taglo is 35: we want it to be 31 so roll left 4 */
#define	TAGADDRLROLL	4

/* roll the 3 vindex (2ndary cache) bits from their taglo spot (9..7)
 * to their real positions (14..12) for easy comparision by get_tag_info */
#define	SVINDEXLROLL	5
#define	INFOVINDMASK	0x7000

/* define bit-rolls to get cache-state portion to bottom of tag_lo word.  */
#define	PSTATE_RROLL	6	/* tag_lo bits 7..6 indicate pcache state */
#define	SSTATE_RROLL	10	/* tag_lo bits 12..10 indicate scache state */

/* several primitives return MISSED_2NDARY when a cache instruction on
 * an address doesn't hit the secondary cache */
#define	MISSED_2NDARY	(-2)

/* sbd.h contains the masks for accessing the primary and secondary
 * fields in the TagLo register.  The below #defines manipulate
 * those values into positions in the tag_info struct and then mask
 * the valid parts of those fields for use by get_tag_info.
 *
 * The tag_info struct holds the state (in the low 2 or 3 bits depending
 * upon which cache is being queried), the 3 Vindex bits shifted to
 * their proper position for use (if the tag is from secondary cache),
 * and the upper bits of the physaddr, also shifted to their useable
 * positions.  If the tag is from a primary line the tag reported bits
 * 35..12, so physaddr contains bits 31..12 of the physical address.
 * If it is a secondary line, the tag contained bits 35..17, so physaddr
 * will contain bits 31..17.
 */
typedef struct tag_info {
	ushort state;
	ushort vindex;
	caddr_t  physaddr;
} tag_info_t;

/* tag_regs structs hold the contents of the TagHi and TagLo registers,
 * which cache tag instructions use to read and write the primary and
 * secondary caches.
 */
typedef struct tag_regs {
	unsigned int tag_lo;
	unsigned int tag_hi;
} tag_regs_t;


extern int _sidcache_size;
extern int _scache_linesize;
extern int _scache_linemask;
extern int _icache_size;
extern int _icache_linesize;
extern int _icache_linemask;
extern int _dcache_size;
extern int _dcache_linesize;
extern int _dcache_linemask;

#define	PD_SIZE		_dcache_size
#define	PDL_SIZE	_dcache_linesize
#define	PI_SIZE		_icache_size
#define	PIL_SIZE	_icache_linesize
#define	SID_SIZE	_sidcache_size
#define	SIDL_SIZE	_scache_linesize

extern int flush2ndline(__psunsigned_t physaddr);
extern int get_tag_info(int which, __psunsigned_t physaddr, int way,
			tag_info_t *tag_info);
extern int pd_HINV(volatile uint *vaddr);
extern int pd_HWBINV(volatile uint *vaddr);
extern int sd_HWBINV(volatile uint *vaddr);
extern int pi_HINV(volatile uint *vaddr);
extern int scachedata(void);
extern int scacheaddr(void);
extern int sd_HWB(volatile uint *vaddr);
extern void DoEret(void);
extern void DoErrorEret(void);
extern void dpline_to_secondary(void *vaddr);
extern void fill_ipline(volatile uint *addr);
extern void flushall_tlb(void);
extern void pd_iwbinv(volatile uint *vaddr);
extern void read_tag(int, __psunsigned_t physaddr, tag_regs_t *tag_info);
extern void sd_hitinv(volatile uint *vaddr);
extern void write_tag(int, __psunsigned_t physaddr, tag_regs_t *tag_info);
unsigned int time_function (__psunsigned_t);
void write_icache_line(void *addr, int way, unsigned long data);
unsigned long read_icache_word(void *addr, int way);

extern jmp_buf cache_fault_buf;
#endif	/* LOCORE */
 
#endif  /* __IDE_CACHE_H__ */
