/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include "sys/gr2hw.h"
#include "ide_msg.h"
#include "sys/param.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern struct gr2_hw *base; 

/* Bit 
   7	SOFT.RESET	Not in reset mode = 0	Reset= 1
   6	VC1RESET	Reset = 0      		Not in reset mode = 1	    
   5    Expanded bckend	Installed = 0  		Not installed = 1 
   4    VIDEO.TR	Active = 0      Inactive = 1
   1-3 	SHIFTREG.SEL[0:2] 
   0	SGIMON		Active = 0      Inactive = 1
*/


#define  EXPANDEDCONFIG	0x40	/*Take VC1 out of reset, expanded backend*/
#define  PROGRAMMABLE 	0x04	/*Use programmable video clock*/
/*
 * wrconfig() - set configuration register
 *
 *	This is a tool to allow writing to the 
 *	HI1 configuration register.
 */
gr2_wrconfig(int argc, char **argv) 
{
    int val;

    val = EXPANDEDCONFIG;
    
    if (argc > 1)
    {
	argv++;
	atob((*argv), &val);
    }

    base->bdvers.rd1  = val & 0xff;
    msg_printf(DBG,"Writing board configuration register with %#08x\n", val);

    return 0;
}

void
gr2_delay(int argc, char **argv)
{
    int val;

    if (argc > 1)
    {
	argv++;
	atob((*argv), &val);
	DELAY(val);
    } else
    	msg_printf(ERR,"Need to give a delay arg\n");
}

int
gr2_videoclk(int argc, char **argv) 
{
    int val;

    val = PROGRAMMABLE;
    
    if (argc > 1)
    {
	argv++;
	atob((*argv), &val);
    } else{
	msg_printf(DBG,"Choose type of video clock: 0=107.352MHz crystal\n");
	msg_printf(DBG,"			    2=132Mhz crystal\n");	
	msg_printf(DBG,"			    4=Programmable, VB1 rev. 1\n");	
	return 0;
    }
    base->bdvers.rd0  = val & 0xff;
    msg_printf(DBG,"Writing video clock select register with %#08x\n", val);

    return 0;
}

#ifdef IP20
void
gr2_reset(void)
{

        /*
         * RESET the graphics board and STALL the ucode.
         */
        *(volatile unsigned int *)PHYS_TO_K1(CPU_CONFIG) &=
                ~CONFIG_GRRESET;
        flushbus();
        DELAY(10);
        *(volatile unsigned int *)PHYS_TO_K1(CPU_CONFIG) |=
                CONFIG_GRRESET;
        flushbus();
        return;
}
#else
#endif
char
gr2_get_test_suite()
{
    char *pvar;
    int  suite;

    pvar = getenv("OSLoader");
    if (pvar) {
        if (strcmp(pvar, "sash") == 0) {
                suite = 0;
        } else {
                suite = 1;
        }
    } else {
                suite = 1;
    }

    return(suite);
}
