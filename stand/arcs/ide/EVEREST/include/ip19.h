#ifndef __IDE_EVEREST_IP19_H__
#define __IDE_EVEREST_IP19_H__

/*
 * /include/ip19.h
 *
 *	-- defines, addresses, and configs for IP19, IP21.
 *
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.15 $"

#include <sys/sbd.h>
#include <setjmp.h>
#include <sys/EVEREST/everest.h>

#ifndef _LANGUAGE_ASSEMBLY
#include <libsk.h>
#include <memstr.h>
#include <parser.h>
#include <fault.h>
#endif /* !_LANGUAGE_ASSEMBLY */

#define FPSR_COND	0x800000	/* condition bit in FPU status reg */
#define FPSR_ENABLE	0xf80		/* enable bits in FPU status reg */

#define BASE1		0x02000000	/* memory tests begin at 32 meg */
					/* must be a multiple of CHUNK_SIZE */
/* PHYS_TO_MEM replaces PHYS_TO_K1 for IP19 in name only. This is done to allow
   the IP21 to use a different set of addresses for accessing the local
   registers and cache tag areas
*/
#define PHYS_TO_MEM(x)  PHYS_TO_K1(x)

#define getcpu_loc(loc)	{ \
    loc[0] = (EV_GET_LOCAL(EV_SPNUM) & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT; \
    loc[1] = (EV_GET_LOCAL(EV_SPNUM) & EV_PROCNUM_MASK); \
}

#define uc_bwrite(x,y)	*(unsigned char *)PHYS_TO_MEM(x) = y
#define uc_bread(x)	*(unsigned char *)PHYS_TO_MEM(x)
#define c_wwrite(x,y)	*(unsigned int *)x = y
#define c_wread(x)	*(unsigned int *)x

#define MAX_PRIM_LSIZE	32	/* IP17: 16 or 32 byte primary line length */
#define MAX_2ND_LSIZE	128	/* IP17: 32, 64, or 128 byte 2nd level lines */

/* SET_SHIFT was a constant in all previous machines: (Log base 2 of #bytes
 * in 2nd cache line, == 4).  In IP17 this value varies from 4 to 7 (linesize
 * of 16 - 128 bytes).  */
#define FOUR_WD_2NDLINE		16
#define EIGHT_WD_2NDLINE	32
#define SIXTEEN_WD_2NDLINE	64
#define THIRTYTWO_WD_2NDLINE	128

/* mask clears to 32-byte boundaries */
#define	ALIGN_TO_32	(0xffffffe0)
#define FILL_CHUNK_SIZE	32	/* pathasm.s:back* fns fill 32-byte chunks */

#define MAX_PHYS_MEM	0x1fffffff	/* 256 meg--max physical mem on IP17 */

/* start generic filling at 4Meg--start of either 1M or 4M secondary cache */
#define PHYS_FILL_LO	BASE1		/* start generic filling at 4M */

#define PHYS_CHECK_LO	BASE1	/* mem checking begins at this addr */

/* check_states() uses a bit field to determine what pieces of the 
 * caches and memory to check, given the physical addr. */

/* selects cache(s) for several path/ primitives to alter.  Note that
 * since the icache tests are separated, the BOTH option refers to 
 * the primary data and (combined) secondary. */
#define PRIMARYD	0
#define PRIMARYI	1
#define SECONDARY	2
#define BOTH		3
#define DO_MEMORY	4

#define TLBLO_19HWBITS		0x3fffffff	/* 24 bit ppn, CDVG */
#define TLBHI_TWIDBITS	(TLBHI_VPNMASK|TLBHI_PIDMASK)


#ifndef _LANGUAGE_ASSEMBLY

/* 
 * configuration struct -- config parms of IP17--some determined at startup
 */

struct config_info {
					/* all cache sizes in bytes */
	int	icache_size;		/* primary instruction cache size */
	int	iline_size;		/* primary instruction line size */
	int	dcache_size;		/* primary data cache size */
	int	dline_size;		/* primary data line size */
	int	sidcache_size;		/* second level instr-data cache size */
	int	sidline_size;		/* second level line size */


	int	set_mask;		/* mask of set */
	int	tag_mask;		/* mask of set */
	int	key_shift;		/* 18 for IP15, 16 for others */
	unsigned int     stop:1,	/* CTRL S and CTRL Q */
		ctrlc : 1,		/* CTRL C */
		noctrlc : 1,		/* NO CTRL C */
		      :29;
};

extern struct config_info config_info;

