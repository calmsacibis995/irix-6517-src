/*
 *
 * Copyright 1991,1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.7 $"

#include "sys/types.h"
#include "menu.h"
#include "coher.h"
#include "prototypes.h"


int scacherrx= YES;
/*
int icache_size;
int icache_linesize;
int dcache_size;
int dcache_linesize;
*/
int scache_size;
int scache_linesize;

u_int error_data[10] = {
	0,			/* address */
	0,			/* expected data */
	0,			/* actual data 	*/
	0,			/* XOR data */
	0,
	0,
	0,
	0,
	0,
	0,
};


/*		s c a c h e r r ( )
**
** scacherr()- Is used to decode the error output of the ChkSdTag routine
*/
void scacherr(unsigned int errtype)
{
	switch(errtype) {

	case	SCSTATE_ERR:
		msg_printf( ERR, "Error in Secondary Cache TAG State field\n");
		break;

	case	SCTAG_ERR:
		msg_printf( ERR, "Error in Secondary Cache TAG physical tag field\n");
		break;

	case	VIRTINDX_ERR:
		msg_printf( ERR, "Error in Secondary Cache TAG Virtual Address field\n");
		break;
	default:
		msg_printf( ERR, "Unknown error type see a Diagnostic Engineer.\n");
		break;
	}


	msg_printf( ERR, "Address 0x%08x\nSecondary TAG Data 0x%08x\n",
		error_data[EXPADR_INDX], error_data[ACTTAG_INDX]);
	msg_printf( ERR, "Expected Cache State:  0x%x = ", error_data[EXPCS_INDX]);


	switch(error_data[EXPCS_INDX]) { /* Decode and display cache state*/

	case	SCState_Invalid:
		msg_printf( ERR, "Invalid\n");
		break;

	case	SCState_Clean_Exclusive:
		msg_printf( ERR, "Clean Exclusive\n");
		break;

	case	SCState_Dirty_Exclusive:
		msg_printf( ERR, "Dirty Exclusive\n");
		break;

	case	SCState_Shared:
		msg_printf( ERR, "Shared\n");
		break;

	case	SCState_Dirty_Shared:
		msg_printf( ERR, "Dirty Shared\n");
		break;

	default:
		msg_printf( ERR, "Unknown error type see a Diagnostic Engineer.\n");
		break;
	}
	msg_printf( ERR, "Virtual address bits:  %x\n", error_data[VIRTADR_INDX]); 
}
