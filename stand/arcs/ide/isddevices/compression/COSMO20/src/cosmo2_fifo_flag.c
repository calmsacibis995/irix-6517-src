/*
 * 
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

extern int cos2_EnablePioMode(void );

int
fifo_flags ( __uint64_t *data_patrn, __uint32_t num_patrn, __uint32_t offset, __uint64_t mask, char *str)
{
		
	int chnl = 0, patrn, value = 0x10  ;
	__uint64_t recv;
	

	msg_printf(VRB, "Testing the %s register of each Fifo \n", str);

	/* test each registers for each patrn */ 
	for ( patrn = 0; patrn < num_patrn ; patrn++) {

		cgi1_write_reg(*(data_patrn+patrn), cosmobase+offset);
		msg_printf(DBG, " fifo_flag: %s write %llx \n", str, *(data_patrn+patrn)) ;
		recv = cgi1_read_reg (cosmobase+offset);
		CGI1_COMPARE_LL(str, cosmobase+offset, recv, *(data_patrn+patrn), mask);
	}

	return 0;
}

int cos2_FifoFlagsPatrn (  int argc, char **argv  )
{
	int chnl = 0;

	cos2_EnablePioMode();
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
            break ;
            default :
                msg_printf(SUM, " usage: -c chnl \n") ;
                return (0);
        }
        argc--; argv++;
    }


	switch (chnl) {
	case 0:
	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_ae_o(0), DATA_FIFO_MASK,"fg_ae_0"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_af_o(0), DATA_FIFO_MASK,"fg_af_0"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_af_o(0), DATA_FIFO_MASK,"tg_af_0"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_ae_o(0), DATA_FIFO_MASK,"tg_ae_0"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_ae_o(0), DATA_FIFO_MASK,"fg_ae_0"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_af_o(0), DATA_FIFO_MASK,"fg_af_0"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_ae_o(0), DATA_FIFO_MASK,"tg_ae_0"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_af_o(0), DATA_FIFO_MASK,"tg_af_0"); 
    if (G_errors)
        return (0);
	break;	

	case 1:
	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_ae_o(1), DATA_FIFO_MASK,"fg_ae_1"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_af_o(1), DATA_FIFO_MASK,"tg_af_1"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_af_o(1), DATA_FIFO_MASK,"fg_af_1"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_ae_o(1), DATA_FIFO_MASK,"tg_ae_1"); 
    if (G_errors)
        return (0);
	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_ae_o(1), DATA_FIFO_MASK,"fg_ae_1"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_af_o(1), DATA_FIFO_MASK,"fg_af_1"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_ae_o(1), DATA_FIFO_MASK,"tg_ae_1"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_af_o(1), DATA_FIFO_MASK,"tg_af_1"); 
    if (G_errors)
        return (0);
	break;	

	case 2:
	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_ae_o(2), DATA_FIFO_MASK,"fg_ae_2"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_af_o(2), DATA_FIFO_MASK,"fg_af_2"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_af_o(2), DATA_FIFO_MASK,"tg_af_2"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_ae_o(2), DATA_FIFO_MASK,"tg_ae_2"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_af_o(2), DATA_FIFO_MASK,"fg_af_2"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_ae_o(2), DATA_FIFO_MASK,"fg_ae_2"); 
    if (G_errors)
        return (0);


	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_ae_o(2), DATA_FIFO_MASK,"tg_ae_2"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_af_o(2), DATA_FIFO_MASK,"tg_af_2"); 
    if (G_errors)
        return (0);

	break;
	case 3:
	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_af_o(3), DATA_FIFO_MASK,"tg_af_3"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_af_o(3), DATA_FIFO_MASK,"fg_af_3"); 
    if (G_errors)
        return (0);

	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_tg_ae_o(3), DATA_FIFO_MASK,"tg_ae_3"); 
    if (G_errors)
        return (0);
	fifo_flags ( patrn64, sizeof(patrn64)/sizeof(patrn64[0]), cgi1_fifo_fg_ae_o(3), DATA_FIFO_MASK,"fg_ae_3"); 
    if (G_errors)
        return (0);





	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_ae_o(3), DATA_FIFO_MASK,"tg_ae_3"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_ae_o(3), DATA_FIFO_MASK,"fg_ae_3"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_fg_af_o(3), DATA_FIFO_MASK,"fg_af_3"); 
    if (G_errors)
        return (0);

	fifo_flags ( walk1s0s_64, sizeof(walk1s0s_64)/sizeof(walk1s0s_64[0]), cgi1_fifo_tg_af_o(3), DATA_FIFO_MASK,"tg_af_3"); 
    if (G_errors)
        return (0);

	break;
	}

	if (G_errors)
        return (0);
    else {
        msg_printf(SUM, " Fifo Flag Pattern test Passed in channel %d \n", chnl + 1);
        return (1);
        }
} 

