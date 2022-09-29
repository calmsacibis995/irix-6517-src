/**************************************************************************
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * mgras power-on graphics test
 */

#if IP30
#include "sys/immu.h"
#include "sys/cpu.h"
#include "ide_msg.h"
#include "uif.h"
#include "libsc.h"
#include "sys/RACER/racermp.h"
#include "sys/fpanel.h"
#endif

#include "mgras_diag.h"		/* for mgras_baseindex prototype */
#include "sys/mgrashw.h"
#include "sys/mgras.h"

int
mgras_pon(struct mgras_hw *base) 
{
	mgras_info myinfo; 
	int board = mgras_baseindex(base);

	if (!MgrasProbe(base, board)) {
	    return (-1);
	}
	if (MgrasGatherInfo(base, &myinfo)) {
	    return (-1);
	}

	if (MgrasInit(base, &myinfo)) {
	    return (-1);
	}
#if IP30
	/* Cover cooling constraints of 2GE board, and keep track of
	 * additional loads for fan speed issues.
	 */
	if (mgras_i2cProbe(base))		/* at least presenter card? */
		MPCONF->fanloads++;

	if (myinfo.NumGEs == 2) {
		MPCONF->fanloads++;		/* takes 2 slots */
	}
#endif
	return 0;
}

#if IP30
int
mgras_pon_chkcfg(struct mgras_hw *base)
{
	mgras_info myinfo;
	if (MgrasGatherInfo(base, &myinfo) < 0)
		return (-1);

	if (myinfo.NumGEs == 2 && base == (struct mgras_hw *)K1_MAIN_WIDGET(0x9))
		return (-1);

	return 0;
}
#endif
