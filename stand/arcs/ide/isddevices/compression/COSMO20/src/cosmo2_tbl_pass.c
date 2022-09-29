
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


extern UBYTE huff_tbl[];
extern UBYTE quant_tbl[];
extern UBYTE com_tbl[] ;

extern int cos2_EnablePioMode( void );
 
#define getbyte(x, y) 	\
	 (UBYTE)  (x >> (y) )  ;

UBYTE tbl[1024];  
__uint32_t sum = 0;

extern void Reset_SCALER_UPC(UWORD);
extern void Reset_050_016_FBC_CPC(UWORD);

void cos2_LedOn ( );
void cos2_LedOff ( ) ;
void cos2_SetMcuAt16MHZ( ) ;

UBYTE mcu_READ(__uint32_t);

UBYTE debug1 = 1;


__uint32_t
tbl_pass_setup_050 (__uint32_t addr, UBYTE mode) 
{

	__uint32_t i  ;
	UBYTE temp, val8;

	sum = 0;

	msg_printf(DBG, " setting in master mode \n");
	mcu_WRITE(addr+Z50_HARDWARE, mode);

	msg_printf(DBG, " setting in TABLE PASS mode \n");
        mcu_WRITE( addr+Z50_MODE, Z50_Tables_Only_Pass );

	msg_printf(DBG, " marker to enable: COMM, DQT, DHT \n");
        mcu_WRITE(addr+Z50_MARKERS_EN,  COMM | DQT | DHT  );

	msg_printf(DBG, " Set the interrupts to : NO \n");
        mcu_WRITE(addr+Z50_INT_REQ_0, 0);

        mcu_WRITE(addr+Z50_INT_REQ_1, END);

	mcu_WRITE(addr+Z50_SF_H, 1);
	mcu_WRITE(addr+Z50_SF_L, 0);

	DelayFor(10000);

	tbl[sum++] = 0xff ; tbl[sum++] = 0xd8 ; 	/* start of image  */

	for ( i = 0; i < SIZE_OF_COM; i++ )
		tbl[sum++] = com_tbl[i] ;
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x3c0, 
						SIZE_OF_COM , com_tbl );


        for ( i = 0; i < SIZE_OF_QUANT_TABLE ; i++)
             tbl[sum++] = quant_tbl[i] ;
	msg_printf(DBG, " Sum %d quant table %d \n", sum, i );

	msg_printf(DBG, " Sending the quantization tables to the 050 \n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x0CC, 
							SIZE_OF_QUANT_TABLE ,  quant_tbl );

		if (debug1) {
			i = 0;
			while ( i < SIZE_OF_QUANT_TABLE ) {
			val8 = mcu_READ(addr+0x0CC+ i);
			if (val8 != tbl[sum-SIZE_OF_QUANT_TABLE+i]) {
				msg_printf(ERR, " LOAD ERROR : recv %x exp %x \n", val8, tbl[sum-SIZE_OF_QUANT_TABLE+i]);
				return (0);
				}
			i++;
			}
		
		}

	DelayFor(10000);

        /* need to send the Huffman table */

                for ( i = 0; i < SIZE_OF_HUFF_TABLE  ; i++)
                        tbl[sum++] = huff_tbl[i];
	msg_printf(DBG, " Sum %d huff table %d \n", sum, i );

        /*      6+412 bytes is sum of LUM DC 28, CHR DC 28, LUM AC 178, CHR AC 178 */
	msg_printf(DBG, " Sending the huffman tables to the 050 \n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x1D4, SIZE_OF_HUFF_TABLE , huff_tbl );

        if (debug1) {
            i = 0;
            while ( i < SIZE_OF_HUFF_TABLE ) {
            val8 = mcu_READ(addr+0x1D4+ i);
            if (val8 != tbl[sum-SIZE_OF_HUFF_TABLE+i]) {
                msg_printf(ERR, " recv %x exp %x \n", val8, tbl[sum-SIZE_OF_HUFF_TABLE+i]);
				return (0);
			}
            i++;
            }
        
        }



	/* end of an image */
        tbl[sum++] = 0xff;
        tbl[sum++] = 0xd9;



	msg_printf(DBG, " Sum %d \n", sum  );

	DelayFor(10000);

	temp = mcu_READ(addr+Z50_HARDWARE);
	msg_printf(DBG, "  Z50_HARDWARE %x \n", temp);

	temp = mcu_READ(addr+Z50_MODE);
	msg_printf(DBG, "  Z50_MODE %x \n", temp);

	temp = mcu_READ(addr+Z50_MARKERS_EN);
	msg_printf(DBG, "  Z50_MARKERS_EN %x \n", temp);

	temp = mcu_READ(addr+Z50_INT_REQ_0);
	msg_printf(DBG, " Z50_INT_REQ_0 %x \n", temp);

        temp = mcu_READ(addr+Z50_INT_REQ_1 );
	msg_printf(DBG, " Z50_INT_REQ_1 %x \n", temp);

        temp = mcu_READ(addr+Z50_SF_H );
	msg_printf(DBG, " Z50_SF_H %x \n", temp);

        temp = mcu_READ(addr+Z50_SF_L );
	msg_printf(DBG, " Z50_SF_L %x \n", temp);

	/* issue a go command to the 050 */
        mcu_WRITE(addr+Z50_GO, 0);

	return (1);
}

