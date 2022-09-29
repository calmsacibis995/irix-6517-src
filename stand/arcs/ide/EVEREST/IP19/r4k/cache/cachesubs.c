
/*
 * path/pathsubs.c
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

#ident "$Revision: 1.6 $"


#include <sys/cpu.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "ip19.h"
#include "pattern.h"
#include "cache.h"
#include "prototypes.h"

#define SHORT_STATE_REPS	10
#define FULL_STATE_REPS		0x1000

/* #define SANITY_CHECK	1 */

/* limit the number of failures per test (called by traversem()) to
 * MAX_FAILS, then stop the test */
#define MAX_FAILS	4

static int cpu_loc[2];
void show_cacherr(uint);
void set_mem_locs(uint, uint, uint, uint);
void printwho(int);
int get_tag_info(uint, uint, tag_info_t *);
uint put_memline(uint, char *);
uint get_memline(uint, char *);
uint check_2ndary_val(uint, uint, uint);
uint traverse_em(PFI, uint, uint);
uint short_traverse_em(PFI, uint, uint);
void write_word(enum mem_space, uint, uint);
int flush2ndline(uint);
int set_tag(uint, enum c_states, uint);
void set_c_states(uint, enum c_states, enum c_states, uint);
uint check_mem_val(uint, uint, uint);
uint read_word(enum mem_space, uint);
uint num_cache_tests(void);
extern uint write_tag(uint, uint, struct tag_regs *);
extern uint read_tag(uint, uint, struct tag_regs *);

#define ERR_PADDR_MASK  0x3FFFFC        /* masks off all but phys address */

#define ERR_DCACHE      0x80000000      /* dcache if set, icache else */
#define ERR_SECONDARY   0x40000000      /* secondary if set, primary else */
#define ERR_DFIELD      0x20000000      /* data field err if set */
#define ERR_TFIELD      0x10000000      /* tag field err if set */
#define ERR_EXTERNAL    0x8000000       /* external ref if set, internal else */
#define ERR_SYSAD       0x4000000       /* SysAD Bys error */
#define ERR_INSTDATA    0x2000000       /* err on both instruction & data */
#define ERR_ECCREFILL   0x1000000       /* secondary ecc err on primary fill */

extern int QuietMsgs;	/* suppress printfs */

int SetUp;


/* display tables for cache-test function names */
char ctest0[] = "RHH_CE_CE";
char ctest1[] = "RHH_DE_DE";
char ctest2[] = "WHH_CE_CE";
char ctest3[] = "WHH_DE_DE";
char ctest4[] = "RMH_I_CE";
char ctest5[] = "RMH_I_DE";
char ctest6[] = "RMH_CE_CE";
char ctest7[] = "RMH_DE_DE";
char ctest8[] = "WMH_I_CE";
char ctest9[] = "WMH_I_DE";
char ctest10[] = "WMH_CE_CE";
char ctest11[] = "WMH_DE_DE";
char ctest12[] = "RMM_I_I";
char ctest13[] = "RMM_I_CE";
char ctest14[] = "RMM_I_DE";
char ctest15[] = "RMM_CE_CE";
char ctest16[] = "RMM_DE_DE";
char ctest17[] = "WMM_I_I";
char ctest18[] = "WMM_I_CE";
char ctest19[] = "WMM_I_DE";
char ctest20[] = "WMM_CE_CE";
char ctest21[] = "WMM_DE_DE";
char *testname[] = {	ctest0, ctest1, ctest2, ctest3, ctest4,
			ctest5, ctest6, ctest7, ctest8, ctest9,
			ctest10, ctest11, ctest12, ctest13, ctest14,
			ctest15, ctest16, ctest17, ctest18, ctest19,
			ctest20, ctest21,
};


struct config_info config_info;


/* set by cache primitives when an error occurred.  The cache diagnostic
 * routines use this to stop traverse_em() from calling zillions of times
 * after an error is encountered once, and to easily detect that they
 * occurred.
 */
int CSError;


/* kernel virtual memory access attributes:
 * k0seg: cached, unmapped.
 * k1seg: uncached, unmapped.
 * k2seg: mapped.
 */

uint num_cache_tests(void)
{
	return(sizeof(testname)/sizeof(char *));
}


