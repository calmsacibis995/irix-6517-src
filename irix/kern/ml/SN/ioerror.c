/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: ioerror.c
 *	All SN0 specific io error routines.
 */


#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>

#include "sn_private.h"
#include "error_private.h"
#include <sys/SN/error.h>


#if 0
/*
 * Function	: bridge_error_init
 * Parameters	: wp -> the widget vertex on the iograph
 * Purpose	: to setup any error initialization for the bridge. This 
 *			function needs to be called on a widget initialization.
 * Assumptions	: 
 * Returns	: 0 on successful initialization, -1 otherwise.
 */

/*ARGSUSED*/
int 
bridge_error_init(v_widget_t *wp)
{
	printf("bridge_error_init, fixme: setup error interrupts\n");

	return 0;

}

/*
 * Function	: xbow_error_init
 * Parameters	: xp -> the xbow vertex on the iograph
 * Purpose	: to setup any error initialization for the xbow. This 
 *			function needs to be called on a xbow initialization.
 * Assumptions	: Called once per xbow.
 * Returns	: 0 on successful initialization, -1 otherwise.
 */


/*ARGSUSED*/
int  
xbow_error_init(v_xbow_t *xp)
{

	printf("xbow_error_init: fixme, setup error interrupts\n");
	return 0;
}



/*
 * Function	: widget_error_init
 * Parameters	: wp -> the widget vertex on the iograph
 * Purpose	: to setup any error initialization for the widget. This 
 *			function needs to be called on a widget initialization.
 * Assumptions	: Called once per widget.
 * Returns	: 0 on successful initialization, -1 otherwise.
 */

int
widget_error_init(v_widget_t *wp)
{
	switch (WIDGET_TYPE(wp)) {
	case BRIDGE_WIDGET_PART_NUM:
		return bridge_error_init(wp);

	default:
		cmn_err(CE_WARN, "widget_error_init: w_id 0x%x on nasid 0x%x, unknown (part num 0x%x)",
			WID_ID(wp),
			WIDGET_NASID_NODEID(wp),
			WIDGET_TYPE(wp));
		return 0;	/* Dont return failure. */
	}
}


#endif /* 0 */