/*
struct check_info {
	unsigned int check_list;  
	unsigned int match_list; 
	unsigned int phys_addr;	
	unsigned int caller;
	unsigned int value;
	unsigned int pdline;
	unsigned int pdtag;
	unsigned int piline;
	unsigned int pitag;
	unsigned int sidline;
	unsigned int sidtag;
	unsigned int memory;


} check_info_t;
*/

/*
 * addr_range structure  for parse_addr()
 */
struct addr_range {
        int lo;                         /* beginning address */
        int hi;                         /* ending address */
        char size;                      /* byte, half word or word */
};
extern struct addr_range addr_range;

/* In reading and writing the cache tags, some of the bits are
 * computed during the write (ECC or parity), and some of the bits
 * indicate illegal states (only 2 of the possible 4 Primary Data 
 * cache states are valid for primary instruction caches even though
 * the taglo definition is shared).  These bits will not read back
 * as they were written and must be exempted from any compares via masks.
 * - The lower 6 bits of the PTagLo register must not be tested for 
 *   primary DATA caches (5 bits undefined + 1 parity bit).
 * - The lower 8 bits of the PTagLo register must not be tested for 
 *   Primary INSTRUCTION caches (as P. Data above, plus the next two
 *   (state) bits, due to the fewer legal states in the instruction cache).
 * - The lower 7 bits of STagLo for the ECC field.
 */
#define P_D_IGNMASK	0x0000003f
#define P_I_IGNMASK	0x000000ff
#define S_IGNMASK	0x0000007f

/* The Unique_Tag_Addrs() tag test in path/cache_rams.c verfies that 
 * all the tags can be uniquely address; in order to generate unique
 * yet easily-recognized addr patterns (given that the lower 7 or less
 * bits can't be written), the address is shifted up TAG_IGN_SHIFT
 * bits for all tags. */
#define TAG_IGN_SHIFT	8

#define PAT_FUNC(x,addr)	((5 * x) + addr + 1)

/* define macro to compute maximum # of 2ndary cache-tags possible for IP17.
 * Some routines need it to size arrays (created dynamically) for tag-storage.
 * One tag per line; max-lines depends on scache-size */
#define MAX_SCACHE_SIZE	0x400000	/* R4K max 2ndary cache size is 4mb */
#define SCACHE_LSIZE	0x80		/* IP17 can't vary 2ndary linesize */
#define MAX_S_TAGS	(MAX_SCACHE_SIZE / SCACHE_LSIZE)

/* some of the diagnostics continue on through errors, logging them up
 * to a certain point.  NUM_ERRORS_ALLOWED is defines that point. */
#define NUM_ERRORS_ALLOWED	3

/* path/cache_states.c: runem uses a jump-table of functions returning ints
 * with 2 unsigned-integer parameters */
typedef int (*PFI)(__psunsigned_t, uint);

/* and others return uints and need one uint parameter */
typedef uint (*PFUI)(uint);


enum mem_space { k0seg,		/* cached, unmapped access */
		 k1seg,		/* uncached, unmapped access */
		 k2seg		/* mapped; specified caching strategy */
};	



/* indicates which cache state to set.
 * Some are specific to primary or secondary.  Note that DIRTY_EXCL in
 * primary sets the writeback bit therefore acting like the secondary 
 * does in that state.  The DIRTY_EXCL_NWB state applies only to the
 * primary and is the special case where a line is read from a dirty
 * secondary but not modified in the primary. */
enum c_states {	INVALIDL,		/* P or S: cache line is invalid */
		CLEAN_EXCL,	/* P or S: clean exclusive */
		DIRTY_EXCL,	/* P or S: dirty exclusive */
		DIRTY_EXCL_NWB	/* P: Dirty Excl. w/ writeback bit clear */
};

/* path/mem_init() either sets memory to the value specified by the 'pattern'
 * parm (M_I_PATTERN mode below), an ascending value incremented by 4 per
 * word, with starting value specified by 'pattern' parm (M_I_ASCENDING mode),
 * the ascending one's complement of 'pattern' (M_I_ASCENDING_INV), or an
 * alternating-one's complement of the specified pattern (M_I_ALT_PATTERN).
 */
enum fill_mode	{ M_I_PATTERN,M_I_ASCENDING,M_I_ASCENDING_INV,M_I_ALT_PATTERN };


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
	uint physaddr;
} tag_info_t;

/* get_tag rolls phys addrs from the taglo register to their correct
 * 31-bit positions before placing them in the tag_info.physaddr 
 * field for easy comparision.  The below masks return the valid portion
 * of the address (depending upon which cache the tag is from).
 */
