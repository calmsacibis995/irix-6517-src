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
extern __psunsigned_t CreateClustrs(__psunsigned_t, __psunsigned_t, uint, uint);

/* End of our code */
extern int end;

extern int error_data[4];
extern int scache_linesize;

/* Set address to beyond 8 M */
#define ADDR1	0x800000

/* this address will alias addr1 in the secondary */
#define ADDR2	(ADDR1 + config_info.sidcache_size)	

#define	K0ADDR1	PHYS_TO_K0(ADDR1)

#define	K1ADDR1	PHYS_TO_K1(ADDR1)

#define	K0ADDR2	PHYS_TO_K0(ADDR2)
#define	K1ADDR2	PHYS_TO_K1(ADDR2)




/*		c l u s t e r ( )
**
**
*/
cluster()
{
	register volatile uint *addr;
	register u_int i;
	register u_int num_clusters;
	register u_int tmp;
	register u_int lines_ut;
	register __psunsigned_t last_virtaddr;
	u_int retval;

	retval = PASS;
	getcpu_loc(cpu_loc);
	msg_printf( INFO, " (cluster) \n");
	msg_printf(DBG, "cpu_loc[0] %d  cpu_loc[1] %d\n", cpu_loc[0], cpu_loc[1]);

	/* get cache sizes and invalidate caches */
	setconfig_info();

	/* set the global used by subroutine */
	scache_linesize= config_info.sidline_size;


	/* a cluster is when a dirty line must be written back
	** to memory before a read miss can replace the cache line
	** in the secondary.
	*/
	num_clusters= 2040;

	/* 1 cluster = 1 word = 4bytes */
	lines_ut = ( (num_clusters * 4)/config_info.sidline_size );

	if( (num_clusters * 4) % config_info.sidline_size)
		lines_ut++;

	/* Go an generate some clusters */
	last_virtaddr = CreateClustrs(K0ADDR1, K0ADDR2, num_clusters, 0xaaaaaaaa);

	/* Check the secondary tags for a dirty excl state. Note this cache
	 * op does not change the state in the primary or secondary.
	 */
	if (tmp= ChkSdTag( ADDR2, lines_ut,SCState_Dirty_Exclusive, K0ADDR2) ) {
		retval = FAIL;
		scacherr(tmp);
	}  

	/* Check memory to ensure that data was written back to memory 
	 * from the secondary cache correctly.
 	 */
	for (i= 0, addr = (u_int *)K1ADDR1; i < num_clusters; i++, addr++) {
		if (*(u_int *)addr != 0xaaaaaaaa) {
			/*Eprintf("Secondary Cache data incorrectly written to memory\n");*/
			err_msg(CLSTR_ERR1, cpu_loc);
			msg_printf( ERR, "1st mem block\n");
			msg_printf( ERR, "Mem Address 0x%08x\n", addr);
			msg_printf( ERR, "Expected 0x%08x, Actual 0x%08x, XOR 0x%08x\n",
				0xaaaaaaaa, *(u_int *)addr, (0xaaaaaaaa ^ *(u_int *)addr));
			retval = FAIL;
		}
	}

	/* Check memory to ensure that data was written back to memory 
	 * from the secondary cache correctly.
 	 */
	for (i= 0, addr = (u_int *)K0ADDR2; i < (num_clusters -1); i++, addr++) {
		if (*(u_int *)addr != 0x55555555) {
			/*Eprintf("Secondary Cache data incorrectly written to memory\n");*/
			err_msg(CLSTR_ERR2, cpu_loc);
			msg_printf( ERR, "2nd mem block\n");
			msg_printf( ERR, "Mem Address 0x%08x\n", addr);
			msg_printf( ERR, "Expected 0x%08x, Actual 0x%08x, XOR 0x%08x\n",
				0xaaaaaaaa, *(u_int *)addr, (0xaaaaaaaa ^ *(u_int *)addr));
			retval = FAIL;
		}
	}

	ide_invalidate_caches();
	msg_printf(INFO, "Completed secondary cluster test\n");
	return(retval);

}
