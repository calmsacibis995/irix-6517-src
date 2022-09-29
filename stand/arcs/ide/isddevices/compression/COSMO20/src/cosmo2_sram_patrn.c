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

#define SRAM_BEGIN      0x100000
#define SRAM_END        (0x13FFFE-0x4000)
#define	SRAM_CODE		0x0008

#define NUM_OF_WORDS	256

extern void putCMD (UWORD *) ;
extern UBYTE  getCMD (UWORD *) ;
/*
extern void mcu_write_cmd (UWORD, UWORD, __uint32_t, __uint32_t, UWORD *) ;
extern void mcu_read_cmd (UWORD, UWORD, __uint32_t, __uint32_t );
*/


cos2_SramWalk (int argc, char **argv)
{

	 _CTest_Info      *pTestInfo = cgi1_info+11 ;
	__uint32_t str_adr, end_adr, i, j,  cnt, Str_Adr = SRAM_BEGIN , End_Adr = SRAM_END ;
	__uint64_t str_adr_64, end_adr_64;
	UWORD num_of_patrns, loop ;
	UWORD buf[SRAM_END - SRAM_BEGIN], recv[SRAM_END - SRAM_BEGIN];
	UWORD data_patrn[WALK_16_SIZE] ;

	if (argc < 2) {
		Str_Adr = SRAM_BEGIN;
		End_Adr = SRAM_END; 
	}
	else 
	{
		atobu_L (argv[1], (unsigned long long *)&str_adr_64); 
		atobu_L (argv[2], (unsigned long long *)&end_adr_64);
		Str_Adr = (__uint32_t)str_adr_64 ;
		End_Adr = (__uint32_t)end_adr_64 ;
	}
	cmd_seq = 0;

	num_of_patrns = sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]) ;

	for ( loop = 0 ; loop < num_of_patrns ; loop++, cmd_seq = 0)	{
	
            msg_printf(SUM, " running iteration %d : num_of_patrns %d\n",   loop , num_of_patrns );
	
	for ( j = 0 ; j < num_of_patrns ; j++)	
	data_patrn[j] = walk1s0s_16[ (loop + j) % num_of_patrns];

        for (str_adr = Str_Adr;  str_adr < End_Adr ; str_adr+=num_of_patrns) {

                cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o) ;

                mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq++, str_adr, 2*num_of_patrns, (UBYTE *)data_patrn);

                mcu_read_cmd (COSMO2_CMD_OTHER, cmd_seq, str_adr, 2*num_of_patrns);

                getCMD(recv);

                if (recv[0] == CMD_SYNC)
                        if ((recv[2] == COSMO2_CMD_RDBYTES) && (recv[3] == cmd_seq)) {

                                cnt = (recv[1] & 0x00ff) - 5 ;

                                msg_printf(DBG, " str_addr %x cnt %d seqno %d\n",   str_adr, cnt, cmd_seq);

                                for ( i = 0; i < cnt; i++) {
                                        CGI1_COMPARE (pTestInfo->TestStr, str_adr+i, recv[7+i], data_patrn[i], _16bit_mask);

					if (G_errors)
        				REPORT_PASS_OR_FAIL (pTestInfo, G_errors);

				}

                        }else {
                               		msg_printf(SUM, "missing seqno or code :seqno %d recv[2] %x recv[3] %d\n", 
								cmd_seq, recv[2], recv[3]);
						G_errors = 1;
        					REPORT_PASS_OR_FAIL (pTestInfo, G_errors);
				}
        	}
	}
        REPORT_PASS_OR_FAIL (pTestInfo, G_errors);
}


cos2_SramPatrn (int argc, char **argv)
{

	_CTest_Info      *pTestInfo = cgi1_info+11 ;
	__uint32_t str_adr, end_adr, i, j, cnt, Str_Adr, End_Adr;
	__uint64_t str_adr_64, end_adr_64;
	UWORD num_of_patrns , loop;
	UWORD  recv[SRAM_END - SRAM_BEGIN];
	UWORD data_patrn[PATRNS_16_SIZE];

	if (argc < 2) {
		Str_Adr = SRAM_BEGIN;
		End_Adr = SRAM_END; 
	}
	else 
	{
		atobu_L (argv[1], (unsigned long long *)&str_adr_64); 
		atobu_L (argv[2], (unsigned long long *)&end_adr_64);
		Str_Adr = (__uint32_t)str_adr_64 ;
		End_Adr = (__uint32_t)end_adr_64 ;
	}

	num_of_patrns = sizeof(patrn16)/sizeof(patrn16[0]) ;
	cmd_seq = 0;

	for ( loop = 0 ; loop < num_of_patrns ; loop++, cmd_seq = 0)	{
            msg_printf(SUM, " running iteration %d : num_of_patrns %d\n",   loop , num_of_patrns );
	
	for ( j = 0 ; j < num_of_patrns ; j++)	
	data_patrn[j] = patrn16[(loop + j)%num_of_patrns];

        for (str_adr = Str_Adr;  str_adr < End_Adr ; str_adr+=num_of_patrns) {

                cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o) ;
                mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq++, str_adr, 2*num_of_patrns, (UBYTE *)data_patrn);
                mcu_read_cmd (COSMO2_CMD_OTHER, cmd_seq, str_adr, 2*num_of_patrns);

                getCMD(recv);

                if (recv[0] == CMD_SYNC)
                        if ((recv[2] == COSMO2_CMD_RDBYTES) && (recv[3] == cmd_seq)) {

                                cnt = (recv[1] & 0x00ff) - 5 ;

                                msg_printf(DBG, " str_addr %x cnt %d seqno %d\n",   str_adr, cnt, cmd_seq);

                                for ( i = 0; i < cnt; i++){
                                        CGI1_COMPARE (pTestInfo->TestStr, str_adr+i, recv[7+i], data_patrn[i], _16bit_mask);
					if (G_errors)
        				REPORT_PASS_OR_FAIL (pTestInfo, G_errors);
				}

                        }else
                             msg_printf(DBG, "missing seqno or code :seqno %d recv[2] %x recv[3] %d\n", cmd_seq, recv[2], recv[3]);
        	}
	}
        REPORT_PASS_OR_FAIL (pTestInfo, G_errors);
}


