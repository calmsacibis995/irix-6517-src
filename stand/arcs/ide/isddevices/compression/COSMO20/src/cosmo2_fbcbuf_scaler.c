/*
 *
 * Cosmo2 Scaler-FBC tests 
 * Scaler cannot be tested separately
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

#define HORI_BLNK_END 16

extern int cos2_EnablePioMode( void );

__uint64_t dma_send_data[DMA_DATA_SIZE], dma_recv_data[DMA_DATA_SIZE];
int  c2_write_only = 1, c2_read_only = 1;

extern void Reset_SCALER_UPC(UWORD );
extern  void Reset_050_016_FBC_CPC(UWORD );

extern __uint64_t dma_data[ DMA_DATA_SIZE ] ;
extern __uint32_t cosmo2_recv_dma_data(UBYTE,UBYTE,__uint64_t *, __uint32_t);
extern cosmo2_1to2_dma_swap ( __uint64_t *, __uint64_t *, __uint32_t );


extern UBYTE checkDataInFifo (UBYTE);

extern void reset_chnl0(void);
extern void reset_chnl1(void);
extern void reset_chnl2(void);
extern void reset_chnl3(void);

extern void unreset_chnl0(void);
extern void unreset_chnl1(void);
extern void unreset_chnl2(void);
extern void unreset_chnl3(void);


void
load_scaler_reg(__uint32_t upc_base,
				__uint32_t genesis_base,
				UBYTE offset,
				UBYTE data , UBYTE val)
{
	__uint32_t  gvs_access;

	gvs_access =  upc_base + UPC_GENESIS_ACCESS ;

        val |= GVS_AD_SEL ;
        mcu_WRITE ( gvs_access , val );     /* set addr sel */
        mcu_WRITE ( genesis_base, offset);

        val &= ~GVS_AD_SEL ;
        mcu_WRITE ( gvs_access , val );     /* set data sel */
        mcu_WRITE ( genesis_base, data  );  /* write data  */

	msg_printf(DBG, "gvs offset %x data %x \n", offset, data);
}

UBYTE get_scaler_reg(__uint32_t upc_base,
                __uint32_t genesis_base,
                UBYTE offset, UBYTE val)
{
    UBYTE  data;
    __uint32_t  gvs_access;

    gvs_access =  upc_base + UPC_GENESIS_ACCESS ;

        val |= GVS_AD_SEL ;
        mcu_WRITE ( gvs_access , val );       /* set addr sel */
        mcu_WRITE ( genesis_base, offset);

        val &= ~GVS_AD_SEL ;
        mcu_WRITE ( gvs_access , val );           /* set data sel */
        data = mcu_READ( genesis_base );  /* read data  */
		return (data);
}

int
cosmo2_upc_setup (__uint32_t  upcbase, UBYTE flow_cr, UBYTE gvs_access,
					UWORD I_hin, UWORD I_gvs_hout, UWORD I_gvs_vout)
{

	UWORD tmp16;

    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, 0);
    mcu_WRITE(UPC_GENESIS_ACCESS + upcbase, gvs_access);

    tmp16 = I_hin - 1 ;
    mcu_WRITE_WORD(UPC_PIXEL_PER_LINE_REG + upcbase, tmp16);

	us_delay(1000);
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, flow_cr);
	
    mcu_WRITE_WORD ( UPC_GENESIS_OUT_HRES + upcbase, 0);
    mcu_WRITE_WORD (UPC_GENESIS_OUT_VRES + upcbase, 0);

	us_delay(1000);
	if (flow_cr & UPC_GENESIS_ENB ) {
    tmp16 = I_gvs_hout  ;
    mcu_WRITE_WORD ( UPC_GENESIS_OUT_HRES + upcbase, tmp16);

	us_delay(1000);
    tmp16 = I_gvs_vout ;
    mcu_WRITE_WORD (UPC_GENESIS_OUT_VRES + upcbase, tmp16);
	}
	return(0);
}

