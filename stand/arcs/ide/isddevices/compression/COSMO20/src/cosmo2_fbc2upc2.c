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

#define BYTES_PER_BLOCK 48

extern UBYTE huff_tbl[];
extern UBYTE quant_tbl[];
extern int cos2_EnablePioMode( void );

extern void Reset_SCALER_UPC(UWORD );
extern  void Reset_050_016_FBC_CPC(UWORD );

extern READ_CC1_FRAME_BUFFER_SETUP( UBYTE, UWORD, UWORD ) ;

extern __uint32_t ImageHorz, ImageVert;

UBYTE   upc2_in_format  = UPC_UVY;
UBYTE   upc2_out_format = UPC_UVY;
UWORD padleft=2, padtop=2, padbot=2, padright=2;
UBYTE ypad= 0x56, upad= 0x89, vpad = 0xef;
UWORD I_HunPad=2, I_VunPad=1;

UBYTE fbc2upc_BusTest ( __uint32_t upcbase,
                	__uint32_t  fbcbase,
					UBYTE *patrn,
                	UBYTE chnl)
{
    UBYTE flow_control , tmp8;
    __uint32_t tmp32;
    __uint64_t tmp64;
	UWORD value;

	
	ypad = *(patrn);
	upad = *(patrn+1);
	vpad = *(patrn+2);

	msg_printf(VRB, "U %x Y %x V %x Y %x \n", upad, ypad, vpad, ypad);

    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, 0);
    mcu_WRITE(UPC_GENESIS_ACCESS + upcbase, 0);

    value = I_HunPad-1+padleft+padright ;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq,
                UPC_PIXEL_PER_LINE_REG + upcbase, 2, (UBYTE *) &value);

    flow_control = UPC_DIR_TOGIO | upc2_out_format ;
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, flow_control);

    flow_control |= UPC_RUN_ENB;
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, flow_control );

    value = padleft;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
                        FBC_H_ACTIVE_START + fbcbase, 2, (UBYTE *) &value);

    value = padtop;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
            FBC_V_ACTIVE_START + fbcbase, 2, (UBYTE *) &value);

	mcu_WRITE(FBC_Y_PAD + fbcbase, ypad);
	mcu_WRITE(FBC_U_PAD + fbcbase, upad);
	mcu_WRITE(FBC_V_PAD + fbcbase, vpad);

    value = I_HunPad - 1 + padleft ;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
            FBC_H_ACTIVEG_END + fbcbase, 2, (UBYTE *) &value);

    value = I_VunPad - 1 + padtop;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
            FBC_V_ACTIVEG_END + fbcbase, 2, (UBYTE *) &value );

    value = I_HunPad - 1 + padleft + padright ;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
            FBC_H_PADDING_END + fbcbase, 2, (UBYTE *) &value);

    value = I_VunPad - 1 + padtop + padbot;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
            FBC_V_PADDING_END + fbcbase, 2, (UBYTE *) &value);

	value = I_HunPad-1 +16 +padleft + padright ;
	mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
		FBC_H_BLANKING_END+fbcbase, 2, (UBYTE *) &value);

	value = I_VunPad - 1 + padtop + padbot;
	mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
		FBC_V_BLANKING_END +fbcbase, 2, (UBYTE *) &value) ;

    mcu_WRITE(FBC_FLOW + fbcbase,  3 );

    tmp8 = mcu_READ (FBC_MAIN + fbcbase);
    mcu_WRITE(FBC_MAIN + fbcbase, GO | UC_PAD_EN | tmp8);

    tmp8 = mcu_READ (FBC_MAIN + fbcbase);
    while ( tmp8 & GO )  {
    msg_printf(SUM, " Pooling FBC_MAIN in decomp mode %x \n", tmp8);
    tmp8 = mcu_READ (FBC_MAIN + fbcbase);
    }
    msg_printf(VRB, " Pooling FBC_MAIN in decomp mode %x \n", tmp8);

  	tmp64 = cgi1_read_reg(cosmobase + cgi1_fifo_rw_o(chnl));
			msg_printf(VRB, "tmp64 %x \n", tmp64);
	
	tmp32 = (tmp64 >> 32) & 0xffffffff ;

	if ( tmp32 !=  (((__uint32_t)upad << 24)|
					((__uint32_t)ypad << 16) |
					((__uint32_t)vpad << 8) |
					((__uint32_t)ypad )) )
	{
		msg_printf(ERR, "fbc2upc2fifo Test failed %x \n", tmp32);
		G_errors++ ; 
		return ( 0 );
	}

	return 1;
}


UBYTE fbc2upc2fifo_test ( int argc, char **argv)
{
	UBYTE ptr[3] ;
	__uint32_t num_of_patrns, i, chnl;

	    argc--; argv++;

	    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
        switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atobu(&argv[1][0], &chnl);
                    argc--; argv++;
                } else {
                   atobu(&argv[0][2], &chnl);
                }
		msg_printf(SUM, "fbc2upc2fifo test on channel %d   \n", chnl+1);

		break;
            default:
                msg_printf(SUM, " usage: fbc2upc2fifo -c channel \n");
                break;
        }
        argc--; argv++;
    }
	
    cos2_EnablePioMode ();

	num_of_patrns = 2 * sizeof(patrn16)/sizeof(patrn16[0]) ;

       for ( i = 0; i < num_of_patrns - 2; i+=3) {
                padleft=2; padtop=0; padbot=0; padright=0;
                I_HunPad=2; I_VunPad=1;

                Reset_050_016_FBC_CPC(chnl);

            if (chnl == 0) 
                fbc2upc_BusTest (VIDCH1_UPC_BASE, VIDCH1_FBC_BASE, 
					((UBYTE *) patrn16) +i, 1);
            else
            if (chnl == 1)
                fbc2upc_BusTest (VIDCH2_UPC_BASE, VIDCH2_FBC_BASE, 
					((UBYTE *) patrn16) +i, 3);

			if (G_errors)
				return (0);	
		}

		msg_printf(SUM, "fbc2upc2fifo test passed \n");
		return (1);

}

