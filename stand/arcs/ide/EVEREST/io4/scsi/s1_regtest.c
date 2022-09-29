/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
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
 *	s1_regtest.c - s1 chips register read/write tests		*
 *									*
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include <setjmp.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/wd95a.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <ide_s1chip.h>

/*
 * address step between one register and the next on s1 chip
 */
#define		REGSTEP		8

/*
 * convenient way to group s1 info for test
 */
typedef struct regstruct {
    unsigned int value;			/* place to save real values */
    int offset;				/* address within s1 chip */
    unsigned int mask;			/* 1's for bits in use */
    char * name;			/* name to give in error messages */
}T_Regs;

static T_Regs s1_testregs[] = {
{ 0, S1_INTF_R_SEQ_REGS + (0 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 0" },
{ 0, S1_INTF_R_SEQ_REGS + (1 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 1" },
{ 0, S1_INTF_R_SEQ_REGS + (2 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 2" },
{ 0, S1_INTF_R_SEQ_REGS + (3 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 3" },
{ 0, S1_INTF_R_SEQ_REGS + (4 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 4" },
{ 0, S1_INTF_R_SEQ_REGS + (5 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 5" },
{ 0, S1_INTF_R_SEQ_REGS + (6 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 6" },
{ 0, S1_INTF_R_SEQ_REGS + (7 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 7" },
{ 0, S1_INTF_R_SEQ_REGS + (8 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 8" },
{ 0, S1_INTF_R_SEQ_REGS + (9 * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 9" },
{ 0, S1_INTF_R_SEQ_REGS + (0xA * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 0xA" },
{ 0, S1_INTF_R_SEQ_REGS + (0xB * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 0xB" },
{ 0, S1_INTF_R_SEQ_REGS + (0xC * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 0xC" },
{ 0, S1_INTF_R_SEQ_REGS + (0xD * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 0xD" },
{ 0, S1_INTF_R_SEQ_REGS + (0xE * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 0xE" },
{ 0, S1_INTF_R_SEQ_REGS + (0xF * REGSTEP), 0x7F, "S1_INTF_R_SEQ_REGS 0xF" },
{ 0, S1_INTF_R_OP_BR_0, 0xF, "S1_INTF_R_OP_BR_0" },
{ 0, S1_INTF_R_OP_BR_1, 0xF, "S1_INTF_R_OP_BR_1" },
{ 0, S1_INTF_W_SEQ_REGS + (0 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 0" },
{ 0, S1_INTF_W_SEQ_REGS + (1 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 1" },
{ 0, S1_INTF_W_SEQ_REGS + (2 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 2" },
{ 0, S1_INTF_W_SEQ_REGS + (3 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 3" },
{ 0, S1_INTF_W_SEQ_REGS + (4 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 4" },
{ 0, S1_INTF_W_SEQ_REGS + (5 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 5" },
{ 0, S1_INTF_W_SEQ_REGS + (6 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 6" },
{ 0, S1_INTF_W_SEQ_REGS + (7 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 7" },
{ 0, S1_INTF_W_SEQ_REGS + (8 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 8" },
{ 0, S1_INTF_W_SEQ_REGS + (9 * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 9" },
{ 0, S1_INTF_W_SEQ_REGS + (0xA * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 0xA" },
{ 0, S1_INTF_W_SEQ_REGS + (0xB * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 0xB" },
{ 0, S1_INTF_W_SEQ_REGS + (0xC * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 0xC" },
{ 0, S1_INTF_W_SEQ_REGS + (0xD * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 0xD" },
{ 0, S1_INTF_W_SEQ_REGS + (0xE * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 0xE" },
{ 0, S1_INTF_W_SEQ_REGS + (0xF * REGSTEP), 0x7F, "S1_INTF_W_SEQ_REGS 0xF" },
{ 0, S1_INTF_W_OP_BR_0, 0xF, "S1_INTF_W_OP_BR_0" },
{ 0, S1_INTF_W_OP_BR_1, 0xF, "S1_INTF_W_OP_BR_1" },
{ 0, (-1), 0, "Undefined S1 Register" }
};

#define T_ARR_SIZE	(sizeof(s1_testregs)/sizeof(T_Regs))

static T_Regs s1_savearr[T_ARR_SIZE];

static int test_patterns[] = {
    0x0, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA, 0xA5A5A5A5, 0x5A5A5A5A
};

#define NUM_TEST_PATS	6

#define SC_RESET_ALL	(S1_SC_S_RESET_0 | S1_SC_S_RESET_1 | S1_SC_S_RESET_2)  

static int s1loc[2];

static int ide_s1_regs(int, int);

int
s1_regtest (int argc, char** argv)
{
    int slot, adap, atype, retval;

    retval = TEST_SKIPPED;
    slot = 0;
    adap = 0;

    /*
     * if bad command line, exit
     */
    if (io4_select (TRUE, argc, argv))
	return(1);

    msg_printf(INFO, "s1_regtest - s1 chip register read/write test\n");
	
    scsi_setup();

    /*
     * iterate through all io boards in the system
     */
    for (slot = EV_MAX_SLOTS; slot > 0; slot--) {

	/*
	 * cheat - if slot was given on command line, use it
	 */
	if (io4_tslot)
	    slot = io4_tslot;

	if (board_type(slot) == EVTYPE_IO4) {
	    /*
	     * iterate through all io adapters in the system
	     */
	    for (adap = 1; adap < IO4_MAX_PADAPS; adap++) {

		/*
		 * cheat - if adapter was given on command line, use it
		 */
		if (io4_tadap)
		    adap = io4_tadap;

		/*
		 * get the current adapter type
		 */
		atype = adap_type(slot, adap);

		if (atype == IO4_ADAP_SCSI) {
		    if (retval == TEST_SKIPPED)
			retval = 0;
		    retval |= ide_s1_regs(slot, adap);
		} else if (io4_tadap) {
		    
		    s1loc[0] = slot;
		    s1loc[1] = adap;
		    err_msg(S1_ADAP, s1loc, adap, atype, IO4_ADAP_SCSI);
		}
	    
		/*
		 * if adap was given on command line, we are done
		 */
		if (io4_tadap)
		    break;
	    }
	
	    /*
	     * if slot was given on command line, we are done
	     */
	    if (io4_tslot)
		break;
	} /* if board_type = io4 */
    } /* for slot */

    return (retval);
}


static int ide_s1_regs(int slot, int adap)
{
    int window, pat_num, retval, readback, stat_cmd;
    int i, chan;
    T_Regs *treg;
#ifndef TFP
    int tomtmp;
#else
    __psunsigned_t tomtmp;
#endif

    retval=0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    msg_printf (INFO, "s1_regtest - looking at slot %x, adap %x\n", slot, adap);

    /*
     * get the real status register contents and force all scsi controllers
     * into reset so we can test the registers
     */
    stat_cmd = S1_GET(window, adap, S1_STATUS_CMD);
    msg_printf(DBG, 
	"stat_cmd at loc 1 is 0x%x, SC_RESET_ALL is 0x%x\n", stat_cmd, SC_RESET_ALL);
    msg_printf(DBG,"stat_cmd | SC_RESET_ALL is 0x%x\n", stat_cmd | SC_RESET_ALL);
    /* S1_SET(window, adap, S1_STATUS_CMD, stat_cmd | SC_RESET_ALL); */
    /* insure scsi is 16-bit mode */
    S1_SET(window, adap, S1_STATUS_CMD, (S1_SC_DATA_16 | 1));
    /* S1_SET(window, adap, S1_STATUS_CMD, (SC_RESET_ALL | 0)); */
    tomtmp = S1_GET(window, adap, S1_STATUS_CMD);
    msg_printf(DBG, "stat_cmd at loc 2 is 0x%x\n", tomtmp);

    /*
     * save existing values from the registers to be tested
     */
    for (treg = s1_testregs; treg->offset != -1; treg++)
	treg->value = S1_GET(window, adap, treg->offset);
    
    /*
     * run all test patterns, now
     */
    for (pat_num = 0; pat_num < NUM_TEST_PATS; pat_num++) {

	/*
	 * write new test pattern
	 */
	for (treg = s1_testregs; treg->offset != -1; treg++)
	    S1_SET(window, adap, treg->offset, test_patterns[pat_num]);

	/*
	 * verify test patterns in the registers
	 */
	for (treg = s1_testregs; treg->offset != -1; treg++) {
	    readback =  S1_GET(window, adap, treg->offset);

	    if (readback != (test_patterns[pat_num] & treg->mask)) {
		s1loc[0] = slot;
		s1loc[1] = adap;
		err_msg(S1_REG_RW, s1loc, treg->name, readback, 
			(treg->mask & test_patterns[pat_num]));
		retval++;
	    }
	}
    }
	
    /*
     * restore existing values to the registers tested
     */
    for (treg = s1_testregs; treg->offset != -1; treg++)
	S1_SET(window, adap, treg->offset, treg->value);

    /* reset wd95a dma channels */
    tomtmp = S1_GET(window, adap, S1_STATUS_CMD);
    msg_printf(DBG, "stat_cmd  at loc 3 is 0x%x\n", tomtmp);
    chan = (adap == S1_ON_MOTHERBOARD) ? 2 : 3;
    for (i = 0; i < chan; i ++) {
	msg_printf(DBG, "chan is %d, i is %d\n", chan, i);
	tomtmp = SWIN_BASE(window, adap) + S1_DMA_BASE+ i*S1_DMA_OFFSET;
	msg_printf(DBG, "tomtmp for dma base is 0x%x\n", tomtmp);
	SET_DMA_REG(tomtmp, S1_DMA_FLUSH, 0);
        SET_DMA_REG(tomtmp, S1_DMA_CHAN_RESET, 0);
        msg_printf(DBG, "debug hello tom 3\n");
        stat_cmd = S1_GET(window, adap, S1_STATUS_CMD);
        msg_printf(DBG, "debug: csr after: 0x%x\n", stat_cmd);
   }
    
    return (retval);
}
