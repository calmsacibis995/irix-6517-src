/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: memerror.c
 *	handles all memory and cache specific errors. Error injection for
 *      debug kernels etc. Single bit error monitoring and scrubbing.
 */
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/debug.h>

#include <sys/cpu.h>
#include <sys/immu.h>
#include <sys/nodepda.h>
#include <sys/pda.h>
#include <ksys/cacheops.h>

#include <sys/SN/SN0/hubmd.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/slotnum.h>

#include "sn0_private.h"
#include "../error_private.h"
#include <sys/SN/error.h>

paddr_t cache_sidx_to_paddr(uint, uint);

#define BYTESPERQUADWD	(sizeof(int) * 4)
#define BYTESPERDBLWD	(sizeof(int) * 2)
#define BYTESPERWD	(sizeof(int) * 1)

#define LBYTEU(addr)	(*(u_char *) (addr))
#define LBYTE(addr)	(*(char	  *) (addr))

/* Tunable parameters defined in mtune/kernel */
extern int	panic_on_sbe;
extern int	sbe_log_errors;
extern int	sbe_mfr_override;
extern int	sbe_report_cons;
extern int      sbe_maxout;
extern int      sbe_max_per_page;

/*
 * Function	: handle_memory_uce
 * Parameters	: ep -> the exception frame.
 *		  eccfp -> the ecc exception frame.
 *		  cache_err -> the cache error register.
 * Purpose	: To handle what appears to be an uncorrectable data error
 *		  in memory.
 * Assumptions	: None.
 * Returns	: 0 -> ok to continue. -1 Fatal.?
 */
/* ARGSUSED */
int
memerror_handle_uce(uint cache_err, __uint64_t tag, eframe_t *ep, void *f)
{
	nasid_t	nasid;		/* Node this memory blongs to in NUMA space */
	paddr_t	paddr;		/* physical address of error */
	eccframe_t *eccf = (eccframe_t *)f;

	/*
	 * At this point we are sure we are here because of an SIE.
	 * Suspect double bit memory error.
	 */
	paddr = SCACHE_ERROR_ADDR(cache_err, tag);

	nasid = pfn_nasid(pnum(paddr));

	/*
	 * If in kernel mode, only speculation should cause this so ignore
	 * and continue. In user mode, it must have been a mapped page, and
	 * so we force an address error on return to kill the process. No
	 * the best - but it works for now.
	 */
	if (REMOTE_PARTITION_NODE(nasid)) {
		if (USERMODE(ep->ef_sr)) {
			eccf->eccf_errorEPC = 0xabadecca;
		}
		return 1;
	}
	
		
	/*
	 * For now, we assume it is bad memory and mark it invalid.
	 * If it is the external bus that is corrupting data, its unlikely
	 * we will be able to do anything. Later, verify this.
	 */

	set_dir_state(paddr, MD_DIR_POISONED);
	return 1;
}



/*
 * Function	: cache_sidx_to_paddr
 * Parameters	: sidx -> secondary cached index paddr (23 bits)
 *		  way  -> which way in secondary cache.
 * Purpose	: to determine the physical address from the secondary cache.
 * Assumptions	: Interrupts blocked.
 * Returns	: Physical address.
 */
paddr_t
cache_sidx_to_paddr(uint sidx, uint way)
{
	__uint64_t	c_tag;	/* composite tag, a 64-bit version
				 * of taglo and taghi combined */
	cacheop_t cop;

	ecc_cacheop_get(CACH_S+C_ILT, (__uint64_t)(sidx | way), &cop);
	c_tag =(((__uint64_t)cop.cop_taghi << 32) | (__uint64_t)cop.cop_taglo);

	return ((c_tag & CTS_TAG_MASK) | sidx);
}


/************************************************************************
 *									*
 *	The next set of routines are for cache sanity checking.		*
 *	Checking for ecc check_bits and parity etc.			*
 *									*
 ************************************************************************/


/*
 * Function	: kl_compute_parity
 * Parameters	: chk_word -> word to check parity on.
 *		  chk_bits -> how many bits to use for parity checking.
 *		  num_pbits-> how many chk_bits to break up the chk_word into
 * Purpose	: to compute the parity of the given word
 * Assumptions	: none.
 * Returns	: Returns the parity of the word.
 */
int
kl_compute_parity (__uint64_t chk_word, int chk_bits, int num_pbits)
{
	__uint64_t	sub_word;	/* partial word for this pass */
	int	parity = 0;	/* Generated parity */
	int i;		/* loop variable */
#define CHK_BIT_MASK(x)	((1 << (x)) - 1)

	ASSERT(num_pbits*chk_bits < sizeof(__uint64_t));

	for (i = 0; i < num_pbits; i++) {
		sub_word = (chk_word & CHK_BIT_MASK(chk_bits));
		chk_word >>= chk_bits;
		if (ecc_bitcount(sub_word) & 1)
		    parity |= (1 << i);
	}

	return parity;
}



struct dir_ecc_mask prem_dir_eccm[PDIR_ECC_BITS] = {
	E7_6W_PDR,
	E7_5W_PDR,
	E7_4W_PDR,
	E7_3W_PDR,
	E7_2W_PDR,
	E7_1W_PDR,
	E7_0W_PDR,
};

struct dir_ecc_mask ord_dir_eccm[PDIR_ECC_BITS] = {
	E5_4W_SDR,
	E5_3W_SDR,
	E5_2W_SDR,
	E5_1W_SDR,
	E5_0W_SDR,
};


/*
 * Function	: kl_compute_dir_ecc_cb
 * Parameters	: dir_word -> the directory word to generate ecc on.
 *		  dir_type -> premium or standard?
 * Purpose	: to generate the ecc of the directory word.
 * Assumptions	: None.
 * Returns	: Returns the ecc check_bits of the directory word.
 */

