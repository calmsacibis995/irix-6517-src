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

#ident "$Revision: 1.6 $"
/*
 *	7/17/91: Richard Frias wrote the original version.
 */

#ifdef	TOAD
#include "bup.h"
#include "types.h"
#include "mach_ops.h"

#else
#include "sys/types.h"

#endif	/* TOAD */

#include "coher.h"
#include "menu.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <ip19.h>
#include "prototypes.h"

static __psunsigned_t cpu_loc[2];

extern int end;

/* Set address to beyond 8 M */
#define ADDR0	0x800000

/* this address will alias addr1 in the secondary */
#define ADDR1	(ADDR0 + config_info.sidcache_size)	

/* this address aliases the primary */
#define ADDR2 (ADDR0 +2000)

/* this address aliases the primary */
#define ADDR3 (ADDR1 +2000)

#define	K0ADDR0	PHYS_TO_K0(ADDR0)
#define	K0ADDR1	PHYS_TO_K0(ADDR1)
#define	K0ADDR2	PHYS_TO_K0(ADDR2)
#define	K0ADDR3	PHYS_TO_K0(ADDR3)

#define	K1ADDR0	PHYS_TO_K1(ADDR0)
#define	K1ADDR1	PHYS_TO_K1(ADDR1)
#define	K1ADDR2	PHYS_TO_K1(ADDR2)
#define	K1ADDR3	PHYS_TO_K1(ADDR3)


extern void Clustrwb(__psunsigned_t, __psunsigned_t, __psunsigned_t, __psunsigned_t);

extern int error_data[4];
extern int scache_linesize;

/*		c l u s t e r w b ( )
**
*/
clusterwb()
{
	register u_int tmp;
	u_int retval;

	retval=PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (clusterwb) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* set global used by subroutine */
	scache_linesize= config_info.sidline_size;

	/* Create some clusters mixed in with some writebacks */
	Clustrwb(K0ADDR0, K0ADDR1, K0ADDR2, K0ADDR3);

	/* Check the secondary tags for a dirty excl state. Note this cache
	 * op does not change the state in the primary or secondary.
	 */
	if (tmp= ChkSdTag( ADDR0, 1,SCState_Dirty_Exclusive, K0ADDR0) ) {
		retval = FAIL;
		scacherr(tmp);
	}  
	if (tmp= ChkSdTag( ADDR3, 1,SCState_Dirty_Exclusive, K0ADDR3) ) {
		retval = FAIL;
		scacherr(tmp);
	}  

	/* Check memory to ensure that data was written back to memory 
	 * from the secondary cache correctly.
 	 */
	if (*(u_int *)K1ADDR0 != 0x11111111) {
		/*Eprintf("Data incorrectly written back to memory\n");*/
		err_msg( CLSTRWB_ERR1, cpu_loc);
		msg_printf( ERR, "Mem Address 0x%08x\n", K1ADDR0);
		msg_printf( ERR, "Expected 0x%08x, Actual 0x%08x, XOR 0x%08x\n",
			0x11111111, *(u_int *)K1ADDR0, 
			(0x11111111 ^ *(u_int *)(K1ADDR0)) );
		retval = FAIL;
	}

	if (*(u_int *)K1ADDR1 != 0x55555555) {
		/*Eprintf("Data incorrectly written back to memory\n");*/
		err_msg( CLSTRWB_ERR2, cpu_loc);
		msg_printf( ERR, "Mem Address 0x%08x\n", K1ADDR0);
		msg_printf( ERR, "Expected 0x%08x, Actual 0x%08x, XOR 0x%08x\n",
			0x11111111, *(u_int *)K1ADDR0, 
			(0x11111111 ^ *(u_int *)K1ADDR0) );
		retval = FAIL;
	}

	if (*(u_int *)K1ADDR2 != 0x55555555) {
		/*Eprintf("Data incorrectly written back to memory\n");*/
		err_msg( CLSTRWB_ERR3, cpu_loc);
		msg_printf( ERR, "Mem Address 0x%08x\n", K1ADDR0);
		msg_printf( ERR, "Expected 0x%08x, Actual 0x%08x, XOR 0x%08x\n",
			0x11111111, *(u_int *)K1ADDR0, 
			(0x11111111 ^ *(u_int *)K1ADDR0) );
		retval = FAIL;
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed secondary cluster writeback test\n");

#ifndef TOAD
	return(retval);
#else

	BupHappy();
#endif	/* TOAD */

}
