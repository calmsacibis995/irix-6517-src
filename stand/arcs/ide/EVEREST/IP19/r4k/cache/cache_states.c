
/*
 * cache/cache_states.c
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

#ident "$Revision: 1.8 $"


/*
 * cache_states -- 
 *
 * tests for correct transition from every possible legal state of 
 * primary <==> secondary <==> memory.
 */

#include <sys/types.h>
#include "pattern.h"
#include <sys/cpu.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include "everr_hints.h"
#include "ide_msg.h"
#include "ip19.h"
#include "cache.h"
#include "prototypes.h"

extern char *testname[];
extern void printwho(int);
extern void mem_init(uint, uint, enum fill_mode, uint);
extern uint num_cache_tests(void);
extern uint short_traverse_em(PFI, uint, uint);
extern uint traverse_em(PFI, uint, uint);
extern int get_tag_info(uint, uint, tag_info_t *);
extern void set_c_states(uint, enum c_states, enum c_states, uint);

int short_runem(int which);
int RHH_CE_CE(uint physaddr, uint routine); 
int RHH_DE_DE(uint physaddr, uint routine); 
int WHH_CE_CE(uint physaddr, uint routine); 
int WHH_DE_DE(uint physaddr, uint routine); 
			 
int RMH_I_CE(uint physaddr, uint routine); 
int RMH_I_DE(uint physaddr, uint routine); 
int RMH_CE_CE(uint physaddr, uint routine); 
int RMH_DE_DE(uint physaddr, uint routine);
			 
int WMH_I_CE(uint physaddr, uint routine); 
int WMH_I_DE(uint physaddr, uint routine); 
int WMH_CE_CE(uint physaddr, uint routine); 
int WMH_DE_DE(uint physaddr, uint routine); 
			 
int RMM_I_I(uint physaddr, uint routine); 
int RMM_I_CE(uint physaddr, uint routine); 
int RMM_I_DE(uint physaddr, uint routine); 
int RMM_CE_CE(uint physaddr, uint routine); 
int RMM_DE_DE(uint physaddr, uint routine); 
			 
int WMM_I_I(uint physaddr, uint routine); 
int WMM_I_CE(uint physaddr, uint routine); 
int WMM_I_DE(uint physaddr, uint routine); 
int WMM_CE_CE(uint physaddr, uint routine); 
int WMM_DE_DE(uint physaddr, uint routine); 

uint check_tag(uint, uint, uint, uint, uint);
static int cpu_loc[2];
static int sanity_check = 1;	/* for initial runs, do extra checking to
				 * ensure that what we THINK happened
				 * actually happened. */

/* jumptable offsets: RHH_CE_CE means Read Hit primary and Hit secondary,
 * primary and secondary lines are Clean Exclusive */
#define _RHH_CE_CE	0
#define _RHH_DE_DE	1
#define _WHH_CE_CE	2
#define _WHH_DE_DE	3

#define _RMH_I_CE	4
#define _RMH_I_DE	5
#define _RMH_CE_CE	6
#define _RMH_DE_DE	7

#define _WMH_I_CE	8
#define _WMH_I_DE	9
#define _WMH_CE_CE	10
#define _WMH_DE_DE	11

#define _RMM_I_I	12
#define _RMM_I_CE	13
#define _RMM_I_DE	14
#define _RMM_CE_CE	15
#define _RMM_DE_DE	16

#define _WMM_I_I	17
#define _WMM_I_CE	18
#define _WMM_I_DE	19
#define _WMM_CE_CE	20
#define _WMM_DE_DE	21

PFI jumptab[] = {
RHH_CE_CE, 
RHH_DE_DE, 
WHH_CE_CE, 
WHH_DE_DE, 
			 
RMH_I_CE, 
RMH_I_DE, 
RMH_CE_CE, 
RMH_DE_DE,
			 
WMH_I_CE, 
WMH_I_DE, 
WMH_CE_CE, 
WMH_DE_DE, 
			 
RMM_I_I, 
RMM_I_CE, 
RMM_I_DE, 
RMM_CE_CE, 
RMM_DE_DE, 
			 
WMM_I_I, 
WMM_I_CE, 
WMM_I_DE, 
WMM_CE_CE, 
WMM_DE_DE, 
};


/* runem() looks at RunICached to determine whether to execute the 
 * cache-state tests in cached (K0SEG) or uncached (K1SEG) space.
 */
int RunICached = 0;

