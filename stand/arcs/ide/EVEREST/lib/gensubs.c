
/*
 * IP19/lib/gensubs.c
 *
 * 	general-purpose primitives used by IP17diags.
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.7 $"

#include <sys/cpu.h>
#include <sys/invent.h>
#include <sys/param.h>
#include "libsc.h"
#include "libsk.h"
#include "ide_msg.h"
#include "ip19.h"
#include "pattern.h"
#include "prototypes.h"


extern struct reg_desc sr_desc[];
extern void pd_hwbinv(__psunsigned_t);

unsigned char *buf_alloc(int, int);
void buf_free (unsigned char *);

/* returns the number of bits necessary to shift to next secondary
 * cache line.  Expects config_info.sidline_size to be set: illegal
 * 2nd-line sizes return -1.  This was a constant (SET_SHIFT, #defined
 * in IP*diags/include) in previous diagnostics.  Note that 
 * sidline_size is in bytes. */
int set_shift(void)
{
	switch(config_info.sidline_size) {
	case FOUR_WD_2NDLINE:
		return(4);
	case EIGHT_WD_2NDLINE:
		return(5);
	case SIXTEEN_WD_2NDLINE:
		return(6);
	case THIRTYTWO_WD_2NDLINE:
		return(7);
	default:
		return(-1);
	}
}


/* buf_alloc() and buf_free() were originally in io/alloc.c */
#ifdef PROM
#define	ALLOCSIZE	0x8000	
#else
#define	ALLOCSIZE	0x80000	
#endif /* PROM */

static unsigned char			alloc_buf[ALLOCSIZE];
static unsigned char 			*alloc_pt = alloc_buf;

/* mem_init() either sets memory via uncached, unmapped accesses (K1SEG)
 * to the value specified by the 'pattern' parm (M_I_PATTERN mode), an
 * ascending value incremented by 4 per word, with starting value
 * specified by 'pattern' (M_I_ASCENDING) or the one's complement
 * of the ascending value (M_I_ASCENDING_INV).
 */
uint
mem_init(first_addr, last_addr, mode, pattern)
  uint first_addr;
  uint last_addr;
  enum fill_mode mode;  /* M_I_PATTERN or M_I_ASCENDING */
  uint pattern; /* value to fill (M_I_PATTERN) or begin with (M_I_ASCENDING) */
{
    uint count = 0;             /* return # bytes filled */
    register uint *firstp, *lastp;
    uint *ptr;

#if defined(R4K_UNCACHED_BUG)
    invalidate_dcache(PD_SIZE, PDL_SIZE);
    invalidate_scache(SID_SIZE, SIDL_SIZE);
#endif /* R4K_UNCACHED_BUG */

    if ((mode < M_I_PATTERN) || (mode > M_I_ALT_PATTERN)) {
        /* message(LOGGED, "mem_init: illegal mode %d\n",mode); */
        printf("mem_init: illegal mode %d\n",mode);
        return((uint)-1);
    }

    if ((first_addr > MAX_PHYS_MEM) || (last_addr > MAX_PHYS_MEM)) {
        /* message(LOGGED, "mem_init: illegal %s addr 0x%x\n", */
        printf("mem_init: illegal %s addr 0x%x\n",
        ((first_addr > MAX_PHYS_MEM) ? "starting": "ending"),
        ((first_addr > MAX_PHYS_MEM) ? first_addr : last_addr));
        return((uint)-1);
    }

    /* access via uncached and unmapped segment */
    firstp = (uint *)PHYS_TO_K1(first_addr);
    lastp = (uint *)PHYS_TO_K1(last_addr);

    switch(mode) {

    case M_I_PATTERN:   /* write memory with user defined pattern */
	msg_printf(VRB, "Write address pattern 0x%x to 0x%x\r",
                pattern,(__psunsigned_t)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = pattern;
            count += sizeof(uint);
        }
        break;

    case M_I_ALT_PATTERN:  /* write mem w/ alternating-not user-defined pat */
	msg_printf(VRB, "Write alternating-one's comp addr pat 0x%x to 0x%x\r",
                pattern,(__psunsigned_t)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = pattern;
            pattern = ~pattern;
            count += sizeof(uint);
        }
        break;

    case M_I_ASCENDING:         /* ascending-value fill mode */
	msg_printf(VRB, "Write ascending-value pat (start-val 0x%x) from 0x%x\r",
                pattern, (__psunsigned_t)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = pattern;
            pattern += sizeof(uint);
            count += sizeof(uint);
        }
        break;

    case M_I_ASCENDING_INV:           /* inverted ascending-value fill mode */
	msg_printf(VRB, "Write inverse ascending-value pat (start-val 0x%x) from 0x%x\r",
                ~pattern, (__psunsigned_t)firstp);
        for (ptr = firstp; ptr <= lastp; ptr++) {
            *ptr = ~pattern;
            pattern += sizeof(uint);
            count += sizeof(uint);
        }
        break;

    }

#if (SANITY_CHECK == 2)
    msg_printf(VRB, "mem_init exits, 0x%x bytes initialized\n",count);
#else
    msg_printf(VRB, "\n");
#endif
    return(count);

} /* fn mem_init */