/* sets cache line tag of primary or 2ndary cache(s) which maps the
 * specified physical addr to state including--if applicable--filling
 * it/them with valid date from memory.  This routine does not deal
 * with the instr. cache functions.
 *  which	 PRIMARYD, SECONDARY, or BOTH 
 *  Pstate, Sstate
 *  phys_addr	 MAX_PHYS_MEM 
 */
void set_c_states(uint which, enum c_states Pstate, enum c_states Sstate, uint phys_addr)
{
	uint k0addr = PHYS_TO_K0(phys_addr);	/* cached but unmapped space */
	uint val;

    	msg_printf(DBG, "set_c_states, which %d, Pstate %d, Sstate %d, paddr 0x%x\n", which, Pstate, Sstate, phys_addr);

	/* first invalidate primary and 2ndary so fetch fills them from mem */
	set_tag(PRIMARYD, INVALIDL, phys_addr);
	set_tag(SECONDARY, INVALIDL, phys_addr);

	/* fill primary & 2nd lines even if we're going to invalidate them */
	val = *(uint *)k0addr;

	if (which == SECONDARY || which == BOTH) {
		set_tag(SECONDARY, Sstate, phys_addr);
	}
	if (which == PRIMARYD || which == BOTH)	{
		set_tag(PRIMARYD,Pstate,phys_addr); /* sets WB bit if needed */
	}
    	msg_printf(DBG, "set_c_states exits\n");

}


/* returns -1 if called with invalid state */
int set_tag(uint which, enum c_states state, uint phys_addr)
{
	struct tag_regs tags;
	uint value;

	msg_printf(DBG, "set_tag, which %d, state %d, paddr 0x%x\n",
		which, state, phys_addr);
	read_tag(which, phys_addr, &tags);

	/* clear old state bits */
	if (which == SECONDARY)
		tags.tag_lo &= ~SSTATEMASK;
	else
		tags.tag_lo &= ~PSTATEMASK;

	switch(state) {
		case INVALIDL:
			break;	/* above clearing did the trick */
		case CLEAN_EXCL:
			if (which == SECONDARY) {
				tags.tag_lo |= SCLEANEXCL;
			}
			else {
				/* PCLEANEXCL sets V bit in P I cache */
				tags.tag_lo |= PCLEANEXCL;
			}
			break;
		case DIRTY_EXCL:
		case DIRTY_EXCL_NWB:
			if (which == PRIMARYI) {
				printf("Invalid state (0x%x) for Primary I-cache, addr 0x%x\n",
				state,phys_addr);
				return(-1);
			}
			if (which == SECONDARY) {
				tags.tag_lo |= SDIRTYEXCL;
			}
			else {
				tags.tag_lo |= PDIRTYEXCL;
			}
			break;
	}
	write_tag(which, phys_addr, &tags);

	/* if primary data cache and need dirty exclusive state, we must
	 * set the writeback bit */
	if ( (state == DIRTY_EXCL) && (which == PRIMARYD) ) {  
		/* read then write it--this sets 'W' bit w/o changing value */
		value = read_word(k0seg, phys_addr);
		write_word(k0seg, phys_addr, value);
	}
	msg_printf(DBG, "set_tag exits\n");
	return(0);
}


/* check_mem_val checks memory at physical addr 'physaddr' for contents.
 * If it matches 'expected' then return 'expected'; else display printf
 * and return the actual value.
 *  physaddr	 address of phys mem to check 
 *  expected	 value caller expects to be in physaddr 
 *  caller	 jump-table offset of calling routine for err msg 
 */
uint
check_mem_val(uint physaddr, uint expected, uint caller)
{
	uint value;

	msg_printf(DBG, "check_mem_val: paddr 0x%x, expected 0x%x, caller %d\n",
		physaddr, expected, caller);
	value = read_word(k1seg, physaddr);
	if (value != expected) {
		printwho(caller);
		err_msg(CSTATE_MEM, 0, physaddr, expected, value);
		CSError++;
		return(value);
	}
	msg_printf(DBG, "check_mem_val exits normally, return expected 0x%x\n",
		expected);
	return(expected);
}


/*
 * flush2ndline(physaddress)
 * Flush 2ndary cache-line at specified address to memory.  To provide
 * calling routine with a sanity-check, return the value of the CH bit
 * in the diagnostic portion of the status register (set if the cache-flush
 * instruction 'hit' secondary).  This routine is non-destructive; i.e.
 * the contents and state of the flushed secondary line is unchanged.
 * 	physaddr	 flush line in second-level cache which contains
 *			 this physical address 
 */
