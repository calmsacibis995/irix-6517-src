/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "stand/arcs/ide/godzilla/ecc/heart_ecc.c:  $Revision: 1.5 $"

/*
 * heart_ecc.c - IP30 ECC test and debug code
 *			the algorithm is similar to that of 
 *			heart_ecc_test_cycle in irix/kern/ml/RACER/heart_ecc.c
 */

#include <sys/pda.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/RACER/IP30.h>
#include <sys/ecc.h>
#include <fault.h>	/* for nofault */
#include "sys/cpu.h"
#include "libsk.h"
#include "libsc.h"
#include "d_ecc.h"
#include "pon.h"	/* run_* protos */
#include "sys/RACER/heart.h"
#include <sys/RACER/IP30addrs.h>
#include "d_mem.h"
#include "d_godzilla.h"
#include "d_prototypes.h"
#include "d_ecc.h"

/* extern functions */
extern ulong_t          heart_ecc_test_read(volatile ulong_t *, ulong_t);

/* forward declarations */
int	_is_ECC_error(void);
int	heart_ecc_test_cycle(volatile ulong_t *, volatile ulong_t);
bool_t	inspect(int single_double, volatile ulong_t test_data, 
	volatile ulong_t *test_address);

/* vars */
extern heart_piu_t     *heart_piu;
extern int		ide_ecc_test ; /* in stand/arcs/lib/libsk/ml/IP30.c */

#define	data_eccsyns	real_data_eccsyns

/* 
 * 	MACROS used by heart_ecc_test_cycle 
 */

#define is_power_of_2(x)	((x) && !((x)&((x)-1)))

/* to make sure data has reached memory */
#define	SYNC	PIO_REG_RD_64(HEART_SYNC, ~0x0, h_sync)

/* plants tdata */
/* the single/double bit gen bit/s is/are reset right away */
#define	PLANT(modebits)							\
    SYNC; PIO_REG_RD_64(HEART_MODE , ~0, h_mode_sav);			\
    SYNC; PIO_REG_WR_64(HEART_MODE , ~0, h_mode_sav | modebits);	\
    SYNC; *there = tdata;						\
    SYNC; PIO_REG_WR_64(HEART_MODE , ~0, h_mode_sav);			\
    SYNC

/* reads back rdata */
#define	CHKREAD()							\
    SYNC; us_delay(10);							\
    rdata = heart_ecc_test_read(there, idata);				\
    SYNC; us_delay(10)							

/* writes tdata's complement first, then tdata, then reads rdata */
#define	WRITEREAD(modebits)						\
    *there = idata;							\
    SYNC; us_delay(100);						\
    PLANT(modebits);							\
    SYNC; us_delay(100);						\
    CHKREAD()

/* writes tdata's complement first, then tdata, without reading rdata */
#define	WRITE(modebits)							\
    *there = idata;							\
    PLANT(modebits)

#define MEMERR_DATA_READ(bad_mem_data)					\
    PIO_REG_RD_64(HEART_MEMERR_DATA, ~0x0, bad_mem_data);		\
    msg_printf(DBG,"bad_mem_data= %x\n",				\
			bad_mem_data)

#define MEMERR_ADDR_READ(bad_mem_addr)					\
    PIO_REG_RD_64(HEART_MEMERR_ADDR, ~0x0, bad_mem_addr);		\
    msg_printf(DBG,"bad_mem_addr = %x\n",				\
			bad_mem_addr)

#define	H_MODE_READ							\
    PIO_REG_RD_64(HEART_MODE, ~0x0, h_mode);				\
    msg_printf(DBG,"h_mode = 0x%x\n", h_mode)


/* exercizes the heart error generation electronics: called 
 *	with different data patterns, with single and double bit errors 
 * returns the # of errors 
 */