int data_fifo_flag_addr_uniq (__uint32_t value, __uint32_t mask, char *str)
{
	__uint32_t recv, temp;
	
	msg_printf(VRB, " data_fifo_flag_addr_uniq %s : \n", str );
	temp = (__uint32_t)(cosmobase+value) ;
	cgi1_write_reg(temp, cosmobase+value );
	msg_printf(DBG, " data_fifo_flag_addr_uniq : %s write %x \n", str, temp ) ;
	recv = ( __uint32_t) cgi1_read_reg( cosmobase+value );
	CGI1_COMPARE(str, temp, recv, temp, mask);

	return 0;
}


UWORD cos2_FifoFlagAddrUniq (  int argc, char **argv )
{
	int chnl = 0;

	cos2_EnablePioMode();
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
            break ;
            default :
                msg_printf(SUM, " usage: -c chnl \n") ;
                return (0);
        }
        argc--; argv++;
    }


    switch (chnl) {
    case 0:

	data_fifo_flag_addr_uniq (cgi1_fifo_fg_ae_o(0), (__uint32_t) DATA_FIFO_MASK, "fg_ae_uniq_0");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_fg_af_o(0), (__uint32_t) DATA_FIFO_MASK, "fg_af_uniq_0");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_ae_o(0), (__uint32_t) DATA_FIFO_MASK, "tg_ae_uniq_0");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_af_o(0), (__uint32_t) DATA_FIFO_MASK, "tg_af_uniq_0");
    if (G_errors)
        return (0);
	break;
	case 1:

	data_fifo_flag_addr_uniq (cgi1_fifo_fg_ae_o(1), (__uint32_t) DATA_FIFO_MASK, "fg_ae_uniq_1");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_fg_af_o(1), (__uint32_t) DATA_FIFO_MASK, "fg_af_uniq_1");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_ae_o(1), (__uint32_t) DATA_FIFO_MASK, "tg_ae_uniq_1");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_af_o(1), (__uint32_t) DATA_FIFO_MASK, "tg_af_uniq_1");
    if (G_errors)
        return (0);
	break;
	case 2:
	data_fifo_flag_addr_uniq (cgi1_fifo_fg_ae_o(2), (__uint32_t) DATA_FIFO_MASK, "fg_ae_uniq_2");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_fg_af_o(2), (__uint32_t) DATA_FIFO_MASK, "fg_af_uniq_2");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_ae_o(2), (__uint32_t) DATA_FIFO_MASK, "tg_ae_uniq_2");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_af_o(2), (__uint32_t) DATA_FIFO_MASK, "tg_af_uniq_2");
    if (G_errors)
        return (0);

	break;
	case 3:
	data_fifo_flag_addr_uniq (cgi1_fifo_fg_ae_o(3), (__uint32_t) DATA_FIFO_MASK, "fg_ae_uniq_3");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_fg_af_o(3), (__uint32_t) DATA_FIFO_MASK, "fg_af_uniq_3");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_ae_o(3), (__uint32_t) DATA_FIFO_MASK, "tg_ae_uniq_3");
    if (G_errors)
        return (0);

	data_fifo_flag_addr_uniq (cgi1_fifo_tg_af_o(3), (__uint32_t) DATA_FIFO_MASK, "tg_af_uniq_3");
    if (G_errors)
        return (0);
	break;
	}

	if (G_errors)
		return (0);
	else {
		msg_printf(SUM, " Fifo Flag Addr Uniq test Passed in channel %d \n", chnl + 1);
		return (1);
		}
}