send_sram_cmd ( __uint32_t Saddr, __uint32_t Eaddr, __uint32_t num, UBYTE *data)
{

	UWORD buf[512], *ptr ;

	ptr = buf ;

	*ptr++ = CMD_SYNC ;
	msg_printf(DBG, " Sram cmd  0x%x :", *(ptr-1));
	*ptr++ = (COSMO2_CMD_OTHER <<8) + (7 + (num+1)/2 ) ;
	msg_printf(DBG, " 0x%x :", *(ptr-1)) ;
	*ptr++ = SRAM_CODE ;
	msg_printf(DBG, " 0x%x :", *(ptr-1)) ;
	*ptr++ = cmd_seq ;
	msg_printf(DBG, "0x%x:", *(ptr-1)) ;
	*ptr++ = (UWORD) ((Saddr & 0xffff0000) >> 16) ;
	msg_printf(DBG, " 0x%x", *(ptr-1)) ;
	*ptr++ = (UWORD) (Saddr & 0xffff) ;
	msg_printf(DBG, " 0x%x :", *(ptr-1)) ;
	*ptr++ = (UWORD) ((Eaddr & 0xffff0000) >> 16) ;
	msg_printf(DBG, " 0x%x", *(ptr-1)) ;
	*ptr++ = (UWORD) (Eaddr & 0xffff) ;
	msg_printf(DBG, " 0x%x :", *(ptr-1)) ;
	*ptr++ = num ;
	msg_printf(DBG, " 0x%x\n", *(ptr-1)) ;

        msg_printf(DBG, " data: ");
	while (num)
         if (num!=1) {
            msg_printf(DBG, " 0x%x%x\n", *data, *(data+1));
            *ptr++ = (*data << 8 ) + *(data+1);
             num-=2 ; data+=2;
        } else {
            msg_printf(DBG, " %x\n", *data);
            *ptr = ((UWORD) *data) << 8 ;
            msg_printf(DBG, "  data 0x%x num %d\n", *ptr, num);
            num-- ;
            }

	putCMD (buf) ;

	return 0;
}

UBYTE receive_sram_result ( )
{
	UWORD buf[512], *ptr ;
	getCMD(buf);

	msg_printf(SUM, " buf[0] %x\n", buf[0]);
	msg_printf(SUM, " buf[1] %x\n", buf[1]);
	msg_printf(SUM, " buf[2] %x\n", buf[2]);
	msg_printf(SUM, " buf[3] %x\n", buf[3]);
	msg_printf(SUM, " buf[4] %x\n", buf[4]);

	if (buf[4] == 0)
	{
		msg_printf(SUM, " buf[5] %x\n", buf[5]);
		msg_printf(SUM, " buf[6] %x\n", buf[6]);
		msg_printf(SUM, " buf[7] %x\n", buf[7]);

		G_errors++;
		return 1;
	}

	msg_printf(SUM, " sram test passed\n"); 
	return 0;
}