int cosmo2_print_upc_register (__uint32_t  upcbase)
{
	UWORD tmp16;
#if 1 /* we need to print and check the values */
    tmp16 = mcu_READ_WORD(UPC_GENESIS_OUT_HRES + upcbase);
    msg_printf(3, "UPC_GENESIS_OUT_HRES 0x%x \n", tmp16);
    tmp16 = mcu_READ_WORD(UPC_GENESIS_OUT_VRES + upcbase);
    msg_printf(3, "UPC_GENESIS_OUT_VRES 0x%x \n", tmp16);
    tmp16 = mcu_READ_WORD(UPC_PIXEL_PER_LINE_REG + upcbase);
    msg_printf(3, "UPC_PIXEL_PER_LINE_REG 0x%x \n", tmp16);

    tmp16 = mcu_READ(UPC_FLOW_CONTROL + upcbase );
    msg_printf(3, " UPC_FLOW_CONTROL 0x%x \n", tmp16);

#endif
	return(0);
}


void
cosmo2_fbc_size_setup (__uint32_t  fbcbase, 
			UWORD p_left, UWORD p_top, UWORD p_right, UWORD p_bot,
			UWORD I_hin, UWORD I_vin ) 
{
	UWORD tmp16;

    tmp16 = p_left ;
    mcu_WRITE_WORD (FBC_H_ACTIVE_START + fbcbase, tmp16);

    tmp16 = p_top ;
    mcu_WRITE_WORD (FBC_V_ACTIVE_START + fbcbase, tmp16);

    tmp16 = I_hin - 1 + p_left   ;
    mcu_WRITE_WORD (FBC_H_ACTIVEG_END + fbcbase, tmp16);

    tmp16 = I_vin - 1  + p_top ;
    mcu_WRITE_WORD (FBC_V_ACTIVEG_END + fbcbase, tmp16);

    tmp16 = I_hin - 1 + p_left + p_right ;
    mcu_WRITE_WORD (FBC_H_PADDING_END + fbcbase, tmp16);

    tmp16 = I_vin - 1  + p_top + p_bot  ;
    mcu_WRITE_WORD(FBC_V_PADDING_END + fbcbase, tmp16);

    tmp16 = I_hin-1 + HORI_BLNK_END +p_left + p_right ;
     mcu_WRITE_WORD(FBC_H_BLANKING_END+fbcbase, tmp16);

    tmp16 = I_vin - 1 + p_top  + p_bot ;
    mcu_WRITE_WORD(FBC_V_BLANKING_END +fbcbase, tmp16);

}

void cosmo2_print_fbc_register ( __uint32_t  fbcbase )
{
	UWORD tmp16;
	msg_printf(DBG, " ####### FBC  ######### \n");
    tmp16 = mcu_READ_WORD(FBC_H_ACTIVE_START + fbcbase);
    msg_printf(3, " FBC_H_ACTIVE_START %x \n", tmp16);

    tmp16 = mcu_READ_WORD(FBC_V_ACTIVE_START + fbcbase);
    msg_printf(3, " FBC_V_ACTIVE_START %x \n", tmp16);

    tmp16 = mcu_READ_WORD(FBC_H_ACTIVEG_END + fbcbase);
    msg_printf(3, " FBC_H_ACTIVEG_END %x \n", tmp16);

    tmp16 = mcu_READ_WORD(FBC_V_ACTIVEG_END + fbcbase);
    msg_printf(3, " FBC_V_ACTIVEG_END %x \n", tmp16);

    tmp16 = mcu_READ_WORD(FBC_H_PADDING_END + fbcbase);
    msg_printf(3, " FBC_H_PADDING_END %x \n", tmp16);

    tmp16 = mcu_READ_WORD(FBC_V_PADDING_END + fbcbase);
    msg_printf(3, " FBC_V_PADDING_END %x \n", tmp16);

    tmp16 = mcu_READ_WORD(FBC_H_BLANKING_END + fbcbase);
    msg_printf(3, " FBC_H_BLANKING_END %x \n", tmp16);

    tmp16 = mcu_READ_WORD(FBC_V_BLANKING_END + fbcbase);
    msg_printf(3, " FBC_V_BLANKING_END %x \n", tmp16);


}