/* The easy cases of hits in both caches or misses in both caches due to
 * invalid lines are set up by a) and b); trickier are the cases when we
 * miss primary or primary and secondary *valid* lines to see that the
 * valid lines are or aren't written (depending on the state) before the
 * new lines are read in.
 * a) hits in both primary and secondary caches--created by doing a cached
 *    read of physaddr,
 * b) misses in both caches whose lines are INVALID, created by setting 
 *    both tags to INVALIDL,
 * c) misses in valid primary lines but hits in secondary, created by 
 *    filling 2ndary with physaddr contents, then doing a read on 
 *    (physaddr+PrimaryDataCacheSize).  A subsequent physaddr access
 *    then misses primary but hits secondary.
 * d) misses in valid primary AND secondary lines, created by filling
 *    the caches with lines from (physaddr + secondary-cache-size).
 *    This address maps to the same primary and secondary slots, thus
 *    ensuring misses on valid (clean or dirty) lines.
 */

short_cache_states()
{
        uint array[2];
        uint val;
        uint addr;

        msg_printf(INFO, " (short_cache_states) \n");
        return(short_runem(500));       /* for now, just run them all */

} /* short_cache_states */


cache_states()
{
        uint array[2];
        uint val;
        uint addr;

        msg_printf(INFO, " (cache_states) \n");
        return(runem(500));     /* for now, just run them all */

} /* cache_states */


int short_runem(which)
  int which;	/* if 0 run all in jmp table, else just requested routine */
{

	int DoEmAll = 1;	/* run all tests by default */
	uint pdsize, pdline, sidsize, sidline, topaddr, dummy1;
	int res, i, Failed = 0;

	getcpu_loc(cpu_loc);
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);
	setconfig_info();
	pdsize = config_info.dcache_size;
	pdline = config_info.dline_size;
	sidsize = config_info.sidcache_size;
	sidline = config_info.sidline_size;
	topaddr = PHYS_FILL_LO+(2*sidsize);

	/* fill 2 consecutive secondary-cache-sized chunks of memory 
	 * with an ascending pattern matching each word's physical
	 * address.  Some tests require reads of an initialized (i.e.
	 * known) alternative value. */
	msg_printf(VRB, "Initialize memory to addr value from 0x%x to  0x%x\n",
		PHYS_FILL_LO,topaddr);

	ide_invalidate_caches();
	mem_init(PHYS_FILL_LO,topaddr,M_I_ASCENDING,PHYS_FILL_LO);

	for (i = 0; i < num_cache_tests(); i++) {
		ide_invalidate_caches();
		msg_printf(SUM, "Execute cache-state test %d (%s)\n",
			i, testname[i]);
		Failed += short_traverse_em(jumptab[i], PHYS_FILL_LO, i);
	}
	ide_invalidate_caches();
	msg_printf(INFO, "Completed short cache state transition test\n");
	return(Failed?1:0);

} /* end fn short_runem */

/* driver routine: executes specified cache-state routine or all if
 * which >= JUMPTABSIZE.  runem sets a segment of phys mem via K1SEG,
 * invalidates primary data and secondary caches, then calls 
 * traverse_em() with the address of the routine to execute on each
 * of the primary and secondary cache lines.
 */
runem(which)
  int which;	/* if 0 run all in jmp table, else just requested routine */
{

	int DoEmAll = 1;	/* run all tests by default */
	uint pdsize, pdline, sidsize, sidline, topaddr, dummy1;
	int res, Failed = 0;

	getcpu_loc(cpu_loc);
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);
	setconfig_info();
	pdsize = config_info.dcache_size;
	pdline = config_info.dline_size;
	sidsize = config_info.sidcache_size;
	sidline = config_info.sidline_size;
	topaddr = PHYS_FILL_LO+(2*sidsize);
	ide_invalidate_caches();
	if (which > num_cache_tests()) {
		msg_printf(INFO, "Execute all %d cache-transition tests %s\n",
			num_cache_tests(),(RunICached?"CACHED":"UNCACHED"));
	} else {
		DoEmAll = 0;
		msg_printf(INFO, "Execute cache-transition test %d (%s) %s\n",
		    which,testname[which],(RunICached?"CACHED":"UNCACHED"));
	}

	/* fill 2 consecutive secondary-cache-sized chunks of memory 
	 * with an ascending pattern matching each word's physical
	 * address.  Some tests require reads of an initialized (i.e.
	 * known) alternative value. */
	msg_printf(VRB, "sidsize 0x%x  sidline 0x%x  pdsize 0x%x  pdline 0x%x\n", sidsize, sidline, pdsize, pdline);
	msg_printf(VRB, "Initialize memory to addr value from 0x%x to  0x%x\n",
		PHYS_FILL_LO,topaddr);

	mem_init(PHYS_FILL_LO,topaddr,M_I_ASCENDING,PHYS_FILL_LO);

	if (!DoEmAll) {
		ide_invalidate_caches();	/* begin tests with invalid caches */
		if (RunICached)
			Failed += traverse_em( (PFI)(K1_TO_K0(jumptab[which])), PHYS_FILL_LO, which);
		else
			Failed += traverse_em(jumptab[which],PHYS_FILL_LO, which);
	} else {
		int i;

		for (i = 0; i < num_cache_tests(); i++) {
			ide_invalidate_caches();
			msg_printf(SUM, "Execute cache-state test %d (%s)\n",
				i, testname[i]);
			if (RunICached)
				Failed+=traverse_em((PFI)(K1_TO_K0(jumptab[i])), PHYS_FILL_LO, i);
			else
				Failed += traverse_em(jumptab[i], PHYS_FILL_LO, i);
		}
	}
	ide_invalidate_caches();
	return(Failed?1:0);

} /* end fn runem */

	/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
	/* +++++++++++++++ Begin cache-state tests +++++++++++++++ */
	/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++ */

