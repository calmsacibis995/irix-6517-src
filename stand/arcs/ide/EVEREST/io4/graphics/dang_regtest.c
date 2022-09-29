/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 *	dang_regtest.c - dang chip register read/write tests		*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
#include <sys/EVEREST/evconfig.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <everr_hints.h>

/*
 * convenient way to group chip info for test
 */
typedef struct regstruct {
    unsigned long long value;		/* place to save real values */
    int offset;				/* address within dang chip */
    unsigned long long mask;		/* 1's for bits in use */
    char * name;			/* name to give in error messages */
}T_Regs;

static T_Regs dang_testregs[] = {

/* General DANG Config registers */

#ifdef	NEVER
{ 0, DANG_PIO_ERR, 0xFFFFL, "dang_pio_err" },
{ 0, DANG_PIO_ERR_CLR, 0xFFFFL, "dang_pio_err_clr" },
#endif /* NEVER */

{ 0, DANG_UPPER_GIO_ADDR, 0x3FL, "dang_upper_gio_addr" },
{ 0, DANG_MIDDLE_GIO_ADDR, 0x7FFL, "dang_middle_gio_addr" },
{ 0, DANG_BIG_ENDIAN, 0x1L, "dang_big_endian" },
{ 0, DANG_GIO64, 0x1L, "dang_gio64" },
{ 0, DANG_PIPELINED, 0x1L, "dang_pipelined" },
{ 0, DANG_GIORESET, 0x1L, "dang_gioreset" },
{ 0, DANG_AUDIO_ACTIVE, 0x1L, "dang_audio_active" },
{ 0, DANG_AUDIO_SLOT, 0xFL, "dang_audio_slot" },
{ 0, DANG_PIO_WG_WRTHRU, 0x1L, "dang_pio_wg_wrthru" },

/* GIO Bus DMA Master Module */

#ifdef NEVER
{ 0, DANG_DMAM_STATUS, 0xFFFFL, "dang_dmam_status" },
{ 0, DANG_DMAM_ERR, 0xFFFFL, "dang_dmam_err" },
{ 0, DANG_DMAM_ERR_CLR, 0xFFFFL, "dang_dmam_err_clr" },
#endif /* NEVER */

{ 0, DANG_DMAM_MAX_OUTST, 0x7L, "dang_dmam_max_outst" },
{ 0, DANG_DMAM_CACHE_LINECNT, 0x7L, "dang_dmam_cache_linecnt" },

#ifdef NEVER
/*
    this may cause problems if read as part of regular test - may need special
    interrupt-catching code to handle this
*/
{ 0, DANG_DMAM_START, 0xFFFFFFFFFFL, "dang_dmam_start" },
{ 0, DANG_DMAM_IBUS_ADDR, 0xFFFFL, "dang_dmam_ibus_addr" },
{ 0, DANG_DMAM_GIO_ADDR, 0xFFFFL, "dang_dmam_gio_addr" },
{ 0, DANG_DMAM_COMPLETE, 0xFFFFL, "dang_dmam_complete" },
{ 0, DANG_DMAM_COMPLETE_CLR, 0xFFFFL, "dang_dmam_complete_clr" },
{ 0, DANG_DMAM_KILL, 0xFFFFL, "dang_dmam_kill" },
{ 0, DANG_DMAM_PTE_ADDR, 0xFFFFL, "dang_dmam_pte_addr" },
#endif /* NEVER */

/* GIO Bus DMA Slave Module */

#ifdef NEVER
{ 0, DANG_DMAS_STATUS, 0xFFFFL, "dang_dmas_status" },
{ 0, DANG_DMAS_ERR, 0xFFFFL, "dang_dmas_err" },
{ 0, DANG_DMAS_ERR_CLR, 0xFFFFL, "dang_dmas_err_clr" },
#endif /* NEVER */

{ 0, DANG_DMAS_MAX_OUTST, 0x3L, "dang_dmas_max_outst" },
{ 0, DANG_DMAS_CACHE_LINECNT, 0x3L, "dang_dmas_cache_linecnt" },

#ifdef NEVER
{ 0, DANG_DMAS_IBUS_ADDR, 0xFFFFL, "dang_dmas_ibus_addr" },
#endif /* NEVER */

/* DANG interrupt control */

#ifdef NEVER
{ 0, DANG_INTR_STATUS, 0xFFFFL, "dang_intr_status" },
/* has odd handling of sets/clears */
{ 0, DANG_INTR_ENABLE, 0x7FEL, "dang_intr_enable" },
#endif /* NEVER */

{ 0, DANG_INTR_ERROR, 0x7F7FL, "dang_intr_error" },
{ 0, DANG_INTR_GIO_0, 0x7F7FL, "dang_intr_gio_0" },
{ 0, DANG_INTR_GIO_1, 0x7F7FL, "dang_intr_gio_1" },
{ 0, DANG_INTR_GIO_2, 0x7F7FL, "dang_intr_gio_2" },
{ 0, DANG_INTR_DMAM_COMPLETE, 0x7F7FL, "dang_intr_dmam_complete" },
{ 0, DANG_INTR_PRIV_ERR, 0x7F7FL, "dang_intr_priv_err" },
#ifdef NEVER
{ 0, DANG_INTR_LOWATER, 0x7FL, "dang_intr_lowater" },
{ 0, DANG_INTR_HIWATER, 0x7FL, "dang_intr_hiwater" },
{ 0, DANG_INTR_FULL, 0x7FL, "dang_intr_full" },
#endif
{ 0, DANG_INTR_PAUSE, 0x7L, "dang_intr_pause" },
{ 0, DANG_INTR_BREAK, 0x7L, "dang_intr_break" },

/* DANG write gatherer */

#ifdef NEVER
{ 0, DANG_WG_STATUS, 0xFFFFL, "dang_wg_status" },
#endif /* NEVER */

{ 0, DANG_WG_LOWATER, 0x7FFL, "dang_wg_lowater" },
{ 0, DANG_WG_HIWATER, 0x7FFL, "dang_wg_hiwater" },
{ 0, DANG_WG_FULL, 0x7FFL, "dang_wg_full" },
{ 0, DANG_WG_PRIV_LOADDR, 0x1FL, "dang_wg_priv_loaddr" },
{ 0, DANG_WG_PRIV_HIADDR, 0x1FL, "dang_wg_priv_hiaddr" },
{ 0, DANG_WG_GIO_UPPER, 0x7FFL, "dang_wg_gio_upper" },
{ 0, DANG_WG_GIO_STREAM, 0x7FFL, "dang_wg_gio_stream" },
#ifdef NEVER
{ 0, DANG_WG_STREAM_WDCNT, 0x7FFFFFFFL, "dang_wg_stream_wdcnt" },
#endif /* NEVER */
{ 0, DANG_WG_PAUSE, 0x1L, "dang_wg_pause" },
{ 0, DANG_WG_STREAM_ALWAYS, 0x1L, "dang_wg_stream_always" },

#ifdef NEVER
{ 0, DANG_WG_FIFO_RDADDR, 0x7FFL, "dang_wg_fifo_rdaddr" },
{ 0, DANG_WG_PRIV_ERR_CLR, 0x1L, "dang_wg_priv_err_clr" },
{ 0, DANG_WG_U_STATUS, 0xFFFFL, "dang_wg_u_status" },
{ 0, DANG_WG_U_WDCNT, 0xFFFFL, "dang_wg_u_wdcnt" },
{ 0, DANG_WG_WDCNT, 0x7FFL, "dang_wg_wdcnt" },
#endif /* NEVER */

/* END of registers to be tested */
{ 0, (-1), 0, "Undefined DANG Register" }
};

