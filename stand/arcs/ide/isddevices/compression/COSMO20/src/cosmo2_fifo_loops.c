/*
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
#include "cosmo2_mcu_def.h"

extern int cos2_EnablePioMode( void );
extern __uint32_t  cos2_BoardReset(void); 


extern void mcu_WRITE (__uint32_t, UBYTE);
extern UBYTE mcu_READ (__uint32_t );
extern void cos2_LedOn ( void  ) ;
void cos2_LedOff (  void ) ;
extern void mcu_fifo_loopback (void);

extern UBYTE getCMD(UWORD *);

void unreset_chnl0 ()
{
    UWORD temp2, buf[10];
#if 0
    mcu_read_cmd(COSMO2_CMD_OTHER, cmd_seq, PORTQS, 2) ;
    getCMD(buf) ;
    temp2 = buf[7] | 0x40 ;
    mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq, PORTQS, 2, (UBYTE *) &temp2) ;
#else
	temp2 = mcu_READ_WORD(PORTQS);
	mcu_WRITE_WORD(PORTQS, (temp2 | 0x40) );
#endif
}


void unreset_chnl1 ()
{
    UBYTE temp ;
        mcu_WRITE(CFORC, 0x3);
        temp = mcu_READ(PWMC);
        temp = temp | 0x2;
        mcu_WRITE(PWMC, temp);
}

void unreset_chnl3 ()
{
    UBYTE temp ;

        temp = mcu_READ(PORTF0);
        temp = temp | 0x40 ;
        mcu_WRITE(PORTF0, temp);
}

void unreset_chnl2 ()
{
    UBYTE temp ;

        temp = mcu_READ(PORTF0);
        temp = temp | 0x80 ;
        mcu_WRITE(PORTF0, temp);
}


void reset_chnl3 ()
{
    UBYTE temp ;

        temp = mcu_READ(PORTF0);
        temp = temp & (~0x40) ;
        mcu_WRITE(PORTF0, temp);
}

void reset_chnl2 ()
{
    UBYTE temp ;

        temp = mcu_READ(PORTF0);
        temp = temp & (~0x80) ;
        mcu_WRITE(PORTF0, temp);
}
void reset_chnl0 ()
{
    UWORD temp2, buf[10];
#if 0
    mcu_read_cmd(COSMO2_CMD_OTHER, cmd_seq, PORTQS, 2); 
    getCMD(buf);
    temp2 = buf[7] & (~0x40) ;
    mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq, PORTQS, 2, (UBYTE *) &temp2);
#else
	temp2 = mcu_READ_WORD(PORTQS);
	mcu_WRITE_WORD(PORTQS, (temp2 & (~0x40)) );
#endif
}

void reset_chnl1 ()
{
    UBYTE temp ;
		mcu_WRITE(CFORC, 0x3);	
		temp = mcu_READ(PWMC);
		temp = temp & (~0x2);
		mcu_WRITE(PWMC, temp);
	
}

void Reset_SCALER_UPC(UWORD chnnl)
{
	UBYTE val ;

         if( chnnl == CHANNEL_0)
         {
			val = mcu_READ(QPDR);
   			mcu_WRITE(QPDR, val & (~PQS6) );
			DelayFor(10000);
			val = mcu_READ(QPDR);
			mcu_WRITE(QPDR, val | PQS6);
         }else{


			val = mcu_READ(PORTF0);
			mcu_WRITE(PORTF0, val & (~RESET_LOW_1_UPC_SCALER) );
			DelayFor(10000);
			val = mcu_READ(PORTF0);
			mcu_WRITE(PORTF0, val | RESET_LOW_1_UPC_SCALER ) ;
         }
}

void Reset_050_016_FBC_CPC(UWORD chnnl)
{
	UBYTE val ;

         if( chnnl == CHANNEL_0)
         {

			val = mcu_READ(PWMC);
			mcu_WRITE(PWMC, val & (~PBWMA) );
			DelayFor(10000);
			val = mcu_READ(PWMC);
			mcu_WRITE(PWMC, val | PBWMA);

         }else{
			val = mcu_READ(PORTF0);
			mcu_WRITE(PORTF0, val & (~RESET_LOW_1_CPC_050_016_FBC) );
			DelayFor(10000);
			val = mcu_READ(PORTF0);
			mcu_WRITE(PORTF0, val | RESET_LOW_1_CPC_050_016_FBC);

         }
}


#if 0
> 1.  CFIFO to UFIFO
>
>    - Register configuration:  Set bit 7 to 1,
>                               Set bit 6 to 0
>                               Set bit 5 to 1,
>                               Set bit 0  to 1
>                               set bit 2-1 XX dont care
#endif

int 
c2u_fifo_loopback_ver2 (	__uint64_t *patrn, __uint32_t size, __uint32_t base , 
								__uint32_t chnl1,  __uint32_t chnl2, UBYTE ptr )
{
	 __uint64_t recv , i;

	msg_printf(SUM, "set fifo lpk mode in UPC: %x\n", ptr );
	mcu_WRITE(UPC_FLOW_CONTROL + base, ptr ) ; 

	msg_printf(SUM, "Dumping values in channel %d ... \n", chnl1 );
	for ( i = 0; i < size ; i++) {
	    cgi1_write_reg(*(patrn+i), cosmobase+cgi1_fifo_rw_o( chnl1) );
	    recv = cgi1_read_reg(cosmobase + cgi1_fifo_rw_o( chnl2 ) );
	    CGI1_COMPARE_LL("FifoLB",(__paddr_t)(cosmobase+cgi1_fifo_rw_o(chnl2)), 
										recv, *(patrn+i), _64bit_mask);
	}

	return (0);
}


UBYTE checkDataInFifo( UBYTE chnl)
{
	UWORD status, l_timeout, flag = 0;

        status = mcu_READ(DDRGP + 1);


		l_timeout = 0xff;

        switch (chnl) {
        case 0:
        while ( (!(status & 0x8)) && (l_timeout--))  {
            status = mcu_READ(DDRGP + 1);
        }
            if (status & 0x8) flag = 1;
            break;

        case 1:
        while ( (!(status & 0x10)) && (l_timeout--))  {
            status = mcu_READ(DDRGP + 1);
        }
            if (status & 0x10) flag = 1;
            break;

        case 2:
        while ( (!(status & 0x20)) && (l_timeout--))  {
            status = mcu_READ(DDRGP + 1);
        }
            if (status & 0x20) flag = 1;
            break;

        case 3:
        while ( (!(status & 0x40)) && (l_timeout--))  {
            status = mcu_READ(DDRGP + 1);
        }
            if (status & 0x40) flag = 1;
            break;

        default:
            msg_printf(SUM, " incorrect channel number \n" );
            break;
        }


	if (flag == 0) {
		msg_printf(VRB, " TIMEOUT OCCURED AND THERE IS NO DATA IN FIFO\n");	
		return (0);
		}
	else 
		return (1);
}

int
c2u_fifo_loopback(__uint64_t *patrn, __uint32_t size, __uint32_t base,
		  __uint32_t chnl1,  __uint32_t chnl2, UBYTE ptr)
{
	 __uint64_t recv , i;

	msg_printf(VRB, "Dumping values in channel %d\n", chnl1 );

	mcu_WRITE(DDRGP, 0x0);

	for ( i = 0; i < size ; i++)
	   cgi1_write_reg(*(patrn+i), cosmobase+cgi1_fifo_rw_o( chnl1) );

	msg_printf(DBG, "set fifo lpk mode in UPC: %x\n", ptr );
	mcu_WRITE(UPC_FLOW_CONTROL + base, ptr ) ; 

	msg_printf(DBG, "Reading channel %d... \n", chnl2 );
	for (i = 0; i < size ; i++) {
		if (checkDataInFifo (chnl2) ) {
			recv = cgi1_read_reg(cosmobase + cgi1_fifo_rw_o( chnl2 ) );
			CGI1_COMPARE_LL("FifoLB",(__paddr_t)(cosmobase+cgi1_fifo_rw_o(chnl2)), 
										recv, *(patrn+i), _64bit_mask);
		}
		else {
			if ( i == size )
				return 0;
			else if (i != size) {
				G_errors++;
				return 0;
			}
		}
			
	}

	return 0;
}

__uint32_t cos2_fifo_lb ( int argc, char **argv)
{
	__uint64_t patrn[SIZEOF_FIFOS] ;
	__uint32_t i, mode;
	_CTest_Info      *pTestInfo = cgi1_info+ 22 ;


	/* store the patrns in an array of size SIZEOF_FIFOS */
	for ( i = 0; i < PATRNS_64_SIZE; i++) {
		patrn[i] = patrn64[i] ;
	msg_printf(DBG, " i %d mode %d %llx \n", i, mode, patrn[i]);
	}


	 argc--; argv++;
	
	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'c' :
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int *)&mode);
					argc--; argv++;
				} else {
					 atob(&argv[0][2], (int *)&mode);
				}

			break;
			default:
			msg_printf(SUM, " Usage: cos2_fifo_lb -c chnl \n");
			break;
		}
		argc--; argv++;
	}


	if (mode == 1)	{
		msg_printf(DBG, "Reseting the channel \n");
		cgi1_write_reg(cgi1_RESET_1, cosmobase+cgi1_dma_control_o);
		cgi1_write_reg(0, cosmobase+cgi1_dma_control_o);

		msg_printf(DBG, "Reseting the UPC, scaler \n");
		Reset_SCALER_UPC ( CHANNEL_0 );

		msg_printf(DBG, "Reset_050_016_FBC_CPC \n");
		Reset_050_016_FBC_CPC (CHANNEL_0) ;	

		DelayFor(10000);
		cos2_EnablePioMode ( );

		msg_printf(VRB, "loopback test from %d to %d is in progress \n", 
									CHANNEL_1, CHANNEL_0);
		c2u_fifo_loopback(patrn, PATRNS_64_SIZE, VIDCH1_UPC_BASE, 
									CHANNEL_1, CHANNEL_0, 0xa0);
	}

	if (mode == 0)	{
		msg_printf(DBG, "Reseting the channel \n");
		cgi1_write_reg(cgi1_RESET_0, cosmobase+cgi1_dma_control_o);
		cgi1_write_reg(0, cosmobase+cgi1_dma_control_o);

		msg_printf(DBG, "Reseting the UPC, scaler \n");
		Reset_SCALER_UPC ( CHANNEL_0 );

		msg_printf(DBG, "Reset_050_016_FBC_CPC \n");
		Reset_050_016_FBC_CPC (CHANNEL_0) ;	

		DelayFor(10000);

		cos2_EnablePioMode ( );

		msg_printf(VRB, "loopback test from %d to %d is in progress \n", 
									CHANNEL_0, CHANNEL_1);
		c2u_fifo_loopback(patrn, PATRNS_64_SIZE, VIDCH1_UPC_BASE, 
									CHANNEL_0, CHANNEL_1, 0xa1 );
	}

	if (mode == 3)	{
		msg_printf(DBG, "Reseting the channel \n");
		cgi1_write_reg(cgi1_RESET_3, cosmobase+cgi1_dma_control_o);
		cgi1_write_reg(0, cosmobase+cgi1_dma_control_o);
		Reset_SCALER_UPC ( CHANNEL_1 );
		Reset_050_016_FBC_CPC (CHANNEL_1) ;	
		cos2_EnablePioMode ( );
        msg_printf(VRB, "loopback test from %d to %d is in progress \n", 
									CHANNEL_3, CHANNEL_2);
        c2u_fifo_loopback(patrn, PATRNS_64_SIZE, VIDCH2_UPC_BASE, 
									CHANNEL_3, CHANNEL_2, 0xa0 );
        }

	if (mode == 2)	{
		msg_printf(DBG, "Reseting the channel \n");
		cgi1_write_reg(cgi1_RESET_2, cosmobase+cgi1_dma_control_o);
		cgi1_write_reg(0, cosmobase+cgi1_dma_control_o);
		Reset_SCALER_UPC ( CHANNEL_1 );
		Reset_050_016_FBC_CPC (CHANNEL_1) ;	
		cos2_EnablePioMode ( );
		msg_printf(VRB, "loopback test from %d to %d is in progress \n", 
									CHANNEL_2, CHANNEL_3);
		c2u_fifo_loopback(patrn, PATRNS_64_SIZE, VIDCH2_UPC_BASE, 
									CHANNEL_2, CHANNEL_3, 0xa1 ); 
	}
	
	    if (G_errors) {
        msg_printf(ERR, " cosmo2 fifo loop back from channel %d failed \n", mode+1);
		return (0);
        }
        else {
        msg_printf(SUM, " cosmo2 fifo loop back from channel %d passed \n", mode+1);
		return (1);
        }


        
}

