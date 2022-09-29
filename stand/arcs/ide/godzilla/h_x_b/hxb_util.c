/*
 * hxb_util.c   
 *
 *	utilities needed for hxb_ functions (system using Heart Xbow Bridge)
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "ide/godzilla/heart_bridge/hxb_util.c:  $Revision: 1.3 $"

/*
 * hxb_util.c -  utilities needed for hxb_ functions
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_prototypes.h"

/*
 * Forward References : in d_prototypes.h
 */

/* function name:	hxb_reset
 * input:		none XXX implement input flags just like in hb_reset
 * output:		resets the major registers in heart, crossbow and bridge
 * assumption:		the order does not matter: 1/heart 2/bridge 3/xbow
 */
bool_t
hxb_reset(void)
{
	msg_printf(DBG,"Calling hxb_reset...\n");

	/* Heart and Bridge */
	if (_hb_reset(RES_HEART, RES_BRIDGE)) return(1);

	/* Xbow */
	if (x_reset()) return(1);

	return(0);
}