static int test_patterns[] = {
    0x0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, 0x5A5A5A5A
};

#define NUM_TEST_PATS	6

extern jmp_buf dangbuf;

static int ide_dang_regs(int, int);
static int loc[3];

int
dang_regtest (int argc, char** argv)
{
    int retval;

    msg_printf(INFO, "\ndang_regtest - dang chip register read/write test\n");

    if (console_is_gfx())
    {
	msg_printf(INFO, "Test skipped. Can run only on an ASCII console\n");
	return(TEST_SKIPPED);
    }

    retval = test_adapter(argc, argv, IO4_ADAP_DANG, ide_dang_regs);

    return (retval);
}


static int ide_dang_regs(int slot, int adap)
{
    int window, pat_num, retval;
    volatile long long * dangchip;
    long long int_mask, readback;
    unsigned long curpat;
    T_Regs *treg;

    retval=0;
    loc[0] = slot; loc[1] = adap; loc[2] = -1;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);
    dangchip = (long long *) SWIN_BASE(window, adap);

    msg_printf(INFO, "dang_regtest - looking at slot %x, adap %x\n", slot, adap);

    /*
     * Get the current interrupt mask contents and disable all interrupt
     * sources - if cannot disable interrupts, exit
     */
    if (setjmp(dangbuf)) {
	msg_printf(ERR, "Exception Accessing DANG chip!\n");
	clear_nofault();
	return (1);
    }
    else {
	set_nofault(dangbuf);
	setup_err_intr(slot, adap);
	int_mask = load_double((long long *)&dangchip[DANG_INTR_ENABLE]);
	int_mask |= DANG_IENA_CLEAR;
	store_double((long long *)&dangchip[DANG_INTR_ENABLE],
		     (long long) int_mask);
	readback = load_double((long long *)&dangchip[DANG_INTR_ENABLE]);
	clear_nofault();
    }

    if (readback != 0) {
	err_msg(DANG_INTSET, loc, (long long)readback, (long long) 0);
	msg_printf(VRB, "  unable to disable DANG interrupts\n");
	retval++;
	goto TEST_DONE;
    }


    /*
     * save existing values from the registers to be tested
     */
    for (treg = dang_testregs; treg->offset != -1; treg++)
	treg->value = load_double((long long *)&dangchip[treg->offset]);
    
    /*
     * run all test patterns, now
     */
    msg_printf(VRB,"\tsimple pattern test\n");
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for (treg = dang_testregs; treg->offset != -1; treg++)
	    store_double((long long *)&dangchip[treg->offset],
		         (long long)(treg->mask & test_patterns[pat_num]));

	/*
	 * verify test patterns in the registers
	 */
	for (treg = dang_testregs; treg->offset != -1; treg++) {
	    readback =  load_double((long long *)&dangchip[treg->offset]);

	    if ((readback & treg->mask) !=
		(test_patterns[pat_num] & treg->mask)) {
		err_msg(DANG_BADREG, loc, treg->name,
		    (long long)(treg->mask & test_patterns[pat_num]),
		    (long long)(readback & treg->mask));
		retval++;
	    }
	}
    }
	
    /*
     * marching ones test
     */
    msg_printf(VRB, "\tmarching ones test\n");
    for (pat_num = 0; pat_num < 32; pat_num++) {

	curpat = 1 << pat_num;

	/*
	 * write new test pattern
	 */
	for (treg = dang_testregs; treg->offset != -1; treg++)
	    store_double((long long *)&dangchip[treg->offset],
		         (long long)(treg->mask & curpat));

	/*
	 * verify test patterns in the registers
	 */
	for (treg = dang_testregs; treg->offset != -1; treg++) {
	    readback =  load_double((long long *)&dangchip[treg->offset]);

	    if ((readback & treg->mask) !=
		(curpat & treg->mask)) {
		err_msg(DANG_BADREG, loc, treg->name,
		    (long long)(treg->mask & curpat),
		    (long long)(readback & treg->mask));
		retval++;
	    }
	}
    }
	
    /*
     * marching zeros test
     */
    msg_printf(VRB, "\tmarching zeros test\n");
    for (pat_num = 0; pat_num < 32; pat_num++) {

	curpat = ~(1 << pat_num);

	/*
	 * write new test pattern
	 */
	for (treg = dang_testregs; treg->offset != -1; treg++)
	    store_double((long long *)&dangchip[treg->offset],
		         (long long)(treg->mask & curpat));

	/*
	 * verify test patterns in the registers
	 */
	for (treg = dang_testregs; treg->offset != -1; treg++) {
	    readback =  load_double((long long *)&dangchip[treg->offset]);

	    if ((readback & treg->mask) !=
		(curpat & treg->mask)) {
		err_msg(DANG_BADREG, loc, treg->name,
		    (long long)(treg->mask & curpat),
		    (long long)(readback & treg->mask));
		retval++;
	    }
	}
    }
	
    /*
     * address-in-address test
     */
    msg_printf(VRB, "\taddress-in-address test\n");

    /*
     * write new test pattern
     */
    for (treg = dang_testregs; treg->offset != -1; treg++)
	store_double((long long *)&dangchip[treg->offset],
		     (long long)(treg->mask & treg->offset));

    /*
     * verify test patterns in the registers
     */
    for (treg = dang_testregs; treg->offset != -1; treg++) {
	readback =  load_double((long long *)&dangchip[treg->offset]);

	if ((readback & treg->mask) !=
	    (treg->offset & treg->mask)) {
	    err_msg(DANG_BADRADR, loc, treg->name,
		(long long)(treg->mask & treg->offset),
		(long long)(readback & treg->mask));
	    retval++;
	}
    }
    
    /*
     * inverse address-in-address test
     */
    msg_printf(VRB, "\tinverse address-in-address test\n");

    /*
     * write new test pattern
     */
    for (treg = dang_testregs; treg->offset != -1; treg++)
	store_double((long long *)&dangchip[treg->offset],
		     (long long)(treg->mask & ~treg->offset));

    /*
     * verify test patterns in the registers
     */
    for (treg = dang_testregs; treg->offset != -1; treg++) {
	readback =  load_double((long long *)&dangchip[treg->offset]);

	if ((readback & treg->mask) !=
	    (~treg->offset & treg->mask)) {
	    err_msg(DANG_BADRADR, loc, treg->name,
		(long long)(treg->mask & ~treg->offset),
		(long long)(readback & treg->mask));
	    retval++;
	}
    }
    
    /*
     * restore existing values to the registers tested
     */
    for (treg = dang_testregs; treg->offset != -1; treg++)
	store_double((long long *)&dangchip[treg->offset],
		     (long long)treg->value);


TEST_DONE:

    /*
     * restore original interrupt mask and return
     */
    store_double((long long *)&dangchip[DANG_INTR_ENABLE], (long long)int_mask);
    return (retval);
}
