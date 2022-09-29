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
**      FileName:       mgv_gal15_dcb.c
*/

#include "mgv_diag.h"

/*
 * Forward Function References
 */
__uint32_t	mgv_CC1Dcb(void);
__uint32_t	mgv_AB1Dcb(void);
__uint32_t	mgv_GPITrig(__uint32_t argc, char **argv);

/*
 * NAME: mgv_CC1Dcb
 *
 * FUNCTION: This routine tests the DCBus to the CC1.
 *	     The CC1 is tested by using a walking 1 pattern on the
 *	     address regsiter.
 *
 * INPUTS: none
 *
 * OUTPUTS: errors
 */
__uint32_t
mgv_CC1Dcb(void)
{
    __uint32_t errors = 0;
    __uint32_t index, exp, rcv ;

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[CC1_DCB_TEST].TestStr);

    _mgv_CC1TestSetup();

    /*
     * Use a walking one pattern to test the DCBus to the CC1
     * Use the address register as the destination
     */
    for (index = 0; index < 8; index++) {
	/* Setup the data */
	exp = (1 << index);

	/* Write the register */
	CC_W1(mgvbase, CC_INDIR1, exp);

	/* Read the register back */
	CC_R1(mgvbase, CC_INDIR1, rcv);

	/*
	 * If the recieved value is incorrect, print a message and
	 * increment the error count
	 */
	if (exp != rcv) {
	    errors++;
	    msg_printf(ERR, "Expected 0x%x; Actual 0x%x\n", exp, rcv);
	    goto __end_mgv_CC1Dcb;
	}
    }

__end_mgv_CC1Dcb:
    return(_mgv_reportPassOrFail((&mgv_err[CC1_DCB_TEST]) ,errors));
}

/*
 * NAME: mgv_AB1Dcb
 *
 * FUNCTION: This routine tests the DCBus to the AB1.
 *	     The AB1 is tested by using a walking 1 pattern on the
 *	     address regsiter.
 *
 * INPUTS: none
 *
 * OUTPUTS: errors
 */
__uint32_t
mgv_AB1Dcb(void)
{
    __uint32_t 	errors = 0;
    __uint32_t 	i, channel, index, exp, rcv ;
    char	chanStr[50];

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[AB1_DCB_TEST].TestStr);

    /*
     * freeze the transfer of data from graphics to video for all 3 windows
     */
    AB_W1(mgvbase, AB_HIGH_ADDR, 0);
    AB_W1(mgvbase, AB_LOW_ADDR, 6);
    AB_W1(mgvbase, AB_INT_REG, 0x3);
    AB_W1(mgvbase, AB_LOW_ADDR, 0x16);
    AB_W1(mgvbase, AB_INT_REG, 0x3);
    AB_W1(mgvbase, AB_LOW_ADDR, 0x26);
    AB_W1(mgvbase, AB_INT_REG, 0x1);

    /*
     * Use a walking one pattern on the 5 lsb's of the DCBus.
     */
    for (index = 0; index < 5; index++) {
	exp = (1 << index);

	/* Write the system control register */
	AB_W1(mgvbase, AB_SYSCON, exp);

	/* Wait, read back and verify */
	us_delay(2);
	AB_R1(mgvbase, AB_SYSCON, rcv);

	/*
	 * If the recieved value is incorrect, print a message and
	 * increment the error count
	 */
	if (exp != rcv) {
	    errors++;
	    msg_printf(ERR, "Expected 0x%x; Actual 0x%x\n", exp, rcv);
	    goto __end_mgv_AB1Dcb;
	}
    }

    for (i = 0; i < 4; i++) {
	switch (i) {
	  case 0:
		sprintf(chanStr, "Red Channel");
		channel = AB1_RED_CHAN;
		break;
	  case 1:
		sprintf(chanStr, "Green Channel");
		channel = AB1_GRN_CHAN;
		break;
	  case 2:
		sprintf(chanStr, "Blue Channel");
		channel = AB1_BLU_CHAN;
		break;
	  case 3:
		sprintf(chanStr, "Alpha Channel");
		channel = AB1_ALP_CHAN;
		break;
	}

	AB_W1(mgvbase, AB_SYSCON, channel);

    	/*
     	 * Use a walking one pattern to test the DCBus to the AB1
     	 * Use the address register as the destination
     	*/
    	for (index = 0; index < 8; index++) {
	    /* Setup the data */
	    exp = (1 << index);

	    /* Write the DRAM location */
	    AB_W1(mgvbase, AB_HIGH_ADDR, 0);
	    AB_W1(mgvbase, AB_LOW_ADDR, 0);
	    AB_W1(mgvbase, AB_EXT_MEM, exp);

	    /* Wait, Read Back and verify */
	    us_delay(1);
	    AB_W1(mgvbase, AB_HIGH_ADDR, 0);
	    AB_W1(mgvbase, AB_LOW_ADDR, 0);
	    AB_R1(mgvbase, AB_EXT_MEM, rcv) ;

	    /*
	     * If the recieved value is incorrect, print a message and
	     * increment the error count
	     */
	    if (exp != rcv) {
	        errors++;
		msg_printf(ERR, "AB1 Dram %s Dcb Test Failed\n", chanStr);
	        msg_printf(ERR, "Expected 0x%x; Actual 0x%x\n", exp, rcv);
	        goto __end_mgv_AB1Dcb;
	    }
	}
    }

