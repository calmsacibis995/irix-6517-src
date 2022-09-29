
/*
 * mc3/force_ecc.c
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

#ident "$Revision: 1.6 $"

/*
 *  	force_ecc
 *	Force a double-bit ECC error. Single big errors are not force-able
 * 	because we can't get to the right registers.
 *
 * 	Based on code from nissen:/usr/people/bh/everest/diags/newt/basic/intr
 */

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everror.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "mc3_reg.h"

#define WORKLINE        PHYS_TO_K0(PHYS_CHECK_LO+0x1000)
#define AMEG    0x100000
#define OUTADDR 0xa0001080 /* make stuff appear on the everest bus */
#define  ALINE   0x80    /* 2ndry cache line size in bytes */

#define ERR_SR_IBITCLR 	0x1
#define ERR_SR_NOIP6ERR 	0x2
#define ERR_ERTOIP	0x4
#define ERR_SR_IBITOFF	0x8
#define ERR_CERTOIP	0x10
#define ERR_CACHEWR	0x20
#define ERR_SET_ECC	0x40

#if DEBUG || DEBUG2
	uint mytmp = 0x0;
	int oneOrtwo = 0;
	int err1 = 0;
	int err2 = 0;
	int err4 = 0;
	int err8 = 0;
	int err10 = 0;
	uint debugReg = 0x0;
	uint debugReg2 = 0x0;
	uint debugReg3 = 0x0;
	uint debugReg4 = 0x0; /* cause reg after test */
	uint debugReg41 = 0x0; /* cause reg before test */
	uint debugReg51 = 0x0; /* ev_ertoip reg before test */
	uint debugReg5 = 0x0; /* ev_ertoip reg after test */
	uint debugReg6 = 0x0; /* ev_ile reg */
#endif


int
force_ecc()
{
	register int spnum;
	register uint osr, tmp;
	uint fail = 0x0;
	uint bank, leaf, simm, slot;

#if DEBUG || DEBUG2
	msg_printf(VRB, "fail at starting time is 0x%x\n", fail);
	/* printf("fail at starting time is 0x%x\n", fail); */
#endif
	msg_printf(SUM, "Force ECC errors test\n");
	/* printf("Force ECC errors test\n");  */
	spnum = dold(EV_SPNUM);
	osr = get_sr();
	set_sr(osr & ~SR_IE);  /* disable R4k intrs */	

	/* enable CC level 3 intrs */
	/* EV_ILE is the interrupt level enable register location */
	/* EV_ERTOINT_MASK is 0x8 which sets the CC intr level to 3 */
	dosd(dold(EV_ILE) | EV_ERTOINT_MASK, EV_ILE);  
#if DEBUG2
	debugReg6 = dold(EV_ILE);
	msg_printf(VRB,"debugReg6 (ev_ile after setting)is 0x%x\n", debugReg6);
#endif

	set_sr(osr | SR_IBIT6 | SR_IE);  /* enable R4000 IP6 (intr pending)*/

        /* see if there are any CC level 3's pending */
        tmp = get_cause();
#if DEBUG || DEBUG2
	debugReg41 |= tmp;
	debugReg51 = dold(EV_ERTOIP);
	msg_printf(VRB,"debugReg41 (cause reg before) is 0x%x\n", debugReg41);
	msg_printf(VRB,"debugReg51 (ertoip before) is 0x%x\n", debugReg51);
#endif
        if (tmp & SR_IBIT_ERR) {
                tmp = dold(EV_ERTOIP);
                /* clear whatever is currently pending */
                dosd(tmp, EV_CERTOIP);

                /* make sure R4K level 6 is now clear -- XXX delay? */
                tmp = get_cause();
                if (tmp & SR_IBIT_ERR) {
			fail |= ERR_SR_IBITCLR;
#if DEBUG || DEBUG2
			err1 = 1;
#endif
		}
        }


	/* Can't force 1 bit err due to chip inaccessiblity for now */
	/*forcememerr(spnum, WORKLINE,           0x1, 1);  */

	/* force 2 bit err */
        forcememerr(spnum, WORKLINE + ALINE,   0x1, 2, &fail); 

 	/* disable CC level 3 intrs */
	dosd(dold(EV_ILE) & ~EV_ERTOINT_MASK, EV_ILE);

        set_sr(osr);    /* restore old setting of Status register */
#if DEBUG || DEBUG2
	msg_printf(VRB,"fail code is 0x%x\n", fail);
#endif
	if (fail != 0x0) {
		if (fail & ERR_SR_IBITCLR) 
			 err_msg(FECC_IBIT);
			/* printf("Can't clear level 6 IP\n"); */
		if (fail & ERR_CACHEWR) 
			 err_msg(FECC_CACHEWR);
			/* printf("Can't write new cached data\n"); */
		if (fail & ERR_SET_ECC) 
			 err_msg(FECC_SET_ECC);
		/* printf("Can't set ECC register to be bad\n"); */
		if (fail & ERR_SR_NOIP6ERR)
			 err_msg(FECC_NOIP6ERR);
			/* printf("IP6 error not passed to cpu\n"); */
		if (fail & ERR_ERTOIP)
			 err_msg(FECC_ERTOIP);
			/* printf("Double bit error not registered\n"); */
		if (fail & ERR_SR_IBITOFF)
			 err_msg(FECC_IBITOFF);
	  /* printf("Can't turn level 6 IP off after writing\n");*/
		if (fail & ERR_CERTOIP)
			 err_msg(FECC_CERTOIP, &slot);
		/* printf("Can't clear error in ERTOIP register\n"); */
	}
	else
		msg_printf(SUM, "Force ECC errors test -- PASSED\n");
			/* printf("Force ECC errors test -- PASSED\n");*/
}