uint altaddr;	/* many of the functions use a second address */


/* Read hit primary (CE) and 2nd (CE) (ctest 0).
 * Check that the value is correct (the physmem addr) 
 * and that both tags are still CE. ###
 */
int RHH_CE_CE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set both caches to Clean Exclusive */
	set_c_states(BOTH, CLEAN_EXCL, CLEAN_EXCL, physaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);
	}

	/* read word from cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	/* each word in memory was set to contain its address */
	if (value != physaddr) {
		err_msg(CSTATE0_ERR, cpu_loc, physaddr, value);
		CSError++;
	}
	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read hit primary (DE) and 2nd (DE) (ctest 1).
 * Check value and that both are still DE. ###
 */
int RHH_DE_DE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set both caches to Dirty Exclusive */
	set_c_states(BOTH, DIRTY_EXCL, DIRTY_EXCL, physaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);
	}

	/* read word from cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	/* each word in memory was set to contain its address */
	if (value != physaddr) {
		err_msg(CSTATE1_ERR, cpu_loc, physaddr, value);
		CSError++;
	}
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write hit primary (CE) and 2nd (CE) (ctest 2).
 * Check that 2nd and memory still have old value and that both
 * cache lines are now DE. ###
 */
int WHH_CE_CE(uint physaddr, uint routine)
{

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set both caches to Clean Exclusive */
	set_c_states(BOTH, CLEAN_EXCL, CLEAN_EXCL, physaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);
	}

	/* write word via cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+2);

	/* neither memory nor 2nd cache line should have been
	 * changed; but the primary and 2ndary tags should be DE */
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write hit primary (DE) and 2nd (DE) (ctest 3).
 * Check that 2nd and memory still have old value and
 * that both lines are still DE. ###
 */
int WHH_DE_DE(uint physaddr, uint routine)
{

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set both caches to Dirty Exclusive */
	set_c_states(BOTH, DIRTY_EXCL, DIRTY_EXCL, physaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);
	}

	/* write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+2);

	/* neither memory nor 2nd cache line should have been
	 * changed; but the primary and 2ndary tags should be DE */
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (I) and hit 2nd (CE) (ctest 4).
 * Check that 2nd and memory still have old value and
 * that both lines are CE. ###
 */
int RMH_I_CE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set p cache to I, s to CE */
	set_c_states(BOTH, INVALIDL, CLEAN_EXCL, physaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, physaddr, PINVALID, routine);
		check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);
	}

	/* read word from cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	if (value != physaddr) {
		err_msg(CSTATE4_ERR, cpu_loc, physaddr, value);
		CSError++;
	}

	/* neither memory nor 2nd cache line should have been
	 * changed; and both tags should now be CE */

	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);

	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);
/* check_tag p was here */
	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (I) and hit 2nd (DE) (ctest 5).
 * Check that 2nd and memory still have old value and
 * that both lines are DE. ###
 */
int RMH_I_DE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set p cache to I, s to DE */
	set_c_states(BOTH, INVALIDL, DIRTY_EXCL, physaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, physaddr, PINVALID, routine);
		check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);
	}

	/* read word from cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	if (value != physaddr) {
		err_msg(CSTATE5_ERR, cpu_loc, physaddr, value);
		CSError++;
	}

	/* neither memory nor 2nd cache line should have been
	 * changed; and both tags should now be DE */
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}



