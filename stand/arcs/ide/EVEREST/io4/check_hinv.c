/*
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

/**********************************************************************
*	File:		check_hinv.c					*
*									*
*       Check the system configuration					*
*									*
***********************************************************************/

#ident "arcs/ide/EVEREST/io/io4/check_hinv.c $Revision: 1.4 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/addrs.h>
#include <uif.h>
#include <ide_msg.h>
#include <prototypes.h>


int
check_hinv(int argc, char **argv)
{
    int slot, retval, msglev;
    char *nptr;

    retval = 0;
    msg_printf(SUM, "\ncheck_hinv - checking system hardware configuration\n");
    if (argc > 1) {
	msg_printf(ERR, "check_hinv - no command line options needed, (%s)\n",
		argv[argc-1]);
    }

    /*
     * iterate through all io adapters in the system
     */
    for (slot = EV_MAX_SLOTS - 1; slot >= 0; slot--) {
	msglev = SUM;
	switch (EVCFGINFO->ecfg_board[slot].eb_type) {

	    case EVTYPE_WEIRDCPU :
		nptr = "Unknown CPU Type";
		msglev = ERR;
		retval = 1;
		break;

	    case EVTYPE_IP19 :
		nptr = "IP19";
		break;

	    case EVTYPE_IP21 :
		nptr = "IP21";
		break;

	    case EVTYPE_IP25 :
		nptr = "IP25";
		break;

	    case EVTYPE_WEIRDIO :
		nptr = "Unknown IO Type";
		msglev = ERR;
		retval = 1;
		break;

	    case EVTYPE_IO4 :
		nptr = "IO4";
		break;

	    case EVTYPE_IO5 :
		nptr = "IO5";
		break;

	    case EVTYPE_WEIRDMEM :
		nptr = "Unknown Memory Type";
		msglev = ERR;
		retval = 1;
		break;

	    case EVTYPE_MC3 :
		nptr = "MC3";
		break;

	    case EVTYPE_NONE :
		nptr = "No Board Installed";
		msglev = DBG;
		break;

	    default :
		nptr = "Totally Unknown Board Type";
		msglev = ERR;
		retval = 1;
	}

	msg_printf (msglev, "  Slot 0x%x has %s\n", slot, nptr);
    }

    return (retval);
}
