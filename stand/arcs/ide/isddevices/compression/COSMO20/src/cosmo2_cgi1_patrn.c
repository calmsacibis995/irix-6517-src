/*
 * cgi1_patrn.c
 *
 * Cosmo2 gio 1 chip hardware definitions
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


int cos2_DtpPatrn ( __uint32_t *data_patrn, __uint32_t num_patrn, __uint32_t mask)
{
	
	__uint64_t recv ;
	__uint32_t recvl= 0,  patrn = 0, i  ;
	

	__uint32_t dtp_reg_masks[] = 
		{cgi1_DTP_RESET_0, cgi1_DTP_RESET_1,cgi1_DTP_RESET_2,cgi1_DTP_RESET_3};


	msg_printf(DBG, "CGI1 Register: Pattern Test on dtps is in Progress ......\n");

	/* Need to test the registers except productID and BCR */


		for ( i = 0; i < cgi1_NUM_CHANNELS ; i++) {
		for ( patrn = 0; patrn < num_patrn ; patrn++) {

			/* Reset the DTP first  every time you write into this register */
			recvl = (__uint32_t)cgi1_read_reg(cosmobase+cgi1_dma_control_o ) ;	

		msg_printf(DBG, " reg value %x dma_o %x \n", recvl, cosmobase+cgi1_dma_control_o ) ; 

			cgi1_write_reg(recvl | dtp_reg_masks[i], cosmobase+cgi1_dma_control_o );
			cgi1_write_reg(recvl & ~(dtp_reg_masks[i]), cosmobase+cgi1_dma_control_o );
			cgi1_write_reg(*(data_patrn+patrn), cosmobase+cgi1_tpo[i] );

			recvl =  (__uint32_t) cgi1_read_reg(cosmobase+cgi1_tpo[i]);
			msg_printf(DBG, " recv %x \n", recv ) ;
	
                CGI1_COMPARE("DTP patrn test", cosmobase+cgi1_tpo[i], recvl, *(data_patrn+patrn), mask);
        if (G_errors)
          return  ( 0 ) ;

		
		}
			msg_printf(DBG, "channel %d patrn test is over\n", i);
	}

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," DTP Patrn test passed \n");
            return ( 1 );
            }

			
}

int cgi1_reg_patrn(__uint32_t *data_patrn, __uint32_t num_patrn, __uint32_t offset, __uint32_t mask, char *str)
{
	__uint32_t patrn, recv;
	
	msg_printf(DBG, "Testing the patrns %s \n", str);

        for ( patrn = 0; patrn < num_patrn ; patrn++) {
                cgi1_write_reg(*(data_patrn+patrn) & mask, cosmobase+offset);
				recv = (__uint32_t)cgi1_read_reg(cosmobase+offset);
			CGI1_COMPARE(str, cosmobase+offset, recv, *(data_patrn+patrn), mask);
		}

	return (0);
}

	 
__uint32_t cos2_OffTimePatrn (void)
{
        
        __uint32_t recvl= 0, patrn = 0,  temp;

	msg_printf(DBG, "CGI1 Register: Pattern Test on off time register is in Progress ......\n");

	temp = sizeof(patrn32)/sizeof(patrn32[0]) ;

	for ( patrn = 0; patrn < temp ; patrn++) {
			cgi1_write_reg(patrn32[patrn],   cosmobase + cgi1_off_time/cgi1_register_bytes  ); 
			recvl =  (__uint32_t) cgi1_read_reg( cosmobase + cgi1_off_time/cgi1_register_bytes ); 
                CGI1_COMPARE("OffTime", (__paddr_t) (cosmobase+cgi1_off_time/cgi1_register_bytes), 
								recvl, patrn32[patrn],  _16bit_mask);
	}
        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," OffTime Register test passed \n");
            return ( 1 );
            }

}


__uint32_t cos2_BcrPatrn (void)
{

        __uint32_t recvl= 0, patrn = 0,  temp;

        msg_printf(DBG, "CGI1 Register: Pattern Test on BCR register is in Progress ......\n");

	temp  = sizeof(patrn32)/sizeof(patrn32[0]) ;

        for ( patrn = 0; patrn < temp ; patrn++) {
            cgi1_write_reg(patrn32[patrn] & _BCR_mask, 
			cosmobase+cgi1_board_con_reg_o );
            recvl = (__uint32_t) cgi1_read_reg(cosmobase+cgi1_board_con_reg_o );
            CGI1_COMPARE("BcrPatrn",(__paddr_t) (cosmobase+cgi1_board_con_reg_o) , 
											recvl, patrn32[patrn], _BCR_mask );
        }
        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," BCR Pattern test passed \n");
            return ( 1 );
            }

}