int flush2ndline(uint physaddr)
{
	struct tag_regs oldPtag, oldStag;
	uint caddr = PHYS_TO_K0(physaddr);

	msg_printf(DBG, "flush2ndline: paddr 0x%x\n", physaddr);

	/* hit writeback cache instruction on secondary will try to
	 * get newer data from the primary cache if it is dirty unless
	 * we temporarily mark it invalid. */
	read_tag(PRIMARYD, physaddr, &oldPtag);	/* save real tags */
	set_tag(PRIMARYD, INVALIDL, physaddr);

	/* secondary must be DE to flush */
	read_tag(SECONDARY, physaddr, &oldStag);
	set_tag(SECONDARY, DIRTY_EXCL, physaddr);
	
	if (!sd_HWB(caddr))		/* instr didn't hit secondary?? */
		return(MISSED_2NDARY);

	/* restore previous states */
	write_tag(PRIMARYD, physaddr, &oldPtag);
	write_tag(SECONDARY, physaddr, &oldStag);

	msg_printf(DBG, "flush2ndline exits\n");
	return(0);
}


/* given a physical address, read_word converts it to its equivalent
 * in the specified kernel space and reads the value there.
 *  accessmode	 	k0seg, k1seg, or k2seg 
 *  phys_addr		read from this address 
 */
uint
read_word(enum mem_space accessmode, uint phys_addr)
{
	uint vaddr;
#ifdef R4K_UNCACHED_BUG
	tag_regs_t PITagSave,PDTagSave,STagSave,TagZero;
	uint uval;

	/* If R4K is rev 1.2 and this read is via k1seg we must
	 * ensure that the pdcache doesn't currently have this
	 * line as valid (1.2 sometimes read/wrote the k0 value
	 * instead of k1 to memory); we must invalidate line to
	 * force R4K to read from memory, not cache.  But, check
	 * the processor id before wasting this time. */
	if ((accessmode == k1seg) && (get_prid() & 0xF)) {
		TagZero.tag_lo = 0;
		read_tag(PRIMARYI, phys_addr, &PITagSave);
		read_tag(PRIMARYD, phys_addr, &PDTagSave);
		read_tag(SECONDARY, phys_addr, &STagSave);
		write_tag(PRIMARYI, phys_addr, &TagZero);
		write_tag(PRIMARYD, phys_addr, &TagZero);
		write_tag(SECONDARY, phys_addr, &TagZero);
	}
#endif

	switch(accessmode) {
	case k0seg:
			vaddr = PHYS_TO_K0(phys_addr);
			break;
	case k1seg:
			vaddr = PHYS_TO_K1(phys_addr);
			break;
	case k2seg:
			vaddr = phys_addr+K2BASE;
			break;
	}

#ifdef R4K_UNCACHED_BUG
	uval = *(uint *)vaddr;
	if ((accessmode == k1seg) && (get_prid() & 0xF)) {
		write_tag(PRIMARYI, phys_addr, &PITagSave);
		write_tag(PRIMARYD, phys_addr, &PDTagSave);
		write_tag(SECONDARY, phys_addr, &STagSave);
	}
	return (uval);
#else
	return (*(uint *)vaddr);
#endif

}


/* given a physical address, write_word converts it to its equivalent
 * in the specified kernel space and stores value there.
 *  accessmode	 	k0seg, k1seg, or k2seg 
 *  phys_addr		write to this address 
 */
void write_word(enum mem_space accessmode, uint phys_addr, uint value)
{
	uint vaddr;
#ifdef R4K_UNCACHED_BUG
	tag_regs_t PITagSave,PDTagSave,STagSave,TagZero;

	/* if writing through k1seg and R4K is rev 1.2, must
	 * invalidate line in primary dcache to force R4K to
	 * write to memory, not the cache, due to bug */
	if ((accessmode == k1seg) && (get_prid() & 0xF)) {
		TagZero.tag_lo = 0;
		read_tag(PRIMARYI, phys_addr, &PITagSave);
		read_tag(PRIMARYD, phys_addr, &PDTagSave);
		read_tag(SECONDARY, phys_addr, &STagSave);
		write_tag(PRIMARYI, phys_addr, &TagZero);
		write_tag(PRIMARYD, phys_addr, &TagZero);
		write_tag(SECONDARY, phys_addr, &TagZero);
	}
#endif

	switch(accessmode) {
	case k0seg:
			vaddr = PHYS_TO_K0(phys_addr);
			break;
	case k1seg:
			vaddr = PHYS_TO_K1(phys_addr);
			break;
	case k2seg:
			vaddr = phys_addr+K2BASE;
			break;
	}
	*(uint *)vaddr = value;

#ifdef R4K_UNCACHED_BUG
	if ((accessmode == k1seg) && (get_prid() & 0xF)) {
		write_tag(PRIMARYI, phys_addr, &PITagSave);
		write_tag(PRIMARYD, phys_addr, &PDTagSave);
		write_tag(SECONDARY, phys_addr, &STagSave);
	}
#endif

}

