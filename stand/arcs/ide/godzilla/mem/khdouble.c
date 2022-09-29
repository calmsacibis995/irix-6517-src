/*
 * khdouble.c -  Knaizuk Hartmann Doubleword Test
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * for IP30: this is used in memtest only for IP22 usable by IP30?? XXX
 */
#ident "ide/godzilla/mem/khdouble.c :  $Revision: 1.2 $"


#include <sys/types.h>
#include <sys/sbd.h>
#include "libsc.h"

extern u_int	dwd_rdbuf[2];
extern u_int	dwd_wrtbuf[2];
extern u_int	dwd_tmpbuf[2];

int kh_dw(__psunsigned_t,__psunsigned_t);

int
khdouble_drv(__psunsigned_t first, __psunsigned_t last)
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