cosmo2_poll_genesis(__uint32_t upcbase)
{
	UBYTE val, tmp8;
    val = 0xf0 ;
    tmp8 = mcu_READ(upcbase + UPC_GENESIS_ACCESS );
    while (!(tmp8 & GVS_LD_PAR)) {
        val-- ;
        tmp8 = mcu_READ ( upcbase + UPC_GENESIS_ACCESS ) ;
        if (val == 0) {
            msg_printf(SUM, "timeout occured: UPC_GENESIS_ACCESS %x\n",tmp8);
            G_errors++;
            return (0);
            }
    }

	return (1);
}


void cosmo2_reset_chnl ( void )
{
		reset_chnl0();
		reset_chnl1();
		reset_chnl2();
		reset_chnl3();

		us_delay(10000);
		unreset_chnl0();
		unreset_chnl1();
		unreset_chnl2();
		unreset_chnl3();
		us_delay(10000);
        
}

__uint32_t
cosmo2_fbc_poll( __uint32_t fbcbase)
{
	UBYTE tmp8;
	UBYTE val8 = 0xf0;

    tmp8 = mcu_READ (FBC_MAIN + fbcbase) ;
    while ( tmp8 & GO )  {
        msg_printf(DBG, " mode 9: Pooling FBC_MAIN %x \n", tmp8);
        tmp8 = mcu_READ (FBC_MAIN + fbcbase) ;
		val8--;
		if (val8 == 0) {
        msg_printf(ERR, " Timeout occured in Pooling FBC_MAIN reg %x \n", tmp8);
		G_errors++;
		return (1);
		}
    }
	return(0);
}

void
cosmo2_swap_dma_data ( __uint64_t *to, __uint64_t *from, __uint32_t str)
{
	__uint32_t i, j;

	str = str % DMA_DATA_SIZE ;

	for ( j = 0, i = str ; i < DMA_DATA_SIZE; i++, j++) 
		*(to+j)	= *(from+i);

	for ( i = 0 ; i < str            ; i++, j++) 
		*(to+j)	= *(from+i);
}
		