/*
  short_traverse_em:

  function	 addr of cache-test function to execute 
  start_paddr	 bagin calling function with this phys. addr 
  parm1		 generic parameter: cast to correct type by called routine 
*/

uint short_traverse_em(PFI function, uint start_paddr, uint parm1)
{
	uint pdsize = config_info.dcache_size;
	register uint pdline = config_info.dline_size;
	uint sidsize = config_info.sidcache_size;
	register uint sidline = config_info.sidline_size;
	register uint secslots = sidsize/sidline;  /* # lines in 2nd cache */
	uint primaryslots = pdsize/pdline;   /* # lines in primary cache */
	register uint p_to_s_ratio = secslots/primaryslots;
	register uint physaddr = start_paddr;
	uint i,res;
	uint Failed = 0;

	CSError = 0;
	for (i = 0; i < SHORT_STATE_REPS; i++) {
		/* if called routine returns -1 the test isn't applicable */
		if (res = function(physaddr, parm1))
			Failed++;
		if (res == -1 || Failed > 4) {
			msg_printf(ERR, "Terminated abnormally after addr 0x%x\n",physaddr);
			break;
		}

/*		if (!(i % 0x100))
			msg_printf(VRB, "Checked through addr 0x%x\r", physaddr);
*/
		physaddr += sidline;		/* next 2ndary slot */
		if (((i+1) % p_to_s_ratio) == 0) /* use loop val of next iter */
			physaddr += pdline;	/* next primary slot */
	}
	msg_printf(VRB, "\n");
	return(Failed?1:0);

}



/* traverse_em calls the specified function once for each slot in the
 * secondary cache, using an address algorithm that ensures that each
 * primary slot is used also.  Simply, if for example the secondary
 * has 10x more slots than the primary then each primary slot is selected
 * 10 times.  I have assumed that testing one word in each secondary
 * line for all state-changes will verify that the line is acting
 * correctly (since other tests ensure that none of the bits in the
 * secondary cache are stuck, for example).  This function allows any
 * operation to be easily performed on all lines.
 *  function	 addr of cache-test function to execute 
 *  start_paddr	 bagin calling function with this phys. addr 
 *  parm1	 generic parameter: cast to correct type by called routine 
 */
uint traverse_em(PFI function, uint start_paddr, uint parm1)
{
	uint pdsize = config_info.dcache_size;
	register uint pdline = config_info.dline_size;
	uint sidsize = config_info.sidcache_size;
	register uint sidline = config_info.sidline_size;
	register uint secslots = sidsize/sidline;  /* # lines in 2nd cache */
	uint primaryslots = pdsize/pdline;   /* # lines in primary cache */
	register uint p_to_s_ratio = secslots/primaryslots;
	register uint physaddr = start_paddr;
	uint i,res;
	uint Failed = 0;

	CSError = 0;
	for (i = 0; i < FULL_STATE_REPS; i++) {
		/* if called routine returns -1 the test isn't applicable */
		if (res = function(physaddr, parm1))
			Failed++;
		if (res == -1 || Failed > 4) {
			msg_printf(ERR, "Terminated abnormally after addr 0x%x\n",physaddr);
			break;
		}

/*		if (!(i % 0x100))
			msg_printf(VRB, "Checked through addr 0x%x\r", physaddr);
*/
		physaddr += sidline;		/* next 2ndary slot */
		if (((i+1) % p_to_s_ratio) == 0) /* use loop val of next iter */
			physaddr += pdline;	/* next primary slot */
	}
	msg_printf(VRB, "\n");
	return(Failed?1:0);

}


/* 
 * there is no cache-primitive to fetch the contents of a particular 
 * secondary cache line, so we check them by flushing the line to 
 * memory after saving the target-memory for subsequent restoration.
 *  uint physaddr	 address of phys mem to check 
 *  uint expected	 value caller expects to be in physaddr 
 *  int caller		 jump-table offset of calling routine for err msg 
 */
