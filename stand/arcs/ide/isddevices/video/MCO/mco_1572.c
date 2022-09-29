/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  VC2 Diagnostics.
 *	SRAM Diagnostics :- 
 *	 	Full Memory Pattern Test.
 *	       	Walking Ones & Zeros :- Address Bus Test.
 *		Address Uniqueness Test.
 *		Walking Ones & Zeros :- Data Bus Test.
 *	CURSOR Tests :-
 *		Blackout Display Test.
 *		Cursor Update Test.
 *		VC2 Reset & Check.
 *		Cursor Mode Test.
 *
 *  $Revision: 1.1 $
 */


#ifdef IP30
#include "ide_msg.h"
#include "sys/mgrashw.h"
#include "mco_diag.h"

extern mgras_hw *mgbase;
extern void sr_mco_set_dcbctrl(mgras_hw *base, int crs);
extern unsigned char ICS1572_504000[56];    /* defined in mco_diag_timing.c */
extern unsigned char ICS1572_756000[56];    /* defined in mco_diag_timing.c */

#define NUMBER_OF_BITS 56

/* Forward References */
int _mco_setpllICS(int);

int
mco_SetPLLICS(int argc, char **argv)
{
    int errors = 0;
    int freq = 756000;
    char tmpstr[20];

    if (argc < 2) {
	msg_printf(VRB, "Usage: %s FREQ \t(in MHz)\n", argv[0]);
        msg_printf(VRB, "       Supported FREQ values are: 75.6, 50.4\n");
	return (1);
    }
    else {

	sprintf(tmpstr,"%s", argv[1]);
	msg_printf(DBG,"argv[1] = %s\n", tmpstr);
	if (!strcasecmp(tmpstr, "75.6") ) {
	    msg_printf(VRB, "%s: ", argv[0]);
	    freq = 756000;
	}
	else if (!strcasecmp(tmpstr, "50.4")) {
	    msg_printf(VRB, "%s: ", argv[0]);
	    freq = 504000;
	}
	else {
	    msg_printf(VRB, "Error: Invalid argument to %s\n", argv[0]);
	    msg_printf(VRB, "Usage: %s FREQ \t(in MHz)\n", argv[0]);
	    msg_printf(VRB, "       Supported FREQ values are: 75.6, 50.4\n");
	    return(1);
	}
    }

    errors = _mco_setpllICS(freq);
    return (errors);
}

int
_mco_setpllICS(int freq) {
    unsigned char *freqdata_ptr;
    int Count;

    switch (freq) {
	case 756000:    /* 75.6MHz */
	    msg_printf(VRB, "Initializing ICS1572 Clock to 75.6 MHz\n");
	    freqdata_ptr = &ICS1572_756000[0];
	    break;
	case 504000:    /* 50.4MHz */
	    msg_printf(VRB, "Initializing ICS1572 Clock to 50.4 MHz\n");
	    freqdata_ptr = &ICS1572_504000[0];
	    break;
	default:        /* 75.6MHz */
	    freqdata_ptr = &ICS1572_756000[0];
	    msg_printf(VRB,"mco_setpllICS: unknown frequency %d\n");
	    msg_printf(VRB,"\t\tdefaulting to 756000 (75.6 MHz)\n");
    }

    /* Now write out the data to program the ICS1572.
     * Unfortunately, there is no way to readback the contents of
     * the ICS1572, so we just have to hope the data gets written
     * correctly.....
     */

    sr_mco_set_dcbctrl(mgbase, MCO_INDEX);
    MCO_SETINDEX(mgbase, ICS_PLL, 0);

    sr_mco_set_dcbctrl(mgbase, MCO_DATAIO);

    for( Count = 0; Count < NUMBER_OF_BITS; Count++ ) {
	msg_printf(DBG,"%d:\tWriting to ICS_PLL: 0x%x\n", Count, *freqdata_ptr);
	MCO_IO_WRITE(mgbase, *freqdata_ptr++);
    }

    return(0);
}
#endif /* IP30 */
