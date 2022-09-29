
/*
 * cgi1_rst.c
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


__uint32_t  cos2_BoardReset(void);
extern  void putCMD (UWORD *);

extern __uint32_t timeOut ;

void cosmo2_reset_chnl(void);

void
cosmo2_init(void)
{
 cmd_seq = 0;
 G_errors = 0;
 cosmo2_reset_chnl();
}

int
cosmo2_dc(void)
{
    UBYTE val8;
    val8 = mcu_READ(PORTQS+1);
    if (val8 & 0x10) {
    msg_printf(SUM, " Daughter card is not present\n");
	cosmo2_init();
    return(0);
    }
    return(1);
}


__uint32_t  cos2_BoardReset( ) 
{
	_CTest_Info	*pTestInfo = cgi1_info+1;	
        __uint32_t value = 0xdead, curr = 0xdead , recv, i;
	long long recv1;
	UWORD buf[32];

	G_errors = 0;
	timeOut = 0xffffff ;


	msg_printf(SUM, "Board Reset Test in Progress ......\n");

        msg_printf(DBG, " writing 1 to the BCR in bit 0\n" ) ;
#if defined(IP30)
		cgi1_write_reg(0x1, cosmobase+cgi1_board_con_reg_o);
#else
        cgi1_write_bcr(cgi1ptr, 0x1) ;
#endif

	us_delay(10000);

        msg_printf(DBG, " reading IMR\n" ) ;
#if defined(IP30)
		recv1=cgi1_read_reg(cosmobase+cgi1_interrupt_mask_o);
#else
		recv1=cgi1_read_imr(cgi1ptr) ;
#endif
		recv = (__uint32_t)  recv1;
#if 0
		recvh= (__uint32_t) (recv1>>32);
#endif

	CGI1_COMPARE ("CGI1:IMR", (__paddr_t)(cosmobase+cgi1_interrupt_mask_o), recv, CGI1_INTERRUPT_MASK_DEF, _32bit_mask); 
	msg_printf(DBG, " reading DMA status register\n" ) ;

#if defined(IP30)
		recv1 = cgi1_read_reg(cosmobase+cgi1_overview_o);
#else
		recv1=cgi1_read_summt(cgi1ptr) ;
#endif
		recv = (__uint32_t)  recv1;
#if 0
		recvh= (__uint32_t) (recv1>>32);
#endif


	CGI1_COMPARE ("DMA-STATUS",  (__paddr_t)(cosmobase+cgi1_overview_o), recv, CGI1_DMA_STAT_DEF, _32bit_mask);

        msg_printf(DBG, " Reading DMA control register\n" ) ;

#if defined(IP30)
			recv1= cgi1_read_reg(cosmobase+cgi1_dma_control_o);
#else
                recv1=cgi1_read_dma_cr(cgi1ptr) ;
#endif

                recv = (__uint32_t)  recv1;
#if 0
                recvh= (__uint32_t) (recv1>>32);
#endif

        CGI1_COMPARE ("DMA-CONTROL register",  (__paddr_t)(cosmobase+cgi1_dma_control_o), recv, CGI1_DMA_STAT_DEF, _16bit_mask);


	for ( i = 0; i < cgi1_NUM_CHANNELS ; i++) {
        msg_printf(DBG, " Reading Desp. Table Pointer register for channel %x\n" , i) ;
                recv1	=	cgi1_read_reg	(cosmobase+i+2) ;
                recv 	= 	(__uint32_t) 	recv1;
#if 0
                recvh	= 	(__uint32_t) 	(recv1>>32);
#endif

        	CGI1_COMPARE ("CGI1:DTP", (__paddr_t)(cosmobase+cgi1_tpo[i]), recv, CGI1_FIFO_RW_0_DEF, _32bit_mask);
	}

	
	us_delay(10000000);
	if (getCMD(buf) == 0) {
		msg_printf(SUM, "didn't get mcu response\n"); 
		G_errors++;
		return (0) ;
	}

	recv = buf[1] & 0x00ff;

	if (recv != 5) {
		msg_printf(ERR, " MCU Response :ready resp has incorrect length %d\n", recv );
		G_errors++ ;
		return (0) ;

	}

	if (buf[2] != COSMO2_CMD_COSMO_READY) {
		msg_printf(ERR, " MCU Response :ready resp code not found %d\n", buf[2]);  
		G_errors++ ;
		return (0) ;

	}

	if (buf[3] != 0) {
		msg_printf(ERR, " MCU Response :seqno is not zero: but it is %d\n", buf[3]);
		G_errors++ ;
		return (0) ;

	}

	if (buf[4] != 0) {
		msg_printf(ERR, " MCU Response :SRAM : Addr Uniq : %d errors\n", (buf[4] & 0xff00) >> 8);
		msg_printf(ERR, " MCU Response :SRAM : walk 1s 0s %d errors\n", buf[4] & 0xff );
		G_errors++ ;
		return (0) ;

	}

	if (buf[5] != 0) {
		msg_printf(ERR, " MCU Response : channel 1 016 %x errors\n", (buf[5] & 0xff00) >> 8 );
		msg_printf(ERR, " MCU Response : channel 1 050 %x errors\n", buf[5]  );
		G_errors++ ;
		return (0) ;

	}

	if (buf[6] != 0) {
		msg_printf(ERR, " MCU Response : channel 2 016 %x errors\n", (buf[6] & 0xff00) >> 8 );
		msg_printf(ERR, " MCU Response : channel 2 050 %x errors\n", buf[6]  );
		G_errors++ ;
		return (0) ;

	}


        REPORT_PASS_OR_FAIL(pTestInfo, G_errors);
}