#define PINFOADDRMASK	0xFFFFF000
#define SINFOADDRMASK	0xFFFE0000
/* top bit if phys addrs in taglo is 35: we want it to be 31 so roll left 4 */
#define TAGADDRLROLL	4



/* roll the 3 vindex (2ndary cache) bits from their taglo spot (9..7)
 * to their real positions (14..12) for easy comparision by get_tag_info */
#define SVINDEXLROLL	5
#define INFOVINDMASK	0x7000

/* define bit-rolls to get cache-state portion to bottom of tag_lo word.  */
#define PSTATE_RROLL	6	/* tag_lo bits 7..6 indicate pcache state */
#define SSTATE_RROLL	10	/* tag_lo bits 12..10 indicate scache state */

/* DONT_DOIT is used by a variety of routines to indicate that a particular
 * parameter is a dummy for that particular call, and to NOT do the operation
 * for that parameter. */
#define UINTMINUSONE	(0xFFFFFFFF)
#define DONT_DOIT	UINTMINUSONE
#define DONT_CHECK	DONT_DOIT
#define DONT_PRINTIT	DONT_DOIT

/* several primitives return MISSED_2NDARY when a cache instruction on
 * an address doesn't hit the secondary cache */
#define MISSED_2NDARY	(-2)


/* mem/memsubs.c uses 32 of these chunks to determine how much phys mem
 * the system has.  8*1024*1024*32 == 2**28 == 256meg == max. phys mem.
 */
#define CHUNK_SIZE      4*1024*1024

/* Most of the IP17stress diagnostic routines are coded to run icached or
 * not icached.  The following enum tells the routines which mode to use.
 */
enum run_mode	{ ICACHED, IUNCACHED };

/* memory-sizes are given in hex.  Here are a few handy mnemonic defines
 * for loop values, etc.
 */
#define _4_K		0x001000
#define _8_K		0x002000
#define _16_K		0x004000
#define _32_K		0x008000
#define _64_K		0x010000
#define _128_K		0x020000

#define _1_MEG		0x100000
#define _2_MEG		0x200000
#define _3_MEG		0x300000
#define _4_MEG		0x400000
#define _5_MEG		0x500000
#define _6_MEG		0x600000
#define _7_MEG		0x700000
#define _8_MEG		0x800000
#define _9_MEG		0x900000
#define _10_MEG		0x1000000


/* Decimal iteration-counts for diag loops.  Base 10 is more 
 * intuitive for most humans than hex
 */
#define TEN_THOU	10000
#define HUNDRED_THOU	100000
#define ONE_MIL		1000000
#define FIVE_MIL	5000000
#define TEN_MIL		10000000
#define HUNDRED_MIL	100000000


#define SEC_VADDR_MASK	(_128_K-1)
#define PRIM_VADDR_MASK	(_4_K-1)

/* many of the cache-test routines change a value in a cache to ensure
 * that it does or does not flush out.  GENERIC_INC is commonly used as
 * that increment-value. */
#define GENERIC_INC	3

typedef struct tlbptepairs {
	uint evenpg;
	uint oddpg;
} tlbptepairs_t;

#define RETCSERROR	(!CSError ? 0 : 1)

/* we want to ensure that the space we copy code into (for uncached 
 * access) never maps to a cached-data access.  Therefore we sandwich
 * the actual array between two 2nd-cache-sized lines.  (The second-
 * cache-linesize constant is currently correct but could change.)
 */
#define SEC_CACHE_LSIZE	0x80
#define CACHED_CODE_SIZE	0x1000
#define EXTRA_SPACE	(2*(SEC_CACHE_LSIZE/sizeof(uint)))


/* 2ndary cache tags consist of 25 data bits monitored by 7 checkbits.
 * The internal format of the tags is different than when fetched into
 * the taglo register.  These overly-complicated defines and macros
 * allow the taglo format to be converted into the internal one, which
 * anyone trying to diagnose hardware problems needs. */
#define STAG_DBIT_SIZE	25
#define STAG_CBIT_SIZE	7
#define STAG_SIZE	(STAG_DBIT_SIZE+STAG_CBIT_SIZE)

/* S_taglo field format:
 *	bitpositions-->    31..13  12..10  9..7  6..0
 *	fields --> 	 < p_addr, cstate, vind, ecc >.
 * Internal format:
 *			   31..25  24..22  21..19 18..0 
 *			 <   ecc,  cstate,  vind, p_addr >. 
 * the following defines tell ecc_swap_s_tag() how to shift the fields to
 * create the internal format from the s_taglo format. 
 */
