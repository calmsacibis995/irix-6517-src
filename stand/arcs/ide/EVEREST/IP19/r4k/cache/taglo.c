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

#ident "$Revision: 1.4 $"


/*=============================================================================
 *
 *	File Name: taglo.c
 *
 *	Diag name: Taglotst
 *
 *	Description: This diag tests the data integrity of the taglo register.
 *		     A sliding one and a sliding zero pattern is used.
 *
 *===========================================================================*/


#include "sys/types.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include "ip19.h"
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

extern void SetTagLo(uint); 
extern uint GetTagLo(void);


Taglotst()
{
	register u_int actual;
	register u_int expected;
	register u_int tmp;
	register u_int i;
	uint retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf(INFO, " (Taglotst) \n");
	msg_printf(VRB, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* Try sliding one */
	for( i=1, expected =1; i <= 32; i ++, expected <<= 1) {
		SetTagLo(expected);
		msg_printf( DBG, "data %x\n", expected);
		if( (actual= GetTagLo() ) != expected) {
			/*Eprintf("Taglo register failed sliding one test.\n");*/
			err_msg( TAGLO_ERR1, cpu_loc);
			msg_printf( ERR, "Expected data: 0x%08x Actual data: 0x%08x\n",
					expected, actual);
			retval = FAIL;
		}	
	}

	/* Try sliding zero */
	for( i=1, expected =1; i <= 32; i ++, expected <<= 1) {
		tmp= ~expected;
		/* Load register */
		SetTagLo(tmp);
		msg_printf( DBG, "data %x\n", tmp);
		if( (actual= GetTagLo() ) != tmp ) {
			err_msg( TAGLO_ERR2, cpu_loc);
			/*Eprintf("Taglo register failed sliding zero test.\n");*/
			msg_printf( ERR, "Expected data: 0x%08x Actual data: 0x%08x\n",
					tmp, actual);
			retval = FAIL;
		}	
	}
	msg_printf(INFO, "Completed TAGLO register test\n");
	return (retval);
}

