
/*
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
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 */



#include "cosmo2_defs.h"



int cos2_ReadRevision( void) ;

int cos2_ReadRevision( void)
{
        cgi1_t *nptr;
        __uint32_t value = 0xdead, curr = 0xdead , recv;
        long long recv1;
        _CTest_Info      *pTestInfo = cgi1_info+0;

	recv1=cgi1_read_reg(cosmobase)  ;
	recv = (__uint32_t)  recv1;
#if 0
	recvh= (__uint32_t) (recv1>>32);
#endif

	CGI1_COMPARE (pTestInfo->TestStr, cosmobase+0, recv, cgi1_cosmo_id_val,  _32bit_mask);

	REPORT_PASS_OR_FAIL(pTestInfo, G_errors);
}

