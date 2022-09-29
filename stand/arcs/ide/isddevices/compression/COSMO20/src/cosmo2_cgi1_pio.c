/*
 * SetPio.c 
 *
 * Cosmo2 set PIO mode 
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

#include "cosmo2_defs.h"


int EnablePioMode( void );
int DisEnableSetPioMode(void);


int cos2_EnablePioMode( void )
{
	_CTest_Info      *pTestInfo = cgi1_info+8;
	__uint32_t value = 0xdead, curr = 0xdead , recv, i;
	unsigned long long recv1;


	msg_printf(DBG, "Setting PIO mode......\n");

	recv1 = cgi1_read_reg(cosmobase+1); 
	recv = (__uint32_t)  recv1;
#if 0
	recvh= (__uint32_t) (recv1>>32);
#endif
	cgi1_write_reg ((recv1 | 0x600), cosmobase+1);
	
	msg_printf(DBG, "doing PIO mode \n");

	recv1 = cgi1_read_reg(cosmobase+1);
	recv = (__uint32_t)  recv1;
#if 0
	recvh= (__uint32_t) (recv1>>32);
#endif

	CGI1_COMPARE (pTestInfo->TestStr, cosmobase+0x1, recv, 0x600, 0xf00);
	
	msg_printf(DBG, "Setting PIO mode done \n");

	if (G_errors)
		return ( 0 ) ;
	else
		return ( 1 );
}

int cos2_DisEnablePioMode(void)
{
        _CTest_Info      *pTestInfo = cgi1_info+9;
        __uint32_t value = 0xdead, curr = 0xdead , recv, recvh, i;
        long long recv1;


        msg_printf(DBG, "Unsetting PIO mode......\n");


                recv1 = cgi1_read_reg(cosmobase+1);
                recv = (__uint32_t)  recv1;
                cgi1_write_reg ( recv1 & 0xf0ff, cosmobase+1); 

                recv1 = cgi1_read_reg(cosmobase+1);
                recv = ((__uint32_t)  recv1) & 0x0f00;

                CGI1_COMPARE (pTestInfo->TestStr, cosmobase+0x1, recv, 0, 0x0f00);

	if (G_errors)
		return ( 0 ) ;
	else
		return ( 1 );
}