void
cosmo2_print_genesis_reg (__uint32_t upcbase, 
						__uint32_t genesis_base, UBYTE val)
{
	UBYTE tmp8;
    msg_printf(DBG, "  ########  genisis registers  ######## \n") ;
    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_VERT_SRC_L, val) ;
    msg_printf(DBG, " GEN_Y_VERT_SRC_L %x \n", tmp8 );

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_VERT_SRC_H, val) ;
    msg_printf(DBG, " GEN_Y_VERT_SRC_H %x \n", tmp8 ) ;

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_HOR_SRC_L, val);
    msg_printf(DBG, " GEN_Y_HOR_SRC_L %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_HOR_SRC_H, val);
    msg_printf(DBG, " GEN_Y_HOR_SRC_H %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_VERT_TARG_L, val);
    msg_printf(DBG, " GEN_Y_VERT_TARG_L %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_VERT_TARG_H, val);
    msg_printf(DBG, " GEN_Y_VERT_TARG_H %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_HOR_TARG_L, val);
    msg_printf(DBG, " GEN_Y_HOR_TARG_L %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_Y_HOR_TARG_H, val);
    msg_printf(DBG, " GEN_Y_HOR_TARG_H %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_VERT_SRC_L, val) ;
    msg_printf(DBG, " GEN_UV_VERT_SRC_L %x \n", tmp8 );

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_VERT_SRC_H, val) ;
    msg_printf(DBG, " GEN_UV_VERT_SRC_H %x \n", tmp8 );

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_HOR_SRC_L, val);
    msg_printf(DBG, " GEN_UV_HOR_SRC_L %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_HOR_SRC_H, val);
    msg_printf(DBG, " GEN_UV_HOR_SRC_H %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_VERT_TARG_L, val);
    msg_printf(DBG, " GEN_UV_VERT_TARG_L %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_VERT_TARG_H, val);
    msg_printf(DBG, " GEN_UV_VERT_TARG_H %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_HOR_TARG_L, val);
    msg_printf(DBG, " GEN_UV_HOR_TARG_L %x \n", tmp8);

    tmp8 = get_scaler_reg(upcbase, genesis_base, GEN_UV_HOR_TARG_H, val);
    msg_printf(DBG, " GEN_UV_HOR_TARG_H %x \n", tmp8);

}

void
cosmo2_load_scaler ( __uint32_t upcbase, __uint32_t genesis_base, UBYTE tmp8,
						UWORD I_Vingen, UWORD I_Voutgen, 
						UWORD I_Hingen, UWORD I_Houtgen )
{
        load_scaler_reg(upcbase, genesis_base,GEN_Y_VERT_SRC_L, (I_Vingen & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_Y_VERT_SRC_H, (I_Vingen & 0xff00)>>8, tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_Y_HOR_SRC_L,  (I_Hingen & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_Y_HOR_SRC_H,  (I_Hingen & 0xff00)>>8, tmp8);

        load_scaler_reg(upcbase, genesis_base,GEN_Y_VERT_TARG_L,((I_Voutgen) & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_Y_VERT_TARG_H,((I_Voutgen) & 0xff00)>>8, tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_Y_HOR_TARG_L, (I_Houtgen & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_Y_HOR_TARG_H, (I_Houtgen & 0xff00)>>8, tmp8);

        load_scaler_reg(upcbase, genesis_base,GEN_UV_VERT_SRC_L, (I_Vingen & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_UV_VERT_SRC_H, (I_Vingen & 0xff00)>>8, tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_UV_HOR_SRC_L,  (I_Hingen & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_UV_HOR_SRC_H,  (I_Hingen & 0xff00)>>8, tmp8);

        load_scaler_reg(upcbase, genesis_base,GEN_UV_VERT_TARG_L,((I_Voutgen) & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_UV_VERT_TARG_H,((I_Voutgen) & 0xff00)>>8, tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_UV_HOR_TARG_L, (I_Houtgen & 0x00ff), tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_UV_HOR_TARG_H, (I_Houtgen & 0xff00)>>8, tmp8);

        load_scaler_reg(upcbase, genesis_base,GEN_VBLANK_ODD, 0, tmp8);
        load_scaler_reg(upcbase, genesis_base,GEN_VBLANK_EVEN, 0, tmp8);

}

int
cosmo2_fbc_scaler_setup (__uint32_t upcbase, 
					__uint32_t  fbcbase, 
					__uint32_t genesis_base,
					UBYTE chnl,
					UBYTE field_sel,
					UBYTE bypass )
{
	UBYTE upc2_in_format  = UPC_UVY;
	UBYTE upc2_out_format = UPC_UVY;
	UWORD padleft=0, padtop=0, padbot=0, padright=0;
	UWORD I_HunPad=32, I_VunPad=32;
	UWORD I_Hingen=16,  I_Vingen=32;
	UWORD I_Houtgen=16, I_Voutgen=32;
    UBYTE flow_control , tmp8;
    __uint32_t  i, j;
	__paddr_t  Iterations ;

/*
	__uint64_t *recv_data;
    volatile unsigned char *lio_isr0 = K1_LIO_0_ISR_ADDR;
    recv_data=(__uint64_t *)(((((unsigned long)recv_data1)+127)>>7) <<7);
*/

	I_HunPad=256 ;  I_VunPad = 128;
	I_Hingen=256 ;  I_Vingen = 128;
	I_Houtgen=256; I_Voutgen = 128;

	if (c2_write_only) {
#if 1
	/* need to set the VIDEO_EN bit to zero for mode changes */
    tmp8 = mcu_READ (FBC_MAIN + fbcbase);
    mcu_WRITE(FBC_MAIN + fbcbase, tmp8 & (~VIDEO_EN) ) ;
#endif

    mcu_WRITE(FBC_MAIN + fbcbase, 0)  ;
    mcu_WRITE(FBC_MAIN + fbcbase, 0)  ;
	Iterations = (I_HunPad*I_VunPad*2/(DMA_DATA_SIZE*sizeof(__uint64_t) ))  ;

	/* first send the data to the FBC buffers under mode 9.
	then , set the chips for mode 3.
	*/

	cosmo2_upc_setup (upcbase, upc2_in_format, 0, I_HunPad, 0, 0);

    /* flow_control = mcu_READ(UPC_FLOW_CONTROL + upcbase ); */
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, upc2_in_format | UPC_RUN_ENB );

#if 1 /* check if there is a timing issue */
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, 0);
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, upc2_in_format | UPC_RUN_ENB );
#endif
	cosmo2_fbc_size_setup(fbcbase, padleft, padtop, padright, padbot,
							I_HunPad, I_VunPad); 
	
	/* 
		we loaded the FBC image size parameters. 
		Now load the control registers
		and issue a go command 
	*/

	if (field_sel) {
    	mcu_WRITE(FBC_FLOW + fbcbase, MODE9 | FIELD_SELECT ) ;
		tmp8 = mcu_READ(FBC_FLOW + fbcbase);	
		while (tmp8 != (MODE9 | FIELD_SELECT)) {
    		mcu_WRITE(FBC_FLOW + fbcbase, MODE9 | FIELD_SELECT ) ;
			us_delay(10000);
			tmp8 = mcu_READ(FBC_FLOW + fbcbase);	
		}
	}
	else {
    	mcu_WRITE(FBC_FLOW + fbcbase, MODE9  ) ;
		tmp8 = mcu_READ(FBC_FLOW + fbcbase);	
		while (tmp8 != (MODE9 )) {
    		mcu_WRITE(FBC_FLOW + fbcbase, MODE9 ) ;
			us_delay(10000);
			tmp8 = mcu_READ(FBC_FLOW + fbcbase);	
		}
	}


    tmp8 = mcu_READ (FBC_MAIN + fbcbase) ;
   	mcu_WRITE(FBC_MAIN + fbcbase, GO | tmp8 ) ;

	msg_printf(DBG, " Iterations %d \n", Iterations);
	for ( i = 0 ; i < Iterations ; i++) {
		cosmo2_1to2_dma_swap(dma_send_data, dma_data, i); 
		us_delay(10000);
    	cosmo2_recv_dma_data (chnl, FROM_GIO, dma_send_data, 
						DMA_DATA_SIZE*sizeof(__uint64_t)); 
		if (G_errors)
			return (0);
	}

	cosmo2_fbc_poll (fbcbase);

#if 0
	cosmo2_reset_chnl();
#endif
#if 1
	/* need to set the VIDEO_EN bit to zero for mode changes */
    tmp8 = mcu_READ (FBC_MAIN + fbcbase);
    mcu_WRITE(FBC_MAIN + fbcbase, tmp8 & (~VIDEO_EN) ) ;
#endif
	} /* c2_write_only */

	cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o);
	cgi1_write_reg(0x0, cosmobase+cgi1_dma_control_o);

	msg_printf(DBG, " ######### MODE 3 ########### \n");
	if ( c2_read_only ) {
	if (bypass ) 
	{
	
    	flow_control = UPC_DIR_TOGIO | upc2_out_format  ;
		cosmo2_upc_setup (upcbase, flow_control, 
						0, I_HunPad+padleft+padright, 0, 0);

    	flow_control = flow_control | UPC_RUN_ENB ;
    	mcu_WRITE(UPC_FLOW_CONTROL + upcbase, flow_control );

#if 1 /* check if there is a timing issue */
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, 0);
    mcu_WRITE(UPC_FLOW_CONTROL + upcbase, flow_control);
#endif
		us_delay(10000);
	}
	else {
	/* chuck */
    	flow_control = UPC_DIR_TOGIO | upc2_out_format | UPC_GENESIS_ENB;

		cosmo2_upc_setup (upcbase, flow_control , 
							0, I_HunPad+padleft+padright, 
							I_Houtgen+padleft+padright,
							I_Voutgen+padtop+padbot );

		/* You have to enable the UPC to load the scaler values */

    	flow_control = flow_control | UPC_RUN_ENB ;
    	mcu_WRITE(UPC_FLOW_CONTROL + upcbase, flow_control );
    	flow_control = mcu_READ(UPC_FLOW_CONTROL + upcbase );

		msg_printf(DBG, " flow control %x \n", flow_control);

		/* POLL the UPC register to check if scaler values can be loaded */
		if (!cosmo2_poll_genesis(upcbase)) return (0) ;
    	tmp8 = mcu_READ(upcbase + UPC_GENESIS_ACCESS);

		/* If the values can be loaded , then load the values */
		cosmo2_load_scaler(upcbase, genesis_base, tmp8, I_Vingen+1, I_Voutgen+1,
													I_Hingen, I_Houtgen);

		us_delay(10000);
	/* scaler needs 600 cycles, so reset bit in UPC */
		tmp8 = tmp8 & (~GVS_LD_PAR); 
    	mcu_WRITE( upcbase + UPC_GENESIS_ACCESS  , tmp8);
    	tmp8 = mcu_READ(upcbase + UPC_GENESIS_ACCESS);

		us_delay(10000);

		cosmo2_print_genesis_reg (upcbase, genesis_base, tmp8);

		padbot = 0;
		I_VunPad -= padbot; 
	}
#if 0
    mcu_WRITE(FBC_Y_PAD + fbcbase, 0xa5);
    mcu_WRITE(FBC_U_PAD + fbcbase, 0xa5);
    mcu_WRITE(FBC_V_PAD + fbcbase, 0xa5);



    mcu_WRITE(FBC_MAIN + fbcbase, 0)  ;
    mcu_WRITE(FBC_MAIN + fbcbase, 0)  ;
#endif
    mcu_WRITE(FBC_MAIN + fbcbase, 0)  ;
    mcu_WRITE(FBC_FLOW + fbcbase, 0) ;

	if (field_sel) {
    mcu_WRITE(FBC_FLOW + fbcbase, MODE3 | FIELD_SELECT ) ;
        tmp8 = mcu_READ(FBC_FLOW + fbcbase);    
        while (tmp8 != (MODE3 | FIELD_SELECT)) {
            mcu_WRITE(FBC_FLOW + fbcbase, MODE3 | FIELD_SELECT ) ;
            us_delay(10000);
            tmp8 = mcu_READ(FBC_FLOW + fbcbase);
        }

	}
	else {
    mcu_WRITE(FBC_FLOW + fbcbase, MODE3  ) ;
        tmp8 = mcu_READ(FBC_FLOW + fbcbase);    
        while (tmp8 != (MODE3)) {
            mcu_WRITE(FBC_FLOW + fbcbase, MODE3) ;
            us_delay(10000);
            tmp8 = mcu_READ(FBC_FLOW + fbcbase);
        }

	}
#if 1
	cosmo2_fbc_size_setup(fbcbase, padleft, padtop, padright, padbot,
							I_HunPad, I_VunPad); 

#endif

    tmp8 = mcu_READ (FBC_MAIN + fbcbase);
    mcu_WRITE(FBC_MAIN + fbcbase, GO | tmp8 );

#if 0
	mcu_WRITE(DDRGP, 0x0);
#endif 

    for ( i = 0 ; i < Iterations ; i++) {
		DelayFor(10000) ;
		cosmo2_1to2_dma_swap(dma_send_data, dma_data, i); 
        cosmo2_recv_dma_data(chnl, TO_GIO, dma_recv_data,
					DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
		 if (G_errors) { 
			tmp8 = mcu_READ (FBC_MAIN + fbcbase) ;	
			msg_printf(SUM, " FBC_MAIN %x \n", tmp8);
			tmp8 = mcu_READ (FBC_FLOW + fbcbase) ;	
			msg_printf(SUM, " FBC_FLOW %x \n", tmp8);
			cosmo2_print_fbc_register (fbcbase);
			cosmo2_print_upc_register (upcbase);
			return (0); 
		}
    for ( j = 0 ; j < DMA_DATA_SIZE ; j++ ) {
                if (dma_recv_data[j] != dma_send_data[j] ) {
                    msg_printf(SUM, "ERR i %3x j %2d recv %16llx exp %16llx \n",
	                            i, j, dma_recv_data[j], dma_send_data[j] );
                    G_errors++;
                }else
                    msg_printf(DBG, "    i %3x j %2d recv %16llx exp %16llx \n",
	                            i, j, dma_recv_data[j], dma_send_data[j] );
        }

	for ( j = 0 ; j < DMA_DATA_SIZE ; j++ ) dma_recv_data[j] = 0x0 ;
    }

	cosmo2_fbc_poll (fbcbase);
	} /* c2_read_only */

            tmp8 = mcu_READ (FBC_MAIN + fbcbase) ;
            msg_printf(3, " FBC_MAIN %x \n", tmp8);
            tmp8 = mcu_READ (FBC_FLOW + fbcbase) ;
            msg_printf(3, " FBC_FLOW %x \n", tmp8);
            cosmo2_print_fbc_register (fbcbase);
			cosmo2_print_upc_register (upcbase);

	return(0);
}

fbc_scaler_test ( int argc, char **argv )
{

	int bufs = 0, chnl = 0, bypass = 0, j;


	cosmo2_init();

	argc--; argv++;

	while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
            case 'b':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &bufs);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &bufs);
                }
            break;
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &chnl);
                }
            break;
            case 's' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &bypass);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &bypass);
                }
            break;
            case 'w' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &c2_write_only);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &c2_write_only);
                }
            break;
            case 'r' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], &c2_read_only);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], &c2_read_only);
                }
            break;
            default:
                msg_printf(SUM, " usage: -c chnl -s bypass -b bufs \n") ;
                break ;
        }
        argc--; argv++;
    }

	cgi1_write_reg(0xffffffff, cosmobase+cgi1_interrupt_status_o);
	cgi1_write_reg(0x0, cosmobase+cgi1_dma_control_o);

	for ( j = 0 ; j < DMA_DATA_SIZE ; j++ ) dma_recv_data[j] = 0x0 ;

	if (chnl == 0)
	cosmo2_fbc_scaler_setup (VIDCH1_UPC_BASE, VIDCH1_FBC_BASE, 
				VIDCH1_GENESIS_BASE, 1, (UBYTE) bufs, (UBYTE) bypass );	
	else
	if (chnl == 1)
	cosmo2_fbc_scaler_setup (VIDCH2_UPC_BASE, VIDCH2_FBC_BASE, 
				VIDCH2_GENESIS_BASE, 
				3, (UBYTE) bufs, (UBYTE) bypass);	
	else {
	 msg_printf(SUM, " provide correct channel number \n");
	return ( -1);
	}

        if (G_errors) {
            if (bypass)
            msg_printf(SUM, "fbc FB test Failed in chnl %d buffer %d\n",
							chnl+1, bufs+1 );
            else
            msg_printf(SUM, " scaler test Failed \n");
			return (0);

            }

        else {

            if (bypass)
            msg_printf(5, "fbc FB test passed in chnl %d buffer %d\n",
							chnl+1, bufs + 1);
            else
            msg_printf(SUM, "scaler test passed \n");
			return (1);
        }

}