/*
 * allocate an 'n_byte' bytes buffer, align on 'alignment' byte-boundary
 */
unsigned char *buf_alloc (int n_byte, int alignment)
{
    unsigned char *pt;

    alignment--;
    pt = (unsigned char *)((((__psunsigned_t)alloc_pt) + alignment) & ~alignment);
    if ((pt + n_byte) <= (alloc_buf + ALLOCSIZE)) {
	alloc_pt = pt + n_byte;
	return (pt);
    } else
	return (NULL);

}   /* buf_alloc */


/*
 * free up buffer allocated by alloc ()
 */
void buf_free (unsigned char *pt)
{
    if (pt == (unsigned char *)NULL)
	alloc_pt = alloc_buf;
    else if ((pt >= alloc_buf) && (pt < (alloc_buf + ALLOCSIZE)))
	alloc_pt = pt;

}   /* buf_free */


/* the format of the taglo register when it contains a 2ndary tag is 
 * <paddr, state, vindex, ecc>.  Internally the fields are ordered
 * <ecc, state, vindex, paddr>, and the checkbits are generated and
 * checked with the fields ordered in the internal format.
 * st_to_i() converts the 's_taglo' value into the internal format.
 */
uint
st_to_i(uint s_taglo)
{
	uint swapped_sval = 0;

#ifdef IP19
	swapped_sval = SADDR_TTOI(s_taglo);
	swapped_sval |= SSTATE_TTOI(s_taglo);
	swapped_sval |= SVIND_TTOI(s_taglo);
	swapped_sval |= SECC_TTOI(s_taglo);
#endif
	return(swapped_sval);

} /* ecc_swap_s_tag */


#ifdef BOTH_WAYS
ecc_swap_s_tag(which_way, swap_infop)
  uint which_way;
  tag_swap_info_t *swap_infop;
{
	uint swapped_val;
	uint in_val = swap_infop->ts_in_val;

	switch (which_way) {
	case TAG_TO_INTERNAL:	/* swap, then set 25-bit value and cbits */
		swapped_val = SADDR_TTOI(in_val);
		swapped_val |= SSTATE_TTOI(in_val);
		swapped_val |=  SVIND_TTOI(in_val);
		swap_infop->ts_out_25 = swapped_val;
		swap_infop->ts_out_32 = (swapped_val | SECC_TTOI(in_val));
		/* low 7 bits of TagLo reg are checkbits */
		swap_infop->ts_cbits = (in_val & SECC_MASK);
		return(0);

	case INTERNAL_TO_TAG:	/* set entire 32-bits plus cbits */
		swapped_val = SADDR_ITOT(in_val);
		swapped_val |= SSTATE_ITOT(in_val);
		swapped_val |=  SVIND_ITOT(in_val);
		swapped_val |=  SECC_ITOT(in_val);
		swap_infop->ts_out_32 = swapped_val;
		/* high 7 bits of internal format are checkbits */
		swap_infop->ts_cbits = SECC_ITOT(in_val);
		return(0);

	default:
		printf("ecc_swap_s_tag: illegal direction (%d)\n",which_way);
		return(-1);
	}

} /* ecc_swap_s_tag */
#endif /* BOTH_WAYS */

/* begin tag-ecc-comp stuff */





#ifdef NOT_NEC
/*
 * C0_TAGLO definitions for setting/getting cache states and physaddr bits
 */
#define SADDRMASK  	0xFFFFE000	/* 31..13 -> 2ndary paddr bits 35..17 */
#define SVINDEXMASK	0x00000380	/* 9..7: prim virt index bits 14..12 */
#define SSTATEMASK	0x00001c00	/* bits 12..10 hold 2ndary line state */
#define SINVALID	0x00000000	/* invalid --> 000 == state 0 */
#define SCLEANEXCL	0x00001000	/* clean exclusive --> 100 == state 4 */
#define SDIRTYEXCL	0x00001400	/* dirty exclusive --> 101 == state 5 */
#define SECC_MASK	0x0000007f	/* low 7 bits are ecc for the tag */
#define SADDR_SHIFT	4		/* shift STagLo (31..13) to 35..17 */
#endif /* NOT_NEC */