int
kl_compute_dir_ecc_cb(__uint64_t dir_word, int dir_type)
{
	uint	check_bits;
	int i;				/* loop variables */
	uint	use_ecc_bits;
	struct dir_ecc_mask	*use_ecc_mask;

	check_bits = 0;
	if (dir_type) {			/* Premium directory */
		use_ecc_bits = PDIR_ECC_BITS;
		use_ecc_mask = prem_dir_eccm;
		dir_word = dir_word & PREM_DIR_MASK;
	}
	else {
		use_ecc_bits = SDIR_ECC_BITS;
		use_ecc_mask = ord_dir_eccm;
		dir_word = dir_word & STD_DIR_MASK;
	}

	for (i = 0; i < use_ecc_bits; i++) {
		if (ecc_bitcount(dir_word &
				((use_ecc_mask + i)->dir_emask)) & 1)
		    check_bits |= (1 << i);
	}
	return check_bits;
}


struct mem_ecc_mask mem_eccm[MEM_ECC_BITS] = {
	E8_7W,
	E8_6W,
	E8_5W,
	E8_4W,
	E8_3W,
	E8_2W,
	E8_1W,
	E8_0W,
};


/*
 * Function	: kl_compute_mem_ecc_cb
 * Parameters	: mem_word -> the memory word to generate ecc on.
 * Purpose	: to generate the ecc of the memory word.
 * Assumptions	: None.
 * Returns	: Returns the ecc check_bits of the memory word.
 */

int
kl_compute_mem_ecc_cb(__uint64_t mem_word)
{
	uint	check_bits;		/* the generated check_bits */
	int i;				/* loop variables */

	check_bits = 0;
	for (i = 0; i < MEM_ECC_BITS; i++) {
		if (ecc_bitcount(mem_word & mem_eccm[i].mem_emask) & 1)
		    check_bits |= (1 << i);
	}
	return check_bits;
}


#if defined (SN0)  && defined (FORCE_ERRORS)
extern void uncached(void);
extern void runcached(void);
#define ICACHE_LINESIZE 64
void
corrupt_icache_parity(int index, int way)
{
	__psunsigned_t k0addr;
	cacheop_t cop;
	int s;
	extern void forcebad_icache_parity(cacheop_t *);
	s = splerr();

	k0addr = PHYS_TO_K0(index*ICACHE_LINESIZE);
	cop.cop_address = k0addr | way;
	forcebad_icache_parity(&cop);

	splx(s);
}


int	stop_iparity = 0;

void
do_iparity_force(paddr_t paddr)
{
	int index = paddr & 0xfff;
	int force = (paddr >> 12) & 0xffff;
	int way = (paddr >> 28) & 0xff;

	corrupt_icache_parity(index, way);
	(void) timeout(do_iparity_force, (void *)paddr, force);
}


#include <sys/runq.h>
/*ARGSUSED*/
int
kl_force_piecc(paddr_t paddr, int type)
{
	int force = (paddr >> 16) & 0xff;
	int cpu = type & 0xff;
	cpu_cookie_t cook;

	if (!cpu_isvalid(cpu))
	    cpu = 0;

	cook = setmustrun(cpu);
	(void) timeout(do_iparity_force, (void *)paddr, force);
	restoremustrun(cook);
	return 0;

}


/*
 * Function: kl_force_pdecc
 * Purpose:	force bad ecc in primary data cache, data or tag
 * Parameters:	paddr (physical address),
 *	        type (CORRUPT_DATA or CORRUPT_ECC)
 * Returns: 0 on success, -1 otherwise.
 */

int
kl_force_pdecc(paddr_t paddr, md_error_params_t* uap, rval_t *rvp)
{
	__psunsigned_t	k0addr, pcupper;

	uint	pidx;
	int way = 0;			/* which way? */
	cacheop_t cop;
	__uint64_t	c_tag, pc_paddr[PCACHE_WAYS];
	int	s;

	paddr &= CACHE_DLINE_MASK;	/* Force cache line  alignment */
	k0addr = PHYS_TO_K0(paddr);	/* unmapped cached address space */

	s = spl7();		/* no intrs ensure we dont lose cache line */

	/* Now this data access should generate a cache exception */
	error_force_daccess((volatile __uint64_t *)k0addr, 
			    uap->read_write, uap->access_val, uap->flags, 
			    rvp);

	uncached();		/* run uncached so line is not replaced */
	pidx = (k0addr & ((1 << 14) - 1)); /* this is not virtual index */
	do {
		ecc_cacheop_get(CACH_PD + C_ILT, k0addr | way, &cop);
		c_tag = (((__uint64_t)cop.cop_taghi << 32) |
			 (__uint64_t)cop.cop_taglo);
		pcupper = ((c_tag & CTP_TAG_MASK) >> CTP_TAG_SHFT);
		pcupper <<= 12;
		if ((pc_paddr[way] = pcupper | pidx) == paddr)
		    break;
	} while (++way < PCACHE_WAYS);

	if (pc_paddr[way] != paddr) {	/* should never happen? */
		runcached();
		printf("kl_force_pdecc: paddr %x not found. Got %x and %x",
		       paddr, pc_paddr[0], pc_paddr[1]);
		splx(s);
		return -1;
	}

	if (uap->corrupt_target & CORRUPT_DATA) {
		/* load cache data*/
		ecc_cacheop_get(CACH_PD + C_ILD, k0addr | way, &cop);
		
		cop.cop_ecc ^= ((uap->flags & ERRF_SBE) ? 0x1 : 0xf);

		/* store it back */
		ecc_cacheop_get(CACH_PD + C_ISD, k0addr | way, &cop);
	}

	if (uap->corrupt_target & CORRUPT_TAG) {
		cop.cop_taglo ^= 0x1;	/* toggle parity bit */
		ecc_cacheop_get(CACH_PD + C_IST, k0addr | way, &cop);
	}

	runcached();

	/* Now this data access should generate a cache exception */
	error_force_daccess((volatile __uint64_t *)k0addr, 
			    uap->read_write, uap->access_val, uap->flags, 
			    rvp);

	splx(s);

	cmn_err(CE_WARN, "kl_force_pdecc: force done");
	return 0;
}