void
tbl_pass_setup_upc ( __uint32_t addr )
{

        __uint32_t  temp;


#if PX
	/* issue a go command to the CPC */
	int CPC_LOOPB_TOGIO = 0x01 ;
        mcu_WRITE(addr + CPC_CTL_STATUS, CPC_RUN_ENB | CPC_LOOPB_TOGIO );
#else
	mcu_WRITE(addr + CPC_CTL_STATUS, CPC_DIR_TOGIO );
	mcu_WRITE(addr + CPC_CTL_STATUS, CPC_RUN_ENB | CPC_DIR_TOGIO );
#endif

	temp = mcu_READ(addr+CPC_CTL_STATUS);
	msg_printf(DBG, " CPC_CTL_STATUS %x \n", temp);

}

__uint32_t cos2_TblPass ( int argc, char **argv)
{
	__uint32_t value, z050base, upcbase, fifobase, recvl, num, chnl;
	__uint64_t recv;
	UBYTE recvB;
	UWORD  i, cnt, t_timeout = 0xff;
	_CTest_Info      *pTestInfo = cgi1_info + 27 ;

	argc-- ;
	argv++ ;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int*)&chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int*)&chnl);
                }
            break;
            default:
                break ;
        }
        argc--; argv++;
    }


	cos2_EnablePioMode( );

	msg_printf(VRB, " TABLE PASS MODE  \n" ) ;

	if (chnl == 0) {
		z050base = VIDCH1_050_BASE ;
		upcbase = VIDCH1_UPC_BASE ;
		fifobase = cgi1_fifo_rw_o(0) ;
	}
	else
	if (chnl == 1) {
		z050base = VIDCH2_050_BASE ;
		upcbase = VIDCH2_UPC_BASE ;
		fifobase = cgi1_fifo_rw_o(2) ;
	}

	msg_printf(VRB, "chnl %d 050-base %x upcbase %x fifobase %x \n", 
			chnl, z050base, upcbase, fifobase); 

		Reset_SCALER_UPC(chnl);
		Reset_050_016_FBC_CPC(chnl);

	tbl_pass_setup_050( z050base , Z50_MSTR);
	tbl_pass_setup_upc ( upcbase );


	value = 0;
    while  (( value == 0) && (t_timeout--)) {
            recvB = mcu_READ(z050base + Z50_STATUS_1);
        if ( recvB & END) value = 1 ;
        msg_printf(VRB, " pooling END status of 050 0x%x t_timeout %x \n", recvB,
						t_timeout  );
    }

    if ((value == 0) && (t_timeout == 0)) {
    msg_printf(SUM, "END status never occured in chnl %x t_timeout %x \n", 
						chnl+1, t_timeout);
    G_errors++;
    REPORT_PASS_OR_FAIL(pTestInfo, G_errors);
    }

    msg_printf(VRB, " value %x chnl %x t_timeout %x \n", value, chnl+1, t_timeout );

	DelayFor(10000);
	cnt = 0; num = 0; 

	msg_printf(DBG, " sum is  %d \n ", sum ) ;

	for ( value = 0 ; value <  sum / cgi1_register_bytes + 1 ; value++)  {

			if ( (sum-cnt) < cgi1_register_bytes) 
					/* read the last few bytes */
				num =  (sum % cgi1_register_bytes)  ; 		
			else
				num = cgi1_register_bytes ; /* else read 8 bytes from fifo */

		msg_printf(DBG, " num is  %d \n", sum - cnt );

		recv = cgi1_read_reg(cosmobase+fifobase);
		for ( i = cgi1_register_bytes ; i != (cgi1_register_bytes - num);i--, cnt++) {
			recvl = getbyte(recv, 8*(i-1)) ;	
			CGI1_COMPARE ("Table Pass", fifobase, recvl, tbl[cnt], _8bit_mask);
			msg_printf(VRB, " cnt %d value %d rcv %x exp %x \n", 
						cnt, value, recvl, tbl[cnt]  ); 
		}

		if (num == (sum % cgi1_register_bytes ) ) 
		msg_printf(DBG, " high %x low %x \n ", 
					(__uint32_t)(recv >> 32), (__uint32_t) recv );
	}

	REPORT_PASS_OR_FAIL(pTestInfo, G_errors);
}