#define E_TREE7B6 0x0a8f888
#define E_TREE7B5 0x114ff04
#define E_TREE7B4 0x0620f42
#define E_TREE7B3 0x09184f0
#define E_TREE7B2 0x10a40ff
#define E_TREE7B1 0x045222f
#define E_TREE7B0 0x1ff1111

struct emask7b {
	uint emask;
};

struct emask7b ptrees[] = {
	 E_TREE7B0,
	 E_TREE7B1,
	 E_TREE7B2,
	 E_TREE7B3,
	 E_TREE7B4,
	 E_TREE7B5,
	 E_TREE7B6,
};

#define S_TAGLO_ECC	0x7f
#define ECC_TAGCBITS	7

#define CBITS_IN	0
#define CBITS_OUT	1
#define SYNDROME	2

#define VINDEX_BITS	3
#define CS_BITS		3
#define SVINDEX_ROLL	(ECC_TAG_CBITS)
#define SSTATE_ROLL	(VINDEX_BITS+ECC_TAG_CBITS)
#define SADDR_ROLL	(CS_BITS+VINDEX_BITS+ECC_TAG_CBITS)



#ifdef LATER_MAYBE
main(argc, argv)
char *argv[];
{
	extern int ecc_tsyn(), ecc_tcbits();

	ulong taglo;
	ulong data;
	unchar cbits_in;
	unchar cbits;
	unchar syndrome;
	unchar badsyn;
	uint cbitxor = 0;
	uint badbit = 0x1;
	uint ints3[3];
	int i;

	argc--; argv++;

	if(argc < 1) {
		printf("usage: ecomp data\n");
		exit(1);
	}

	data = strtoul(argv[0], 0, 0);

	cbits = ecc_tcbits(data);
	ints3[CBITS_IN] = cbits;
	printf("data 0x%x, cbits 0x%x, syndrome 0x%x\n",
		data, cbits, ints3[SYNDROME]);

/*
	cbits = ecc_tcbits(data, ints3);
	cbits = compute_cbits(data);
	syndrome = compute_syndrome(data, cbits);

	if (cbitxor) {
		cbitxor ^= cbits;
		badsyn = compute_syndrome(data, cbitxor);
	}
	syndrome = cbits_in ^ cbits;
*/

	printf("data 0x%x, cbits_in 0x%x, cbits out 0x%x, syndrome 0x%x\n",
		data, ints3[CBITS_IN], ints3[CBITS_OUT], ints3[SYNDROME]);




/*
	if (cbitxor)
		printf("	cbitxor 0x%x, messed up syndrome 0x%x\n",
			cbitxor, badsyn);
*/

}

ecc_tcbits(data)
uint data;
{
	uint shi;
	register pbit;
	register int i;
	register int j;
	struct emask7b *ep;
	unchar cbits;

	cbits = 0;
	ep = &ptrees[0];
	for (i = 0; i < 7;  i++, ep++) {
		shi = data & ep->emask;
		pbit = 0;

		for (j = 0; j < 25; j++) {
			if (shi & (1 << j))
				pbit++;
		}
		if (pbit & 1)
			cbits |= 1 << i;
	}
	return cbits;
}
#endif /* LATER_MAYBE */


 /* given a 25-bit tag value (i.e. without checkbits), arranged in 
  * the s_TagLo register-format, compute and return the 7 checkbits
  * derived from the tagvalue, using the algorithm applied by the R4K
  */
uint
calc_tag_cbits(uint s_taglo)
{
	uint shi;
	uint true_val = 0;
	register pbit;
	register int i,j;
	struct emask7b *ep;
	unchar cbits = 0;

	/* Internally the R4k stores the fields comprising secondary 
	 * tags in a different format than the one it uses for the
	 * fetched TagLo register.  
	 * The internal format is: <ECC, CS, PIdx, Stag>; 
	 * the S-Taglo register format is: <STag, CS, Vidx, ECC>.
	 * The R4K computes and checks the cbits with the tag-fields
	 * in the internal format; since the incoming tag is from the
	 * taglo register, we must rearrange before computing. */
	true_val = st_to_i(s_taglo);

	ep = &ptrees[0];
	for (i = 0; i < 7;  i++, ep++) {
		shi = true_val & ep->emask;
			pbit = 0;
	
			for (j = 0; j < 25; j++) {
				if (shi & (1 << j))
					pbit++;
			}
			if (pbit & 1)
				cbits |= 1 << i;
	}
	return(cbits);

} /* calc_tag_cbits */