/* 4 of the next 6 tests involve primary misses with secondary hits where
 * both lines are valid (CE or DE).  They are conducted by filling the
 * secondary line mapping physaddr, then the one that maps 
 * physaddr+primarycachesize, and validating the primary (that maps
 * to both secondary lines) with the latter address.  This sets up a
 * primary miss, secondary hit with valid lines.
 */

/* Read miss primary (CE) and hit 2nd (CE) (ctest 6).
 * Check that 2nd and memory still have old value and
 * that both lines are still CE. ###
 */
int RMH_CE_CE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	altaddr = physaddr+PD_SIZE;  /* up one prim-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* first fill secondary line with physaddr and set to CE */
	set_c_states(SECONDARY, CLEAN_EXCL, CLEAN_EXCL, physaddr);
	/* then fill lines with altaddr and set both to CE: load from
	 * physaddr will then miss primary but hit 2nd */
	set_c_states(BOTH, CLEAN_EXCL, CLEAN_EXCL, altaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, altaddr, PCLEANEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);
		check_tag(SECONDARY, altaddr, altaddr, SCLEANEXCL, routine);
	}

	/* begin test: read word from cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	if (value != physaddr) {
		err_msg(CSTATE6_ERR, cpu_loc, physaddr, value);
		CSError++;
	}

	/* neither memory nor either 2nd cache lines should have been
	 * changed; and both tags of physaddr lines should be CE and
	 * primary contains physaddr */
	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);
	check_2ndary_val(altaddr, altaddr, routine);
/* check_tag prim was here */
	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (DE) and hit 2nd (DE) (ctest 7).
 * Check that 2nd and memory still have old value and
 * that both lines are still CE. ###
 */
int RMH_DE_DE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	altaddr = physaddr+PD_SIZE;  /* up one prim-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* first fill secondary line with physaddr and set to DE */
	set_c_states(SECONDARY, DIRTY_EXCL, DIRTY_EXCL, physaddr);
	/* then fill lines with altaddr and set both to DE: the 
	 * load from physaddr will then miss primary but hit 2nd */
	set_c_states(BOTH, DIRTY_EXCL, DIRTY_EXCL, altaddr); /* sets W bit */

	/* modify altaddr in primary to be sure it is flushed to secondary
	 * but not to memory */
	write_word(k0seg,altaddr,(uint)altaddr+GENERIC_INC);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, altaddr, PDIRTYEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);
		check_tag(SECONDARY, altaddr, altaddr, SDIRTYEXCL, routine);
	}

	/* begin test: read word from cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	if (value != physaddr) {
		err_msg(CSTATE7_ERR, cpu_loc, physaddr, value);
		CSError++;
	}

	/* altaddr line in 2nd should contain value 'altaddr+GENERIC_INC',
	 * altaddr in memory should contain value 'altaddr',
	 * physaddr primary and 2ndary lines should be DE and 
	 * mapped for physaddr, and physaddr's 2ndary and memory should
	 * be unchanged (== 'physaddr'). */

	check_mem_val(altaddr, altaddr, routine);
	check_2ndary_val(altaddr, altaddr+GENERIC_INC, routine);

	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);

	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (I) and hit 2nd (CE) (ctest 8).
 * Check that 2nd and memory still have old value and
 * that both lines are DE. ###
 */
