/*
 * scache.c
 *
 *
 * Copyright 1993, Silicon Graphics, Inc.
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

#ident "$Revision: 1.5 $"

#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include <setjmp.h>
#include <uif.h>
#include <libsc.h>
#include <libsk.h>
#include <IP32_status.h>
#include "cache.h"

static jmp_buf fault_buf;

uint *K0_BASE = (uint *) PHYS_TO_K0 (PHYS_CHECK_LO);
uint *K1_BASE = (uint *) PHYS_TO_K1 (PHYS_CHECK_LO);
#define SC_ALL_TAGS	0
#define SC_TAG_NONZERO	1


uint	save_tags1[0x8000];
uint	save_tags2[0x8000];
uint	save_tags3[0x8000];
uint	save_tags4[0x8000];
code_t	scache_ecode;

int scachedata(void);
int scacheaddr(void);
int scachepageinval(void);
int scacherandomline(void);
int scache2MBaddr(void);
void sc_fill_mem_uint(uint *, uint, uint);
void sc_fill_mem_addr(uint *, uint *, uint);
int sc_chk_mem_addr(uint *, uint *, uint, char *);
int sc_chk_mem_uint(uint *, uint, uint, char *);
void scache_close(void);
int scache_init(void);
void sc_flush_dcache(uint *);
/* enable the scache - set scache enable in CONFIG Register. */

void r5000_enable_scache(void)
{
	int config_reg = 0;

	config_reg = Read_C0_Config();
	msg_printf(VDBG, "r5000_enable_scache read (0x%x)\n", config_reg);
	config_reg |= CONFIG_TR_SE;
	Write_C0_Config( config_reg );
	msg_printf(VDBG, "r5000_enable_scache read2 (0x%x)\n", Read_C0_Config());
}

/* disable the scache - clear scache enable in CONFIG Register. */

void r5000_disable_scache(void)
{
	int config_reg = 0;

	config_reg = Read_C0_Config();
	config_reg &= ~CONFIG_TR_SE;
	msg_printf(VDBG, "r5000_disable_scache writing (0x%x)\n", config_reg);
	Write_C0_Config( config_reg );
	msg_printf(VDBG, "r5000_disable_scache readback (0x%x)\n", Read_C0_Config());
}

scache_save_tags(uint *sc_save_addr)
{
	register uint *fillptr, i;

	fillptr = K0_BASE;
	for ( i=0; i < (SID_SIZE/SIDL_SIZE); i++, fillptr+=(SIDL_SIZE/sizeof(int)) ) {
		_read_tag(CACH_SD,(ulong)fillptr,sc_save_addr++);
	}
}

scache_cmp_tags(uint *sc_save_addr1, uint *sc_save_addr2)
{
	register uint i;
	register uint found_diff;

	found_diff = 0;
	for ( i=0; i < (SID_SIZE/SIDL_SIZE); i++ ) {
		if ( *sc_save_addr1++ != *sc_save_addr2++ ) {
			found_diff++;
		}
	}
	return(found_diff);
}

scache_pr_cmp_tags(uint *sc_save_addr1, uint *sc_save_addr2)
{
	register uint *fillptr, i;

	fillptr = K0_BASE;
	for ( i=0; i < (SID_SIZE/SIDL_SIZE); i++, fillptr+=(SIDL_SIZE/sizeof(int)) ) {
		if ( *sc_save_addr1 != *sc_save_addr2 ) {
			msg_printf(VDBG, "Tags for 0x%x differ, 0x%x != 0x%x\n", fillptr,
					*sc_save_addr1, *sc_save_addr2);
		}
		sc_save_addr1++;
		sc_save_addr2++;
	}
}

#if 0
scache_is_invalid(uint *sc_addr)
{
	uint tags;

	_read_tag(CACH_SD,(ulong)sc_addr,&tags);
	if ( (tags & XXXXXX) == 0 ) {
		return(1);
	}
	return(0);
}
#endif /* 0 */

scache_print_tags(uint sc_prtype)
{
	register uint *fillptr, i;
	uint tags, one_nonzero;

	fillptr = K0_BASE;
	switch ( sc_prtype ) {
	case SC_ALL_TAGS:
		for ( i=0; i < (SID_SIZE/SIDL_SIZE); i++, fillptr+=(SIDL_SIZE/sizeof(int)) ) {
			_read_tag(CACH_SD,(ulong)fillptr,&tags);
			msg_printf(VDBG, "Read ALL TAGS for 0x%x==0x%x\n", fillptr, tags);
		}
		break;
	case SC_TAG_NONZERO:
		one_nonzero = 0;
		for ( i=0; i < (SID_SIZE/SIDL_SIZE); i++, fillptr+=(SIDL_SIZE/sizeof(int)) ) {
			_read_tag(CACH_SD,(ulong)fillptr,&tags);
			if ( tags != 0 ) {
				msg_printf(VDBG, "Read TAG NON-ZERO for 0x%x==0x%x\n",
							fillptr, tags);
				one_nonzero++;
			}
		}
		if ( !one_nonzero ) {
			msg_printf(VDBG, "All TAGS are ZERO\n");
		}
		break;
	default:
		msg_printf(ERR, "Illegal scache_print_tags TYPE (0x%x)\n",
							sc_prtype);

	}
}
int scache_init()
{
	extern int _sidcache_size;
	extern u_int _r5000_sidcache_size;
	int tmp, config_reg = 0;
	int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

	if (!r5000) {
		cache_K0();
		return(0);
	}
	
	r5000_enable_scache();
	tmp = config_cache();
	tmp = size_2nd_cache();
	msg_printf(VDBG,"size 2nd cache returned %x\n",tmp);

	msg_printf(VDBG, "_r5000_sidcache_size==0x%x,_sidcache_size==0x%x\n",
				_r5000_sidcache_size, _sidcache_size);
	msg_printf(VDBG, "i_ls=0x%x,d_ls==0x%x,s_ls=0x%x\n", _icache_linesize,
				_dcache_linesize, _scache_linesize);
	if(! _sidcache_size) {
		msg_printf(VRB, "*** No Secondary Cache detected, test skipped\n");
		return(1);
	}
	if ( _r5000_sidcache_size != _sidcache_size ) {
		msg_printf(ERR, "Config reg vs. Probed Scache Size Error\n");
		msg_printf(ERR, "Probed Size==0x%x,Config Reg Size==0x%x\n",
				_r5000_sidcache_size, _sidcache_size);
		config_reg = Read_C0_Config();
		msg_printf(ERR, "CONFIG Register==0x%x\n", config_reg);
		return(1);
	}
	/* print warning if more than 2MB */
	if ( _sidcache_size > MAXCACHE ) {
		msg_printf(INFO, "Secondary Cache Greater than MAXCACHE\n");
	}
	msg_printf(VDBG, "Enabling S-Cache (bit 12 Config Register)\n");
	uncache_K0();
	r5000_enable_scache();
	config_reg = Read_C0_Config();
	msg_printf(VDBG, "New CONFIG Register==0x%x\n", config_reg);
	msg_printf(VDBG,"Invalidate Caches\n");
	invalidate_caches();
	scache_print_tags(SC_TAG_NONZERO);

	run_uncached();
	cache_K0();
	return(0);
}

