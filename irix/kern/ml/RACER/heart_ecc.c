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

#ident "$Revision: 1.9 $"

/*
 * heart_ecc.c - IP30 ECC test and debug code
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/ecc.h>
#include <sys/kabi.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/sbd.h>
#include <sys/atomic_ops.h>

#include <sys/RACER/IP30.h>

#include "RACERkern.h"

extern ulong_t          heart_ecc_test_read(volatile ulong_t *, ulong_t);
extern int              dobuserr_common(eframe_t *, inst_t *, uint, int);
extern void		chill_caches(void);

#define	SYNC	(void)HEART_PIU_K1PTR->h_sync

#define	PLANT(modebits)							\
    SYNC; h_mode = HEART_PIU_K1PTR->h_mode;					\
    SYNC; HEART_PIU_K1PTR->h_mode = h_mode | modebits;			\
    SYNC; *there = tdata;						\
    SYNC; HEART_PIU_K1PTR->h_mode = h_mode;					\
    SYNC

#define	CHKREAD()							\
    ecc_test_addr = (ulong_t) there;					\
    sbc = ecc_test_sb_count;						\
    dbc = ecc_test_db_count;						\
    SYNC; us_delay(10);							\
    rdata = heart_ecc_test_read(there, idata);				\
    SYNC; us_delay(10);							\
    ecc_test_addr = 0;							\
    nsbc = ecc_test_sb_count;						\
    ndbc = ecc_test_db_count;

#define	WRITEREAD(modebits)						\
    *there = idata;							\
    PLANT(modebits);							\
    CHKREAD()

void			heart_ecc_plant_sb(iopaddr_t paddr);

#if ECC_DEBUG
int			heart_ecc_test_cycle(volatile ulong_t *, volatile ulong_t);
void                    heart_ecc_test(void);
#endif


#if ECC_DEBUG
volatile ulong_t        ecc_test_addr = 0;
volatile ulong_t	ecc_test_sb_count = 0;
volatile ulong_t	ecc_test_db_count = 0;
#endif

/*
 * plant_corr_ecc_error:
 * plant a correctable ECC error at the
 * specified memory address, after making
 * sure that the location is not cached.
 *
 * returns zero for "all cool" or -1 for
 * an error (currently, "paddr is not memory")
 *
 * also useful for doing nasty things from symmon :)
 */
int
plant_corr_ecc_error(paddr_t paddr)
{
    long	       *k0addr;
    volatile u_long    *there;
    heartreg_t		h_mode;
    u_long		tdata;
    extern void	__dcache_wb_inval(void *, int);
    extern int	is_in_main_memory(paddr_t);

    if (!is_in_main_memory(paddr))
	return -1;

    paddr &= ~7ull;
    k0addr = (long *)PHYS_TO_K0(paddr);
    there = (volatile u_long *)PHYS_TO_K1(paddr);
    atomicAddLong(k0addr, 0);		/* get the line exclusive */
    __dcache_wb_inval(k0addr, 8);	/* kick it out of our cache too */
    tdata = *there;			/* get the target data */
    PLANT(HM_SB_ERR_GEN);		/* put it back with an SB ECC errror */

    return 0;
}