/* sizes of the fields */
#define S_TAG_PADDR_BITS	19
#define S_TAG_CS_BITS		3
#define S_TAG_VIND_BITS		3
#define S_TAG_ECC_CBITS		7

/* bit positions of the fields in the s_taglo format */
#define S_TAG_ECC_BITPOS	0
#define S_TAG_VIND_BITPOS	(S_TAG_ECC_BITPOS+S_TAG_ECC_CBITS)	/*  7 */
#define S_TAG_CS_BITPOS		(S_TAG_VIND_BITPOS+S_TAG_VIND_BITS)	/* 10 */
#define S_TAG_PADDR_BITPOS	(S_TAG_CS_BITPOS+S_TAG_CS_BITS)		/* 13 */

/* bit positions of the fields in the internal format */
#define S_INT_PADDR_BITPOS	0
#define S_INT_VIND_BITPOS	(S_INT_PADDR_BITPOS+S_TAG_PADDR_BITS)	/* 19 */
#define S_INT_CS_BITPOS		(S_INT_VIND_BITPOS+S_TAG_VIND_BITS)	/* 22 */
#define S_INT_ECC_BITPOS	(S_INT_CS_BITPOS+S_TAG_CS_BITS)		/* 25 */

/* masks for the four fields in the 2ndary-cache-internal format:
 * < ecc, cstate, vindex, p_addr > */
#define S_INT_PADDR_MASK	0x0007ffff
#define S_INT_VIND_MASK		0x00380000
#define S_INT_CS_MASK		0x01c00000
#define S_INT_ECC_MASK		0xfe000000

/* Below macros enable easy swapping of each of the 4 2ndary tag fields.
 * Note that the mask used by the conversion macros in extracting the
 * bits of the field depends on the direction of the swap */

/* paddr: tag bit 13 <--> internal bit 0 */
#define SADDR_SWAP_ROLL		(S_TAG_PADDR_BITPOS-S_INT_PADDR_BITPOS)/* 13 */
/* the saddr conversion rolls opposite from the other 3 fields: TagTOInternal
 * rolls addr DOWN to bottom */
#define SADDR_TTOI(S_TAG)	((S_TAG & SADDRMASK) >> SADDR_SWAP_ROLL)
#define SADDR_ITOT(S_TAG)	((S_TAG & S_INT_PADDR_MASK) << SADDR_SWAP_ROLL)

/* cache state: tag bit 10 <--> internal bit 22 */
#define SSTATE_SWAP_ROLL	(S_INT_CS_BITPOS-S_TAG_CS_BITPOS)    /*  12 */
#define SSTATE_TTOI(S_TAG)	((S_TAG & SSTATEMASK) << SSTATE_SWAP_ROLL)
#define SSTATE_ITOT(S_TAG)	((S_TAG & S_INT_CS_MASK) >> SSTATE_SWAP_ROLL)

/* vindex: tag bit 7 <--> internal bit 19 */
#define SVIND_SWAP_ROLL		(S_INT_VIND_BITPOS-S_TAG_VIND_BITPOS) /* 12 */
#define SVIND_TTOI(S_TAG)	((S_TAG & SVINDEXMASK) << SVIND_SWAP_ROLL)
#define SVIND_ITOT(S_TAG)	((S_TAG & S_INT_VIND_MASK) >> SVIND_SWAP_ROLL)

/* ecc: tag bit 0 <--> internal bit 25 */
#define SECC_SWAP_ROLL		(S_INT_ECC_BITPOS-S_TAG_ECC_BITPOS)  /* 25 */
#define SECC_TTOI(S_TAG)	((S_TAG & SECC_MASK) << SECC_SWAP_ROLL)
#define SECC_ITOT(S_TAG)	((S_TAG & S_INT_ECC_MASK) >> SECC_SWAP_ROLL)


/* provide macro to convert cached_code array to bytes for easy length comps */
#define CC_BYTES	((CACHED_CODE_SIZE+EXTRA_SPACE)*sizeof(uint))

extern uint cached_code[];

extern tlbptepairs_t wpte[]; 
extern unsigned char *k0a, *k1a, *pa; 

extern int CSError;

/* begin prototype declarations, primarily for the primitives used 
 * by the IP17 diagnostics. */

/* from path/cache_states.c */
extern runem(int);

/* from path/IP17stress.c */
extern uint ReadStress(enum run_mode, uint, uint);
extern uint WriteStress(enum run_mode, uint, uint);
extern uint WritebackStress(enum run_mode, uint, uint);


/* from prims/bussubs.c */
extern int clear_sema(void);
extern int clear_pend(uint);