void scache_close(void)
{
	int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

	if (!r5000) {
		invalidate_caches();
		cache_K0();
		return;
	}
	uncache_K0();
	invalidate_caches();
	r5000_disable_scache();
	run_cached();
	cache_K0();
}

/*
 * scache - secondary cache SRAM test
 */
u_int
scache_ad()
{

	int error = 0;
	int config_reg = 0;

        scache_ecode.sw = 0x08;
        scache_ecode.func = 0x03;
        scache_ecode.test = 0x02;
        scache_ecode.code = 0;
        scache_ecode.w1 = 0;
        scache_ecode.w2 = 0;

	if ( scache_init() ) {
		return(0);
	}
	config_reg = Read_C0_Config();
	msg_printf(VDBG, "CONFIG Register==0x%x\n", config_reg);
	msg_printf(VDBG,"Invalidate Caches\n");
	invalidate_caches();
	scache_print_tags(SC_TAG_NONZERO);
	msg_printf(VRB, "Secondary cache data and address tests\n");

	error += (scachedata() || scacheaddr());

	scache_close();

	if (error) {
		sum_error("secondary cache data or address");
	} else {
		okydoky();
	}
	return error;
}


/*
 * scache_addr - secondary cache address test
 */
u_int
scache_addr()
{
	int error = 0;
	int config_reg = 0;

	scache_ecode.sw = 0x08;
	scache_ecode.func = 0x03;
	scache_ecode.test = 0x01;
	scache_ecode.code = 0;
	scache_ecode.w1 = 0;
	scache_ecode.w2 = 0;

	if ( scache_init() ) {
		return(0);
	}
	config_reg = Read_C0_Config();
	msg_printf(VDBG, "CONFIG Register==0x%x\n", config_reg);

	msg_printf(VRB, "Secondary cache address tests\n");
	error += (scacheaddr());

	scache_close();

	if (error) {
		sum_error("secondary cache data or address");
	} else {
		okydoky();
	}
	return error;
}

/*
 * scache_page_inval - secondary cache address test
 */
u_int
scache_page_inval()
{

	int error = 0;
	int config_reg = 0;
	int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

        scache_ecode.sw = 0x08;
        scache_ecode.func = 0x03;
        scache_ecode.test = 0x03;
        scache_ecode.code = 0;
        scache_ecode.w1 = 0;
        scache_ecode.w2 = 0;

	if ( scache_init() ) {
		return(0);
	}
	config_reg = Read_C0_Config();
	msg_printf(VDBG, "CONFIG Register==0x%x\n", config_reg);

	msg_printf(VRB, "Secondary cache page invalidate test\n");
	if (r5000)
		error += (scachepageinval());
	else
		msg_printf(VRB, "skipping  - N/A for R10000/R12000\n");
	scache_close();

	if (error) {
		sum_error("secondary cache data or address");
	} else {
		okydoky();
	}
	return error;
}


/*
 * scache_random_line - secondary cache random line miss test
 */

u_int
scache_random_line()
{

	int error = 0;
	int config_reg = 0;

        scache_ecode.sw = 0x08;
        scache_ecode.func = 0x03;
        scache_ecode.test = 0x04;
        scache_ecode.code = 0;
        scache_ecode.w1 = 0;
        scache_ecode.w2 = 0;

	if ( scache_init() ) {
		return(0);
	}
	config_reg = Read_C0_Config();
	msg_printf(VDBG, "CONFIG Register==0x%x\n", config_reg);

	msg_printf(VRB, "Secondary cache random line miss test\n");
	error += (scacherandomline());

	scache_close();

	if (error) {
		sum_error("secondary cache data or address");
	} else {
		okydoky();
	}
	return error;
}


/*
 * scache - secondary cache SRAM test, using all memory
 */
u_int
scache_allmem()
{

	int error = 0;
	int config_reg = 0;
	unsigned int mem_size;
	uint *k0_base = K0_BASE;
	uint *k1_base = K1_BASE;
	extern unsigned int memsize;

        scache_ecode.sw = 0x08;
        scache_ecode.func = 0x03;
        scache_ecode.test = 0x05;
        scache_ecode.code = 0;
        scache_ecode.w1 = 0;
        scache_ecode.w2 = 0;

	if ( scache_init() ) {
		return(0);
	}
	config_reg = Read_C0_Config();
	msg_printf(DBG, "CONFIG Register==0x%x\n", config_reg);
	if (memsize < 0x10000000) /*less than 256Mbytes? */
		mem_size = memsize - K1_TO_PHYS(K1_BASE);
	else
		mem_size = 0x10000000 - K1_TO_PHYS(K1_BASE);
	msg_printf(DBG, "Memory size is 0x%x, using 0x%x (0x%x)\n",
				memsize, mem_size, K1_TO_PHYS(K1_BASE));
	while ( mem_size ) {
		msg_printf(VDBG,"Invalidate Caches\n");
		invalidate_caches();
		scache_print_tags(SC_TAG_NONZERO);
		msg_printf(DBG, "Scache data and address tests (0x%x,0x%x)\n",
						K0_BASE, K1_BASE);

		error += (scachedata() || scacheaddr());
		mem_size -= SID_SIZE;
		K0_BASE += (SID_SIZE/sizeof(int));
		K1_BASE += (SID_SIZE/sizeof(int));

	}
	scache_close();
	K0_BASE = k0_base;;
	K1_BASE = k1_base;;

	if (error) {
		sum_error("secondary cache data or address");
	} else {
		okydoky();
	}
	return error;
}