mcu_sram_patrn( )
{
	
	__uint32_t str_adr = SRAM_BEGIN , end_adr = SRAM_END-1  ;

	UWORD buf[512], *ptr ;
	__uint32_t  temp, iter, i, cnt;

	UWORD patrn[5]= {
    0x1234, 0x5678, 0xa5a5, 0x9abc, 0xdef0
	};
	
	msg_printf(SUM, " sram test in progress....\n");
	for ( iter =  0; iter < 10; iter++) {
		msg_printf(DBG, "iteration %x\n", iter) ;

		for ( i = 0 ; i < 5 ; i++ ) {
			buf[i] = patrn[(i + iter) % (5) ];
			msg_printf(DBG, "i %d buf %x\n",i,buf[i] );
		}
		send_sram_cmd (str_adr, end_adr, 2*(5), (UBYTE *) buf);   
 
	us_delay(100000);
	getCMD(buf);
    if (buf[4] == 0)
    {
    	msg_printf(SUM, " buf[0] %x\n", buf[0]);
    	msg_printf(SUM, " buf[1] %x\n", buf[1]);
    	msg_printf(SUM, " buf[2] %x\n", buf[2]);
    	msg_printf(SUM, " buf[3] %x\n", buf[3]);
    	msg_printf(SUM, " buf[4] %x\n", buf[4]);
        msg_printf(SUM, " buf[5] %x\n", buf[5]);
        msg_printf(SUM, " buf[6] %x\n", buf[6]);
        msg_printf(SUM, " buf[7] %x\n", buf[7]);

        G_errors++;
    }
		msg_printf(SUM, "." ) ;

	if (G_errors)
		{
		msg_printf(ERR, " sram patrn test failed\n");
        G_errors=0;
		return (0);
		}

	}
	msg_printf(SUM, "\nsram tests passed\n");
	return 0;
}

mcu_sram_walk1( )
{
	
	__uint32_t str_adr = SRAM_BEGIN , end_adr = SRAM_END-1  ;

	UWORD buf[512], *ptr ;
	__uint32_t  temp, iter, i, cnt;

	UWORD c2_walk1[16]= {
    0x0001, 0x0002, 0x0004, 0x0008,
    0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800,
    0x1000, 0x2000, 0x4000, 0x8000
	};

	msg_printf(SUM, " sram test in progress....\n");
	for ( iter =  0; iter < 16; iter++) {
		msg_printf(DBG, "iteration %x\n", iter) ;

		for ( i = 0 ; i < 16 ; i++ ) {
			buf[i] = c2_walk1[(i + iter) % (16) ];
			msg_printf(DBG, "i %d buf %x\n",i,buf[i] );
		}
		msg_printf(SUM, " num 0x%x\n", 2*(16));
		send_sram_cmd (str_adr, end_adr, 2*(16), (UBYTE *) buf);   
 
	us_delay(100000);
	getCMD(buf);
    if (buf[4] == 0)
    {
    	msg_printf(SUM, " buf[0] %x\n", buf[0]);
    	msg_printf(SUM, " buf[1] %x\n", buf[1]);
    	msg_printf(SUM, " buf[2] %x\n", buf[2]);
    	msg_printf(SUM, " buf[3] %x\n", buf[3]);
    	msg_printf(SUM, " buf[4] %x\n", buf[4]);
        msg_printf(SUM, " buf[5] %x\n", buf[5]);
        msg_printf(SUM, " buf[6] %x\n", buf[6]);
        msg_printf(SUM, " buf[7] %x\n", buf[7]);

        G_errors++;
    }
		msg_printf(SUM, "." ) ;

	if (G_errors)
		{
		msg_printf(ERR, " sram patrn test failed\n");
        G_errors=0;
		return (0);
		}

	}
	msg_printf(SUM, "\nsram tests passed\n");

	return 0;
}

mcu_sram_walk0( )
{
	
	__uint32_t str_adr = SRAM_BEGIN , end_adr = SRAM_END-1  ;

	UWORD buf[512], *ptr ;
	__uint32_t  temp, iter, i, cnt;

	UWORD c2_walk0[16]= {
    0xfffe, 0xfffd, 0xfffb, 0xfff7,
    0xffef, 0xffdf, 0xffbf, 0xff7f,
    0xfeff, 0xfdff, 0xfbff, 0xf7ff,
    0xefff, 0xdfff, 0xbfff, 0x7fff
	};

	msg_printf(SUM, " sram test in progress....\n");
	for ( iter =  0; iter < 16; iter++) {
		msg_printf(DBG, "iteration %x\n", iter) ;

		for ( i = 0 ; i < 16 ; i++ ) {
			buf[i] = c2_walk0[(i + iter) % (16) ];
			msg_printf(DBG, "i %d buf %x\n",i,buf[i] );
		}
		send_sram_cmd (str_adr, end_adr, 2*(16), (UBYTE *) buf);   
 
	us_delay(100000);
	getCMD(buf);
    if (buf[4] == 0)
    {
    	msg_printf(SUM, " buf[0] %x\n", buf[0]);
    	msg_printf(SUM, " buf[1] %x\n", buf[1]);
    	msg_printf(SUM, " buf[2] %x\n", buf[2]);
    	msg_printf(SUM, " buf[3] %x\n", buf[3]);
    	msg_printf(SUM, " buf[4] %x\n", buf[4]);
        msg_printf(SUM, " buf[5] %x\n", buf[5]);
        msg_printf(SUM, " buf[6] %x\n", buf[6]);
        msg_printf(SUM, " buf[7] %x\n", buf[7]);

        G_errors++;
    }
		msg_printf(SUM, "." ) ;

	if (G_errors)
		{
		msg_printf(ERR, " sram patrn test failed\n");
        G_errors=0;
		return (0);
		}
	}
		msg_printf(SUM, "\n" ) ;
	msg_printf(SUM, " sram tests passed\n");

	return 0;
}