#if ECC_DEBUG
int
heart_ecc_test_cycle(volatile ulong_t *there, volatile ulong_t tdata)
{
    heartreg_t              h_mode;
    ulong_t                 idata;
    ulong_t                 rdata;
    int			    errors = 0;
    int			    sbc, nsbc;
    int			    dbc, ndbc;

    idata = ~tdata;

    /*
     * baseline: without asking for induced
     * ecc errors, store the test data and retrieve
     * it again. fail if an ecc error occurred or
     * if the data was incorrect.
     */

    WRITEREAD(0);

    if (rdata != tdata) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: unexpected data compare error\n"
	 "\twrote data 0x%X to address 0x%X with HM_[SD]B_ERR_GEN off,\n"
		"\treadback data was 0x%X (error bits 0x%X)\n",
		tdata, there, rdata, rdata ^ tdata);
	us_delay(1000000);
    }

    if (sbc != nsbc) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: unexpected SB ECC error\n"
	 "\twrote data 0x%X to address 0x%X with HM_[SD]B_ERR_GEN off,\n"
		"\tand readback triggered a SB ECC error.",
		tdata, there);
	us_delay(1000000);
    }

    if (dbc != ndbc) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: unexpected DB ECC error\n"
	 "\twrote data 0x%X to address 0x%X with HM_[SD]B_ERR_GEN off,\n"
		"\tand readback triggered a DB ECC error.",
		tdata, there);
	us_delay(1000000);
    }

    /*
     * singlebit data fixup: ask for a single bit
     * error, write the data, and read it back.
     * fail if the readback does not trigger
     * an ecc error, or if the data mismatches.
     */

    WRITEREAD(HM_SB_ERR_GEN);

    if (sbc == nsbc) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: unable to induce singlebit error\n"
	     "\twrote data 0x%X to address 0x%X with HM_SB_ERR_GEN on,\n"
		"\tno error detected on readback,\n"
		"\tand no SB ECC errors were handled.",
		tdata, there);
	us_delay(1000000);
    }


    if (dbc != ndbc) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: unexpected DB ECC error (wanted SB)\n"
	 "\twrote data 0x%X to address 0x%X with with HM_SB_ERR_GEN on,\n"
		"\tand readback triggered a DB ECC error.",
		tdata, there);
	us_delay(1000000);
    }

    if (rdata != tdata) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: data compare error in singlebit error test\n"
	     "\twrote data 0x%X to address 0x%X with HM_SB_ERR_GEN on,\n"
		"\treadback data was 0x%X (error bits 0x%X)\n",
		tdata, there, rdata, rdata ^ tdata);
	us_delay(1000000);
    }
    /*
     * singlebit memory fixup: revisit the
     * cell that previously had an ECC error,
     * to verify that the error has been corrected
     * in the memory system. fail if the readback
     * triggers an ecc error, or if the data mismatches.
     */

    CHKREAD();

    if (sbc != nsbc) {
	errors ++;
	cmn_err(CE_ALERT,
	     "heart_ecc_test_cycle: ecc not repaired by SB ECC handler\n"
	     "\twrote data 0x%X to address 0x%X with HM_SB_ERR_GEN on,\n"
		"\tfirst readback triggered error but did not correct:\n"
		"\tsecond readback triggered another error.",
		tdata, there);
	us_delay(1000000);
    }

    if (dbc != ndbc) {
	errors ++;
	cmn_err(CE_ALERT,
	     "heart_ecc_test_cycle: SB ECC repair left a DB error\n"
	     "\twrote data 0x%X to address 0x%X with HM_SB_ERR_GEN on,\n"
		"\tfirst readback triggered SB error correction, but\n"
		"\tsecond readback triggered a DB error.",
		tdata, there);
	us_delay(1000000);
    }

    if (rdata != tdata) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: data mangled by SB ECC handler\n"
	     "\twrote data 0x%X to address 0x%X with HM_SB_ERR_GEN on,\n"
		"\treadback data was 0x%X (error bits 0x%X)\n",
		tdata, there, rdata, rdata ^ tdata);
	us_delay(1000000);
    }

    /*
     * doublebit error detection: plant a doublebit
     * ecc error in memory, then read it back. Special
     * code in the ecc error handler will detect that
     * this test is running and "repair" the data rather
     * than panicing the system. Fail if no ECC error
     * is detected.
     */

    WRITEREAD(HM_DB_ERR_GEN);

    if (dbc == ndbc) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: unable to induce doublebit error\n"
	     "\twrote data 0x%X to address 0x%X with HM_DB_ERR_GEN on,\n"
		"\tno DB error detected on readback.",
		tdata, there);
	us_delay(1000000);
    }

    if (sbc != nsbc) {
	errors ++;
	cmn_err(CE_ALERT,
		"heart_ecc_test_cycle: SB error when DB wanted\n"
	     "\twrote data 0x%X to address 0x%X with HM_DB_ERR_GEN on,\n"
		"\tand an SB ECC error occurred on readback.",
		tdata, there);
	us_delay(1000000);
    }

    /*
     * doublebit recovery: in order for this test
     * sequence to continue, the test location must
     * be free of ECC errors again. Test it here,
     * so we can report correctly what caused the
     * location to still have an ECC error.
     */

    CHKREAD();

    if (sbc != nsbc) {
	errors ++;
	cmn_err(CE_ALERT,
	 "heart_ecc_test_cycle: unable to recover from doublebit error\n"
	     "\twrote data 0x%X to address 0x%X with HM_DB_ERR_GEN on,\n"
		"\tinitial readback triggered ECC handling,"
		"\tsecond readback got an SB ECC error.",
		tdata, there);
	us_delay(1000000);
    }

    if (dbc != ndbc) {
	errors ++;
	cmn_err(CE_ALERT,
	 "heart_ecc_test_cycle: unable to recover from doublebit error\n"
	     "\twrote data 0x%X to address 0x%X with HM_DB_ERR_GEN on,\n"
		"\tinitial readback triggered ECC handling,"
		"\tsecond readback got another DB ECC error.",
		tdata, there);
	us_delay(1000000);
    }

    rdata = rdata;

    return errors;
}