uint check_2ndary_val(uint physaddr, uint expected, uint caller)
{
	char savemem[MAX_2ND_LSIZE];
	uint baseaddr = (physaddr & ~SECLINEMASK);    /* addr of line-base */
	uint retval;

    msg_printf(DBG, "check_2ndary_val: paddr 0x%x, expected 0x%x, caller %d\n",
	physaddr, expected, caller);
	get_memline(baseaddr, savemem);
	if ((retval = flush2ndline(physaddr)) != 0) {	/* missed 2ndary */
		printwho(caller);
		err_msg(CSTATE_WRBK, 0, physaddr);
		CSError++;
		return(retval);
	}
	retval = read_word(k1seg, physaddr);
	if (retval != expected) {
		printwho(caller);
		err_msg(CSTATE_2ND, 0, physaddr,expected, retval);
		CSError++;
	}
	put_memline(baseaddr, savemem);

    	msg_printf(DBG, "check_2ndary_val exits\n");
	return(retval);
}


/* fetch contents of the secondary-cache-line beginning at 'start_addr'
 * via reads through uncached and unmapped memory (k1seg).
 */
uint get_memline(uint phys_start_addr, char *buffer)
{
	uint llength = config_info.sidline_size;
	uint *k1addr = (u_int *)PHYS_TO_K1(phys_start_addr);
	uint wds = (uint)(llength/(sizeof(int)));
	int i;
	uint *uptr = (u_int *)buffer;	/* fill by words for efficiency */
#ifdef R4K_UNCACHED_BUG
	tag_regs_t PITagSave,PDTagSave, STagSave, TagZero;

	/* must invalidate line to force R4K to read from memory, not
	 * cache, due to bug */
	TagZero.tag_lo = 0;
	read_tag(PRIMARYI, phys_start_addr, &PITagSave);
	read_tag(PRIMARYD, phys_start_addr, &PDTagSave);
	read_tag(SECONDARY, phys_start_addr, &STagSave);
	write_tag(PRIMARYI, phys_start_addr, &TagZero);
	write_tag(PRIMARYD, phys_start_addr, &TagZero);
	write_tag(SECONDARY, phys_start_addr, &TagZero);
#endif

	for (i = 0; i < wds; i++) {
		*uptr++ = *k1addr++;
	}

#ifdef R4K_UNCACHED_BUG
	write_tag(PRIMARYI, phys_start_addr, &PITagSave);
	write_tag(PRIMARYD, phys_start_addr, &PDTagSave);
	write_tag(SECONDARY, phys_start_addr, &STagSave);
#endif
	return(wds);
}


/* copy contents of buffer out to memory beginning at 'start_addr'
 * via writes through uncached and unmapped memory (k1seg).
 */
uint put_memline(uint phys_start_addr, char *buffer)
{
	uint llength = config_info.sidline_size;
	uint *k1addr = (u_int *)PHYS_TO_K1(phys_start_addr);
	uint wds = (uint)(llength/(sizeof(int)));
	int i;
	uint *uptr = (u_int *)buffer;
#ifdef R4K_UNCACHED_BUG
	tag_regs_t PITagSave,PDTagSave,STagSave, TagZero;

	/* must invalidate line to force R4K to write to memory, not
	 * cache, due to bug */
	TagZero.tag_lo = 0;
	read_tag(PRIMARYI, phys_start_addr, &PITagSave);
	read_tag(PRIMARYD, phys_start_addr, &PDTagSave);
	read_tag(SECONDARY, phys_start_addr, &STagSave);
	write_tag(PRIMARYI, phys_start_addr, &TagZero);
	write_tag(PRIMARYD, phys_start_addr, &TagZero);
	write_tag(SECONDARY, phys_start_addr, &TagZero);
#endif

	for (i = 0; i < wds; i++) {
		*k1addr++ = *uptr++;
	}
#ifdef R4K_UNCACHED_BUG
	write_tag(PRIMARYI, phys_start_addr, &PITagSave);
	write_tag(PRIMARYD, phys_start_addr, &PDTagSave);
	write_tag(SECONDARY, phys_start_addr, &STagSave);
#endif
	return(wds);
}

/*
  which		 fetch tags from this cache 
  physaddr	 from the line containing this physical address 
  tag_info	 and store the state and addr here 
*/