int
mcu_fifo_flag_addr_uniq (__uint32_t value, UWORD mask, char *str)
{

	__uint32_t recv  , temp;
	msg_printf(VRB, "mcu_fifo_flag_addr_uniq %s \n", str);
	temp =  (__uint32_t)(cosmobase+value) ;

	cgi1_write_reg( temp, cosmobase+value );
	msg_printf(DBG, "mcu_fifo_flag_addr_uniq %s write %x \n", str, temp);
	recv = (__uint32_t) cgi1_read_reg( cosmobase+value );
	CGI1_COMPARE(str, temp, recv, temp , mask);

	return 0;
}

UWORD cos2_McuFifoFlagAddrUniq ( )
{
	UWORD mask = 0x1ff;

	mcu_fifo_flag_addr_uniq (cgi1_mcu_fifo_fg_ae_o, mask, "mcu_fg_ae_uniq");
    if (G_errors)
        return (0);

	mcu_fifo_flag_addr_uniq (cgi1_mcu_fifo_fg_af_o, mask, "mcu_fg_af_uniq");
    if (G_errors)
        return (0);

	mcu_fifo_flag_addr_uniq (cgi1_mcu_fifo_tg_ae_o, mask, "mcu_tg_ae_uniq");
    if (G_errors)
        return (0);

	mcu_fifo_flag_addr_uniq (cgi1_mcu_fifo_tg_af_o, mask, "mcu_tg_af_uniq");

    if (G_errors)
        return (0);
    else {
        msg_printf(SUM, " MCU Fifo Addr Uniq test Passed \n");
        return (1);
        }


}


int mcu_fifo_flag ( UWORD *data_patrn, __uint32_t num_patrn, __uint32_t offset, __uint32_t mask, char *str)
{
        int patrn, value = 0x10, recv = 0xbeef ;
        unsigned long long recv1;
	UWORD recvs;


        msg_printf(DBG, "Testing the mcu PAE WM of each Fifo \n");


	for ( patrn = 0; patrn < num_patrn ; patrn++) {
               	msg_printf(DBG, " patrn %x\n", patrn) ;

		cgi1_write_reg(*(data_patrn+patrn), cosmobase+offset);
		recvs = cgi1_read_reg(cosmobase+offset);
               	CGI1_COMPARE (str, cosmobase+offset, recvs, *(data_patrn+patrn), mask);
        }

	return 0;
}

int cos2_McuFifoflagPatrn ( )
{


	mcu_fifo_flag(walk1s0s_16, sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]), cgi1_mcu_fifo_fg_ae_o, _9BIT_MASK, "mcu_fg_ae"); 
    if (G_errors)
        return (0);
	mcu_fifo_flag(walk1s0s_16, sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]), cgi1_mcu_fifo_fg_af_o, _9BIT_MASK, "mcu_fg_af"); 
    if (G_errors)
        return (0);
	mcu_fifo_flag(walk1s0s_16, sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]), cgi1_mcu_fifo_tg_af_o, _9BIT_MASK, "mcu_tg_af"); 
    if (G_errors)
        return (0);
	mcu_fifo_flag(walk1s0s_16, sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]), cgi1_mcu_fifo_tg_ae_o, _9BIT_MASK, "mcu_tg_ae"); 
    if (G_errors)
        return (0);

	mcu_fifo_flag(patrn16, sizeof(patrn16)/sizeof(patrn16[0]), cgi1_mcu_fifo_fg_ae_o, _9BIT_MASK, "mcu_fg_ae"); 
    if (G_errors)
        return (0);
	mcu_fifo_flag(patrn16, sizeof(patrn16)/sizeof(patrn16[0]), cgi1_mcu_fifo_fg_af_o, _9BIT_MASK, "mcu_fg_af"); 
    if (G_errors)
        return (0);
	mcu_fifo_flag(patrn16, sizeof(patrn16)/sizeof(patrn16[0]), cgi1_mcu_fifo_tg_af_o, _9BIT_MASK, "mcu_tg_af"); 
    if (G_errors)
        return (0);
	mcu_fifo_flag(patrn16, sizeof(patrn16)/sizeof(patrn16[0]), cgi1_mcu_fifo_tg_ae_o, _9BIT_MASK, "mcu_tg_ae"); 


    if (G_errors)
        return (0);
    else {
        msg_printf(SUM, " MCU Fifo Flag Pattern test Passed \n");
        return (1);
        }


}