int WMH_I_CE(uint physaddr, uint routine)
{

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines, invalidate the primary and set the 2ndary to CE */
	set_c_states(BOTH, INVALIDL, CLEAN_EXCL, physaddr);
	/* write word (i.e. modify value) to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* neither memory nor 2nd cache line should have been
	 * changed; but the primary and 2ndary tags should now be DE */
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (I) and hit 2nd (DE) (ctest 9).
 * Check that 2nd and memory still have old value and
 * that both lines are DE. ###
 */
int WMH_I_DE(uint physaddr, uint routine)
{

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines, invalidate the primary and set the 2ndary to DE */
	set_c_states(BOTH, INVALIDL, DIRTY_EXCL, physaddr);
	/* write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* neither memory nor 2nd cache line should have been
	 * changed; but the primary and 2ndary tags should be DE */
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (CE) and hit 2nd (CE) (ctest 10).
 * ###
 */
int WMH_CE_CE(uint physaddr, uint routine)
{

	CSError = 0;
	altaddr = physaddr+PD_SIZE;  /* up one prim-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* first fill secondary line with physaddr and set to CE */
	set_c_states(SECONDARY, CLEAN_EXCL, CLEAN_EXCL, physaddr);
	/* then fill lines with altaddr and set both to CE: store to
	 * physaddr will then miss primary but hit 2nd */
	set_c_states(BOTH, CLEAN_EXCL, CLEAN_EXCL, altaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, altaddr, PCLEANEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);
		check_tag(SECONDARY, altaddr, altaddr, SCLEANEXCL, routine);
	}

	/* begin test: write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* neither memory nor either 2nd cache lines should have been
	 * changed, both tags of physaddr lines should now be DE, and
	 * primary contains physaddr */
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);

	check_2ndary_val(altaddr, altaddr, routine);
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (DE) and hit 2nd (DE) (ctest 11).
 * ###
 */
int WMH_DE_DE(uint physaddr, uint routine)
{

	CSError = 0;
	altaddr = physaddr+PD_SIZE;  /* up one prim-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* first fill secondary line with physaddr and set to DE */
	set_c_states(SECONDARY, DIRTY_EXCL, DIRTY_EXCL, physaddr);
	/* then fill lines with altaddr and set both to DE: store to
	 * physaddr will then miss primary but hit 2nd */
	set_c_states(BOTH, DIRTY_EXCL, DIRTY_EXCL, altaddr);

	/* modify altaddr in primary to be sure it is flushed
	 * to secondary but not to memory */
	write_word(k0seg,altaddr,(uint)altaddr+GENERIC_INC);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, altaddr, PDIRTYEXCL, routine);
		check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);
		check_tag(SECONDARY, altaddr, altaddr, SDIRTYEXCL, routine);
	}

	/* begin test: write word to cached, unmapped memory (i.e. pdcache) */
	write_word(k0seg, physaddr, (uint)(physaddr+GENERIC_INC));

	/* altaddr line in 2nd should contain value 'altaddr+GENERIC_INC',
	 * altaddr in memory should contain value 'altaddr',
	 * physaddr primary and 2ndary lines should be DE and mapped 
	 * for physaddr,
	 * and physaddr's 2ndary and memory should
	 * be unchanged (== 'physaddr'). */

	check_mem_val(altaddr, altaddr, routine);
	check_2ndary_val(altaddr, altaddr+GENERIC_INC, routine);

	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);

	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (I) and 2nd (I) (ctest 12).
 * Check that value is correct, that 2nd and memory
 * still have old value and that both lines are CE. ###
 */
int RMM_I_I(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set both caches to invalid. */
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);

	if (sanity_check) {
		check_tag(PRIMARYD, physaddr, physaddr, INVALIDL, routine);
		check_tag(SECONDARY, physaddr, physaddr, INVALIDL, routine);
	}

	/* begin test: read word from cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	if (value != physaddr) {
		err_msg(CSTATE12_ERR, cpu_loc, physaddr, value);
		CSError++;
	}

	/* neither memory nor 2nd cache line should have been
	 * changed; but the primary and 2ndary tags should be CE */
	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);
/* check_tag p was here */
	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (I) and miss 2nd (CE) (ctest 13).
 * Check that value is correct, that 2nd and memory
 * still have old value and that both lines are CE. ###
 */
int RMM_I_CE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines with altaddr to allow a CE secondary miss.  
	 * Invalidate primary and set secondary to CE */
	set_c_states(BOTH, INVALIDL, CLEAN_EXCL, altaddr);

	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, INVALIDL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SCLEANEXCL, routine);
	}

	/* begin test: read word via cached, unmapped memory */
	value = read_word(k0seg, physaddr);
	if (value != physaddr) {
		err_msg(CSTATE13_ERR, cpu_loc, physaddr, value);
		CSError++;
	}

	/* 2ndary should have sucked-in mem contents and still match
	 * them.  Tag addrs should match physaddr now, and the contents
	 * of altaddr in memory should NOT have been altered. */
	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);
	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);
	/* check that altaddr in mem wasn't written when new line was read */
	check_mem_val(altaddr, altaddr, routine);

	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (I) and miss 2nd (DE) (ctest 14).
 * Check that 2ndary line matches memory, that both tags
 * are CE, that the addr tags on both lines are correct,
 * and that the dirty altaddr secondary line was flushed
 * to memory. ###
 */