/* end of tag-ecc-comp stuff */

/* when ecc errors occur, the handlers must be able to clear not only
 * the tags of all caches, but the DATA in them also; otherwise when
 * the data is replaced (after reseting the tag) the faulty data will
 * cause another exception.  This must be done with the SR_DE bit set
 * (no exceptions), interrupts disabled, and very carefully in a specific
 * order.  We'll try to clear the icache also.  If the error came in via
 * the SysAD bus this'll allow us to continue until we mess with that
 * address again; if the error is in the caches we are dead in the
 * water if we continue to cached-references.
 */
#define ONE_WD		(sizeof(int))

ecc_clr_caches()
{
	register __psunsigned_t paddr;
	register volatile __psunsigned_t caddr;
	volatile __psunsigned_t dummy;
	__psunsigned_t oldsr;
	uint dsize = get_dcachesize();
	uint isize = get_icachesize();
	uint ssize = get_scachesize();
	uint dline = get_dcacheline();
	uint iline = get_icacheline();
	uint sline = get_scacheline();
	uint hi_pdc_paddr = PHYS_FILL_LO + dsize;
	uint hi_pic_paddr = PHYS_FILL_LO + isize;
	uint hi_sc_paddr = PHYS_FILL_LO + ssize;

	oldsr = get_sr();

#ifndef TFP
	if (!(SR_BEV & oldsr)) {
		/* message(LOGGED,"ecc_clr_caches: SR_BEV NOT SET!!!\n\tSR %R\n", */
		printf("ecc_clr_caches: SR_BEV NOT SET!!!\n\tSR %R\n",
			oldsr, sr_desc);
		oldsr |= SR_BEV;
	}

	/* no interrupts or ecc exceptions */
	set_sr (oldsr & ~(SR_DE | SR_IMASK));
#endif
	/* clear any pending bus interrupts */
	dummy = EV_GET_LOCAL(EV_CIPL124);

	/* set pdcache tags DE and mapped consecutively
	 * from PHYS_FILL_LO to cause all primary hits for
	 * the initial pdcache-sized writes.
	 */
	for (paddr = PHYS_FILL_LO; paddr < hi_pdc_paddr; paddr += dline)
		set_tag(PRIMARYD, DIRTY_EXCL, paddr);

	/* Set scache tags also to DE and mapped consecutively from
	 * PHYS_FILL_LO.  We can then initialize the entire scache
	 * without missing or fetching from memory.  The initial
	 * (pdcache-size) writes will stay in primary; they will begin
	 * to flush to 2ndary as write addrs beyond the pdcache's size  */ 
	for (paddr = PHYS_FILL_LO; paddr < hi_sc_paddr; paddr += sline)
		set_tag(SECONDARY, DIRTY_EXCL, paddr);

	/* now zero-out all primary-d and secondary data-rams: R4K 
	 * will automatically calculate new ecc checkbits */
	caddr = PHYS_TO_K0(PHYS_FILL_LO);
	for (paddr = PHYS_FILL_LO; paddr < hi_sc_paddr; paddr += ONE_WD) {
		*(uint *)caddr = 0;
		caddr += ONE_WD;
	}
	/* flush the top 8k of data from the dcache by reading one word
	 * per pdcache-line beginning back at PHYS_FILL_LO (this fetches
	 * data from secondary that was zeroed and flushed at the beginning
	 * of the write-loop above). */
	caddr = PHYS_TO_K0(PHYS_FILL_LO);
	for (paddr = PHYS_FILL_LO; paddr < hi_pdc_paddr; paddr += ONE_WD) {
		*(uint *)caddr = 0;
		caddr += ONE_WD;
	}

	/* Finally, clear the PI cache.  Set all tags to invalid, then 
	 * do cache-fills for each line, beginning at addr PHYS_FILL_LO.
	 * The cacheop calls will all hit secondary, filling the p-icache
	 * data-rams with zeros */
	for (paddr = PHYS_FILL_LO; paddr < hi_pic_paddr; paddr += iline)
		set_tag(PRIMARYI, INVALIDL, paddr);

	caddr = PHYS_TO_K0(PHYS_FILL_LO);
	for (paddr = PHYS_FILL_LO; paddr < hi_pic_paddr; paddr += iline) {
		pi_fill(caddr);
		caddr += iline;
	}
	
	invalidate_caches();	 /* don't want any valid tags now */

	/* clear any pending bus interrupts */
	dummy = EV_GET_LOCAL(EV_CIPL124);
	set_sr(oldsr);	/* restore SR--with SR_BEV added if necessary! */

	return(0);

} /* ecc_clr_caches */