int
heart_ecc_test_cycle(volatile ulong_t *there, volatile ulong_t tdata)
{
	heartreg_t	h_mode, h_cause, h_mode_sav;
	volatile	ulong_t		idata;
	volatile	ulong_t		rdata;
	int		errors = 0;
	unsigned char	data_syndrome;
	int		is_ecc_error;
	jmp_buf		faultbuf;
	heartreg_t	h_sync = 0, bad_mem_data, bad_mem_addr; 
	heartreg_t	h_isr =0, h_imr0 = 0, h_imsr = 0;

	ide_ecc_test = TRUE;

	/* the exception handler is called when a NCOR err is detected */
	if (setjmp(faultbuf)) {
		msg_printf(DBG,"exception:\n");
		is_ecc_error = _is_ECC_error();
		if(is_ecc_error == 0) {
			msg_printf(ERR,"no ECC error!\n");
			errors ++;
		}
		else if (is_ecc_error == IS_ECC_ERROR_SINGLE) {
			msg_printf(ERR,"SINGLE bit ECC error: should be DOUBLE\n");
			errors ++;
		}
		/* display the bits set in the heart cause register */ 
		_disp_heart_cause();
		if(inspect(DOUBLE_BIT_ERR, tdata, there)) {
			errors ++;
		}
		/* delayed from IP30.c/cpu_clearnofault if ide_ecc_test is TRUE */
		heart_clearnofault();
		ide_ecc_test = FALSE ;  	/* used once for each tdata */
		goto _db_error;
	}
	/* call to setjmp returns here */
       	nofault = faultbuf;

	idata = ~tdata;		/* idata is complement of test data */

	/*
	 * baseline: without asking for induced
	 * ecc errors, store the test data and retrieve
	 * it again. fail if an ecc error occurred or
	 * if the data was incorrect.
	 */

	WRITEREAD(0);

	if (rdata != tdata) {
		errors ++;
		msg_printf(DBG, "heart_ecc_test_cycle: unexpected data compare error\n"
	 			"\twrote data 0x%X to address 0x%X with HM_[SD]B_ERR_GEN off,\n"
				"\treadback data was 0x%X (error bits 0x%X)\n",
				tdata, there, rdata, rdata ^ tdata);
		goto _error;
    	}

	if(_is_ECC_error()) { 	
		msg_printf(ERR,"baseline ECC error!\n");
		errors ++;
		goto _error;
	}

	/*
	 * singlebit data fixup: ask for a single bit
	 * error, write the data, and read it back.
	 * fail if the readback does not trigger
	 * an ecc error, or if the data mismatches.
	 */

	msg_printf(DBG,".....................................single\n");

	WRITEREAD(HM_SB_ERR_GEN);

	us_delay(100);

	is_ecc_error = _is_ECC_error();
	if(is_ecc_error == 0) { 
		msg_printf(ERR,"singlebit data fixup: no ECC error!\n");
		errors ++;
		goto _error;
	}
	else if (is_ecc_error == IS_ECC_ERROR_DOUBLE) {
		msg_printf(ERR,"DOUBLE bit ECC error: should be SINGLE\n");
		errors ++;
		goto _error;
	}

	/* display the bits set in the heart cause register */ 
	_disp_heart_cause();

	if(inspect(SINGLE_BIT_ERR, tdata, there)) {
			errors ++;
			goto _error;
	}

	if (rdata != tdata) {
		errors ++;
		msg_printf(DBG,
			"heart_ecc_test_cycle: data compare error in singlebit error test\n"
	     		"\twrote data 0x%X to address 0x%X with HM_SB_ERR_GEN on,\n"
			"\treadback data was 0x%X (error bits 0x%X)\n",
			tdata, there, rdata, rdata ^ tdata);
		goto _error;
	}

	/* correct the error in mem system */
	WRITEREAD(0);

	/* reset the cause register */
	PIO_REG_WR_64(HEART_CAUSE, ~0x0, ~0x0);

	/*
	 * singlebit memory fixup: revisit the
	 * cell that previously had an ECC error,
	 * to verify that the error has been corrected
	 * in the memory system. fail if the readback
	 * triggers an ecc error, or if the data mismatches.
	 */

	CHKREAD();

	us_delay(100);

	/* make sure no ECC error is flagged */
	if(_is_ECC_error()) {  msg_printf(ERR,"singlebit memory fixup: ECC error!\n");
				errors ++;
                                goto _error;
        }               

	if (rdata != tdata) {
		errors ++;
		msg_printf(DBG,
			"heart_ecc_test_cycle: data mangled by SB ECC handler\n"
	     		"\twrote data 0x%X to address 0x%X with HM_SB_ERR_GEN on,\n"
			"\treadback data was 0x%X (error bits 0x%X)\n",
			tdata, there, rdata, rdata ^ tdata);
		goto _error;
	}

	/*
	 * doublebit error detection: plant a doublebit
	 * ecc error in memory, then read it back. Special
	 * code in the ecc error handler will detect that
	 * this test is running and "repair" the data rather
	 * than panicing the system. Fail if no ECC error
	 * is detected.
	 */

	msg_printf(DBG,".....................................double\n");

	WRITE(HM_DB_ERR_GEN);

	us_delay(100);

	/* there should not be a ECC error: only upon reading */
	if(_is_ECC_error() != 0) {
		msg_printf(ERR,"after doublebit error planting: ECC error!\n");
		errors ++;
		goto _db_error;
	}

	/* read from location: should cause an exception */
	CHKREAD();

	/*
	 * if it won't take exception in 1ms, it won't
	 */
	us_delay(1000);
	msg_printf(ERR,"no exception after reading 2-bit err'd data!\n");
	errors++;

_db_error:
	/*
	 * doublebit recovery: in order for this test
	 * sequence to continue, the test location must
	 * be free of ECC errors again. Test it here,
	 * so we can report correctly what caused the
	 * location to still have an ECC error.
	 */

	/* for double-bit errors, turn on the MEM_FORCE_WR bit in */
        /*  heart's MODE register to write data to the word. */
        errors += pio_reg_mod_64(HEART_MODE, HM_MEM_FORCE_WR, SET_MODE);
	/* correct the error in mem system */
	*there = tdata;
	/* SYNC absolutely necessary: NCOR_ERR exceptions otherwise */
	SYNC;
	/* turn off the MEM_FORCE_WR bit ASAP */
        errors += pio_reg_mod_64(HEART_MODE, HM_MEM_FORCE_WR, CLR_MODE);

_error:

	/* correct the error in mem system */
	WRITEREAD(0);

	/* reset the heart */
        if (_hb_reset(RES_HEART, DONT_RES_BRIDGE))       
                msg_printf(ERR, "_hb_reset Failed\n");

	rdata = rdata;

	h_sync = h_sync; /* to get rid of compiler remark */

	return errors;
}