void print_scache_quadword(__psunsigned_t k0addr)
{
        __uint64_t data[2];
        __uint32_t ecc [2];
	cacheop_t  cop;

	extern void qprintf(char *f, ...);
	extern uint ecc_compute_scdata_ecc(__uint64_t,__uint64_t);

        ecc_cacheop_get(CACH_SD + C_ILD, k0addr    , &cop);
        data[0] = (((__uint64_t)cop.cop_taghi << 32) |
                    (__uint64_t)cop.cop_taglo);
        ecc [0] = cop.cop_ecc;

        ecc_cacheop_get(CACH_SD + C_ILD, k0addr + 8, &cop);
        data[1] = (((__uint64_t)cop.cop_taghi << 32) |
                    (__uint64_t)cop.cop_taglo);
        ecc [1] = cop.cop_ecc;

        qprintf("0x%x d[0]=0x%x d[1]=0x%x ECC[0]=0x%x ECC[1]=0x%x CECC=0x%X\n",
		k0addr,data[0],data[1],
                ecc[0],ecc[1],ecc_compute_scdata_ecc(data[0],data[1]));
}




char *cache_state[4] =  {
	"Invalid",
	"shared",
	"clean exclusive",
	"dirty exclusive",
};
	

/*
 * Function: kl_force_scecc
 * Purpose:	Force bad ecc in secondary cache, data or tag
 * Parameters:	paddr: physical address to force bad ecc at.
 *	        type: CORRUPT_DATA or CORRUPT_TAG
 * Returns: 0 on success, -1 otherwise.
 */

int
kl_force_scecc(paddr_t paddr, md_error_params_t *uap, rval_t *rvp)
{
	__psunsigned_t	k0addr, pcupper;
	/*REFERENCED*/
	__uint64_t	cache_word;
	int way = 0;			/* which way? */
	/*REFERENCED*/
	int ecc;
	__uint64_t	c_tag, sidx;
	paddr_t sc_paddr[SCACHE_WAYS];
	int	s;
	cacheop_t	cop;
	extern void qprintf(char *f, ...);


	paddr &= CACHE_SLINE_MASK;	/* Force cache line  alignment */
	k0addr = PHYS_TO_K0(paddr);

	/*
	 * run without interrupts and uncached to avoid that 
	 * the cache line is replaced
	 */
	s = spl7();		
	uncached();

	/*
	 * write pattern, data will be dirty in dcache
	 */

	/*
	 * Hit WriteBack Invalidate D
	 *
	 * dirty data from dcache is now written back to 
	 * scache 
	 */
	ecc_cacheop_get(CACH_PD + C_HWBINV, k0addr, &cop);


	/*
	 * figure out which way of scache our address is in
	 */
	sidx = (k0addr & (getcachesz(getcpuid()) - 1));
	do {
		ecc_cacheop_get(CACH_SD + C_ILT, k0addr | way, &cop);
		c_tag = (((__uint64_t)cop.cop_taghi << 32) |
			 (__uint64_t)cop.cop_taglo);
		pcupper = (c_tag & CTS_TAG_MASK) >> CTS_TAG_SHFT;
		pcupper <<= 18;
		if ((sc_paddr[way] = (pcupper | sidx)) == paddr)
		    break;
	} while (++way < SCACHE_WAYS);

	if (sc_paddr[way] != paddr) {	/* we blew up */
		printf("kl_force_scecc: paddr %x, sidx %x not found. "
		       "Found %x and %x",
		       paddr, sidx, sc_paddr[0], sc_paddr[1]);
		runcached();
		splx(s);
		return -1;
	}

	if (uap->corrupt_target & CORRUPT_DATA) {

		print_scache_quadword(k0addr|way);

		/* Now load the data and ecc from the correct way */
		ecc_cacheop_get(CACH_SD + C_ILD, k0addr | way, &cop);
		cop.cop_ecc ^= ((uap->flags & ERRF_SBE) ? 0x1 : 0x3);
		ecc_cacheop_get(CACH_SD + C_ISD, k0addr | way, &cop);

	}

	if (uap->corrupt_target &  CORRUPT_TAG) {
		ecc_cacheop_get(CACH_SD + C_ILT, k0addr | way, &cop);

		cop.cop_taglo &= ~CTS_STATE_MASK;

		/*cop.cop_taglo ^= ((uap->flags & ERRF_SBE) ? 0x1 : 0x3); */
		ecc_cacheop_get(CACH_SD + C_IST, k0addr | way, &cop);
        	ecc_cacheop_get(CACH_SD + C_IINV, k0addr | way , &cop);

	}

	/*
	 * now, go back to cached mode. Do not enable interrupts yet.
	 * we dont want to lose this cache line
	 */
	runcached();

	splx(s);

	return 0;
}