int RMM_I_DE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines with altaddr to allow a DE secondary miss.  
	 * Invalidate primary and set secondary to DE */
	set_c_states(BOTH, INVALIDL, DIRTY_EXCL, altaddr);

	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, INVALIDL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SDIRTYEXCL, routine);
	}

	/* now change value of 'altaddr' in memory--original val should
	 * flush from 2nd during this read, overwriting our change */
	write_word(k1seg, altaddr, (uint)altaddr+GENERIC_INC);

	/* begin test: read word from cached, unmapped mem (i.e. pdcache) */
	value = read_word(k0seg, physaddr);
	if (value != physaddr) {
		err_msg(CSTATE14_ERR, cpu_loc, physaddr, value);
		CSError++;
	}

	/* 2ndary should have sucked-in mem contents and still match
	 * them; however the primary and 2ndary tags should be CE
	 * and the tag addrs should be that of physaddr */
	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);
	/* check that altaddr in memory changed when new line was read
	 * (i.e. that altaddr line was flushed cuz it was dirty, now
	 * containing the original value (== altaddr) we set in 
	 * set_mem_locs() above, rather than altaddr+GENEERIC_INC, 
	 * which we subsequently poked via k1seg */
	check_mem_val(altaddr, altaddr, routine);

	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (CE) and miss 2nd (CE) (ctest 15).
 * Fill cache lines with a word from physaddr+2ndcachesize;
 * do a read, then check that the tags for both lines are CE
 * and have the correct phys addrs, and that the alternate
 * memory word hasn't changed ###.
 */
int RMM_CE_CE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines and set both caches to CE. */
	set_c_states(BOTH, CLEAN_EXCL, CLEAN_EXCL, altaddr);
	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, PCLEANEXCL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SCLEANEXCL, routine);
	}

	/* begin test: read word from physaddr via cached, unmapped memory */
	value = read_word(k0seg, physaddr);

	/* primary and secondary should contain the new line with physaddr.
	 * Tags should still be CE but now contain 'physaddr' instead
	 * of altaddr.
	 * Check that altaddr wasn't written before the new line was read.
	 * What the hell, check physaddr in memory too.
	 */
	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);

	check_mem_val(altaddr, altaddr, routine);
	check_mem_val(physaddr, physaddr, routine);

	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Read miss primary (DE) and miss 2nd (DE) (ctest 16).
 * Fill cache lines with a word from physaddr+2ndcachesize;
 * do a read, then check that the tags for both lines are now
 * CE and have the correct phys addrs, and that the alternate
 * memory word was written when the altaddr line was flushed. ###
 */
int RMM_DE_DE(uint physaddr, uint routine)
{
	uint value;

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines & set both caches to DE (sets primary writeback bit) */
	set_c_states(BOTH, DIRTY_EXCL, DIRTY_EXCL, altaddr);

	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, PDIRTYEXCL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SDIRTYEXCL, routine);
	}

	/* now change value of 'altaddr' in memory--original value should
	 * flush from 2nd during this read, overwriting our change */
	write_word(k1seg, altaddr, (uint)altaddr+GENERIC_INC);

	/* begin test: read word from physaddr via cached, unmapped memory */
	value = read_word(k0seg, physaddr);

	/* primary and secondary should contain the new line with physaddr.
	 * Tags should now be CE and contain physaddr instead of altaddr. */
	check_tag(PRIMARYD, physaddr, physaddr, PCLEANEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SCLEANEXCL, routine);

	/* Check that 'altaddr' 2ndary line was written to memory before
	 * the new line was read (i.e. that it contains its original 
	 * value (== altaddr)--set above in set_mem_locs()--rather then
	 * 'altaddr+GENERIC_INC', which we subsequently poked */
	check_mem_val(altaddr, altaddr, routine);
	check_mem_val(physaddr, physaddr, routine);

	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (I) and 2nd (I) (ctest 17).
 * Check that 2ndary line matches memory, that both tags
 * are DE, and that the addr tags on both lines are correct. ###
 */
int WMM_I_I(uint physaddr, uint routine)
{

	CSError = 0;
	set_mem_locs(physaddr, physaddr, DONT_DOIT, 0);
	/* fill lines and set both caches to invalid. */
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);

	/* begin test: write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* 2ndary should have sucked-in mem contents and still match
	 * them; however the primary and 2ndary tags should be DE */
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (I) and miss 2nd (CE) (ctest 18).
 * Check that 2ndary line matches memory, that both tags
 * are DE, and that the addr tags on both lines are correct. ###
 */