/*
 * scache_2M_addr - secondary cache address test through 2MB
 *		    address space
 */

u_int
scache_2MB_addr()
{
	int error = 0;
	int config_reg = 0;

        scache_ecode.sw = 0x08;
        scache_ecode.func = 0x03;
        scache_ecode.test = 0x06;
        scache_ecode.code = 0;
        scache_ecode.w1 = 0;
        scache_ecode.w2 = 0;

	if ( scache_init() ) {
		return(0);
	}
	config_reg = Read_C0_Config();
	msg_printf(VDBG, "CONFIG Register==0x%x\n", config_reg);

	msg_printf(VRB, "Secondary cache addr test through 2MB\n");
	error += (scache2MBaddr());

	if ( error == 0 ) {
		scache_close();
	}

	if (error) {
		sum_error("secondary cache address in 2MB");
	} else {
		okydoky();
	}
	return error;
}

#define TWO_MEGABYTE	(0x200000/sizeof(int))

/*
 * scache2MBaddr - secondary address uniqueness test in 2MB
 *
 * does not provide the bit error coverage of icachedata, but by storing
 * address-in-address and ~address-in-address, should find if each
 * cache location is individually addressable
 */
int scache2MBaddr(void)
{
	register uint *fillptr, *cfillptr, i;
	uint *k0_base = K0_BASE;
	uint *k1_base = K1_BASE;
	uint fail = 0, inv = 0, config_reg = 0, config_reg_save = 0;
	uint *Cfillptr, *UCfillptr;

	msg_printf(DBG, "scache2MBaddr - test scache w/ addresses in 2MB\n");
	config_reg = Read_C0_Config();
	config_reg_save = config_reg;
	msg_printf(VDBG, "Saved Config Register==0x%x\n", config_reg_save);
	invalidate_caches();

	/* Fill two megabyte range of memory, secondary and primary */

	fillptr = k0_base;
	msg_printf(VDBG, "Fill Scache/memory w/ addresses (address==0x%x)\n",
							fillptr);
	sc_fill_mem_addr(fillptr,fillptr,TWO_MEGABYTE);

	/* Read back and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address==0x%x)\n",
							fillptr);
	scache_ecode.code = 1;	/* set error code before calling check that reports the error */
	fail += sc_chk_mem_addr(fillptr,fillptr,TWO_MEGABYTE,
		"Scache addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n");

	/* Read memory back w/o Scache and check */

	sc_flush_dcache(k0_base);
	fillptr = k1_base;
	cfillptr = k0_base;	/* cached address was written */
	msg_printf(VDBG, "Read/Check Memory (address==0x%x, cached==0x%x)\n",
							fillptr, cfillptr);
	scache_ecode.code = 2;	/* set error code before calling check that reports the error */
	fail += sc_chk_mem_addr(fillptr,cfillptr,TWO_MEGABYTE,
		"Memory addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n");
	/* again, with address inverted */

	invalidate_caches();

	/* Fill secondary through primary */

	fillptr = k0_base;
	msg_printf(VDBG, "Fill Scache/memory w/ ~addresses (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++) {
		*fillptr++ = ~((uint)K0_TO_PHYS(fillptr));
	}
	msg_printf(VDBG, "Fill (complement) ended at 0x%x\n", fillptr);

	/* Read back and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++, fillptr++) {
		if (*fillptr != ~((uint)K0_TO_PHYS(fillptr))) {
			scache_ecode.code = 3;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache address error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, ~((uint)K0_TO_PHYS(fillptr)));
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	msg_printf(VDBG, "Read and check (complement) ended at 0x%x\n", fillptr);
	/* Read back memory w/o scache and check */
	sc_flush_dcache(k0_base);

	fillptr = k1_base;
	cfillptr = k0_base;	/* cached addres was used as data */
	msg_printf(VDBG, "Read/Check Memory (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++, fillptr++, cfillptr++) {
		if (*fillptr != ~((uint)K0_TO_PHYS(cfillptr))) {
			scache_ecode.code = 4;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory address error (2): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, ~((uint)K0_TO_PHYS(cfillptr)));
			fail++;
			Cfillptr = (uint *)PHYS_TO_K0(K0_TO_PHYS(fillptr));
			UCfillptr = (uint *)PHYS_TO_K1(K0_TO_PHYS(fillptr));
			msg_printf(DBG, "Cached address contents (0x%x)==0x%x\n",
					Cfillptr, *Cfillptr);
			msg_printf(DBG, "Uncached address contents (0x%x)==0x%x\n",
					UCfillptr, *UCfillptr);
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	msg_printf(VDBG, "Memory Read and check (complement) ended at 0x%x\n", fillptr);


	/* Fill two megabyte range, backwards, of memory, secondary and primary */

	fillptr = (k0_base+(TWO_MEGABYTE-1));
	msg_printf(VDBG, "Fill Scache/memory w/ addresses (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++) {
		*fillptr-- = (uint)K0_TO_PHYS(fillptr);
	}
	msg_printf(VDBG, "Fill ended at 0x%x\n", fillptr);


	/* Read back and check */

	fillptr = (k0_base+(TWO_MEGABYTE-1));
	msg_printf(VDBG, "Read/Check Scache (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++, fillptr--) {
		if (*fillptr != (uint)K0_TO_PHYS(fillptr)) {
			scache_ecode.code = 5;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, (uint)K0_TO_PHYS(fillptr));
			fail++;
			if ( *Reportlevel > DBG) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	msg_printf(VDBG, "Read and check ended at 0x%x\n", fillptr);

	/* Read memory back w/o Scache and check */

	__dcache_wb(fillptr,_dcache_size);
	fillptr = k1_base;
	cfillptr = k0_base;	/* cached address was written */
	msg_printf(VDBG, "Read/Check Memory (address==0x%x, cached==0x%x)\n",
							fillptr, cfillptr);
	for (i = 0; i < TWO_MEGABYTE; i++, fillptr++, cfillptr++) {
		if (*fillptr != (uint)K0_TO_PHYS(cfillptr)) {
			scache_ecode.code = 6;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, (uint)K0_TO_PHYS(cfillptr));
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	msg_printf(VDBG, "Memory Read and check ended at 0x%x\n", fillptr);
	/* again, with address inverted */

	invalidate_caches();

	/* Fill secondary through primary */

	fillptr = k0_base;
	msg_printf(VDBG, "Fill Scache/memory w/ ~addresses (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++) {
		*fillptr++ = ~((uint)K0_TO_PHYS(fillptr));
	}
	msg_printf(VDBG, "Fill (complement) ended at 0x%x\n", fillptr);

	/* Read back and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++, fillptr++) {
		if (*fillptr != ~((uint)K0_TO_PHYS(fillptr))) {
			scache_ecode.code = 7;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache address error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, ~((uint)K0_TO_PHYS(fillptr)));
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	msg_printf(VDBG, "Read and check (complement) ended at 0x%x\n", fillptr);
	/* Read back memory w/o scache and check */

	__dcache_wb(fillptr,_dcache_size);
	fillptr = k1_base;
	cfillptr = k0_base;	/* cached addres was used as data */
	msg_printf(VDBG, "Read/Check Memory (address==0x%x)\n",
							fillptr);
	for (i = 0; i < TWO_MEGABYTE; i++, fillptr++, cfillptr++) {
		if (*fillptr != ~((uint)K0_TO_PHYS(fillptr))) {
			scache_ecode.code = 8;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory address error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, ~((uint)K0_TO_PHYS(fillptr)));
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	msg_printf(VDBG, "Memory Read and check (complement) ended at 0x%x\n", fillptr);
	return (fail);
}


/*
 * scachepageinval - scache page invalidate test
 *
 */
int scachepageinval(void)
{
	register uint *fillptr, i, pattern;
	uint *k0_base = K0_BASE;
	uint *k1_base = K1_BASE;
	uint fail = 0, odd = 0, config_reg = 0, config_reg_save;
	uint tag1 = 0, tag2 = 0;
	uint *page_base;
	uint page_limit;
	uint page_size;

	msg_printf(DBG, "scachepageinval - test scache page invalidate operation\n");
	config_reg_save = config_reg = Read_C0_Config();
	msg_printf(VDBG, "Saved Config Register==0x%x\n", config_reg_save);
	pattern = 0x1;
	msg_printf(VDBG, "Invalidate Caches\n");
	scache_save_tags(save_tags1);
	invalidate_caches();
	scache_save_tags(save_tags2);
	scache_print_tags(SC_TAG_NONZERO);
	/* Fill secondary and memory */
	fillptr = k0_base;
	msg_printf(VDBG, "Fill Scache/memory (address 0x%x)\n",
							fillptr);
	sc_fill_mem_uint(fillptr,pattern,(SID_SIZE / sizeof (uint)));
	scache_save_tags(save_tags1);

	/* Read back Scache and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address 0x%x)\n",
							fillptr);
	sc_chk_mem_uint(fillptr,pattern,(SID_SIZE/sizeof(uint)),
		"Scache data error (2): Address 0x%x, Actual 0x%x, Expected 0x%x\n");
	scache_save_tags(save_tags2);
	if ( i == 0 ) {
		msg_printf(VDBG, "Scache check OK (1)\n");
	}

	/* now see that memory look good too */
	fillptr = k1_base;
	msg_printf(VDBG, "Read/Check memory (uncached 0x%x)\n",
						fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
		if (*fillptr != pattern) {
			scache_ecode.code = 2;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory error (3): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, pattern);
			fail++;
			if ( *Reportlevel > DBG) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	if ( i == 0 ) {
		msg_printf(VDBG, "Memory check OK\n");
	}
	/* now change memory w/o using scache */
	fillptr = k1_base;
	msg_printf(VDBG, "Fill memory (uncached 0x%x)\n",
						fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i) {
		*fillptr++ = ~pattern;
	}

	/* Read back and check */

	fillptr = k1_base;
	msg_printf(VDBG, "Read/Check memory (uncached 0x%x)\n",
						fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
		if (*fillptr != ~pattern) {
			scache_ecode.code = 3;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory error (3): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, ~pattern);
			fail++;
			if ( *Reportlevel > DBG) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	if ( i == 0 ) {
		msg_printf(VDBG, "Memory check OK\n");
	}
	/* Read back Scache and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address 0x%x)\n",
							fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
		if (*fillptr != pattern) {
			scache_ecode.code = 4;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache data error (2): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, pattern);
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	if ( i == 0 ) {
		msg_printf(VDBG, "Scache check OK (2)\n");
	}
	page_base = K0_BASE;
	page_limit = SID_SIZE;
	page_size = 0x1000;
	while ( page_limit ) {
		if ( *Reportlevel < DBG ) {
			busy (1);
		}
		msg_printf(VDBG, "Invalidate 0x%x based page\n", page_base);
		scache_save_tags(save_tags2);
		_read_tag(CACH_SD,(ulong)k0_base,&tag1);
		invalidate_sc_page(page_base);
		/* Read back Scache and check */
		_read_tag(CACH_SD,(ulong)k0_base,&tag2);
		scache_save_tags(save_tags3);
		if ( scache_cmp_tags(save_tags2, save_tags3) ) {
			if ( *Reportlevel == VDBG ) {
				msg_printf(VDBG, "Below is after page invalidate\n");
				scache_pr_cmp_tags(save_tags2, save_tags3);
			}
		} else {
			scache_ecode.code = 5;
			scache_ecode.w1 = 0;
			scache_ecode.w2 = 0;
			report_error(&scache_ecode);
			msg_printf(ERR, "TAGS compare before/after page invalidate\n");
		}
		msg_printf(VDBG, "Before page invalidate==0x%x,after==0x%x\n", tag1, tag2);
	
		fillptr = page_base;
		msg_printf(VDBG, "Read/Check Scache (address 0x%x)\n",
								fillptr);
		for (i = (page_size / sizeof (uint)); i > 0; --i, fillptr++) {
			if (*fillptr != ~pattern) {
				scache_ecode.code = 6;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Scache data error (2): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr, *fillptr, ~pattern);
				fail++;
				if ( *Reportlevel > DBG ) {
					if ( fail > 32 ) {
						msg_printf(DBG, "Loop Break\n");
						fail = 1;
						break;
					}
				}
			}
		}
		if ( i == 0 ) {
			msg_printf(DBG, "Scache invalidate page address 0x%x check OK (3)\n",
									page_base);
		}
		page_limit -= page_size;
		page_base += (page_size/sizeof(uint));
	}
	if ( *Reportlevel < DBG ) {
		busy (0);
	}
	return(fail);
	_read_tag(CACH_SD,(ulong)k0_base,&tag1);
	fillptr = k0_base;
	for ( i=0; i<32; i++ ) {
		invalidate_sc_line(fillptr);
		fillptr += SIDL_SIZE/sizeof(uint);
	}
	/* Read back Scache and check */
	_read_tag(CACH_SD,(ulong)k0_base,&tag2);
	scache_save_tags(save_tags3);
	if ( scache_cmp_tags(save_tags2, save_tags3) ) {
		msg_printf(VDBG, "Below is after line invalidate\n");
		scache_pr_cmp_tags(save_tags2, save_tags3);
	} else {
		msg_printf(VDBG, "TAGS compare before/after line invalidates\n");
	}
	scache_pr_cmp_tags(save_tags2,save_tags3);
	msg_printf(VDBG, "Before line invalidate==0x%x,after==0x%x\n", tag1, tag2);
	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address 0x%x)\n",
							fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
		if (*fillptr != pattern) {
			scache_ecode.code = 7;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache data error (2): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, pattern);
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	if ( i == 0 ) {
		msg_printf(VDBG, "Scache check OK (4)\n");
	}
}
/*
 * scachedata - secondary walking bit test
 *
 */
int scachedata(void)
{
	register uint *fillptr, i, pattern;
	uint *k0_base = K0_BASE;
	uint *k1_base = K1_BASE;
	uint fail = 0, odd = 0, config_reg = 0, config_reg_save;
	int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

	msg_printf(DBG, "scachedata - test scache w/ walking ones/zeros\n");
	config_reg_save = config_reg = Read_C0_Config();
	msg_printf(VDBG, "Saved Config Register==0x%x\n", config_reg_save);
	pattern = 0x1;
	while (pattern != 0) {
		if ( *Reportlevel < DBG ) {
			busy (1);
		}
		msg_printf(DBG, "Test scache w/ 0x%x 0x%x for 0x%x\n",
					pattern, k0_base, SID_SIZE);

		msg_printf(DBG, "Invalidate Caches\n");
		scache_save_tags(save_tags1);
		invalidate_caches();
		scache_save_tags(save_tags2);
		scache_print_tags(SC_TAG_NONZERO);
		/* disable scache and write ~pattern into memory, not */
		/* scache.  */
	/*	r5000_disable_scache(); */
		fillptr = k1_base;
		msg_printf(VDBG, "Fill memory (using uncached address 0x%x)\n",
								fillptr);
		scache_print_tags(SC_TAG_NONZERO);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i) {
			*fillptr++ = ~pattern;
		}
		scache_print_tags(SC_TAG_NONZERO);
		scache_save_tags(save_tags3);
		/* Read back memory and and check */

		fillptr = k1_base;
		msg_printf(VDBG, "Read/Check memory (uncached address 0x%x)\n",
								fillptr);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
			if (*fillptr != ~pattern) {
				scache_ecode.code = 1;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Memory error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr, *fillptr, ~pattern);
				fail++;
				if ( *Reportlevel > DBG ) {
					if ( fail > 32 ) {
						msg_printf(DBG, "Loop Break\n");
						fail = 1;
						break;
					}
				}
			}
		}
		scache_save_tags(save_tags4);
		if ( i == 0 ) {
			msg_printf(VDBG, "Memory check OK\n");
		}
		if ( scache_cmp_tags(save_tags1, save_tags2) ) {
			msg_printf(DBG, "Below is before and after invalidate\n");
			scache_pr_cmp_tags(save_tags1, save_tags2);
		}
		if ( scache_cmp_tags(save_tags2, save_tags3) ) {
			msg_printf(DBG, "Below is after invalidate and non-cached write\n");
			scache_pr_cmp_tags(save_tags2, save_tags3);
		} else {
			msg_printf(DBG, "TAGS compare before/after uncached write\n");
		}
		if ( scache_cmp_tags(save_tags3, save_tags4) ) {
			msg_printf(DBG, "Below is after read non-cached\n");
			scache_pr_cmp_tags(save_tags3, save_tags4);
		} else {
			msg_printf(DBG, "TAGS compare before/after uncached read\n");
		}

		
		/* Fill secondary and memory */
		/* this should wipe out the ~pattern in memory */

		/* r5000_enable_scache();	 turn on scache */
		fillptr = k0_base;
		msg_printf(VDBG, "Fill Scache/memory (address 0x%x)\n",
								fillptr);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i) {
			*fillptr++ = pattern;
		}
		scache_save_tags(save_tags1);

		/* Read back Scache and check */

		fillptr = k0_base;
		msg_printf(VDBG, "Read/Check Scache (address 0x%x)\n",
								fillptr);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
			if (*fillptr != pattern) {
				scache_ecode.code = 2;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Scache data error (2): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr, *fillptr, pattern);
				fail++;
				if ( *Reportlevel > DBG ) {
					if ( fail > 32 ) {
						msg_printf(DBG, "Loop Break\n");
						fail = 1;
						break;
					}
				}
			}
		}
		fillptr = k0_base;
		__dcache_wb(fillptr,_sidcache_size);
		scache_save_tags(save_tags2);
		if ( i == 0 ) {
			msg_printf(VDBG, "Scache check OK\n");
		}
		/* disable secondary cache */
		/* r5000_disable_scache(); */

		/* Read memory around secondary cache */
		/* make sure it has what we wrote to secondary and memory */
		/* it should have pattern, not ~pattern */
		fillptr = k1_base;
		msg_printf(VDBG, "Read mem w/o scache - check (address==0x%x)\n",
								fillptr);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
			if (*fillptr != pattern) {
				scache_ecode.code = 3;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Memory error (2): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr, *fillptr, pattern);
				fail++;
				if ( *Reportlevel > DBG) {
					if ( fail > 32 ) {
						msg_printf(DBG, "Loop Break\n");
						fail = 1;
						break;
					}
				}
			}
		}
		scache_save_tags(save_tags3);
		if ( i == 0 ) {
			msg_printf(VDBG, "Memory check OK\n");
		}
		if ( scache_cmp_tags(save_tags2, save_tags3) ) {
			msg_printf(DBG, "Below is before/after noncached read\n");
			scache_pr_cmp_tags(save_tags2, save_tags3);
		} else {
			msg_printf(DBG, "TAGS compare before/after uncached read\n");
		}
		/* now change memory w/o using scache */
		fillptr = k1_base;
		msg_printf(VDBG, "Fill memory (uncached 0x%x)\n",
							fillptr);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i) {
			*fillptr++ = ~pattern;
		}
		scache_save_tags(save_tags3);
		if ( scache_cmp_tags(save_tags2, save_tags3) ) {
			msg_printf(DBG, "Below is before/after noncached write\n");
			scache_pr_cmp_tags(save_tags2, save_tags3);
		} else {
			msg_printf(DBG, "TAGS compare before/after uncached write\n");
		}

		/* Read back and check */

		fillptr = k1_base;
		msg_printf(VDBG, "Read/Check memory (uncached 0x%x)\n",
							fillptr);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
			if (*fillptr != ~pattern) {
				scache_ecode.code = 4;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Memory error (3): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr, *fillptr, ~pattern);
				fail++;
				if ( *Reportlevel > DBG) {
					if ( fail > 32 ) {
						msg_printf(DBG, "Loop Break\n");
						fail = 1;
						break;
					}
				}
			}
		}
		scache_save_tags(save_tags3);
		if ( i == 0 ) {
			msg_printf(VDBG, "Memory check OK\n");
		}
		if ( scache_cmp_tags(save_tags2, save_tags3) ) {
			msg_printf(DBG, "Below is before/after noncached read\n");
			scache_pr_cmp_tags(save_tags2, save_tags3);
		} else {
			msg_printf(DBG, "TAGS compare before/after uncached read\n");
		}
		/* restore secondary cache enable bit */
		/* depending on orignal state, either enable or *
		/* maintain disabled secondary cache */
		/* Write_C0_Config(config_reg_save); */
		msg_printf(VDBG, "Config register read (0x%x)\n",
							Read_C0_Config());

		if (r5000) {
		/* now read the Scache and it should still be pattern */
		fillptr = k0_base;
		msg_printf(VDBG, "Read scache/check (address==0x%x)\n",
								fillptr);
		for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
			if (*fillptr != pattern) {
				scache_ecode.code = 5;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Scache data error (3): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr, *fillptr, pattern);
				fail++;
				if ( *Reportlevel > DBG) {
					if ( fail > 32 ) {
						msg_printf(DBG, "Loop Break\n");
						fail = 1;
						break;
					}
				}
			}
		}
		if ( i == 0 ) {
			msg_printf(VDBG, "Scache check OK\n");
		}
		}

		pattern = ~pattern;		/* invert */
		odd = !odd;
		if (!odd) {			/* every other time... */
			pattern <<= 1;		/* shift bit left */
		}
	}
	if ( *Reportlevel < DBG ) {
		busy (0);
	}
	return (fail);
}