int
kl_force_direcc(md_error_params_t *uap, rval_t *rvp)
{
	/*REFERENCED*/
	__uint64_t	cache_word;
	__uint64_t	bd_word1;
	__uint64_t	bd_word2;
	__psunsigned_t	k0addr;
	cacheop_t cop;

	k0addr = PHYS_TO_K0(uap->mem_addr);

	bd_word1 = *(volatile __uint64_t *)BDDIR_ENTRY_LO(uap->mem_addr);
	bd_word2 = *(volatile __uint64_t *)BDDIR_ENTRY_HI(uap->mem_addr);

	/* corrupt ecc */
	if (uap->flags & ERRF_SBE)
	    bd_word1 ^= 0x1;
	else {
	    bd_word1 = 0x888;
	    bd_word2 = 0x888;
	}

	*(volatile __uint64_t *)BDDIR_ENTRY_LO(uap->mem_addr) = bd_word1;
	*(volatile __uint64_t *)BDDIR_ENTRY_HI(uap->mem_addr) = bd_word2;

	/*
	 * ensure line is not in cache so that we are forced to go to the
	 * directory.
	 */
	ecc_cacheop_get(CACH_SD + C_HINV, k0addr, &cop);
	error_force_daccess((volatile __uint64_t *)k0addr, uap->read_write, 
			    uap->access_val, uap->flags, rvp);

	cmn_err(CE_WARN, "kl_force_decc: force done");
	return 0;
}



int
kl_force_memecc(paddr_t paddr, md_error_params_t *uap, rval_t *rvp)
{
	uint mem_ecc;
	uint new_ecc;
	uint dword, dword_mask;

	/*REFERENCED*/
	__uint64_t cache_word;
	__psunsigned_t k0addr;
	cacheop_t cop;

	k0addr = PHYS_TO_K0(paddr);

	*(volatile __uint64_t *)(k0addr) = 0xdeadbeaf;

	ecc_cacheop_get(CACH_SD + C_HWBINV, k0addr, &cop);

	mem_ecc = *(volatile uint *)(BDECC_ENTRY(paddr) & ~0x3);

	dword = (paddr >> 3) & 0x3;
	dword = 3 - dword;

	dword_mask = ((uap->flags & ERRF_SBE) ? 1 : 3) << (8 * dword);

	printf("dword mask is 0x%x\n", dword_mask);
	new_ecc = mem_ecc ^ dword_mask;		/* corrupt ecc  */

	*(volatile uint *)(BDECC_ENTRY(paddr) & ~0x3) = new_ecc;

	error_force_daccess((volatile __uint64_t *)k0addr, 
			    uap->read_write, uap->access_val, uap->flags, 
			    rvp);

	return 0;
}

#endif /* SN0 && FORCE_ERRORS */



/**************************************************************************
 * Everything in the following section is derived from and should be kept
 * in sync with stand/arcs/IP27prom/mdir.c
 *************************************************************************/

/*
 * Syndrome to bit mapping
 *
 *   A syndrome of zero indicates no errors.  A syndrome in the table below
 *   indicates a single check bit or data bit correctable error.  A syndrome
 *   not in the table indicates a multiple-bit uncorrectable error.
 */

typedef struct synent_s {
    uchar_t		syn, bit;
} synent_t;

