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
**      FileName:       mgv_displaybus.c
**      $Revision: 1.1 $
*/

#include "sys/types.h"
#include "sys/sbd.h"
#include "sys/sema.h"
#include "sys/param.h"
#include "ide_msg.h"
#include "uif.h"
#include "sys/mgrashw.h"
#include "sys/mgras.h"
#include "mgv1_regio.h"

/*
** External Variables and Function Declarations
*/

extern  mgras_hw *mgbase;

/*
** Local Variables and Function Declarations
*/

static struct ev1_info info;

#define         RED             0x0
#define         GREEN           0x8
#define         BLUE            0x10

#define DISPLAY_BUS_WIDE 8

/*
** NAME: ab1_disp_bus
**
** FUNCTION: This routine tests the DCBus to the AB1's.
**           The AB1's are tested by using a walking 1 pattern
**           on the LSB's of the sys control register and walking
**           1 on a memory loaction.
**           The CC1 is tested by using a walking 1 pattern on the 
**           address regsiter.
**
** INPUTS: none
**
** OUTPUTS: errCnt -> 0 or more errors
*/
int
ab1_disp_bus(int argc, char *argv[])
{
	int channel, index, exp, rcv, errCnt;
	char rgb[10];
	char err_cd[10];

	msg_printf(SUM, "Display control bus AB1 internal reg test\n");
	errCnt = 0;

	/* Freeze the g2v transfer */
	mgv1_freeze_all_ABs();

	/*
	** Use a walking one pattern on the 5 lsb's of
	** the DCBus.
	*/
	for (index = 0; index < 5; index++) {
		exp = (1 << index);

		msg_printf(VRB, "DCbus AB1: Writing sysctl with walking one data 0x%x\n", exp);

		/* Write the system control register */
		AB_W1(mgbase, sysctl, exp);

		/* Wait, read back and verify */
		DELAY(2);
		rcv = AB_R1(mgbase, sysctl);

		/*
		** If received value is incorrect,
		** print error message and increment error
		** counter.
		*/
		if (exp != rcv) {
			msg_printf(ERR, "*** DCbus AB1 Register test failed\n");
                        msg_printf(ERR, "*** Error Code: AB1_4_00\n");          
                        msg_printf(INFO, "\tExpected 0x%x, Actual 0x%x\n", 
							exp, rcv);          
                        msg_printf(INFO, "\tFailed bits 0x%x\n", exp ^ rcv);
		}
	}

	/* Walking One test across the display bus (8 bits wide ) */
	channel = RED;
	sprintf(rgb, "RED CHANNEL"); 
	sprintf(err_cd, "AB1_0_01"); 
	while (channel < 0x18) {

		/* Select the desired AB1 channel, also diagnostic mode */
		AB_W1(mgbase, sysctl, channel);

		/*
		** Perform a walking one test on the DCBus for each AB1
		** using the DRAM as the destination.
		*/
		for (index = 0; index < DISPLAY_BUS_WIDE; index++) {

			/* Setup the data to write */
			exp = (1 << index);
	
			msg_printf(VRB, "DCbus %s AB1: Writing DRAM with walking one data 0x%x\n", rgb, exp);
						
			/* Write the DRAM location */
			AB_W1(mgbase, high_addr, 0);
			AB_W1(mgbase, low_addr, 0);
			AB_W1(mgbase, dRam, exp);
	
			/* Wait, Read Back and verify */
			DELAY(1);
			AB_W1(mgbase, high_addr, 0);
			AB_W1(mgbase, low_addr, 0);
			rcv = AB_R1(mgbase, dRam) ;

			/*
			** If the recieved value is incorrect,
			** print a message and increment the error count
			*/
			if (exp!= rcv){
				msg_printf(ERR, "*** DCbus AB1 Register test failed\n");
				msg_printf(ERR, "*** Error Code: %s\n", err_cd);          
				msg_printf(INFO, "\tExpected 0x%x, Actual 0x%x\n", 
								exp, rcv);          
				msg_printf(INFO, "\tFailed bits 0x%x\n", 
								exp ^ rcv);
			}
		}

		/* Switch to the next channel and test it */
		channel += 0x8;
		if (channel == GREEN) {
			sprintf(rgb, "GREEN CHANNEL"); 
			sprintf(err_cd, "AB1_1_01"); 
		} else if (channel == BLUE) {
			sprintf(rgb, "BLUE CHANNEL"); 
			sprintf(err_cd, "AB1_2_01"); 
		}
	}
	return(errCnt);
}

