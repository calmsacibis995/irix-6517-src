/*
 *
 * Command format to be sent to mcu fifo 
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
 */

#include "cosmo2_defs.h"
#include "sys/cosmo2_asm.h"
extern __uint32_t timeOut ;


void putCMD (UWORD *ptr)
{
	int  value;
	__uint64_t recv;
 	UWORD recvs;

	/* unset the mask in IMR */ 
	recvs = cgi1_fifos_read_mcu_fg_af(cgi1ptr); 
	recvs = recvs & (~(cgi1_GIOToCosmoNotPAF) );

	recv = cgi1_read_isr(cgi1ptr);
	 while (! (recv & (cgi1_GIOToCosmoNotPAF) ) )  {/* ! (enough space) */
		recv = cgi1_read_isr(cgi1ptr) ;
	}
	cgi1_fifos_write_mcu (cgi1ptr, *ptr++);
	value = (*ptr & 0x00ff) ; /* find the len of words, */ 

#if 0
	msg_printf(V4DBG, " putCMD: type-len %x \n", *ptr);
#endif
	cgi1_fifos_write_mcu (cgi1ptr, *ptr++); 

	while ( value-- ) {
		cgi1_fifos_write_mcu(cgi1ptr, *ptr++ ) ; 
	}
}


UBYTE getCMD (UWORD *ptr )
{

	UWORD len = 0, i = 0 ;
	__uint32_t t_out = 0xfffffff ;
	__uint32_t temp;

	msg_printf(DBG, "timeOut %x \n", timeOut);

	us_delay(10000);
	temp = (__uint32_t)cgi1_read_reg(cosmobase+cgi1_interrupt_status_o) ;
	msg_printf(DBG, "interrupt_status 0x%x \n", temp );

#if defined(IP30)
	temp = (__uint32_t)cgi1_read_reg(cosmobase+cgi1_interrupt_status_o) & cgi1_CosmoToGIONotEmpty;
	msg_printf(DBG, "interrupt_status 0x%x \n", temp );
	for (i = 0; i < t_out ; i++) {
		temp = ((__uint32_t)cgi1_read_reg(cosmobase+cgi1_interrupt_status_o)) & cgi1_CosmoToGIONotEmpty;
		msg_printf(DBG, "interrupt_status 0x%x \n", temp );
		if (temp) break;
	}
	if (!temp)  {
		msg_printf(SUM, " timeOut counter is %x \n", t_out);
				return (0);
	}

#else
	while( (!( ((__uint32_t)cgi1_read_reg(cosmobase+cgi1_interrupt_status_o)) & 
							cgi1_CosmoToGIONotEmpty)) && t_out ) 
	{
				t_out-- ;
	}
#endif
	temp = (__uint32_t)cgi1_read_reg(cosmobase+cgi1_interrupt_status_o) ;
	msg_printf(DBG, "temp %x \n", temp);
				

	if (!t_out) {
		msg_printf(SUM, " timeOut counter is %x \n", t_out);
		return ( 0 );
	}


		*ptr = (UWORD) cgi1_fifos_read_mcu (cgi1ptr) ;
 
		if (*ptr != CMD_SYNC ) 
			{
				msg_printf(SUM, " missing sync \n");
				G_errors++;
				return ( 0 );
			}

	while  (t_out && ( ! (((__uint32_t) cgi1_read_reg(cosmobase+cgi1_interrupt_status_o)) & cgi1_CosmoToGIONotEmpty ))) t_out-- ;
			*++ptr = (UWORD) cgi1_fifos_read_mcu (cgi1ptr) ;
#if 0
			msg_printf(DBG, "GetCMD type %x  \n", *ptr) ; 
#endif
			i = len = *ptr & 0x00ff ;
		 

	for ( i = 0; i < len ; i++ ) {
	while  (t_out && ( ! (((__uint32_t) cgi1_read_reg(cosmobase+cgi1_interrupt_status_o)) & cgi1_CosmoToGIONotEmpty )) ) t_out-- ;
			*++ptr = (UWORD) cgi1_fifos_read_mcu (cgi1ptr) ;
#if 0
			msg_printf(VRB, " GETCMD parameter %x value %x  \n", i,  *ptr );;
#endif
	}

	/* we are here after reading one response : quit now */
	return ( 1 );

}
