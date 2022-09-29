/*
 * mc3/connect.c
 *
 *
 * Copyright 1991, 1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.20 $"

/*
 * connect -- 	
 *		This is a memory sockets connection test. It writes patterns
 *		to the first 16 bytes of each SIMM then
 *		read it back. If doesn't match, the socket has connection
 *		problem.
 *
 *
 * NOTICE: Reserved 1M memory for diagnostics.
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/EVEREST/mc3.h>
#include <sys/EVEREST/evconfig.h>
#ifdef IP19
#include "ip19.h"
#elif IP21
#include "ip21.h"
#elif IP25
#include "ip25.h"
#endif
#include "pattern.h"
#include "mc3_reg.h"
#include "libsk.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include "uif.h"
#include "prototypes.h"

#define PATTERN  0xA5A5A5A5
#define WORDS_PER_BLOCK 32
#define MAX_INTERLEAVE_F 16

extern uint mem_slots;
extern long long vir2phys(__psunsigned_t);

int
connect(void)
{
    register int bank, leaf, slot;
    register uint *ptr;
    register uint i;
    __psunsigned_t badaddr;
    register uint enable;
    int fail = 0;
    uint badbits = 0;
    jmp_buf con_buf;
    int decslot;
    int first_time = 1;
#ifdef IP19
    __psunsigned_t lomem;
#endif
    unsigned long long base, bank_base_array[EV_MAX_MC3S*MC3_NUM_LEAVES*MC3_BANKS_PER_LEAF];
    int val;
    int bank_count = 0;
    int new_bank;   

    /* write to the CPU LEDS */

    msg_printf(SUM, "Memory sockets connection test\n");
    for (i = 0; i < EV_MAX_MC3S*MC3_NUM_LEAVES*MC3_BANKS_PER_LEAF; i++) 
	bank_base_array[i] = 0;

    mem_slots = mc3slots();
    mem_err_clear(mem_slots); /* clear error regs */
    memtest_init();
    if (val = setjmp(con_buf)) {
        if (val == 1){
                msg_printf(ERR, "Unexpected exception during memory test\n");
                show_fault();
        }
        mem_err_log();
        mem_err_show();
        /* everest_error_show(); */
	fail = 1;
        goto SLOTFAILED;
    }
    else {
        set_nofault(con_buf);

    for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
        if (board_type(slot) == EVTYPE_MC3) {
            msg_printf(DBG, "Board %d is memory\n", slot);
            /* It's a memory board, do leaf loop */
            for (leaf = 0; leaf < MC3_NUM_LEAVES; leaf++) {
                msg_printf(DBG, "leaf %d\n", leaf);
                for (bank = 0; bank < MC3_BANKS_PER_LEAF; bank++) {
                  msg_printf(DBG, "leaf %d, bank %d\n", leaf,bank);
                  /* Nonzero means enabled, nonzero == enabled */
		  if (enable = 
			(read_reg(slot, MC3_BANKENB) & MC3_BENB(leaf, bank))) 
		  {
                    msg_printf(DBG,"lf %d,bnk %d,Enabl=%x\n",leaf,bank,enable);
                    /* simm_type = 
			read_reg(slot, MC3_BANK(leaf, bank, BANK_SIZE)); */
                    /* msg_printf(VRB, "Type = %x\n", simm_type); */
/*                  i_factor = read_reg(slot, MC3_BANK(leaf, bank, BANK_IF));
                    i_position = read_reg(slot, MC3_BANK(leaf, bank, BANK_IP));
*/
                    base = read_reg(slot, MC3_BANK(leaf, bank, BANK_BASE));
                    msg_printf(DBG, "Base = 0x%Lx\n",  base);
		    /* I will write 16x128 bytes in order to make sure I write
		    * to each SIMM within each bank. The 16 comes from the 
		    * max ways I can interleave. The 128 is because
		    * interleaving is done on 128-byte chunks.
		    * The SIMM addressing is
		    * extremely complicated, so at this point we're just 
		    * going to write a complete block to verify that we can
		    * at least write to all SIMMs. In the future, if we have
		    * code to decode to the SIMM level, we can modify our
		    * algorithm
		    */

		    /*
	 	    * if check addr is within RAM used by diags, bump by the
	 	    * next address up - we used to add PHYS_CHECK_LO, but since
	 	    * PHYS_CHECK_LO is ~= SIMSIZE, this is not a safe thing 
	 	    */

		    /* base is the base address with its low 8-bits dropped */
		    /* So left shift 8 0's to make it its full address */
		    base = (base << 8);

		    if (first_time) {
			first_time = 0;
			new_bank = 1;
		    }
		    else {
			new_bank = 1;
			for (i = 0; i < bank_count; i++)
				if (bank_base_array[i] == base)
					new_bank = 0;
		    }
		    if (new_bank) {
			bank_base_array[bank_count] = base;
                        bank_count++;

		    if (base < PHYS_CHECK_LO)
			base = PHYS_CHECK_LO;
		    msg_printf(DBG,"Shifted base is 0x%Lx\n",  base);

#if _MIPS_SIM == _ABI64
		    base = PHYS_TO_MEM(base);
		    for (ptr = (uint *)base, i=0; 
			i < (MAX_INTERLEAVE_F*WORDS_PER_BLOCK); ptr+=1, i++) {
		   	msg_printf(VRB, "uncached addr 0x%x\n", ptr);
#else
		    lomem = map_addr(base, DIRECT_MAP, UNCACHED);
		    for (ptr = (uint *)lomem, i=0; 
			i < (MAX_INTERLEAVE_F*WORDS_PER_BLOCK); ptr+=1, i++) {
		   	msg_printf(VRB, "virtual addr 0x%x\n", ptr);
#endif
	    		*ptr = PATTERN;
	    		if (*ptr != PATTERN)
	    		{
			   badbits = *ptr ^ PATTERN;
			   badaddr = vir2phys((__psunsigned_t)ptr); /*****/
			   decode_address(badaddr, badbits, SW_WORD, &decslot);
			   err_msg(CONNECT_ERR, &decslot, badaddr,
				PATTERN, *ptr);
			   longjmp(con_buf, 2);
	    		}
	    		*ptr = ~PATTERN;
	    		if (*ptr != ~PATTERN)
	    		{
			   badbits = *ptr ^ ~PATTERN;
			   badaddr = vir2phys((__psunsigned_t)ptr); /*****/
			   decode_address(badaddr, badbits, SW_WORD, &decslot);
			   err_msg(CONNECT_ERR, &decslot, badaddr, 
				~PATTERN, *ptr);
			   longjmp(con_buf, 2);
	    		}
/*
#ifdef IP19
			if (((__psunsigned_t)ptr % 0x80) == 0)
                        	if (cpu_intr_pending())
                                	longjmp(con_buf, 2);
#endif
*/
		    }   /* for i < WORDS_PER_BLOCK */
		    } /* if new_bank */
		  }   /* if enabled */
		}   /* for bank */
	    }    /* for leaf */
        }    /* if mc3 boards */
    }    /* for all slots */
    } /* else ... setjmp */

SLOTFAILED:

    /* Post processing */
    if (chk_memereg(mem_slots)){
	msg_printf(ERR, "Memory sockets connection test detected non-zero MC3 error register contents\n");
	check_memereg(mem_slots);
	mem_err_clear(mem_slots);
	fail = 1;
    }

    clear_nofault();
    return (fail);
}