static synent_t synmap_mem[] = {
    /* Data bits */
    { 0xc8, 71 }, { 0xc4, 70 }, { 0xc2, 69 }, { 0xc1, 68 },
    { 0xf4, 67 }, { 0x8f, 66 }, { 0xe0, 65 }, { 0xb0, 64 },
    { 0x0e, 63 }, { 0x0b, 62 }, { 0xf2, 61 }, { 0x1f, 60 },
    { 0x86, 59 }, { 0x46, 58 }, { 0x26, 57 }, { 0x16, 56 },
    { 0x38, 55 }, { 0x34, 54 }, { 0x32, 53 }, { 0x31, 52 },
    { 0xa8, 51 }, { 0xa4, 50 }, { 0xa2, 49 }, { 0xa1, 48 },
    { 0x98, 47 }, { 0x94, 46 }, { 0x92, 45 }, { 0x91, 44 },
    { 0x58, 43 }, { 0x54, 42 }, { 0x52, 41 }, { 0x51, 40 },
    { 0x8a, 39 }, { 0x4a, 38 }, { 0x2a, 37 }, { 0x1a, 36 },
    { 0x89, 35 }, { 0x49, 34 }, { 0x29, 33 }, { 0x19, 32 },
    { 0x85, 31 }, { 0x45, 30 }, { 0x25, 29 }, { 0x15, 28 },
    { 0x8c, 27 }, { 0x4c, 26 }, { 0x2c, 25 }, { 0x1c, 24 },
    { 0x68, 23 }, { 0x64, 22 }, { 0x62, 21 }, { 0x61, 20 },
    { 0xf8, 19 }, { 0x4f, 18 }, { 0x70,	17 }, { 0xd0, 16 },
    { 0x07, 15 }, { 0x0d, 14 }, { 0xf1,	13 }, { 0x2f, 12 },
    { 0x83, 11 }, { 0x43, 10 }, { 0x23,	 9 }, { 0x13,  8 },
    /* Check bits */
    { 0x80, 128 | 7 }, { 0x40, 128 | 6 }, { 0x20, 128 | 5 },
    { 0x10, 128 | 4 }, { 0x08, 128 | 3 }, { 0x04, 128 | 2 },
    { 0x02, 128 | 1 }, { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

static synent_t synmap_pdir[] = {
    /* Data bits */
    { 0x70, 47 }, { 0x0b, 46 }, { 0x07, 45 }, { 0x43, 44 },
    { 0x7c, 43 }, { 0x1f, 42 }, { 0x45, 41 }, { 0x4c, 40 },
    { 0x51, 39 }, { 0x61, 38 }, { 0x46, 37 }, { 0x4a, 36 },
    { 0x64, 35 }, { 0x23, 34 }, { 0x73, 33 }, { 0x6e, 32 },
    { 0x25, 31 }, { 0x29, 30 }, { 0x31, 29 }, { 0x26, 28 },
    { 0x2a, 27 }, { 0x62, 26 }, { 0x2c, 25 }, { 0x1a, 24 },
    { 0x5d, 23 }, { 0x2f, 22 }, { 0x13, 21 }, { 0x32, 20 },
    { 0x52, 19 }, { 0x16, 18 }, { 0x34, 17 }, { 0x54, 16 },
    { 0x15, 15 }, { 0x68, 14 }, { 0x0d, 13 }, { 0x1c, 12 },
    { 0x19, 11 }, { 0x0e, 10 }, { 0x38,	 9 }, { 0x58,  8 },
    { 0x49,  7 },
    /* Check bits */
    { 0x40, 128 | 6 }, { 0x20, 128 | 5 }, { 0x10, 128 | 4 },
    { 0x08, 128 | 3 }, { 0x04, 128 | 2 }, { 0x02, 128 | 1 },
    { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

static synent_t synmap_sdir[] = {
    /* Data bits */
    { 0x1f, 15 }, { 0x1a, 14 }, { 0x13, 13 }, { 0x07, 12 },
    { 0x0e, 11 }, { 0x0b, 10 }, { 0x16,	 9 }, { 0x15,  8 },
    { 0x19,  7 }, { 0x1c,  6 }, { 0x0d,	 5 },
    /* Check bits */
    { 0x10, 128 | 4 }, { 0x08, 128 | 3 }, { 0x04, 128 | 2 },
    { 0x02, 128 | 1 }, { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

/*
 * mdir_xlate_syndrome
 *
 *   Translates a syndrome into a string. Input "type" should be 0 for
 *   memory, 1 for premium directory, or 2 for standard directory.
 *
 *   - For no errors, says "good".
 *
 *   - For a single bit error in the data, says "data00" to "data63", "dir00"
 *     to "dir40", or "dir00" to "dir10", according to type 0, 1, or 2,
 *     respectively.
 *
 *   - For a single bit error in check bits, says "check0" to "check7",
 *     "check0" to "check6", or "check0" to "check4", according to type
 *     0, 1, or 2, respectively.
 *
 *   - For a multiple-bit uncorrectable error, says "multi".
 *
 *   Returns the SIMM bit number for single bit memory errors (71 to 0).
 */

int mdir_xlate_syndrome(char *buf, int type, __uint64_t syn)
{
    int			r	= -1;
    synent_t	       *table;
    char	       *bitname;
    int			bitnum, bit;
    __uint64_t		forced, x;

    switch (type) {
    case 0:
	table = synmap_mem;
	bitname = "data";
	bitnum = 8;
	forced = 0xff;
	break;
    case 1:
	table = synmap_pdir;
	bitname = "dir";
	bitnum = 7;
	forced = 0x7f;
	break;
    case 2:
	table = synmap_sdir;
	bitname = "dir";
	bitnum = 5;
	forced = 0x1f;
	break;
    }

    sprintf(buf, "0x%02lx (", syn);
    while (*buf)
	buf++;

    if (syn == 0)
	strcpy(buf, "good");
    else if (syn == forced)
	strcpy(buf, "forced_multi");
    else {
	int		i;

	for (i = 0; (x = (__uint64_t) LBYTEU(&table[i].syn)) != 0; i++)
	    if (x == syn)
		break;

	bit = LBYTE(&table[i].bit);

	if (x == 0)
	    strcpy(buf, "multi");
	else if (bit & 128) {
	    r = bit & 127;
	    sprintf(buf, "check_%d", r);
	} else {
	    r = bit;
	    sprintf(buf, "%s_%02d", bitname, bit - bitnum);
	}
    }

    strcat(buf, ")");

    return r;
}

/*
 * mdir_xlate_addr_mem
 *
 *   Expects a physical memory address and a SIMM bit from 71:0, where bits
 *   71:8 correspond to data 63:0 and 7:0 correspond to the check bits.
 */

void mdir_xlate_addr_mem(char *buf, __uint64_t address, int bit)
{
    __uint64_t		bank_dimm, bank_lgcl, bitbase;

    bank_dimm = GET_FIELD(address, MD_BANK);
    bank_lgcl = (bit >= 36);

    bitbase = (((address & 8) ? 18 : 0) +
	       (bit >= 18 && bit < 36 || bit >= 54 ? 36 : 0));

    if (SN00) {
	if (bit < 0)
	    sprintf(buf, "SLOT%lld and/or SLOT%lld",
		    bank_dimm * 2 + 1, bank_dimm * 2 + 2);
	else
	    sprintf(buf, "SLOT%lld line %lld",
		    bank_dimm * 2 + bank_lgcl + 1,
		    bitbase + (bit % 18));
    } else {
	if (bit < 0)
	    sprintf(buf, "MM%cH%c and/or MM%cL%c",
		    bank_dimm < 4 ? 'X' : 'Y',
		    (int) (bank_dimm + '0'),
		    bank_dimm < 4 ? 'X' : 'Y',
		    (int) (bank_dimm + '0'));
	else
	    sprintf(buf, "MM%c%c%c line %lld",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_lgcl ? 'H' : 'L',
		    (int) (bank_dimm + '0'),
		    bitbase + (bit % 18));
    }
}

/*
 * mdir_xlate_addr_pdir
 *
 *   Expects a physical memory address and a SIMM bit from 47:0, where bits
 *   47:7 correspond to data 40:0 and 6:0 correspond to the check bits.
 */

void mdir_xlate_addr_pdir(char *buf, __uint64_t address, int bit,
			  int dimm0_sel)
{
    __uint64_t		bank_dimm, bank_phys;

#ifdef N_MODE
    bank_dimm = (address >> 27) & 3;
#else
    bank_dimm = (address >> 27) & 7;
#endif

    bank_phys = (address >> 20 ^ address >> 18 ^
		 address >> 10 ^ dimm0_sel >> 1) & 1;

    if (bit < 0)
	sprintf(buf, "MM%c%c%c and/or DIR%c%c",
		bank_dimm < 4 ? 'X' : 'Y',
		bank_phys ? 'H' : 'L',
		(int) (bank_dimm + '0'),
		bank_dimm < 4 ? 'X' : 'Y',
		(int) (bank_dimm + '0'));
    else if (bit < 16)
	sprintf(buf, "MM%c%c%c line %d",
		bank_dimm < 4 ? 'X' : 'Y',
		bank_phys ? 'H' : 'L',
		(int) (bank_dimm + '0'),
		bit);
    else
	sprintf(buf, "DIR%c%c line %d",
		bank_dimm < 4 ? 'X' : 'Y',
		(int) (bank_dimm + '0'),
		bit - 16);
}

/*
 * mdir_xlate_addr_sdir
 *
 *   Expects a physical memory address and a SIMM bit from 15:0, where bits
 *   15:5 correspond to data 10:0 and 4:0 correspond to the check bits.
 */

void mdir_xlate_addr_sdir(char *buf, __uint64_t address, int bit,
			  int dimm0_sel)
{
    __uint64_t		bank_dimm, bank_phys;

#ifdef N_MODE
    bank_dimm = (address >> 27) & 3;
#else
    bank_dimm = (address >> 27) & 7;
#endif

    bank_phys = (address >> 20 ^ address >> 18 ^
		 address >> 10 ^ dimm0_sel >> 1) & 1;

    if (SN00) {
	if (bit < 0)
	    sprintf(buf, "SLOT%lld",
		    bank_dimm * 2 + bank_phys + 1);
	else
	    sprintf(buf, "SLOT%lld line %d",
		    bank_dimm * 2 + bank_phys + 1,
		    bit);
    } else {
	if (bit < 0)
	    sprintf(buf, "MM%c%c%c",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_phys ? 'H' : 'L',
		    (int) (bank_dimm + '0'));
	else
	    sprintf(buf, "MM%c%c%c line %d",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_phys ? 'H' : 'L',
		    (int) (bank_dimm + '0'),
		    bit);
    }
}

/**************************************************************************
 * Correctable errors must be processed on the node that received them
 * since only the local processor can get the error interrupt.
 *************************************************************************/

static void sbe_printf(char *fmt, ...)
{
    va_list		ap;
    char		buf[128], *s;

    if (sbe_log_errors) {
	s = buf;

	if (! sbe_report_cons)
	    *s++ = '!';

	va_start(ap, fmt);
	vsprintf(s, fmt, ap);
	va_end(ap);

	cmn_err(CE_WARN, buf);
    }
}

static void bank_fatal(int bank, char *locbuf)
{
    char		slotname[16];

    if (sbe_mfr_override)
	return;

    get_slotname(SLOTNUM_GETSLOT(nodepda->slotdesc), slotname);

    cmn_err(CE_WARN | CE_MAINTENANCE,
	    "Excessive single-bit ECC corrections on");
    cmn_err(CE_WARN | CE_MAINTENANCE,
	    "/hw/module/%d/slot/%s/node/memory/dimm_bank/%d (%s)",
	    nodepda->module_id, slotname, bank, locbuf);
#if 0
    cmn_err(CE_WARN,
	    "Bank will be disabled on reboot. ** REBOOT RECOMMENDED **");
#endif
    cmn_err(CE_WARN | CE_MAINTENANCE,
	    "Further SBEs will not be monitored.");

    /*** XXX set disable bank promlog variables */
    /*** XXX if bank is 0, might not want to disable it */

    intr_disconnect_level(CNODE_TO_CPU_BASE(cnodeid()),
			  INT_PEND1_BASELVL + MD_COR_ERR_INTR);

    nodepda->sbe_info->disabled = 1;
}

/*
 * memerror_sbe_intr
 *
 *   If arg is non-NULL, memerror_sbe_intr assumes it is polling for a
 *   single-bit error.  If no error is present, it just returns.
 *
 *   If arg is NULL, memerror_sbe_intr expects it was called due to an
 *   interrupt.  If no error is present, it complains.
 */

void memerror_sbe_intr(void *arg)
{
    md_dir_error_t	dir_err;
    md_mem_error_t	mem_err;
    int			dir_ce;
    int			mem_ce;
    __uint64_t		mc;
    paddr_t		bdaddr, memaddr;
    int			bit, premium, dimm0_sel, ignore;
    char		locbuf[32];
    int			i, j, bank;
    pfn_t		pfn;
    sbe_info_t	       *info;
    sbe_event_t	       *log;
    char		slotname[16];
    int			munged;

    info = nodepda->sbe_info;

    if (info->disabled)
	return;

    dir_err.derr_reg = LOCAL_HUB_L(MD_DIR_ERROR_CLR);
    mem_err.merr_reg = LOCAL_HUB_L(MD_MEM_ERROR_CLR);

    dir_ce = ((dir_err.derr_reg	  >> DIR_ERROR_VALID_SHFT) ==
	      (DIR_ERROR_VALID_CE >> DIR_ERROR_VALID_SHFT));

    mem_ce = ((mem_err.merr_reg	  >> MEM_ERROR_VALID_SHFT) ==
	      (MEM_ERROR_VALID_CE >> MEM_ERROR_VALID_SHFT));

    if (! dir_ce && ! mem_ce) {
	int		spurious;

	if (arg)		/* Just polling for CE, and there wasn't. */
	    return;

	/*
	 * Additional errors could occur after the error interrupt is
	 * taken, overwriting correctable error information. This should
	 * be exceedingly rare.  If this happens, we'll print a message
	 * and ignore it.
	 */

	spurious = 1;

	if (dir_err.derr_fmt.uce_vld) {
	    sbe_printf("Unexpected uncorrectable directory ECC error");
	    spurious = 0;
	} else if (dir_err.derr_fmt.ae_vld) {
	    sbe_printf("Unexpected protection rights ECC error");
	    spurious = 0;
	}

	if (mem_err.merr_fmt.uce_vld) {
	    sbe_printf("Unexpected uncorrectable memory ECC error");
	    spurious = 0;
	}

	if (spurious)
	    sbe_printf("Spurious correctable ECC error interrupt");

	return;
    }

#if 0
    printf("DIR_ERR=0x%x MEM_ERR=0x%x", dir_err.derr_reg, mem_err.merr_reg);
#endif

    mc		= LOCAL_HUB_L(MD_MEMORY_CONFIG);
    premium	= (mc & MMC_DIR_PREMIUM_MASK) != 0;
    dimm0_sel	= (int) (mc		    >> MMC_DIMM0_SEL_SHFT &
			 MMC_DIMM0_SEL_MASK >> MMC_DIMM0_SEL_SHFT);

    memaddr	= 0;
    ignore	= 0;
    strcpy(locbuf, "unknown bit");

    if (dir_ce) {
	sbe_printf("Correctable directory ECC error (%s)",
		   arg ? "polled" : "intr");

	/*
	 * Correct for Hub errata #402537 if HSPEC address is munged.
	 *
	 * Bit 3 of bdaddr (bit 1 of dir_err.derr_fmt.hspec_addr) is set
	 * if the error was in the high word, even with the errata.
	 * We currently don't care if it was in the high or low word and
	 * that bit gets thrown out.
	 */

	bdaddr =
	    TO_NODE(COMPACT_TO_NASID_NODEID(cnodeid()),
					    dir_err.derr_fmt.hspec_addr << 3);

	if ((bdaddr & 0xf0000000) == 0x30000000) {
	    bdaddr = BDDIR_TO_MEM(bdaddr);
	    munged = 1;
	} else
	    munged = 0;

	memaddr = TO_NODE(COMPACT_TO_NASID_NODEID(cnodeid()),
			  BDDIR_TO_MEM(bdaddr));

	/*
	 * Converting memaddr back to bdaddr gives a proper HSPEC address.
	 */

	bdaddr = BDDIR_ENTRY_LO(memaddr);

	bit = mdir_xlate_syndrome(locbuf, 2 - premium,
				  dir_err.derr_fmt.bad_syn);

	sbe_printf(" Dir: 0x%x, Mem: 0x%x, Syn: %s%s",
		   bdaddr, memaddr,
		   locbuf, dir_err.derr_fmt.ce_ovr ? ", Ovrn" : "");

	(premium ? mdir_xlate_addr_pdir : mdir_xlate_addr_sdir)
	    (locbuf, bdaddr, bit, dimm0_sel);

	get_slotname(SLOTNUM_GETSLOT(nodepda->slotdesc), slotname);

	sbe_printf(" Loc: "
		   "/hw/module/%d/slot/%s/node/memory/dimm_bank/%d (%s)",
		   nodepda->module_id, slotname,
		   (int) (memaddr      >> MD_BANK_SHFT &
			  MD_BANK_MASK >> MD_BANK_SHFT), locbuf);

	if (! memory_present(memaddr)) {
	    sbe_printf(" Memory address not present!");
	    ignore = 1;
	} else if (page_ispoison(memaddr)) {
	    sbe_printf(" Ignoring correctable error in poisoned page");
	    ignore = 1;
	} else {
	    /*
	     * We would like to re-access the directory entry to see if
	     * the error recurs, so we could tell if it was a transient
	     * read error or if the data is actually bad in memory.
	     * Unfortunately, if the directory error is due to a memory
	     * access (as opposed to a directory access), the Hub scrubs
	     * the directory ECC automatically, erasing the evidence.
	     *
	     * Scrubbing is performed in any case to repair
	     * the potential single-bit error.
	     *
	     * Need to guarantee that the coherency state for the
	     * cache line(s) changes so the ECC will be recalculated.
	     * Atomically adding 0 will change the state to exclusive,
	     * then a Hit-WriteBack-Inv-SD will cause it to change
	     * to unowned.
	     */

	    if (munged)
		for (i = 0; i < 0x400; i += 0x80) {
		    atomicAddInt((int *) TO_CAC(memaddr + i), 0);
		    __cache_wb_inval((void *) TO_CAC(memaddr + i), 8);
		}
	    else {
		atomicAddInt((int *) TO_CAC(memaddr), 0);
		__cache_wb_inval((void *) TO_CAC(memaddr), 8);
	    }

	    /*
	     * Clear any error that resulted from scrub.
	     */

	    LOCAL_HUB_L(MD_DIR_ERROR_CLR);

	    sbe_printf(" Single-bit error has been repaired.");
	}
    }

    if (mem_ce) {
	sbe_printf("Correctable memory ECC error (%s)",
		   arg ? "polled" : "intr");

	memaddr = TO_NODE(COMPACT_TO_NASID_NODEID(cnodeid()),
			  mem_err.merr_fmt.address << 3);

	bit = mdir_xlate_syndrome(locbuf, 0, mem_err.merr_fmt.bad_syn);

	sbe_printf(" Mem: 0x%x, Syn: %s%s",
		   memaddr, locbuf,
		   mem_err.merr_fmt.ce_ovr ? ", Ovrn" : "");

	mdir_xlate_addr_mem(locbuf, memaddr, bit);

	get_slotname(SLOTNUM_GETSLOT(nodepda->slotdesc), slotname);

	sbe_printf(" Loc: "
		   "/hw/module/%d/slot/%s/node/memory/dimm_bank/%d (%s)",
		   nodepda->module_id, slotname,
		   (int) (memaddr      >> MD_BANK_SHFT &
			  MD_BANK_MASK >> MD_BANK_SHFT), locbuf);

	if (! memory_present(memaddr)) {
	    sbe_printf(" Memory address not present!");
	    ignore = 1;
	} else if (page_ispoison(memaddr)) {
	    sbe_printf(" Ignoring correctable error in poisoned page");
	    ignore = 1;
	} else {
	    /*
	     * Re-access the memory location to see if the error recurs,
	     * so we can tell if it was a transient read error or if the
	     * data is actually bad in memory.
	     */

	    *(volatile __psunsigned_t *) TO_UNCAC(memaddr);

	    mem_err.merr_reg = LOCAL_HUB_L(MD_MEM_ERROR_CLR);

	    mem_ce = ((mem_err.merr_reg	  >> MEM_ERROR_VALID_SHFT) ==
		      (MEM_ERROR_VALID_CE >> MEM_ERROR_VALID_SHFT));

	    if (mem_ce) {
		/*
		 * Scrub (repair single-bit error)
		 *
		 * Reading the corresponding byte in back door ECC memory
		 * causes the Hub to perform an atomic scrub operation and
		 * recalculate the ECC.
		 */

		(void) *(volatile u_char *) BDECC_ENTRY(memaddr);
		
		*(volatile __psunsigned_t *) TO_UNCAC(memaddr);
                mem_err.merr_reg = LOCAL_HUB_L(MD_MEM_ERROR_CLR);

                mem_ce = ((mem_err.merr_reg   >> MEM_ERROR_VALID_SHFT) ==
                          (MEM_ERROR_VALID_CE >> MEM_ERROR_VALID_SHFT));


		if (mem_ce)
			sbe_printf("Hard Single-bit error.");
		else
			sbe_printf(" Single-bit error has been repaired.");
	    } else {
		/*
		 * The error was a transient read error.  The data in RAM
		 * is actually OK.  This is likely to be one of those NEC
		 * DIMMs with a timing problem.  We are going to ignore
		 * these for now (and possibly keep track of them later).
		 */

		sbe_printf(" Transient read error ignored");

		ignore = 1;
	    }
	}
    }

    if (panic_on_sbe)
	cmn_err(CE_PANIC,
		"ECC single-bit error (systune panic_on_sbe enabled)");

    /*
     * Check for debilitating interrupt rate.  If too many interrupts
     * are occurring, stop monitoring them.
     */

    if (time > info->intr_tm) {
	info->intr_tm = time + 60;
	info->intr_ct = 0;
    }

    if (++info->intr_ct > SBE_MAX_INTR_PER_MIN) {
	bank_fatal((int) (memaddr >> MD_BANK_SHFT &
			  MD_BANK_MASK >> MD_BANK_SHFT), locbuf);
	return;
    }

    /*
     * Processing ends if error was not located.
     */

    if (ignore)
	return;

    pfn = pnum(memaddr);
    bank = (int) (memaddr >> MD_BANK_SHFT & MD_BANK_MASK >> MD_BANK_SHFT);
    log = info->log;

    info->bank_cnt[bank]++;

    /*
     * Discard expired events.
     */

    for (i = j = 0; i < info->log_cnt; i++)
	if (time < log[i].expire) {
	    if (j != i)
		log[j] = log[i];
	    j++;
	}

    info->log_cnt = j;

    /*
     * See if page is a repeat offender.
     */

    for (i = 0; i < info->log_cnt; i++)
	if ((dir_ce && (log[i].flags & SBE_EVENT_DIR) != 0 ||
	     mem_ce && (log[i].flags & SBE_EVENT_DIR) == 0) &&
	    log[i].pfn == pfn)
	    break;

    if (i == info->log_cnt) {
	/*
	 * First time for page.
	 *
	 * If there is no room for a new entry, a high error rate is in
	 * effect to all different pages.  It is likely all errors are
	 * coming from the same bank because a data line is stuck.
	 * Assuming the current error is also from the bad bank, we
	 * disable the current error's bank.
	 */

	if (info->log_cnt == SBE_EVENTS_PER_NODE)
	    bank_fatal(bank, locbuf);
	else {
	    i = info->log_cnt++;

	    log[i].pfn = pfn;
	    log[i].flags = (dir_ce ? SBE_EVENT_DIR : 0);
	    log[i].expire = time + SBE_TIMEOUT;
	    log[i].repcnt = 1;
	}
    } else {
	/* Repeat offender */

	log[i].repcnt++;

	if (log[i].repcnt == sbe_max_per_page) {
	    sbe_printf("Excessive single-bit ECC corrections "
		       "in pfn %d", pfn);
	    get_slotname(SLOTNUM_GETSLOT(nodepda->slotdesc), slotname);
	    sbe_printf("Bank /hw/module/%d/slot/%s/node/"
		       "memory/dimm_bank/%d (%s)",
		       nodepda->module_id, slotname, bank, locbuf);
	    sbe_printf("Attempting to remove this page from service.");

	    (void) page_discard(memaddr, PERMANENT_ERROR, CORRECTABLE_ERROR);

	    log[i].expire = time + SBE_DISCARD_TIMEOUT;
	} else if (log[i].repcnt == sbe_maxout)
	    bank_fatal(bank, locbuf);
	else
	    log[i].expire = time + SBE_TIMEOUT;
    }
}

void install_eccintr(cnodeid_t cnode)
{
    cpuid_t		cpu;

    if ((cpu = CNODE_TO_CPU_BASE(cnode)) != CPU_NONE)
	intr_connect_level(cpu,
			   INT_PEND1_BASELVL + MD_COR_ERR_INTR,
			   INTR_SWLEVEL_NOTHREAD_DEFAULT,
			   memerror_sbe_intr, 0, NULL);
}

/*
 * memerror_get_stats
 *
 *   Gets the SBE count for each bank.
 */

void memerror_get_stats(cnodeid_t cnode,
			int *bank_stats, int *bank_stats_max)
{
    sbe_info_t	       *info;
    int			i;

    info = NODEPDA(cnode)->sbe_info;

    for (i = 0; i < *bank_stats_max && i < MD_MEM_BANKS; i++)
	bank_stats[i] = info->bank_cnt[i];

    *bank_stats_max = i;
}