__uint32_t fifo_addr_uniq ( __uint64_t *patrn, __uint32_t size, 
		__uint32_t base, __uint32_t chnl1, __uint32_t chnl2, UBYTE ptr )
{

	int i   ;
	 __uint64_t recv  ;


	cos2_EnablePioMode ( );
	for ( i = 0 ; i < size ; i++) 
		cgi1_write_reg(*(patrn+i),  cosmobase + cgi1_fifo_rw_o( chnl1 ) );

		mcu_WRITE(UPC_FLOW_CONTROL + base, ptr ) ; 

		DelayFor(1000);

		for ( i = 0 ; i < size ; i++ ) {
			recv = cgi1_read_reg(cosmobase+cgi1_fifo_rw_o( chnl2 ) );
			CGI1_COMPARE_LL("fifo uniqness test", *(patrn + i), recv, *(patrn + i), _64bit_mask); 
		}

	return 0;
}


__uint32_t cos2_fifo_uniqueness ( int argc, char **argv )
{

 __uint64_t patrn[SIZEOF_FIFOS] , temp;
 UWORD i; 
	int chnl = 0;


    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &chnl);
                }
			break;
            default:
                msg_printf(SUM, " usage: -c chnl -v value \n");
                break;
        }
        argc--; argv++;
    }

	switch (chnl) {
	case 0:
	/* channel 0 to channel 1 */
	 	temp = (__paddr_t) (cosmobase + cgi1_fifo_rw_offset(CHANNEL_0)); 
		temp = temp + ((temp & 0xffffffff) << 32) ;

	for ( i = 0; i < SIZEOF_FIFOS ; i++ ) 
		patrn[i] = temp + i ;

		Reset_SCALER_UPC ( CHANNEL_0 );
		Reset_050_016_FBC_CPC (CHANNEL_0) ;	
	fifo_addr_uniq (patrn, SIZEOF_FIFOS, VIDCH1_UPC_BASE, CHANNEL_0 , CHANNEL_1, 0xa1 );
		break;
		case 2:
	/* channel 2 to channel 3 */
	 	temp = (__paddr_t) (cosmobase + cgi1_fifo_rw_offset(CHANNEL_2)) ;
		temp = temp + ((temp & 0xffffffff) << 32) ;

	for ( i = 0; i < SIZEOF_FIFOS ; i++ ) 
		patrn[i] = temp + i ;
	
		Reset_SCALER_UPC ( CHANNEL_1 );
		Reset_050_016_FBC_CPC (CHANNEL_1) ;	
	fifo_addr_uniq (patrn, SIZEOF_FIFOS, VIDCH2_UPC_BASE, CHANNEL_2 , CHANNEL_3, 0xa1 );
		break;
		case 1:
	/* channel 1 to channel 0 */
	 	temp = (__paddr_t) (cosmobase + cgi1_fifo_rw_offset(CHANNEL_1)) ;
		temp = temp + ((temp & 0xffffffff) << 32) ;

	for ( i = 0; i < SIZEOF_FIFOS ; i++ ) 
		patrn[i] = temp + i ;
	
		Reset_SCALER_UPC ( CHANNEL_0 );
		Reset_050_016_FBC_CPC (CHANNEL_0) ;	
	fifo_addr_uniq (patrn, SIZEOF_FIFOS, VIDCH1_UPC_BASE, CHANNEL_1 , CHANNEL_0, 0xa0 );

		break;
		case 3:
	/* channel 3 to channel 2 */
	 	temp = (__paddr_t) (cosmobase + cgi1_fifo_rw_offset(CHANNEL_3)) ;
		temp = temp + ((temp & 0xffffffff) << 32) ;

	for ( i = 0; i < SIZEOF_FIFOS ; i++ ) 
		patrn[i] = temp + i ;
	
		Reset_SCALER_UPC ( CHANNEL_1 );
		Reset_050_016_FBC_CPC (CHANNEL_1) ;	
	fifo_addr_uniq (patrn, SIZEOF_FIFOS, VIDCH2_UPC_BASE, CHANNEL_3 , CHANNEL_2, 0xa0);

		default:
		break;
	}
        if (G_errors) {
        msg_printf(ERR, " cosmo2 fifo uniqueness in channel %d failed \n", chnl+1);
        return (0);
        }
        else {
        msg_printf(SUM, " cosmo2 fifo uniqueness in channel %d passed \n", chnl+1);
        return (1);
        }

}