/* from prims/cpusubs.c */
/* extern walking_1(struct memstr *, int, uint, enum bitsense);
extern bwaking_1(struct memstr *, int, uint, enum bitsense);
extern walking_addr(struct memstr *, int);
extern bwalking_addr(struct memstr *, int);
extern addr_pattern(struct memstr *, int, enum bitsense);
extern read_wr(struct memstr *, uint);
extern bread_wr(struct memstr *, uint);
*/
extern void pte_setup(void);
extern void flushall_tlb(void);
extern scinv(void);

/* from prims/genasm.s */
extern uint pd_iwbinv(uint);
extern uint sd_iwbinv(uint);
extern uint pd_CDE(uint);
extern uint sd_CDE(uint);
extern uint pd_HINV(uint);
extern uint sd_HINV(uint);
extern uint pd_HWBINV(uint);
extern uint pd_HWB(uint);
extern uint sd_HSV(uint);
extern int  ICached(void);

extern int ml_run_it(uint, uint, uint, uint);
extern int ml_fast_reads();
extern int ml_fr_end();
extern int ml_fast_w_hwbinvs();
extern int ml_fwh_end();
extern int ml_fast_dirty_wbs();
extern int ml_fdw_end();



/* from prims/gensubs.c */
extern int getcpuid(void);
extern int cpuboard(void);
extern uint calc_tag_cbits(uint);
extern uint st_to_i(uint);
extern int set_shift(void);
extern unsigned char *buf_alloc(int, int);
extern void buf_free(unsigned char *);

/* from prims/handler.s */
extern int handler_setup(void);

/* from prims/memsubs.c */
extern int size_mem(void);
extern int EnableBusIntr(void);
extern int DisableBusIntr(void);

/* from prims/pathasm.s */
extern back1(unsigned int, unsigned int);
extern back2(unsigned int, unsigned int);
/* extern read_tag(unsigned int, unsigned int, struct tag_regs *); */
/* extern write_tag(unsigned int, unsigned int, struct tag_regs *); */
extern int fill_ipline(uint *);
extern int write_ipline(uint *);

/* from prims/pathsubs.c */
extern fill_caches(unsigned int);
extern fill_line(unsigned int, unsigned int);

/* extern unsigned int mem_init(uint, uint, enum fill_mode, uint); */

extern numtlbslots(void);
extern store_n_fetch_tags(void);
extern ch_bit_tests(void);
extern ce_test(void);
extern de_test(void);
extern ce_to_de_test(void);

extern test_c_instrs(int);


/* from prims/tlb.s */
extern int ta_spl(void);
extern int dinvaltlb(int);
extern int clr_tlb_regs(void);
extern int settlbslot(int, uint, __psunsigned_t, uint, uint);
extern int dset_tlbpid(int);
extern int write_indexed_lo(int, tlbptepairs_t *);
extern int write_indexed_hi(int, uint);
extern uint read_indexed_hi(int);
extern uint read_indexed_lo(int, tlbptepairs_t *);
extern int matchtlb(int, uint);
extern int tlb_setup(uint, uint, uint, uint);


/* from prims/iosubs.c */
extern void close_on_exception(int);
extern void exception_close(void);
extern int write_test(char *, char *, uint, uint, uint);
extern int read_test(char *, char *, uint, uint, uint, uint);
extern int add_map(int, int);
extern int dump_map(int);
extern int dir_write(char *, char *, uint);
extern int setlev(char *);
extern int test_map(int);
extern int cleariomask(void);
extern int checkbuserr(void);
extern int clearbuserr(void);
extern int ecc_loop(void);
/* extern int force_ecc(int, int, char); */
extern int force_ecc_ip9(volatile int, volatile int, char);
extern int check_log(uint, uint, uint);
extern int lookup(uint, uint);
extern int logprint(uint);
extern int clearecc(uint, uint);
extern int vme_reset(void);
extern int clear_log(void);
extern int idbg_ecc(void);
extern struct iob * get_iob(int);

/* from saio/lib/cache.s */
extern uint get_dcachesize(void);
extern uint get_icachesize(void);
extern uint get_scachesize(void);
extern uint get_dcacheline(void);
extern uint get_icacheline(void);
extern uint get_scacheline(void);



extern int config_cache(void);
extern void flush_cache(void);
extern int invalidate_icache(uint, uint);

extern int _icache_size;
extern int _dcache_size;
extern int _sidcache_size;

extern int setup_icache(__psunsigned_t, __psunsigned_t);


#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* __IDE_EVEREST_IP19_H__ */