ulong	ecc_test_patterns[] = {
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
#define	ECC_TEST_PATTERNS	(sizeof ecc_test_patterns / sizeof ecc_test_patterns[0])

void
heart_ecc_test(void)
{
    volatile ulong_t       *there;
    int			    tsum, esum;
    int			    ix, iy, iz;

    there = (volatile ulong_t *)PHYS_TO_K1(0x20000000);
    tsum = esum = 0;

    HEART_PIU_K1PTR->h_cause = ~0;

    us_delay(1000000);

    chill_caches();

    cmn_err(CE_CONT, "heart ecc test: %d base patterns\n", ECC_TEST_PATTERNS);
    us_delay(1000000);
    tsum += ECC_TEST_PATTERNS;
    for (ix=0; ix<ECC_TEST_PATTERNS; ix++)
	esum += heart_ecc_test_cycle(there, ecc_test_patterns[ix]);

    cmn_err(CE_CONT, "heart ecc test: 64 walking single one-bits\n");
    us_delay(1000000);
    tsum += 64;
    for (ix=0; ix<64; ++ix)
	esum += heart_ecc_test_cycle(there, 1ull<<ix);

    cmn_err(CE_CONT, "heart ecc test: 64 walking single zero-bits\n");
    us_delay(1000000);
    tsum += 64;
    for (ix=0; ix<64; ++ix)
	esum += heart_ecc_test_cycle(there, ~(1ull<<ix));

    cmn_err(CE_CONT, "heart ecc test: %d two walking single one-bits\n", 32*63);
    us_delay(1000000);
    tsum += 32*63;
    for (ix=1; ix<64; ++ix)
	for (iy=0; iy<ix; ++iy)
	    esum += heart_ecc_test_cycle(there, ((1ull<<ix) | (1ull<<iy)));

    cmn_err(CE_CONT, "heart ecc test: %d two walking single zero-bits\n", 32*63);
    us_delay(1000000);
    tsum += 32*63;
    for (ix=1; ix<64; ++ix)
	for (iy=0; iy<ix; ++iy)
	    esum += heart_ecc_test_cycle(there, ~((1ull<<ix) | (1ull<<iy)));

    cmn_err(CE_CONT, "heart ecc test: %d digit on digits\n", 16*15*16);
    us_delay(1000000);
    tsum += 16*15*16;
    for (ix=0; ix<16; ++ix) {
	for (iy=0; iy<16; ++iy) {
	    if (ix != iy) {
		for (iz=0; iz<64; iz += 4) {
		    esum += heart_ecc_test_cycle(there,
					 ((0x1111111111111111 * ix) ^
					  ((ix^iy)<<iz)));
		}
	    }
	}
    }

    us_delay(1000000);

    cmn_err(CE_CONT, "heart ecc test: %d patterns checked, %d errors\n", tsum, esum);
}

#endif