#if 0
> 2.  UFIFO to CFIFO
>    - Register configuration:  Set bit 7 to 1,
>                               Set bit 6 to 0
>                               Set bit 5 to 1,
>                               Set bit 0 to 0
>                               set bit 2-1 to XX dont care
>
#endif



#if 0
> 4.  MCU to UFIFO
>    - Register configuration:  Set bit 7 to 1,
>                               set bit 6 to 1
>                               Set bit 5 to 1,
>                               set bit 0 t0 0
>                               Load JVR with data
>
>
#endif

__uint32_t cos2_mcu2upc2fifo ( int argc, char **argv)
{
	__uint32_t  i,j , sizeof64 , mode, tmp, chnl, addr; 
	UWORD buf[20], l_timeout;
	UBYTE *ptr  , status,  flag = 0;
	__uint64_t recv , exp, temp;
	

	l_timeout = 0xfff ;
	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
		switch (argv[0][1]) {
			case 'c':
				if (argv[0][2]=='\0') {
					atob(&argv[1][0], (int *)&chnl);
					argc--; argv++;
                        } else {
						atob(&argv[0][2], (int *)&chnl);
				}

    			if ((chnl == 0) || (chnl == 1) ) {
        			addr = VIDCH1_UPC_BASE ;
        			Reset_SCALER_UPC ( CHANNEL_0 );
        			Reset_050_016_FBC_CPC (CHANNEL_0) ;
        		} else
				if ((chnl == 2) || (chnl == 3) ) {
					        addr = VIDCH2_UPC_BASE ;
							Reset_SCALER_UPC ( CHANNEL_1 );
							Reset_050_016_FBC_CPC (CHANNEL_1 ) ;
				}
			break;
			
			case 'v' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *)&mode);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *)&mode);
				}
			break;
			default:
				msg_printf(SUM, " usage: -c chnl -v value \n");
				break;
		}
		argc--; argv++;
	}

	mcu_WRITE(DDRGP, 0x0);

	msg_printf(DBG, " mode %x \n", mode);
	msg_printf(DBG, " channel %x \n", chnl);

	sizeof64 = sizeof(patrn64)/sizeof(patrn64[0]) ;

		mcu_WRITE( UPC_FLOW_CONTROL + addr, mode ) ;

	for ( i = 0 ; i < sizeof64 ; i++ ) {
		ptr = (UBYTE *) (patrn64 + i) ;
		us_delay(10000);

		for (j = 0 ; j < sizeof(__uint64_t); j++) 
			mcu_WRITE(CPC_JPEG_VERT_RES + addr, *(ptr+j) );
	}

		status = mcu_READ(DDRGP + 1);

		switch (chnl) {
		case 0:
		while (!(status & 0x8) && (l_timeout--))  {
			msg_printf(DBG, " Reading the status of DDRGP \n");
			status = mcu_READ(DDRGP + 1);
		}
			flag = 1;
			break;

		case 1:
		while (!(status & 0x10) && (l_timeout--))  {
			msg_printf(DBG, " Reading the status of DDRGP \n");
			status = mcu_READ(DDRGP + 1);
		}
			flag = 1;
			break;

		case 2:
        while (!(status & 0x20) && (l_timeout--))  {
			msg_printf(DBG, " Reading the status of DDRGP \n");
            status = mcu_READ(DDRGP + 1);
        }
            flag = 1;
			break;

		case 3:
        while (!(status & 0x40) && (l_timeout--))  {
            status = mcu_READ(DDRGP + 1);
        }
            flag = 1;
			break;

		default:
			msg_printf(SUM, " incorrect channel number \n" );
			break;
		}


		if (flag )  {
	for ( i = 0 ; i < sizeof64 ; i++ ) {
		recv = cgi1_read_reg(cosmobase + cgi1_fifo_rw_o(chnl));
		CGI1_COMPARE_LL("mcu2upc2fifo", chnl, recv, *(patrn64+i), _64bit_mask);
		us_delay(10000);
		}
		flag = 0;
	}
		else {
		msg_printf(SUM, " timeout occurred \n" );
		G_errors = 1;
	}


    if (G_errors) {
		msg_printf(ERR, " mcu2upc2fifo test from channel %d failed \n", chnl + 1);
		return (0);
		}
		else {
		msg_printf(SUM, " mcu2upc2fifo test from channel %d Passed \n", chnl + 1);
		return (1);
		}

}
	