/* definitions for _ecc_testerrgen(void) */
volatile ulong	ecc_test_patterns[] = {
    0xFFFFFFFFFFFFFFFF,
    0xFFFFFFFF00000000,
    0xFFFF0000FFFF0000,
    0xFF00FF00FF00FF00,
    0xF0F0F0F0F0F0F0F0,
    0xCCCCCCCCCCCCCCCC,
    0xAAAAAAAAAAAAAAAA,
    0x5555555555555555,
    0x3333333333333333,
    0x0F0F0F0F0F0F0F0F,
    0x00FF00FF00FF00FF,
    0x0000FFFF0000FFFF,
    0x00000000FFFFFFFF,
    0x0000000000000000,
};
#define ECC_TEST_PATTERNS       (sizeof ecc_test_patterns / sizeof ecc_test_patterns[0])

/* function name: 	_ecc_testerrgen, like heart_ecc_test 
 *				in irix/kern/ml/RACER/heart_ecc.c
 *			called by ecc_test in ecc.c
 *   	input:		none
 * 	output:  	error flag
 *                      1 = not OK, 0 = OK
 * 	description:	tests the memory ECC by generating single/double bit
 *			errors through the HEART mode register (bits 19,20)
 *	Hardware tested:
 *                      a) Bad ECC generator from HEART to memory
 *                      b) Single/Double bit ECC reporting in R10k
 *                              (if such reporting exists)
 *	remarks:	delays are necessary as the ECC registers
 *			 report errors etc. with latency
 */