__uint32_t cos2_TblSlavePass ( int argc, char **argv)
{
	__uint32_t  z50base, fbcbase, chnl;
	/*REFERENCED*/
	__uint32_t fifobase;
	UBYTE  datardy, val8;
	UWORD   cnt   =0,
	timeout = 0xff, polled = 0, end_timeout=0xffff, end_polled=0 ;

	_CTest_Info      *pTestInfo = cgi1_info + 27 ;

	argc-- ;
	argv++ ;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int*)&chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int*)&chnl);
                }
            break;
            default:
                break ;
        }
        argc--; argv++;
    }

	cos2_EnablePioMode( );

	msg_printf(VRB, " TABLE PASS MODE  \n" ) ;

	if (chnl == 0) {
		z50base = VIDCH1_050_BASE ;
		fbcbase = VIDCH1_FBC_BASE ;
		fifobase = cgi1_fifo_rw_o(0) ;
	}
	else
	if (chnl == 1) {
		z50base = VIDCH2_050_BASE ;
		fbcbase = VIDCH2_FBC_BASE ;
		fifobase = cgi1_fifo_rw_o(2) ;
	}


		Reset_SCALER_UPC(chnl);
		Reset_050_016_FBC_CPC(chnl);

	tbl_pass_setup_050( z50base , 0);
						G_errors = 0 ;


	DelayFor(10000);
	cnt = 0;

	msg_printf(DBG, " sum is  %d \n ", sum ) ;
        datardy = 0;

        val8 = mcu_READ(fbcbase+FBC_INT_STAT) ;
        while ((end_timeout != end_polled) && (!(val8 & IRQ_050))) {

            val8 = mcu_READ( z50base + Z50_STATUS_1 );

                datardy = 0; timeout = 0xff ; polled = 0;
            while (( timeout != polled) && (datardy == 0) ) {
                if (val8 & DATRDY) {
                    datardy = 1;
                    val8 = mcu_READ(z50base+Z50_COMPRESSED_DATA);
                	msg_printf(VRB, " recv %x exp %x \n", val8, tbl[cnt]);
					if (val8 != tbl[cnt++]) 
						G_errors++ ;
                }else
                    val8 = mcu_READ(z50base+Z50_STATUS_1);

                    polled++;
            }

            if (!datardy) {
                msg_printf(SUM, "data ready bit is not set, timeout occurred %d \n",
                    polled);
                G_errors++;
				REPORT_PASS_OR_FAIL(pTestInfo, G_errors);
                }
        val8 = mcu_READ(fbcbase+FBC_INT_STAT) ;
        end_polled++;
        }

	REPORT_PASS_OR_FAIL(pTestInfo, G_errors);
}

#if 1
check_bit_errors( __uint64_t v1, __uint64_t v2)
{

UBYTE i = 0, cnt = 0;
 while ( i < 64 ) {
    if ( ((v1 >> i) & 1) != ((v2 >> i) & 1) ) {
        cnt++;
		msg_printf(VRB, " i %d v1 %llx  v2  %llx  \n",i, ((v1 >> i) & 1), ((v2 >> i) & 1) );
		}
	i++;
    }
	return(cnt);
}


void
check_error()
{
	UBYTE i;
	i = check_bit_errors(0xffffeeffefff2fffLL, 0x3fffffffffffffeeLL);
	msg_printf(SUM, "i %x \n", i);
}
#endif