__uint32_t cos2_IntrMaskPatrn (void)
{

	 __uint32_t recvl= 0,  patrn = 0,  temp;

        msg_printf(DBG, 
			"CGI1 Register: Pattern Test on Interrupt Mask Register is i Progress ...\n");  

	temp  = sizeof(patrn32)/sizeof(patrn32[0]) ;

	for ( patrn = 0; patrn < temp ; patrn++) {
		msg_printf(DBG, " IntrMask : write %x \n", patrn32[patrn]);
		cgi1_write_reg(patrn32[patrn], cosmobase+cgi1_interrupt_mask_o );
		recvl = (__uint32_t) cgi1_read_reg(cosmobase+cgi1_interrupt_mask_o );
		CGI1_COMPARE("IntrMask", (__paddr_t)(cosmobase+cgi1_interrupt_mask_o), 
										recvl, patrn32[patrn], _32bit_mask);
	}
        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," Intr Mask Register test passed \n");
            return ( 1 );
            }

}

__uint32_t cos2_Cgi1DataBusTest( void )
{
        
        __uint32_t curr = 0xdead , recv ;
        long long recv1;
        _CTest_Info      *pTestInfo = cgi1_info+2;

        for (curr = 0; curr < sizeof(patrn32)/sizeof(patrn32[0]) ; curr++) {

#if defined (IP30)
				cgi1_write_reg(patrn32[curr],cosmobase+cgi1_interrupt_mask_o);
				recv1= (long long )cgi1_read_reg(cosmobase+cgi1_interrupt_mask_o);
				recv = (__uint32_t)  recv1;
#else
                cgi1_write_imr (cgi1ptr,  patrn32[curr] ) ;

                recv1= (long long ) cgi1_read_imr (cgi1ptr )  ;
                recv = (__uint32_t)  recv1;
	
#endif
                
                CGI1_COMPARE (pTestInfo->TestStr, 3, recv,  patrn32[curr], _32bit_mask);
        }

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," cgi1 data bus  test passed \n");
            return ( 1 );
            }

                
}

__uint32_t cos2_Cgi1AddrUniq (void)
{

	__paddr_t  recv,  expt , mask = 0x7ffe;

	expt = (__paddr_t) (cosmobase+cgi1_board_con_reg_o) & mask;
	cgi1_write_reg(expt, cosmobase+cgi1_board_con_reg_o);
	recv =  cgi1_read_reg(cosmobase+cgi1_board_con_reg_o) & mask;
	CGI1_COMPARE ("addr uniq of BCR", cosmobase+cgi1_board_con_reg_o, recv, expt, mask); 

	expt = (__paddr_t) (cosmobase+cgi1_dma_control_o) & _16bit_mask ;
	cgi1_write_reg(expt, cosmobase+cgi1_dma_control_o) ;
	recv =  cgi1_read_reg(cosmobase+cgi1_dma_control_o)  & _16bit_mask;
	CGI1_COMPARE ("addr uniq of DMA CONTROL", cosmobase+cgi1_dma_control_o, 
							recv, expt, _16bit_mask ); 


	expt = (__paddr_t)(cosmobase+cgi1_interrupt_mask_o)  & _32bit_mask;
	cgi1_write_reg(expt, cosmobase+cgi1_interrupt_mask_o) ;
	recv =  cgi1_read_reg(cosmobase+cgi1_interrupt_mask_o)  & _32bit_mask;
	CGI1_COMPARE ("addr uniq of Intr Mask", (cosmobase+cgi1_interrupt_mask_o),  
							recv, expt, _32bit_mask);


	expt = (__paddr_t)(cosmobase+cgi1_off_time_o)  & _16bit_mask;
	cgi1_write_reg(expt, cosmobase+cgi1_off_time_o) ;
	recv =  cgi1_read_reg(cosmobase+cgi1_off_time_o)  & _16bit_mask;
	CGI1_COMPARE ("addr uniq of Off time ", (cosmobase+cgi1_off_time_o),  
							recv , expt, _16bit_mask);

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," CGI1 Addr Uniq  test passed \n");
            return ( 1 );
            }

    
}


__uint32_t cos2_Cgi1PatrnTest ( )
{
	__uint32_t size;
        _CTest_Info      *pTestInfo = cgi1_info+3;

	size = sizeof(patrn32)/sizeof(patrn32[0]) ;

	cos2_Cgi1DataBusTest ( ) ;
	cos2_IntrMaskPatrn  ();
	cos2_BcrPatrn ( );
	cos2_OffTimePatrn ( );

	cgi1_reg_patrn(patrn32, size, cgi1_off_time_o, _16bit_mask, "OffTime");  
	cgi1_reg_patrn(patrn32, size, cgi1_board_con_reg_o, _BCR_mask, "BCR");  
	cgi1_reg_patrn(patrn32, size, cgi1_interrupt_mask_o, _32bit_mask, "InterMask");  
	cos2_DtpPatrn(patrn32, size, _32bit_mask ) ;


       if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," CGI1 Patrn test passed \n");
            return ( 1 );
            }
 

}
