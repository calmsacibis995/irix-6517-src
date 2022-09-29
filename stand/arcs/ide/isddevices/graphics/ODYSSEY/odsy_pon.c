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
 * odyssey power-on graphics test
 */

#include "odsy_internals.h"
int odsyProbe(int board);

int
odsy_pon(struct odsy_hw *base) 
{
	int board = odsyPipeNrFromBase(base);

	if (board == -1)
	    return(-1);
	if (!odsyProbe(board)) {
	    return(-1);
	}
	/*
	 * if the board has been found via the odsyProbe()
	 * then its has already been initialized.
	 * run the diags we think necessary to support
	 * textport and return result
	 */

#if XXX_NOTYET
	/* Cover cooling constraints of 2GE board, and keep track of
	 * additional loads for fan speed issues.
	 * buzz fanloads, flatpanel or dual chan card may boost cooling needs
	  MPCONF->fanloads++
	 */
#endif
	return(0);
}

int
odsy_pon_chkcfg(struct odsy_hw *base)
{
	int board = odsyPipeNrFromBase(base);

	if ( !(OdsyBoards[board].info_found))
	    odsyGatherInfo(board);
#if XXX_NOTYET
	based of the Info gathered about the board check the config
		return(-1);
#endif

	return(0);
}