int get_tag_info(uint which, uint physaddr, tag_info_t *tag_info)
{
	struct tag_regs tags;	/* taghi and taglo regs */

	msg_printf(DBG, "get_tag_info: which %d, paddr 0x%x\n", which,physaddr);

	if (which != PRIMARYD && which != PRIMARYI && which != SECONDARY) {
		return(-2);
	}

	read_tag(which, physaddr, &tags);

	if (which == SECONDARY) {
		tag_info->state = ((tags.tag_lo & SSTATEMASK) >> SSTATE_RROLL);
		tag_info->vindex = ((tags.tag_lo&SVINDEXMASK) << SVINDEXLROLL);
		tag_info->physaddr = (tags.tag_lo&SADDRMASK) << TAGADDRLROLL;
#if SANITY_CHECK == 5
		printf("SEC: taglo 0x%x; state 0x%x, vindx 0x%x, paddr 0x%x\n",
		    tags.tag_lo,tag_info->state,tag_info->vindex,
		    tag_info->physaddr);
#endif
	} else { /* PRIMARYI or PRIMARYD */
		tag_info->state = ((tags.tag_lo & PSTATEMASK) >> PSTATE_RROLL);
		/* cache paddr tops at bit 35.  we want bit 31 at top of word */
		tag_info->physaddr = (tags.tag_lo&PADDRMASK) << TAGADDRLROLL;
#if SANITY_CHECK == 5
		printf("PRIM: taglo 0x%x; state 0x%x, vindx 0x%x, paddr 0x%x\n",
		    tags.tag_lo,tag_info->state,tag_info->vindex,
		    tag_info->physaddr);
#endif
	}

	msg_printf(DBG, "get_tag_info exits\n");
	return(0);
}


/* call printf with 'where' disposition using 'caller' as an index into
 * the 'testname' stringptr array.  If illegal caller, print generic printf
 */
void printwho(int caller)
{
	if (caller < 0 || caller >= num_cache_tests())
		msg_printf(VRB, "    %s: ",(SetUp?"setup":"verify"));
	else
		msg_printf(VRB, "%s (cache_state %d) %s: ",testname[caller],caller,
			(SetUp?"setup":"verify"));
}


/* set either or both of the specified addresses through k1seg iff the
 * addr isn't DONT_DOIT */

void set_mem_locs(uint firstaddr, uint firstval, uint secondaddr, uint secondval)
{
	msg_printf(DBG, "set_mem_locs: a1 0x%x, v1 0x%x, a2 0x%x, v2 0x%x\n",
		firstaddr, firstval, secondaddr, secondval);
	if (firstaddr != DONT_DOIT)
		write_word(k1seg, firstaddr, firstval);
	if (secondaddr != DONT_DOIT)
		write_word(k1seg, secondaddr, secondval);
	msg_printf(DBG, "set_mem_locs exits\n");
}

static char * cerr_locs[] =
{
    "internal primary icache",
    "internal primary dcache",
    "internal secondary icache",
    "internal secondary dcache",
    "external primary icache",
    "external primary dcache",
    "external secondary icache",
    "external secondary dcache"
};

/*
 * pass valid cache error register contents to show_cacherr
 * and it prints out the error data can be called directly, but
 * mainly used as printout routine for parse_cacherr()
 */
void show_cacherr(uint cur_cacherr)
{
    uint errloc;

    /*
     * first, figure out which of the eight possible locations
     */
    errloc = 0;

    if (cur_cacherr & ERR_EXTERNAL)
	errloc |= 4;

    if (cur_cacherr & ERR_SECONDARY)
	errloc |= 2;

    if (cur_cacherr & ERR_DCACHE)
	errloc |= 1;


    msg_printf(ERR, "Cache exception on %s at Addr: 0x%x\n",
	    cerr_locs[errloc], (cur_cacherr & ERR_PADDR_MASK));

    /*
     * now, report all error types
     */
    msg_printf(ERR, "Error types: %s%s%s%s%s\n",
	    (cur_cacherr & ERR_DFIELD) ? "datafield " : "",
	    (cur_cacherr & ERR_TFIELD) ? "tagfield " : "",
	    (cur_cacherr & ERR_SYSAD)  ? "sysAD " : "",
	    (cur_cacherr & ERR_INSTDATA) ? "inst+data " : "",
	    (cur_cacherr & ERR_ECCREFILL) ? "ecc_on_refill" : "");
	
}