int
_ecc_testerrgen(void)
{
	volatile ulong_t	*there;
	heartreg_t		h_imr0;
	heartreg_t		h_mode, h_sync, bad_mem_addr;
	int			esum, tsum, ix;
	heartreg_t		h_cause;
	
	/* need to avoid R10k dirty writebacks, so make sure we run uncached */
	run_uncached();
	flush_cache();

	/* init error and pattern counters */
	esum = 0; tsum = 0;

	/* write always to the same memory location: */
	/* 	the assumption is that there is no pattern */
 	/* 	sensitivity to address lines in the data ECC system */
	there = (volatile ulong_t *)PHYS_TO_K1(IP30_SCRATCH_MEM); 
	
	/* save the heart mode register */
	PIO_REG_RD_64(HEART_MODE, ~0x0, h_mode);
	/* program the heart for ECC tests */
	PIO_REG_WR_64(HEART_MODE, ~0x0, h_mode &~ (HM_COR_ECC_LCK 
						| HM_MEM_FORCE_WR
	    					| HM_DB_ERR_GEN
	    					| HM_SB_ERR_GEN)
					| HM_PE_SYS_COR_ERE
					| HM_GLOBAL_ECC_EN
					| HM_INT_EN
					| HM_DATA_CHK_EN
					| HM_BAD_SYSWR_ERE
					| HM_BAD_SYSRD_ERE
					| HM_NCOR_SYS_ERE
					| HM_COR_SYS_ERE
					| HM_DATA_ELMNT_ERE
					| HM_MEM_ADDR_PROC_ERE
					| HM_MEM_ADDR_IO_ERE
					| HM_NCOR_MEM_ERE
					| HM_COR_MEM_ERE );

	/* reset the heart */
	_hb_reset(RES_HEART, DONT_RES_BRIDGE);

	/* base patterns */
	msg_printf(INFO, 
		"%d base patterns\n", ECC_TEST_PATTERNS);
	for (ix=0; ix<ECC_TEST_PATTERNS; ix++) {
		msg_printf(INFO, "\t\tbase pattern = 0x%016x\n", 
				ecc_test_patterns[ix]);
		esum += heart_ecc_test_cycle(there, ecc_test_patterns[ix]);
	}
	tsum += ECC_TEST_PATTERNS;

	/* other patterns necessary ? if so, from heart_ecc_test */
	/*				in irix/kern/ml/RACER/heart_ecc.c */
	/* XXX */

	/* print out the total number of errors */
	msg_printf(INFO, "heart ecc test: %d patterns checked, %d errors\n\n", 
			tsum, esum);

	if (_hb_reset(RES_HEART, DONT_RES_BRIDGE)) {
		esum++;
	}

	run_cached(); 

	return(esum);
}

/* function name: 	_is_ECC_error
 *   	input:		none
 * 	output:  	1 if correctable ECC error came up, 
 *			2 if non-correctable ECC error came up
 *                      else 0
 * 	description:	tests the heart cause register for COR and NCOR ERR bits
 *	remarks:	
 */
int
_is_ECC_error(void)
{
	unsigned long	    count_down;
	heartreg_t	heart_cause;

	count_down = COUNT_DOWN;
	do	{
		PIO_REG_RD_64(HEART_CAUSE, ~0x0, heart_cause);
		count_down --;
		}
	while ((count_down != 0) && !(heart_cause & (HC_COR_MEM_ERR | HC_NCOR_MEM_ERR)));

	if(count_down==0) {
		/* msg_printf(DBG, "is_ECC_error: no ECC error\n"); */
	}
	else if (heart_cause & HC_COR_MEM_ERR) {
		/* msg_printf(DBG, "is_ECC_error: HC_COR_MEM_ERR set\n"); */
		return(1);
	}
	else if (heart_cause & HC_NCOR_MEM_ERR) {
		/* msg_printf(DBG, "is_ECC_error: HC_NCOR_MEM_ERR set\n"); */
		return(2);
	}
	return(0);

} /* _is_ECC_error */