/*
** NAME: cc1_disp_bus
**
** FUNCTION: This routine tests the DCBus to the CC1.
**           The CC1 is tested by using a walking 1 pattern on the 
**           address regsiter.
**
** INPUTS: none
**
** OUTPUTS: errCnt -> 0 or more errors
*/
int
cc1_disp_bus(int argc, char *argv[])
{
        int channel, index, exp, rcv, errCnt;

	msg_printf(SUM, "Display control bus CC1 internal reg test\n");
	errCnt = 0;

	/*
	** Use a walking one pattern to test the 
	** DCBus to the CC1
	** Use the address register as the destination
	*/
	for (index = 0; index < 8; index++) {

		/* Setup the data */
		exp = (1 << index);

		msg_printf(VRB, "DCbus CC1: Writing address reg with walking one data 0x%x\n", exp);

		/* Write the register */
		CC_W1(mgbase, indrct_addreg, exp);

		/* Read the register back */
		rcv = CC_R1(mgbase, indrct_addreg) ;

		/*
		** If the recieved value is incorrect,
		** print a message and increment the error count
		*/
		if (exp != rcv) {
			msg_printf(ERR, "*** DCbus CC1 Register test failed\n");
			msg_printf(ERR, "*** Error Code: CC1_0_00\n");
			msg_printf(INFO, "\tExpected 0x%x, Actual 0x%x\n", 
							exp, rcv);          
			msg_printf(INFO, "\tFailed bits 0x%x\n", 
							exp ^ rcv);
		}
	}

	return(errCnt);
}

/*
** NAME: cc1_get_revision
**
** FUNCTION: This routine fetches the revision register
**           contents of the CC1 ASIC and places it in the
**           revision struct.
**
** INPUTS: none
**
** OUTPUTS: none
*/
int
cc1_get_revision(void)
{
        /* Read the version register and store it away */
        CC_W1(mgbase, indrct_addreg, 128);
        info.CC1Rev = CC_R1(mgbase, indrct_datareg);
	msg_printf(SUM, "CC1Rev = %d\n", info.CC1Rev);
	return(0);
}

/*
** NAME: ab1_get_revision
**
** FUNCTION: This routine fetches the revision register
**           contents of each AB1 ASIC and places them in the
**           revision struct.
**
** INPUTS: none
**
** OUTPUTS: none
*/
int
ab1_get_revision(void)
{

        /* Select the Red AB1 channel */
        AB_W1(mgbase, sysctl, RED);

        /* Get AB1 rev. */
        AB_W1(mgbase, high_addr, 0);
        AB_W1(mgbase, low_addr, 3);
        info.AB1RRev = AB_R1(mgbase, tst_reg) & 0xff;
	msg_printf(SUM, "AB1 Red Rev = %d\n", info.AB1RRev);

        /* Select the Green AB1 channel */
        AB_W1(mgbase, sysctl, GREEN);

        /* Get AB1 rev. */
        AB_W1(mgbase, high_addr, 0);
        AB_W1(mgbase, low_addr, 3);
        info.AB1GRev = AB_R1(mgbase, tst_reg) & 0xff;
	msg_printf(SUM, "AB1 Green Rev = %d\n", info.AB1GRev);

        /* Select the Blue AB1 channel */
        AB_W1(mgbase, sysctl, BLUE);

        /* Get AB1 rev. */
        AB_W1(mgbase, high_addr, 0);
        AB_W1(mgbase, low_addr, 3);
        info.AB1BRev = AB_R1(mgbase, tst_reg) & 0xff;
	msg_printf(SUM, "AB1 Blue Rev = %d\n", info.AB1BRev);

	return(0);
}