#if 0
>
> 3.  MCU to CFIFO
>    - Register configuration:  Set bit 7 to 1,
>                               set bit 6 to 1
>                               Set bit 5 to 1,
>                               Set bit 0 to 1
>                               Load JVR with data
>
#endif

cos2_McuFifoLPB_old ( )
{

        UWORD buf[512], *Wptr, Wcnt;
        UBYTE *Bptr, i, Bcnt, temp;
        /* Write cmd at the MCU_FIFO_base itself */
        _CTest_Info      *pTestInfo = cgi1_info+10;

        Bcnt = 2*sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]) ;

        mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq, MCU_FIFO_BASE, Bcnt, (UBYTE *)walk1s0s_16 );

        Wptr = buf ; /* fill the buffer with the data from the FIFO */

#if 0	/* XXX ?? What do you want to do here? */
        walk1s0s_16 ;
#endif

        DelayFor(100);

        while  (((__uint32_t) cgi1_read_reg(cosmobase+cgi1_interrupt_status_o)) & cgi1_CosmoToGIONotEmpty ) {
                 *Wptr++ = (UWORD) cgi1_fifos_read_mcu (cgi1ptr) ;
                CGI1_COMPARE (pTestInfo->TestStr, MCU_FIFO_BASE, *(Wptr-1), *Bptr, _8bit_mask);
                Bptr++;
        }

        REPORT_PASS_OR_FAIL (pTestInfo, G_errors);

}

cos2_McuFifoLPB ( )
{

       UWORD buf[512], Wptr, Wcnt;
        UBYTE i, Bcnt; 

		Bcnt = 2*sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]) ;

		mcu_fifo_loopback () ;

		for ( i = 0 ; i < Bcnt; i++) {
		cgi1_fifos_write_mcu(cgi1ptr, walk1s0s_16[i] ) ;
		us_delay(1000000); 
		 while  (!(((__uint32_t) cgi1_read_reg(cosmobase+cgi1_interrupt_status_o))
& cgi1_CosmoToGIONotEmpty )) ;

		Wptr = (UWORD) (UWORD) cgi1_fifos_read_mcu (cgi1ptr) ;

		CGI1_COMPARE("mcu fifo lpb", MCU_FIFO_BASE, Wptr, walk1s0s_16[i], 0xffff);
	}

		if (G_errors)
			return (0);

		return (1);

}