int WMM_I_CE(uint physaddr, uint routine)
{

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines with altaddr to allow a CE secondary miss.  
	 * Invalidate primary and set secondary to CE */
	set_c_states(BOTH, INVALIDL, CLEAN_EXCL, altaddr);

	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, INVALIDL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SCLEANEXCL, routine);
	}

	/* now change value of 'altaddr' in memory--original
	 * value should NOT flush from 2nd during this read */
	write_word(k1seg, altaddr, (uint)altaddr+GENERIC_INC);

	/* begin test: write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* 2ndary should have sucked-in mem contents and still match
	 * them; however the primary and 2ndary tags should be DE */
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);

	/* check that altaddr in mem wasn't overwritten (with original
	 * value that was in 2ndary) when new line was read */
	check_mem_val(altaddr, altaddr+GENERIC_INC, routine);

	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (I) and miss 2nd (DE) (ctest 19).
 * Check that 2ndary line matches memory, that both tags
 * are DE, that the addr tags on both lines are correct,
 * and that the dirty altaddr secondary line was flushed
 * to memory. ###
 */
int WMM_I_DE(uint physaddr, uint routine)
{

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines with altaddr to allow a DE secondary miss.  
	 * Invalidate primary and set secondary to DE */
	set_c_states(BOTH, INVALIDL, DIRTY_EXCL, altaddr);

	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, INVALIDL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SDIRTYEXCL, routine);
	}

	/* change value of 'altaddr' in memory--original value (in 2ndary)
	 * should flush during this read, overwriting our change */
	write_word(k1seg, altaddr, (uint)altaddr+GENERIC_INC);

	/* begin test: write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* 2ndary should have sucked-in mem contents and still match
	 * them; however the primary and 2ndary tags should be DE */
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	/* physaddr in memory should NOT have been changed, nor should
	 * its secondary line */
	check_mem_val(physaddr, physaddr, routine);
	check_2ndary_val(physaddr, physaddr, routine);

	/* check that altaddr in memory was written when new line was read
	 * (i.e. that altaddr line was flushed cuz it was dirty, over-
	 * writing our poked value of altaddr+GENERIC_INC */
	check_mem_val(altaddr, altaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (CE) and miss 2nd (CE) (ctest 20).
 * fill cache lines with a word from physaddr+2ndcachesize; do a
 * store, then check that the tags for both lines are DE and 
 * have the correct phys addrs, and that the alternate memory
 * word hasn't changed. ###
 */
int WMM_CE_CE(uint physaddr, uint routine)
{

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines with altaddr to allow CE primary & secondary misses. */
	set_c_states(BOTH, CLEAN_EXCL, CLEAN_EXCL, altaddr);

	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, PCLEANEXCL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SCLEANEXCL, routine);
	}

	/* begin test: write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* 2ndary should have sucked-in mem contents and still match
	 * them; however the primary and 2ndary tags should be DE */
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);
	/* check that altaddr in mem wasn't written when new line was read */
	check_mem_val(altaddr, altaddr, routine);

	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}


/* Write miss primary (DE) and miss 2nd (DE) (ctest 21).
 * Check that 2ndary line matches memory, that both tags
 * are DE, that the addr tags on both lines are correct,
 * and that the dirty altaddr primary and secondary lines
 * were flushed to memory. ###
 */
int WMM_DE_DE(uint physaddr, uint routine)
{

	CSError = 0;
	altaddr = physaddr+SID_SIZE;  /* up one 2nd-cache-sized chunk */

	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	/* fill lines with altaddr to allow DE primary & secondary misses. */
	set_c_states(BOTH, DIRTY_EXCL, DIRTY_EXCL, altaddr);

	if (sanity_check) {	/* fetch by physaddr, match on altaddr */
		check_tag(PRIMARYD, physaddr, altaddr, PDIRTYEXCL, routine);
		check_tag(SECONDARY, physaddr, altaddr, SDIRTYEXCL, routine);
	}
	/* change memory-value of altaddr to ensure that the dirty line
	 * is flushed before reading physaddr */
	write_word(k1seg, altaddr, (uint)altaddr+GENERIC_INC);

	/* begin test: write word to cached, unmapped memory */
	write_word(k0seg, physaddr, (uint)physaddr+GENERIC_INC);

	/* 2ndary should have sucked-in mem contents and still match
	 * them; however the primary and 2ndary tags should be DE */
	check_tag(PRIMARYD, physaddr, physaddr, PDIRTYEXCL, routine);
	check_tag(SECONDARY, physaddr, physaddr, SDIRTYEXCL, routine);

	check_mem_val(physaddr, physaddr, routine);
	/* check that altaddr in memory was written when new line 
	 * was read (i.e. that altaddr line was flushed cuz it was dirty */
	check_mem_val(altaddr, altaddr, routine);

	check_2ndary_val(physaddr, physaddr, routine);

	/* reset mem wds to previous vals, & prevent caches from flushing */
	set_mem_locs(physaddr, physaddr, altaddr, altaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, physaddr);
	set_c_states(BOTH, INVALIDL, INVALIDL, altaddr);

	return(RETCSERROR);

}

