
/*
 * mc3/khdouble.c
 *
 *
 * Copyright 1991, 1992 Silicon Graphics, Inc.
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

#ident "$Revision: 1.1 $"


#include <sys/types.h>
#include <sys/sbd.h>

extern u_int	dwd_rdbuf[2];
extern u_int	dwd_wrtbuf[2];
extern u_int	dwd_tmpbuf[2];



khdouble_drv(u_int first, u_int last)
{

    register unsigned int fail = 0;

    printf("Knaizuk Hartmann Doubleword Test\n");
   /*
    *  Kh_l
    */
   fail = kh_dw(first, last);
   if( fail) {
       	printf("Failing Address = %08x\n", dwd_tmpbuf[0]);
       	printf("Expected = %08x %08x\n", dwd_wrtbuf[1], dwd_wrtbuf[0]);
       	printf("  Actual = %08x %08x\n", dwd_rdbuf[1], dwd_rdbuf[0]);
       	printf("failing step %d\n", fail);
   }

    if (!fail)
    	printf("Knaizuk Hartmann test -- PASSED\n");

    return (fail);
}