__end_mgv_AB1Dcb:
    return(_mgv_reportPassOrFail((&mgv_err[AB1_DCB_TEST]) ,errors));
}

__uint32_t
mgv_GPITrig(__uint32_t argc, char **argv)
{
    __uint32_t  errors = 0;
    __uint32_t  badarg = 0;
    __uint32_t	polarity = -1;
    __uint32_t	timeOut = VGI1_POLL_TIME_OUT;
    __uint32_t	latched;
    char	onStr[10] = "ON";
    char	offStr[10] = "OFF";
    char	*pSwitch;

    /* get the args */
    argc--; argv++;
    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
                case 'p':
			if (argv[0][2]=='\0') {
			    atobu(&argv[1][0], &polarity);
			    argc--; argv++;
			} else {
			    atobu(&argv[0][2], &polarity);
			}
                        break;
                default: badarg++; break;
        }
        argc--; argv++;
    }

    if (badarg || argc || ( (polarity != 0) && (polarity != 1) )) {
	msg_printf(SUM, "\
	Usage: mgv_gpitrig -p [0|1]\
	-p --> polarity 0-falling edge 1-rising edge ");
	return(-1);
    }

    msg_printf(SUM, "IMPV:- %s\n", mgv_err[MGV_GPI_TRIGGER_TEST].TestStr); 

    GAL_IND_W1 (mgvbase, GAL_TRIG_CTRL, polarity);
    if (polarity) {
	pSwitch = &(offStr[0]);
    } else {
	pSwitch = &(onStr[0]);
    }

    msg_printf(SUM, "Turn GPI Trigger %s and Press <Space> When Done...", 
	pSwitch);

    if ( (pause(60, " ", " ")) != 2) {
	msg_printf(SUM, "\n");
	/* Space Bar is pressed - do the test */
	latched = 0x0;
	do {
	    GAL_IND_R1(mgvbase, GAL_TRIG_STATUS, latched);
	    latched &= 0x1;
	    timeOut--;
	} while (timeOut && !latched);
	if (!timeOut) {
	    msg_printf(ERR, "IMPV:- GPI trigger not latched...\n");
	    errors++;
	}
    } else {
	msg_printf(SUM, "IMPV:- GPI trigger Timed Out\n");
    }

    return(_mgv_reportPassOrFail((&mgv_err[MGV_GPI_TRIGGER_TEST]), errors));
}