/*
 * scacheaddr - secondary address uniqueness test
 *
 * does not provide the bit error coverage of icachedata, but by storing
 * address-in-address and ~address-in-address, should find if each
 * cache location is individually addressable
 */
int scacheaddr(void)
{
	register uint *fillptr, *cfillptr, i;
	uint *k0_base = K0_BASE;
	uint *k1_base = K1_BASE;
	uint fail = 0, inv = 0, config_reg = 0, config_reg_save = 0;

	msg_printf(DBG, "scacheaddr - test scache w/ addresses\n");
	config_reg = Read_C0_Config();
	config_reg_save = config_reg;
	msg_printf(VDBG, "Saved Config Register==0x%x\n", config_reg_save);
	invalidate_caches();

	/* Fill secondary through primary */

	fillptr = k0_base;
	msg_printf(VDBG, "Fill Scache/memory w/ addresses (address==0x%x)\n",
							fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i) {
		*fillptr++ = (uint) fillptr;
	}

	/* Read back and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address==0x%x)\n",
							fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
		if (*fillptr != (uint) fillptr) {
			scache_ecode.code = 1;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, (uint) fillptr);
			fail++;
			if ( *Reportlevel > DBG) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}

	/* Read memory back w/o Scache and check */

	fillptr = k0_base;
	__dcache_wb(fillptr,_sidcache_size);
	fillptr = k1_base;
	cfillptr = k0_base;	/* cached address was written */
	msg_printf(VDBG, "Read/Check Memory (address==0x%x, cached==0x%x)\n",
							fillptr, cfillptr);
	for (i = SID_SIZE/sizeof (uint); i > 0; --i, fillptr++, cfillptr++) {
		if (*fillptr != (uint) cfillptr) {
			scache_ecode.code = 2;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, (uint) cfillptr);
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	/* again, with address inverted */

	invalidate_caches();

	/* Fill secondary through primary */

	fillptr = k0_base;
	msg_printf(VDBG, "Fill Scache/memory w/ ~addresses (address==0x%x)\n",
							fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i) {
		*fillptr++ = ~(uint)fillptr;
	}

	/* Read back and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address==0x%x)\n",
							fillptr);
	for (i = SID_SIZE / sizeof (uint); i > 0; --i, fillptr++) {
		if (*fillptr != ~(uint)fillptr) {
			scache_ecode.code = 3;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache address error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, ~(uint)fillptr);
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	/* Read back memory w/o scache and check */

	fillptr = k0_base;
	__dcache_wb(fillptr,_sidcache_size);
	fillptr = k1_base;
	cfillptr = k0_base;	/* cached addres was used as data */
	msg_printf(VDBG, "Read/Check Memory (address==0x%x)\n",
							fillptr);
	for (i = SID_SIZE/sizeof (uint); i > 0; --i, fillptr++, cfillptr++) {
		if (*fillptr != ~(uint)cfillptr) {
			scache_ecode.code = 4;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory address error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, ~(uint)cfillptr);
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}


	return (fail);
}

#define PATTERN1	0xaa
scacherandomline()
{
	register unsigned char *fillptr, *fillptr1, pattern, pattern1;
	register uint i, j;
	char *k0_base = (char *)K0_BASE;
	char *k1_base = (char *)K1_BASE;
	uint fail = 0, inv = 0, config_reg = 0, config_reg_save = 0;
	uint tag1, tag2;
	int r5000 = ((get_prid()&C0_IMPMASK)>>C0_IMPSHIFT) == C0_IMP_R5000;

	msg_printf(DBG, "scacheaddr - test scache w/ addresses\n");
	config_reg = Read_C0_Config();
	config_reg_save = config_reg;
	msg_printf(VDBG, "Saved Config Register==0x%x\n", config_reg_save);
	invalidate_caches();

	/* Fill secondary through primary */

	fillptr = k0_base;
	msg_printf(VDBG, "Fill Scache/memory w/ addresses (address==0x%x)\n",
							fillptr);
	pattern = PATTERN1;
	for (i = SID_SIZE; i > 0; --i) {
		*fillptr++ = pattern;
	}
	fillptr = k0_base;
	__dcache_wb(fillptr,_sidcache_size);

	/* Read back and check */

	fillptr = k0_base;
	msg_printf(VDBG, "Read/Check Scache (address==0x%x)\n",
							fillptr);
	for (i = SID_SIZE; i > 0; --i, fillptr++) {
		if (*fillptr != pattern) {
			scache_ecode.code = 1;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, pattern);
			fail++;
			if ( *Reportlevel > DBG) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}

	/* Read memory back w/o Scache and check */

	msg_printf(VDBG, "Flush All caches/Invalidate primary data cache\n");
	__dcache_wb(fillptr,_dcache_size);
	invalidate_dcache(_dcache_size, _dcache_linesize);
	fillptr = k1_base;
	msg_printf(VDBG, "Read/Check Memory (address==0x%x)\n",
							fillptr);
	for (i = SID_SIZE; i > 0; --i, fillptr++) {
		if (*fillptr != pattern) {
			scache_ecode.code = 2;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory addr error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, pattern);
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					return(fail);
					break;
				}
			}
		}
	}
	/* now change memory w/o using scache */
	fillptr = k1_base;
	msg_printf(VDBG, "Fill memory (uncached 0x%x)\n",
						fillptr);
	pattern = 0;
	for (i = SID_SIZE; i > 0; --i, pattern++) {
		*fillptr++ = (pattern % SIDL_SIZE);
	}
	msg_printf(VDBG, "memory filled OK (uncached 0x%x)\n",
						fillptr);

	/* Read back and check */

	fillptr = k1_base;
	msg_printf(VDBG, "Read/Check memory (uncached 0x%x)\n",
						fillptr);
	pattern = 0;
	for (i = SID_SIZE; i > 0; --i, fillptr++, pattern++) {
		if (*fillptr != (pattern % SIDL_SIZE)) {
			scache_ecode.code = 3;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Memory error (3): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr, *fillptr, (pattern % SIDL_SIZE));
			fail++;
			if ( *Reportlevel > DBG) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	if ( i == 0 ) {
		msg_printf(VDBG, "Memory check OK\n");
	}
	/* now go through a line at a time, check that scache is unchanged, 
	 * invalidate the line, read from random location and validate data
	 * came from memory, then check all SIDL_SIZE bytes to ensure 
	 * correct ordering
	 */
	fillptr = k0_base;
	for (i = (SID_SIZE/SIDL_SIZE); i > 0; --i, fillptr+= SIDL_SIZE) {
		if (!r5000)
			if (i == (SID_SIZE/SIDL_SIZE))
				i = i/2;		/* half size for two set scache */
		/* for each line, check that scache is unchanged */
		if ( *Reportlevel < DBG ) {
			busy (1);
		}
		pattern = PATTERN1;
		msg_printf(DBG, "Check that Scache is unchanged (0x%x)\n", fillptr);
		for ( j=0, fillptr1=fillptr; j < SIDL_SIZE; j++, fillptr1++ ) {
			if (*fillptr1 != pattern) {
				scache_ecode.code = 4;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Scache error (A): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr1, *fillptr1, pattern);
				fail++;
				if ( *Reportlevel > DBG) {
					if ( fail > 32 ) {
						msg_printf(DBG, "Loop Break\n");
						fail = 1;
						break;
					}
				}
			}
		}
		/* invalidate the line just checked */
		msg_printf(DBG, "Invalidate line %d (0x%x) and primary dcache\n", 
					(((SID_SIZE/SIDL_SIZE)+1)-i), fillptr);
		_read_tag(CACH_SD,(ulong)fillptr,&tag1);
		invalidate_sc_line((uint *)fillptr);
		invalidate_dcache(_dcache_size, _dcache_linesize);
		_read_tag(CACH_SD,(ulong)fillptr,&tag2);
		msg_printf(DBG, "TAG1==0x%x,TAG2==0x%x\n", tag1, tag2);

		/* read from random location in the scache line */
		pattern = (i & 0x1f);
		fillptr1 = fillptr+pattern;
		if ( (pattern1 = *fillptr1) != pattern ) {
			scache_ecode.code = 5;
			scache_ecode.w1 =(uint) fillptr;
			scache_ecode.w2 = *fillptr;
			report_error(&scache_ecode);
			msg_printf (ERR, "Scache random error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
				fillptr1, pattern1, pattern);
			fail++;
			if ( *Reportlevel > DBG) {
				if ( fail > 32 ) {
					msg_printf(DBG, "Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
		msg_printf(DBG, "Random at 0x%x, got 0x%x\n", fillptr1, pattern1);

		/* now validate all of line is fetched from memory correctly */
		/* each byte should contain its' offset into the line */
		msg_printf(DBG, "Now check all bytes of line\n");
		for ( j=0, fillptr1=fillptr; j < SIDL_SIZE; j++, fillptr1++ ) {
			if (*fillptr1 != j) {
				scache_ecode.code = 6;
				scache_ecode.w1 =(uint) fillptr;
				scache_ecode.w2 = *fillptr;
				report_error(&scache_ecode);
				msg_printf (ERR, "Scache byte error (1): Address 0x%x, Actual 0x%x, Expected 0x%x\n",
					fillptr1, *fillptr1, j);
				fail++;
			}
		}
		if ( j == SIDL_SIZE ) {
			msg_printf(DBG, "All byte OK in scache\n");
		}
		if ( fail ) {
			fail = 1;
		}
	}
	if ( *Reportlevel < DBG ) {
		busy (0);
	}
	return(fail);
}

/*
 * fill fillsize uint's with filldata starting at filladdr.
 */

void sc_fill_mem_uint(uint *filladdr, uint filldata, uint fillsize)
{
	int i;

	for ( i=0; i<fillsize; i++ ) {
		*filladdr++ = filldata;
	}
}

/*
 * Check chksize uint's for equality with chkdata, starting at chkaddr
 */

int sc_chk_mem_uint(uint *chkaddr, uint chkdata, uint chksize, char *chkmsg)
{
	int i, fail = 0;

	for ( i=0; i<chksize; i++, chkaddr++ ) {
		if (*chkaddr != chkdata) {
			scache_ecode.code = 1;
			scache_ecode.w1 =(uint) chkaddr;
			scache_ecode.w2 = *chkaddr;
			report_error(&scache_ecode);
			msg_printf (ERR, chkmsg, chkaddr, *chkaddr, chkdata);
			fail++;
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG,
						"sc_chk_mem_uint Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	return(fail);
}

/*
 * fill, from filladdr, fillsize uint's with addresses starting at fillstart.
 */

void sc_fill_mem_addr(uint *filladdr, uint *fillstart, uint fillsize)
{
	int i;

	for ( i=0; i<fillsize; i++, fillstart++ ) {
		*filladdr++ = (uint)K0_TO_PHYS(fillstart);
	}
}

/*
 * Check, from chkaddr, chksize uint's for address match begining at chkstart
 */

int sc_chk_mem_addr(uint *chkaddr, uint *chkstart, uint chksize, char *chkmsg)
{
	int i, fail = 0;
	uint *cchkaddr;	/* cached address */
	uint *ucchkaddr;	/* uncached address */

	for ( i=0; i<chksize; i++, chkaddr++, chkstart++ ) {
		if (*chkaddr != (uint)K0_TO_PHYS(chkstart)) {
			scache_ecode.w1 =(uint) chkaddr;
			scache_ecode.w2 = *chkaddr;
			report_error(&scache_ecode);
			msg_printf (ERR, chkmsg, chkaddr, *chkaddr,
							K0_TO_PHYS(chkstart));
			fail++;
			cchkaddr = (uint *)PHYS_TO_K0(K0_TO_PHYS(chkaddr));
			ucchkaddr = (uint *)PHYS_TO_K1(K0_TO_PHYS(chkaddr));
			msg_printf(DBG, "Cached address contents (0x%x)==0x%x\n",
					cchkaddr, *cchkaddr);
			msg_printf(DBG, "Uncached address contents (0x%x)==0x%x\n",
					ucchkaddr, *ucchkaddr);
			if ( *Reportlevel > DBG ) {
				if ( fail > 32 ) {
					msg_printf(DBG,
						"sc_chk_mem_addr Loop Break\n");
					fail = 1;
					break;
				}
			}
		}
	}
	return(fail);
}
void sc_flush_dcache(uint *flushaddr)
{
	int i, hit, first_time = 1;
	uint *cflushaddr, *ucflushaddr;
	uint *save_flushaddr;
	uint tag1, tag2;

	/* flush primary data cache */
	save_flushaddr = flushaddr;
	msg_printf(DBG, "sc_flush_dcache 0x%x, 0x%x\n", flushaddr,
					(PD_SIZE / sizeof (uint)));
	for (i = PD_SIZE / PDL_SIZE; i > 0; --i) {
		cflushaddr = (uint *)PHYS_TO_K0(K0_TO_PHYS(flushaddr));
		ucflushaddr = (uint *)PHYS_TO_K1(K0_TO_PHYS(flushaddr));
		_read_tag(CACH_PD,(ulong)flushaddr,&tag1);
		msg_printf(VDBG,"Before flush: 0x%x==0x%x,0x%x==0x%x (0x%x)\n",
			cflushaddr, *cflushaddr, ucflushaddr, *ucflushaddr, tag1);
		hit = pd_HWBINV (flushaddr);
		_read_tag(CACH_PD,(ulong)flushaddr,&tag2);
		msg_printf(VDBG,"After  flush: cached data=0x%x,uncached data==0x%x (0x%x)\n",
				*cflushaddr, *ucflushaddr, tag2);
		if ( hit && first_time ) {
			msg_printf(DBG, "sc_flush_dcache hit at 0x%x (0x%x)\n",
						flushaddr, hit);
			first_time = 0;
		}
		flushaddr += (PDL_SIZE/sizeof(uint));
	}
	msg_printf(DBG,"sc_flush_dcache ended at 0x%x\n", flushaddr);
	invalidate_dcache(_dcache_size, _dcache_linesize);
	flushaddr = save_flushaddr;
	for (i = PD_SIZE / PDL_SIZE; i > 0; --i) {
		cflushaddr = (uint *)PHYS_TO_K0(K0_TO_PHYS(flushaddr));
		ucflushaddr = (uint *)PHYS_TO_K1(K0_TO_PHYS(flushaddr));
		_read_tag(CACH_PD,(ulong)flushaddr,&tag1);
		msg_printf(VDBG,"After Invalidate: 0x%x==0x%x,0x%x==0x%x (0x%x)\n",
			cflushaddr, *cflushaddr, ucflushaddr, *ucflushaddr, tag1);
		if ( *cflushaddr != *ucflushaddr ) {
			save_flushaddr = (uint *)PHYS_TO_K0(*cflushaddr);
			_read_tag(CACH_PD,(ulong)save_flushaddr,&tag1);
			msg_printf(VDBG,"Before flush: 0x%x==0x%x,0x%x==0x%x (0x%x)\n",
				cflushaddr, *cflushaddr, ucflushaddr, *ucflushaddr, tag1);
			hit = pd_HWBINV (save_flushaddr);
			_read_tag(CACH_PD,(ulong)save_flushaddr,&tag2);
			msg_printf(VDBG,"After  flush: cached data=0x%x,uncached data==0x%x (0x%x)\n",
					*cflushaddr, *ucflushaddr, tag2);
		}
		flushaddr += (PDL_SIZE/sizeof(uint));
	}
	msg_printf(DBG,"sc_flush_dcache ended again at 0x%x\n", flushaddr);
}