/* display tables for cache states */
char invalid[] = "INVALID";
char bad1[] = "ILLEGAL STATE (1)";
char pce[] = "CLEAN EXCLUSIVE";
char pde[] = "DIRTY EXCLUSIVE";
char sce[] = "CLEAN EXCLUSIVE";
char sde[] = "DIRTY EXCLUSIVE";
char bad6[] = "ILLEGAL STATE (6)";
char bad7[] = "ILLEGAL STATE (7)";
char *statetab[] = { invalid, bad1, pce, pde, sce, sde, bad6, bad7 };

char primd[] = "PRIMARYD";
char primi[] = "PRIMARYI";
char seclev[] = "SECONDARY";
char *cnames[] = { primd, primi, seclev };

/* given a physical address, cache, and expected value, check_tag checks
 * if the actual state and address of the designated cache line matches
 * expectations (the physaddr is rounded down to a linesize-boundary before
 * checking).  If not, a printf is displayed and FALSE (0) is returned.
 * Else TRUE (1) is silently returned.  If called with 'which' of other 
 * than PRIMARY{D,I} or SECONDARY, -2 is returned.  If 'expectedaddr' or 
 * 'expectedstate' are DONT_CHECK the address or state test, resp. is skipped.
 *  which		 PRIMARYD, PRIMARYI, or SECONDARY 
 *  physaddr
 *  expectedaddr	 tag may not be mapped to physaddr 
 *  expectedstate	 sbd.h: expected primary or 2ndary bits for state 
 *  caller		 defined number of calling routine for error msg 
 */
uint check_tag(uint which, uint physaddr, uint expectedaddr, uint expectedstate, uint caller)
{
    tag_info_t tag_info;
    uint actualstate;

    msg_printf(DBG, "check_tag, which %d, paddr 0x%x, expect 0x%x, state 0x%x\n", which,physaddr, expectedaddr, expectedstate);

    getcpu_loc(cpu_loc);

    if (get_tag_info(which, physaddr, &tag_info) == -2) {
	if (caller != DONT_DOIT)
	    printwho(caller);
	    msg_printf(ERR, "check_tag: illegal 'which' val %d\n", which);
    	return(-2);
    }

    if (expectedstate != DONT_CHECK) {
    	actualstate = (uint)tag_info.state;
    	if (which == SECONDARY)
    		expectedstate = ((expectedstate & SSTATEMASK) >> SSTATE_RROLL);
    	else /* PRIMARYD or PRIMARYI */
    		expectedstate = ((expectedstate & PSTATEMASK) >> PSTATE_RROLL);

    	if (expectedstate != actualstate) {
		printwho(caller);
		msg_printf(ERR, "check_tag-%s failed at addr 0x%x Expect %s, actual %s\n", cnames[which], physaddr, statetab[expectedstate], statetab[actualstate]);
/*    		err_msg(CSTATE_STATE, 0, cnames[which], physaddr,
		    statetab[expectedstate], statetab[actualstate]);
*/
		CSError++;
    	}
    }
    if (expectedaddr != DONT_CHECK) {
	uint actualaddr = tag_info.physaddr;
	uint maskedaddr;

	/* before comparing, must round expected addr down to a 
	 * linesized-boundary */
	if (which == SECONDARY) {
		expectedaddr &= ~(SEC_VADDR_MASK);
		maskedaddr = expectedaddr & (SINFOADDRMASK|INFOVINDMASK);
	} else {
		expectedaddr &= ~(PRIM_VADDR_MASK);
		maskedaddr = expectedaddr & PINFOADDRMASK;
	}

	if (maskedaddr != actualaddr) {
		printwho(caller);
    		err_msg(CSTATE_ADDR, 0, cnames[which], physaddr, 
		    maskedaddr, actualaddr);
		CSError++;
	}
    }
    msg_printf(DBG, "check_tag exits\n");
    return(0);

}
