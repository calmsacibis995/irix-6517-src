/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       mgv_tmi_bus.c
*/

#ifdef IP30

#include "mgv_diag.h"

/*
 * Forward Function References
 */
__uint32_t 	mgv_TMIRegPatrnTest (void);
__uint32_t 	mgv_TMIDataBusTest (void);

/*
 * NAME: mgv_TMIRegPatrnTest
 *
 * FUNCTION: TMI Register Pattern Test
 *
 * INPUTS: none
 *
 * OUTPUTS: -1 if error, 0 if no error
 *           Writes specific data patterns to the Indirect Registers
 */
__uint32_t
mgv_TMIRegPatrnTest (void)
{
/*
 * XXX: Add all TMI registers that are read/write
 */
    mgv_tmi_rwregs	regInfo[] = {
	"TMI Left Start LS", TMI_LEFT_START_LS,
	"TMI Horz_in LS", TMI_HIN_LENGTH_LS,
	"TMI VIN Delay LS", TMI_VIN_DELAY_LS,
	"TMI VOUT Delay LS", TMI_VOUT_DELAY_LS,
	"TMI Pix Count Mid", TMI_PIX_COUNT_MID,
	"TMI Pix Count Low", TMI_PIX_COUNT_LOW,
	"TMI Hout Length LS", TMI_HOUT_LENGTH_LS,
        "",                                            -1    
    };
    mgv_tmi_rwregs	*pRegInfo = regInfo;
    uchar_t		exp, rcv,mask;
    uchar_t		pat, errors=0;

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[TMI_REG_PATTERN_TEST].TestStr);

    while (pRegInfo->offset != -1) {

	    switch (pRegInfo->offset) {
		case TMI_VIN_DELAY_LS: case TMI_VOUT_DELAY_LS: case TMI_PIX_COUNT_MID: 
		     mask = 0xff;
		     break;
		default :
		     mask = 0xfe;
		     break;
	    }

    	for (pat = 0; pat < sizeof(mgv_patrn8)/sizeof(uchar_t); pat++) {

	    exp = mgv_patrn8[pat] & mask;

	    msg_printf(DBG, "pat 0x%x; \n",
		mgv_patrn8[pat]);

	    TMI_IND_W1(mgvbase,pRegInfo->offset,mgv_patrn8[pat]);

	    TMI_IND_R1(mgvbase, pRegInfo->offset, rcv);

	    msg_printf(DBG, "%s exp 0x%llx rcv 0x%llx\n", pRegInfo->str, exp, rcv);

	    if (exp != rcv) {
		errors++;
		msg_printf(ERR, "mgv_TMIRegPatrnTest %s exp 0x%llx rcv 0x%llx\n",
			pRegInfo->str, exp, rcv);
		msg_printf(DBG, "mgv_TMIRegPatrnTest %s offset 0x%x\n",
			pRegInfo->str, pRegInfo->offset);
	    }
	}
	pRegInfo++;
    }

    return (_mgv_reportPassOrFail((&mgv_err[TMI_REG_PATTERN_TEST]) ,errors));
}

/*
 * NAME: mgv_TMIDataBusTest
 *
 * FUNCTION: TMI Register Walking Bit Test for Indirect Data Registers
 *
 * INPUTS: none
 *
 * OUTPUTS: -1 if error, 0 if no error
 */
__uint32_t
mgv_TMIDataBusTest (void)
{
/*
 * XXX: Add all TMI registers that are read/write
 */
    mgv_tmi_rwregs	regInfo[] = {
	"TMI Left Start LS", TMI_LEFT_START_LS,
	"TMI Horz_in LS", TMI_HIN_LENGTH_LS,
	"TMI VIN Delay LS", TMI_VIN_DELAY_LS,
	"TMI VOUT Delay LS", TMI_VOUT_DELAY_LS,
	"TMI Pix Count Mid", TMI_PIX_COUNT_MID,
	"TMI Pix Count Low", TMI_PIX_COUNT_LOW,
	"TMI Hout Len LS", TMI_HOUT_LENGTH_LS,
        "",                                            -1    
    };

    mgv_tmi_rwregs	*pRegInfo = regInfo;
    uchar_t		exp, rcv, mask;
    uchar_t		pat, i, j, errors=0;

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[TMI_REG_DATA_BUS_TEST].TestStr);

    while (pRegInfo->offset != -1) {
	    switch (pRegInfo->offset) {
		case TMI_VIN_DELAY_LS: case TMI_VOUT_DELAY_LS: case TMI_PIX_COUNT_MID: 
		     mask = 0xff;
		     break;
		default :
		     mask = 0xfe;
		     break;
	    }

	for (i = 0; i < 8; i++) {
	    pat = (~0x0) & (1 << i);

	    for (j = 0; j <= 1; j++) {
		if (j) {
		    pat = ~pat; /* get the walk 1 pattern */
		} 

	    	/* compute the expected value from the pattern and mask */
	    	exp = pat & mask; ;

	    	TMI_IND_W1(mgvbase, pRegInfo->offset, pat);

	    	TMI_IND_R1(mgvbase, pRegInfo->offset, rcv);

		msg_printf(DBG, "mgv_TMIRegDataBusTest %s exp 0x%llx rcv 0x%llx\n",
			pRegInfo->str, exp, rcv);

	    	if (exp != rcv) {
		   errors++;
		   msg_printf(ERR, "mgv_TMIRegDataBusTest %s exp 0x%llx rcv 0x%llx\n",
			pRegInfo->str, exp, rcv);
		   msg_printf(DBG, "mgv_TMIRegDataBusTest %s offset 0x%x\n",
			pRegInfo->str, pRegInfo->offset);
	    	}
	    }
	}
	pRegInfo++;
    }

    return (_mgv_reportPassOrFail((&mgv_err[TMI_REG_DATA_BUS_TEST]) ,errors));
}

#endif