#ifdef NOTYET
show_ecc_data()
{
	/* if it's a data error and it came from the SysAD or an scache
	 * error, fetch dbl word and cbits and compute the syndrome */
	if ((dandt == D_REF) && (cache_err&CACHERR_EE||cache_err&CACHERR_EC)) {
		uint ecc;
		tag_regs_t tags;
		uint sidx = (cache_err & CACHERR_SIDX_MASK);
		uint paddr, lo_val, hi_val;

		_read_tag(CACH_SD, sidx, &tags); 
		ecc = get_ecc();
		paddr = (sidx | ((tags.tag_lo & SADDRMASK) << SADDR_SHIFT));
		lo_val = *(uint *)sidx;
		hi_val = *(uint *)(sidx+sizeof(uint));
		/* message(VERBOSE_ON,"loval 0x%x, hival 0x%x, cbits 0x%x\n", */
		printf("loval 0x%x, hival 0x%x, cbits 0x%x\n",
			lo_val, hi_val, ecc);
	}
} /* show_ecc_data */	
#endif

int
setup_icache(__psunsigned_t fn_start, __psunsigned_t fn_end)
{
	extern void _sd_ist(uint *, int);
	uint dsize = get_dcachesize();
	uint ssize = get_scachesize();
	uint iline = get_icacheline();
	uint sline = get_scacheline();
	__psunsigned_t cstart = K1_TO_K0(fn_start);
	__psunsigned_t cend = K1_TO_K0(fn_end);
	__psunsigned_t caches_begin = PHYS_TO_K0(PHYS_FILL_LO);
	__psunsigned_t pcaches_end = (caches_begin + dsize);
	__psunsigned_t scaches_end = (caches_begin + ssize);

	register uint *fillptr, wdsperpline, wdspersline;
	register volatile uint dummy;

	wdsperpline = iline / (uint)(sizeof(uint));	/* # wds per iline */
	wdspersline = sline / (uint)(sizeof(uint));	/* # wds per sline */

	invalidate_caches();
	
	/* fetch instrs into primary-d and secondary caches */
	fillptr = (uint *)cstart;
	while (fillptr <= (uint *)cend)
		dummy = *fillptr++;

	/* primary-d must be invalid for pi_fill to work */
	fillptr = (uint *)caches_begin;
	while (fillptr < (uint *)pcaches_end) {
		pd_hwbinv((__psunsigned_t)fillptr);
		fillptr += wdsperpline;
	}

	/* now fill the icache from secondary */
	fillptr = (uint *)cstart;
	while (fillptr <= (uint *)cend) {
		fill_ipline(fillptr);
		fillptr += wdsperpline;
	}

	/* and invalidate all secondary tags */
	fillptr = (uint *)caches_begin;
	while (fillptr < (uint *)scaches_end) {
		_sd_ist(fillptr, 0);
		fillptr += wdspersline;
	}

	return(0);

} /* setup_icache */

calibrate_1ms()
{
	__psunsigned_t bucaddr = 0xa0003000;
	__psunsigned_t eucaddr = 0xa0003010;

	/* message(NORMAL,"Time 1 ms\n"); */
	printf("Time 1 ms\n");

	*(uint *)bucaddr = 0xdeadbeef;
	us_delay(942);
	*(uint *)eucaddr = 0xa5a5a5a5;

	/* message(NORMAL,"end 1ms time loop\n"); */
	printf("end 1ms time loop\n");
	return(0);

}

/* Set up config_info with IP17 configuration (some are run-time queries) */
void setconfig_info(void)
{

	config_info.icache_size = get_icachesize();
	config_info.iline_size = get_icacheline();
	config_info.dcache_size = get_dcachesize();
	config_info.dline_size = get_dcacheline();
	config_info.sidcache_size = get_scachesize(); /* 0 if no 2ndary */
	config_info.sidline_size = get_scacheline();
	config_info.stop = 0;
	config_info.ctrlc = 0;
	config_info.noctrlc = 0;

	invalidate_caches();	/* clobber 'em all! */

	msg_printf(DBG,"\tInstruction cache size 0x%x\n",config_info.icache_size);
	msg_printf(DBG,"\tInstruction cache linesize 0x%x\n",config_info.iline_size);
	msg_printf(DBG,"\tData cache size 0x%x\n",config_info.dcache_size);
	msg_printf(DBG,"\tData cache linesize 0x%x\n",config_info.dline_size);
	msg_printf(DBG,"\tSecondary cache size 0x%x\n",config_info.sidcache_size);
	msg_printf(DBG,"\tSecondary cache linesize 0x%x\n",config_info.sidline_size);

}
