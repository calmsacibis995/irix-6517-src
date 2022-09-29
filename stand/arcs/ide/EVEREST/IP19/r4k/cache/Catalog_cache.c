/*
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


/***********************************************************************\
*	File:		Catalog_cache.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <err_hints.h>
#include <everr_hints.h>
#include <ide_msg.h>
#include "prototypes.h"

#ident	"arcs/ide/EVEREST/IP19/r4k/cache/Catalog_cache.c $Revision: 1.7 $"

extern void cache_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t cache_hints [] = {
	TAGHI_ERR1, "Taghitst()",
	  "Taghi register failed walking one test\n", 0,

	TAGHI_ERR2, "Taghitst()", 
	  "Taghi register failed walking zero test\n", 0,

	TAGLO_ERR1, "Taglotst()", 
	  "Taglo register failed walking one test\n", 0,

	TAGLO_ERR2, "Taglotst()", 
	  "Taglo register failed walking zero test\n", 0,

	DTAGWLK_ERR1, "pdtagwlk()",
	  "D-cache tag ram data line error\n", 0,

	DTAGADR_ERR1, "pdtagaddr()",
	  "D-cache tag ram address line error\n", 0,

	DTAGKH_ERR1, "PdTagKh()",
	  "Partition 1 error after partition 0 set to 0xaaaaaaaa\n", 0,

	DTAGKH_ERR2, "PdTagKh()",
	  "Partition 2 error after partition 1 set to 0xaaaaaaaa\n", 0,

	DTAGKH_ERR3, "PdTagKh()",
	  "Partition 0 error after partition 1 set to 0xaaaaaaaa\n", 0, 

	DTAGKH_ERR4, "PdTagKh()",
	  "Partition 1 error after partition 1 set to 0xaaaaaaaa\n", 0,

	DTAGKH_ERR5, "PdTagKh()",
	  "Partition 0 error after partition 0 set to 0x55555555\n", 0,

	DTAGKH_ERR6, "PdTagKh()",
	  "Partition 2 error after partition 2 set to 0xaaaaaaaa\n", 0,

	ITAGWLK_ERR1, "pitagwlk()",
	  "I-cache tag ram data line error\n", 0,
	
	ITAGADR_ERR1, "pitagaddr()",
	  "I-cache tag ram address line error\n", 0,

	ITAGKH_ERR1, "PiTagKh()",
	  "Partition 1 error after partition 0 set to 0xaaaaaaaa\n", 0,

	ITAGKH_ERR2, "PiTagKh()",
	  "Partition 2 error after partition 1 set to 0xaaaaaaaa\n", 0,

	ITAGKH_ERR3, "PiTagKh()",
	  "Partition 0 error after partition 1 set to 0xaaaaaaaa\n", 0, 

	ITAGKH_ERR4, "PiTagKh()",
	  "Partition 1 error after partition 1 set to 0xaaaaaaaa\n", 0,

	ITAGKH_ERR5, "PiTagKh()",
	  "Partition 0 error after partition 0 set to 0x55555555\n", 0,

	ITAGKH_ERR6, "PiTagKh()",
	  "Partition 2 error after partition 2 set to 0xaaaaaaaa\n", 0,

	SDTAGWLK_ERR1, "sd_tagwlk()",
	  "Secondary Data TAG RAM Path Error\n", 0,
	
	SDTAGADR_ERR1, "sd_tagaddr()",
	  "Secondary Data TAG Address Error\n", 0, 

	SDTAGKH_ERR1, "sd_tagkh()",
	  "Secondary Data TAG ram data Error\n", 0,

	DTAGPAR_ERR1, "d_tagparity()",
	  "D-cache tag ram parity bit error.\n", 0,

	DTAGCMP_ERR1, "d_tagcmp()",
	  "D-cache tag comparator did not detect a miss\n", 0,

	DTAGCMP_ERR2, "d_tagcmp()",
	  "D-cache tag comparator did not detecta hit\n", 0,

	DTAGFUNCT_ERR1, "d_tagfunct()",
	  "D-cache tag functional error in PTAG field \n", 0,

	DTAGFUNCT_ERR2, "d_tagfunct()",
	  "D-cache tag functional cache state error\n", 0,
	  
	DSLIDEDATA_ERR1, "d_slide_data()",
	  "D-cache data ram data lines failed walking one test\n", 0, 

	DSLIDEDATA_ERR2, "d_slide_data()",
	  "D-cache data ram data lines failed walking zero test\n", 0, 

	DSLIDEADR_ERR1, "d_slide_addr()",
	  "D-cache data ram address lines failed walking one test\n", 0,
	
	DSLIDEADR_ERR2, "d_slide_addr()",
	  "D-cache data ram address lines failed walking zero test\n", 0,

	DKH_ERR1, "d_kh()",
	  "Partition 1 error after partition 0 set to 0xaaaaaaaa\n", 0,

	DKH_ERR2, "d_kh()",
	  "Partition 2 error after partition 1 set to 0xaaaaaaaa\n", 0,

	DKH_ERR3, "d_kh()",
	  "Partition 0 error after partition 1 set to 0xaaaaaaaa\n", 0, 

	DKH_ERR4, "d_kh()",
	  "Partition 1 error after partition 1 set to 0xaaaaaaaa\n", 0,

	DKH_ERR5, "d_Kh()",
	  "Partition 0 error after partition 0 set to 0x55555555\n", 0,

	DKH_ERR6, "d_kh()",
	  "Partition 2 error after partition 2 set to 0xaaaaaaaa\n", 0,

	DSDWLK_ERR1, "dsd_wlk()",
	  "Data Path Error from Memory->Secondary->Primary Data\n", 0,

	DSDWLK_ERR2, "dsd_wlk()",
	  "Data Path Error from Primary ->Secondary->Memory Data\n", 0,

	SDAINA_ERR1, "sd_aina()",
	  "Secondary Memory Error on pattern 1\n", 0, 

	SDAINA_ERR2, "sd_aina()",
	  "Secondary Memory Error on pattern 2\n", 0, 

	DFUNCT_ERR1, "d_function()",
	  "D-cache block fill error 1\n", 0,

	DFUNCT_ERR2, "d_function()",
	  "D-cache block fill error 2\n", 0,

	DFUNCT_ERR3, "d_function()",
	  "D-cache block write back error 1\n", 0,

	DFUNCT_ERR4, "d_function()",
	  "D-cache block fill error 3\n", 0,

	DFUNCT_ERR5, "d_function()",
	  "D-cache block write back error 2\n", 0, 

	DPAR_ERR1, "d_parity()",
	  "D-cache parity generation error\n", 0, 

	ITAGPAR_ERR1, "i_tagparity()",
	  "I-cache tag ram parity bit error\n", 0,

	ITAGCMP_ERR1, "i_tagcmp()",
	  "I-cache tag comparator did not detect a miss (walking l)\n", 0,

	ITAGCMP_ERR2, "i_tagcmp()",
	  "I-cache tag comparator did not detect a hit (walking 1)\n", 0,

	ITAGCMP_ERR3, "i_tagcmp()",
	  "I-cache tag comparator did not detect a miss (walking zero)\n", 0,

	ITAGCMP_ERR4, "i_tagcmp()",
	  "I-cache tag comparator did not detect a hit (walking zero)\n", 0,

	ITAGFUNC_ERR1, "i_tagfunct()",
	  "I-cache tag functional error in PTAG field\n", 0,

	ITAGFUNC_ERR2, "i_tagfunct()",
	  "I-cache tag functional cache state error \n", 0,

	ISLIDED_ERR1, "i_slide_data()",
	  "I-cache data ram data lines failed walking one test\n", 0, 

	ISLIDED_ERR2, "i_slide_data()",
	  "I-cache data ram data lines failed walking zero test\n", 0, 

	IAINA_ERR1, "i_aina()",
	  "I-cache address in address error\n", 0,
	
	IFUNCT_ERR1, "i_function()",
	  "I-cache block write back error.\n", 0, 

	IPAR_ERR1, "i_parity()",
	  "I-cache parity generation error\n", 0, 

	IHITINV_ERR1, "i_hitinv()",
	  "I-cache state error during initialization\n", 0,

	IHITINV_ERR2, "i_hitinv()",
	  "I-cache state error\n", 0, 

	IHITINV_ERR3, "i_hitinv()",
	  "I-cache state error on a Hit Invalidate Cache OP\n", 0, 

	IHITWB_ERR1, "i_hitwb()",
	  "I-cache state error during initialization\n", 0,

	IHITWB_ERR2, "i_hitwb()",
	  "I-cache state error Hit writeback happened on a cache miss\n", 0, 

	IHITWB_ERR3, "i_hitwb()",
	  "I-cache Hit writeback did not happen on a cache hit\n", 0,

	ECCREG_ERR1, "ECC_reg_tst()",
	  "ECC register failed walking one test.\n", 0,

	ECCREG_ERR2, "ECC_reg_tst()",
	  "ECC register failed walking zero test\n", 0,

	DHITINV_ERR1, "d_hitinv()",
	  "D-cache state error during initialization\n", 0,

	DHITINV_ERR2, "d_hitinv()",
	  "D-cache state error\n", 0, 

	DHITINV_ERR3, "d_hitinv()",
	  "D-cache state error on a  Hit Invalidate Cache OP\n", 0, 

	DHITWB_ERR1, "d_hitwb()",
	  "D-cache state error during initialization\n", 0,

	DHITWB_ERR2, "d_hitwb()",
	  "D-cache state error Hit writeback happened on a clean exclusive line\n", 0, 

	DHITWB_ERR3, "d_hitwb()",
	  "D-cache Hit writeback happened on a cache miss\n", 0,

	DHITWB_ERR4, "d_hitwb()",
	  "D-cache Hit writeback did not happen on a cache hit\n", 0,

	DHITWB_ERR5, "d_hitwb()",
	  "D-cache Hit Writeback clears the write back bit\n", 0, 

	DIRTYWBW_ERR1, "d_dirtywbw()",
	  "Unexpected Cache write through to memory\n", 0,

	DIRTYWBW_ERR2, "d_dirtywbw()",
	  "Cache writeback did not occur on a word store to a dirty line\n", 0,

	DREFILL_ERR1, "d_refill()",
	  "Unexpected Cache write through to memory\n", 0,
	
	DREFILL_ERR2, "d_refill()",
	  "Secondary Cache miss, expected a cache hit\n", 0,

	SDIRTYWBW_ERR1, "sd_dirtywbw()",
	  "Unexpected Cache write through to memory\n", 0,

	SDIRTYWBW_ERR2, "sd_dirtywbw()",
	  "Data read replaced a dirty line in Secondary\n", 0,

	SDIRTYWBH_ERR1, "sd_dirtywbh()",
	  "Unexpected Cache write through to memory on store halfword\n", 0,

	SDIRTYWBH_ERR2, "sd_dirtywbh()",
	  "Halfword read replaced a dirty line in Secondary, dirty line not written back to memory\n", 0,

	SDIRTYWBB_ERR1, "sd_dirtywbb()",
	  "Unexpected Cache write through to memory on store byte\n", 0,

	SDIRTYWBB_ERR2, "sd_dirtywbb()",
	  "Byte read replaced a dirty line in Secondary, dirty line not written back to memory\n", 0,

	SDTAGECC_ERR1, "sd_tagecc()",
	  "Secondary Data TAG RAM ECC Path error (walking one as data)\n", 0,

	SDTAGECC_ERR2, "sd_tagecc()",
	  "Secondary Data TAG RAM ECC Path error (walking zero as data)\n", 0,

	SDHITINV_ERR1, "sdd_hitinv()",
	  "S-cache state error during initialization\n", 0,

	SDHITINV_ERR2, "sdd_hitinv()",
	  "S-Cache error during Primary Cache dirty line writeback to Scache\n", 0,

	SDHITINV_ERR3, "sdd_hitinv()",
	  "S-Cache state error on a  Hit Invalidate Cache OP\n", 0, 

	SDHITINV_ERR4, "sdd_hitinv()",
	  "Data written back to memory after a Hit Invalidate on the Secondary\n", 0,

	SDHITINV_ERR5, "sdd_hitinv()",
	  "S-Cache state error on a  Hit Invalidate Cache OP\n", 0, 

	SDHITINV_ERR6, "sdd_hitinv()",
	  "Primary Cache TAG not invalid after a Hit Invalidate on the Scache\n", 0,

	SDHITINV_ERR7, "sdd_hitinv()",
	  "Data written back to memory after a Hit Invalidate on the Secondary\n", 0,

	SDHITWB_ERR1, "sd_hitwb()",
	  "Initialization error, unexpected Cache write through to memory\n", 0,

	SDHITWB_ERR2, "sd_hitwb()",
	  "SCache error during Primary Cache dirty line writeback to Scache\n", 0,

	SDHITWB_ERR3, "sd_hitwb()",
	  "Data not written back from Scache to Memory on Hit Writeback Cache OP\n", 0,

	SDHITWB_ERR4, "sd_hitwb()",
	  "Initialization error, unexpected Cache write through to memory\n", 0,

	SDHITWB_ERR5, "sd_hitwb()",
	  "SCache state error during Hit Writeback on S-Cache dirty line\n", 0,

	SDHITWB_ERR6, "sd_hitwb()",
	  "Error in Primary Cache TAG after a Hit Writeback cache Op on the SCache\n", 0,

	SDHITWB_ERR7, "sd_hitwb()",
	  "Data not written back from D-Cache to Memory on a Hit Writeback on the S-Cache\n", 0,

	SDHITWBINV_ERR1, "sd_hitwbinv()",
	  "Initialization error, unexpected Cache write through to memory\n", 0,

	SDHITWBINV_ERR2, "sd_hitwbinv()",
	  "S-Cache TAG error after Hit Writeback Invalidate cacheop\n", 0,

	SDHITWBINV_ERR3, "sd_hitwbinv()",
	  "Data not written back from Scache to Memory after Hit Writeback Invalidate Cacheop\n", 0,

	SDHITWBINV_ERR4, "sd_hitwbinv()",
	  "Initialization error, unexpected Cache write through to memory\n", 0,
	
	SDHITWBINV_ERR5, "sd_hitwbinv()",
	  "S-Cache TAG error after Hit Writeback Invalidate cacheop, test case 2\n", 0,

	SDHITWBINV_ERR6, "sd_hitwbinv()",
	  "Error in Primary Cache TAG after a Hit Writeback Invalidate cacheop on the SCache\n", 0,

	SDHITWBINV_ERR7, "sd_hitwbinv()",
	  "Data not written back from D-Cache to Memory on a Hit Writeback Invalidate on the S-Cache\n", 0,

	{CLSTR_ERR1, "cluster()",
	  "SCache data incorrectly written to memory during a dirty writeback operation\n", 0}, 

	{CLSTR_ERR2, "cluster()",
	  "SCache data incorrectly written to memory during a dirty writeback operation\n", 0}, 
	
	{CLSTRWB_ERR1, "clusterwb()",
	  "SCache data incorrectly written to memory during a dirty writeback operation\non 1st block", 0}, 

	{CLSTRWB_ERR2, "clusterwb()",
	  "SCache data incorrectly written to memory during a dirty writeback operation\non 2nd block", 0}, 

	{CLSTRWB_ERR3, "clusterwb()",
	  "SCache data incorrectly written to memory during a dirty writeback operation\non 3rd block", 0}, 

	{CSTRESS_ERR, "cache_stress",
	  "Secondary cache stress error at addr : 0x%x Expected 0x%x Got 0x%x\n", 0},
	{CSTRESS_PDERR, "ml_hammer_pdcache",
	  "Primary cache stress error at addr : 0x%x Expected 0x%x Got 0x%x\n", 0},
	{CSTRESS_SERR, "ml_hammer_scache",
	  "Secondary cache stress error at addr : 0x%x Expected 0x%x Got 0x%x\n", 0},
	{CSTATE0_ERR, "cache_states_0",
	  "RHH_CE_CE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE1_ERR, "cache_states_1",
	  "RHH_DE_DE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE4_ERR, "cache_states_4",
	  "RMH_I_CE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE5_ERR, "cache_states_5",
	  "RMH_I_DE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE6_ERR, "cache_states_6",
	  "RMH_CE_CE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE7_ERR, "cache_states_7",
	  "RMH_DE_DE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE12_ERR, "cache_states_12",
	  "RMM_I_I : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE13_ERR, "cache_states_13",
	  "RMM_I_CE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE14_ERR, "cache_states_14",
	  "RMM_I_DE : physaddr 0x%x contents incorrect (0x%x)\n", 0},
	{CSTATE_STATE, "check_tag",
	  "%s cache state error at addr 0x%x : Expected %s Got %s\n", 0},
	{CSTATE_ADDR, "check_tag",
	  "%s addr error at slot 0x%x : Expected 0x%x Got 0x%x\n", 0},
	{CSTATE_MEM, "check_mem_val",
	  "Mem value error at addr 0x%x : Expected 0x%x Got 0x%x\n", 0},
	{CSTATE_WRBK, "check_2ndary_val",
	  "Writeback missed 2ndary level cache at addr 0x%x\n", 0},
	{CSTATE_2ND, "check_2ndary_val",
	  "2ndary cache value error at addr 0x%x : Expected 0x%x Got 0x%x\n", 0},

	(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0
};


uint	cache_hintnum[] = { IP19_PCACHE, IP19_SCACHE, IP19_CACHE, 0 };

catfunc_t cache_catfunc = { cache_hintnum, cache_printf  };
catalog_t cache_catalog = { &cache_catfunc, cache_hints };

void
cache_printf(void *loc)
{
	int *l = (int *)loc;
	msg_printf(ERR, "(IP19 slot:%d cpu:%d)\n", *l, *(l+1));
}