/*
 * Force a single or double bit error to generate a CC level 3 intr
 */
int
forcememerr(int spnum, uint k0addr, uint data, uint singleordouble, uint *fail)
{
        register uint tmp, tmp2;
        register uint old_data;

        /*
         * XXX run this stretch of code uncached or make sure this code
         *     does not fill the cache line for k0addr!
         */

#if DEBUG || DEBUG2
	if (singleordouble ==1)
		oneOrtwo = 1;
	else if (singleordouble == 2)
		oneOrtwo = 2;
	msg_printf(VRB,"single or double is %d\n", oneOrtwo);
#endif
	old_data = dold(k0addr);	
	/* write new and different data */
	if (old_data == data) {
		data ^= 1;
#if DEBUG || DEBUG2	
		debugReg |= 0x1;
#endif
	}

        /* dirty a cache line -- (write to) a memory location, cached */
        /*dosd(data, k0addr);*/
        *(uint*)k0addr = data;

	tmp = dold(k0addr);
	if (old_data == tmp) {
		*fail |= ERR_CACHEWR;
	}
#if DEBUG || DEBUG2
	debugReg2 |= tmp;
	debugReg3 |= old_data;
	if (tmp != data) {
		debugReg |= 0x2;
	}
	if (tmp &= data) {
		debugReg |= 0x4;
	}
	msg_printf(VRB,"debugReg is 0x%x\n", debugReg);
	msg_printf(VRB,"debugReg2 is 0x%x\n", debugReg2);
	msg_printf(VRB,"debugReg3 is 0x%x\n", debugReg3);
#endif

        /* set C0_ECC for this cache line -- 2ndary index load tag */
        sd_ilt(k0addr);

        /* force bad ecc value in C0_ECC */
        if (singleordouble == 1)
                set_ecc(get_ecc() ^ 1); /* this generates a single bit ECC */
                /*set_ecc(get_ecc() ^ 3);/* this generates ??? */
        else if (singleordouble == 2)
                set_ecc(~(get_ecc() ^ 1)); /* this generates a double bit ECC*/

	tmp = get_ecc();
	if (tmp == 0x0) {
		*fail |= ERR_SET_ECC;
	}

        /* CE bit must be on for CACHE op to use C0_ECC */
        set_sr(get_sr() | SR_CE);
        /* write the line back from pdcache to scache */
        pd_hwbinv(k0addr);

        /* turn off CE */
        set_sr(get_sr() & ~SR_CE);

/* XXX  instead of the below, kick another processor which will
 *      read k0addr causing an intervention, causing bad data to hit
 *      the bus, causing CC_ET_ADDRERR in _both_ processors
 *
 *      or get a fixed fake mem which does drive ADDRERR
 */
        /* prevent ECC exceptions in the write back below */
        set_sr(get_sr() | SR_DE);

        /*
         * Force a write back of this line with bad ecc from scache to memory
         * and, hence, an interrupt at CC level 3.
         */
        /*tmp = dold(k0addr + AMEG);            /* scache size is 1 mb */
        *(uint*)(k0addr + AMEG) = tmp;

        /* reenable ECC exceptions */
        set_sr(get_sr() & ~SR_DE);

        get_cc_conf(EV_CPERIOD);      /* delay */


        /* inspect R4K Cause register for IP6 on */
        tmp = get_cause();
#if DEBUG || DEBUG2
	debugReg4 |= tmp;
	msg_printf(VRB,"debugReg4 (cause reg after) is 0x%x\n", debugReg4);
#endif
        if (!(tmp & SR_IBIT_ERR))  {
		*fail |= ERR_SR_NOIP6ERR;
	}

        /*
         * inspect EV ERTOIP for 1) ECC double and data error on ebus,
         * or 2) ECC single.
         */
        tmp = dold(EV_ERTOIP);
#if DEBUG || DEBUG2
	debugReg5 |= tmp;
	msg_printf(VRB,"debugReg5 (ertoip after )is 0x%x\n", debugReg5);
#endif
        if ((singleordouble == 1 && tmp != CC_ERROR_SCACHE_SBE) ||
            (singleordouble == 2 && tmp !=
		 (CC_ERROR_SCACHE_MBE|CC_ERROR_MY_ADDR)))  {
		*fail |= ERR_ERTOIP;
	}

        dosd(tmp, EV_CERTOIP);          /* clear these pups */

        get_cc_conf(EV_CPERIOD); /* no delay doesn't work... */

	/* verify R4K cause for IP6 off */
        tmp = get_cause();
        if (tmp & SR_IBIT_ERR)  {
		*fail |= ERR_SR_IBITOFF;
	}

        tmp = dold(EV_ERTOIP);          /* these should all be off, too */
        if (tmp)  {
		*fail |= ERR_CERTOIP;
	}
}