/* function name: 	inspect
 *   	input:		single/double bit, test data pattern, test address
 * 	output:  	1 if error
 *                      else 0
 * 	description:	Inspect Bad Memory Data register, and 
				Memory Addr/Data Error Reg
 *	remarks:	
 */
bool_t
inspect(int single_double, volatile ulong_t test_data, volatile ulong_t *test_address)
{
	heartreg_t	bad_mem_data, bad_mem_addr;
	unsigned char	data_syndrome;

	/* read the bad memory data register */
	/*  This data should be wrong i.e. the LSB bit should be wrong */
	/*  for SB err gen, the 2 LSB bits should be wrong for DB err gen */
	PIO_REG_RD_64(HEART_MEMERR_DATA, ~0x0, bad_mem_data);
	msg_printf(DBG,"bad_mem_data = %x\n",
			bad_mem_data);
	if(single_double == SINGLE_BIT_ERR) {
		if (!is_power_of_2(bad_mem_data ^ test_data)) {
			msg_printf(ERR,"more than one single bit error\n");
			return(1);
		}
	}
	/* NOTE: for DB, no check XXX write one */
 
	/* read the memory addr/data error register: */
	MEMERR_ADDR_READ(bad_mem_addr);
	if ( ((bad_mem_addr & HME_REQ_SRC_MSK) != ((heartreg_t)cpuid()<<HME_REQ_SRC_SHFT))
	  || ((bad_mem_addr & HME_ERR_TYPE_ADDR) != DATA_TYPE_ERROR)
 	  || ((bad_mem_addr & HME_PHYS_ADDR) != K1_TO_PHYS(test_address)) )
		{
		msg_printf(ERR, "bad HEART_MEMERR_ADDR value\n");
		msg_printf(DBG,"bad_mem_addr & HME_REQ_SRC_MSK = %x\n",
				bad_mem_addr & HME_REQ_SRC_MSK);
		msg_printf(DBG,"bad_mem_addr & HME_ERR_TYPE_ADDR = %x\n",
				bad_mem_addr & HME_ERR_TYPE_ADDR);
		msg_printf(DBG,"bad_mem_addr & HME_SYNDROME = %x\n",
				bad_mem_addr & HME_SYNDROME);
		msg_printf(DBG,"bad_mem_addr & HME_PHYS_ADDR = %x\n",
				bad_mem_addr & HME_PHYS_ADDR);
		return(1);
	}
	/* check the syndrome value */
	data_syndrome = (bad_mem_addr & HME_SYNDROME) >> HME_SYNDROME_SHFT; 
	msg_printf(DBG,"data_eccsyns = %s %d\n", 
			etstrings[data_eccsyns[data_syndrome].type], 
			data_eccsyns[data_syndrome].value);
	msg_printf(DBG,"data_syndrome = 0x%x\n",
			data_syndrome);
	/* NOTE: one cannot test for the bad bit position as it may */
	/*		vary from one Heart rev to the next */
	if (single_double == SINGLE_BIT_ERR) {
		if ((strcmp(etstrings[data_eccsyns[data_syndrome].type], "DB")!=0)) {
			msg_printf(ERR, "bad syndrome: should be ""DB"" \n");
			return(1);
		}
	}
	if (single_double == DOUBLE_BIT_ERR) {
		if ((strcmp(etstrings[data_eccsyns[data_syndrome].type], "2 Bit")!=0)) {
			msg_printf(ERR, "bad syndrome: should be ""2 Bit"" \n");
			return(1);
		}
	}

	return(0); /* no error */

}	/* inspect */
