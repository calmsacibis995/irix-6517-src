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

extern cosmo2_fbc_poll (__uint32_t);
extern __uint32_t cosmo2_recv_dma_data(UBYTE,UBYTE,__uint64_t *, __uint32_t);

extern int cos2_EnablePioMode( void );

extern void cosmo2_reset_chnl (void);

void cosmo2_cbar_vivo_setup (UBYTE ,UBYTE  );
void cosmo2_cbar_chan_setup (UBYTE , UBYTE );
extern __uint64_t dma_data[];
extern cosmo2_1to2_dma_swap (__uint64_t *, __uint64_t *, __uint32_t);
extern __uint64_t dma_send_data[DMA_DATA_SIZE], dma_recv_data[DMA_DATA_SIZE];
extern void cosmo2_tv_std( UBYTE tv_std, UWORD *horz, UWORD *vert, UBYTE genlock);


extern	int _mgv_ext_vin_gonogo( unsigned char ch_sel, unsigned char tv_std, unsigned char format , int inFlag);
extern	__uint32_t	mgv_init();

cosmo2_mod(__uint32_t x, __uint32_t y)
{
 __uint32_t t;
  t = x % y ;
 return(t);
}


void
cosmo2_fbc_poll_video_done_pio (__uint32_t fbcbase)
{
  UBYTE time = 0xff, val8;

   val8 = mcu_READ(fbcbase + FBC_INT_STAT);

   while (( time > 0) && (!(val8 & VIDEO_DONE_IRQ)) ) {
   val8 = mcu_READ(fbcbase + FBC_INT_STAT);
    time--;
  }

    if (time == 0) {
        msg_printf(SUM, " TIMEOUT OCCURRED IN VIDEO SIDE \n");
		G_errors++;
		}
}

void
cosmo2_vid_ce_fbc2_setup (__uint32_t fbcbase,
                                UWORD horz, UWORD vert,
                                UBYTE padright, UBYTE padleft,
                                UBYTE padtop, UBYTE padbot )
{

    UWORD tmp16;

    mcu_WRITE(FBC_MAIN + fbcbase, 0);

    /* setup the fbc registers */

    mcu_WRITE_WORD (FBC_H_ACTIVE_START + fbcbase, padleft );

    mcu_WRITE_WORD (FBC_H_ACTIVEG_END + fbcbase, (horz - 1 + padleft) );

    tmp16 = horz - 1 + padleft + padright ;
    mcu_WRITE_WORD (FBC_H_PADDING_END + fbcbase,  tmp16);

    tmp16 = horz-1 + 16 +padleft + padright ;
    mcu_WRITE_WORD (FBC_H_BLANKING_END+fbcbase, tmp16);

    mcu_WRITE_WORD (FBC_V_ACTIVE_START + fbcbase, padtop );
    mcu_WRITE_WORD (FBC_V_ACTIVEG_END + fbcbase, (vert - 1 + padtop ) );

    tmp16 = vert - 1 + padtop + padbot;
    mcu_WRITE_WORD (FBC_V_PADDING_END + fbcbase, tmp16);

    tmp16 = vert - 1 + padtop + padbot;
    mcu_WRITE_WORD (FBC_V_BLANKING_END +fbcbase, tmp16) ;

    mcu_WRITE(FBC_VERT_CLIP_END + fbcbase, 0 );

    mcu_WRITE(FBC_INT_EN + fbcbase, 0);
    mcu_WRITE(FBC_INT_STAT + fbcbase, 0xff);

    mcu_WRITE(FBC_FLOW + fbcbase, (FB_WR_EN | FIELD_SELECT | MODE3) ) ;

    mcu_WRITE(FBC_MAIN + fbcbase, VIDEO_EN ) ;

}

void
print_fbc_all_reg(__uint32_t fbcbase)
{

msg_printf(SUM, "FBC_H_ACTIVE_START %x \n", 
		mcu_READ_WORD (FBC_H_ACTIVE_START + fbcbase) );
msg_printf(SUM, "FBC_H_ACTIVEG_END %x \n", 
		mcu_READ_WORD (FBC_H_ACTIVEG_END + fbcbase) );
msg_printf(SUM, "FBC_H_PADDING_END %x \n", 
		mcu_READ_WORD (FBC_H_PADDING_END + fbcbase) );
msg_printf(SUM, "FBC_H_BLANKING_END %x \n", 
		mcu_READ_WORD (FBC_H_BLANKING_END + fbcbase) );
msg_printf(SUM, "FBC_V_ACTIVE_START %x \n", 
		mcu_READ_WORD (FBC_V_ACTIVE_START + fbcbase) );
msg_printf(SUM, "FBC_V_PADDING_END %x \n", 
		mcu_READ_WORD (FBC_V_PADDING_END + fbcbase) );
msg_printf(SUM, "FBC_V_BLANKING_END %x \n", 
		mcu_READ_WORD (FBC_V_BLANKING_END + fbcbase) );
msg_printf(SUM, "FBC_VERT_CLIP_END %x \n", 
		mcu_READ (FBC_VERT_CLIP_END + fbcbase) );
msg_printf(SUM, "FBC_VERT_CLIP_END %x \n", 
		mcu_READ (FBC_VERT_CLIP_END + fbcbase) );
msg_printf(SUM, "FBC_INT_EN %x \n", 
		mcu_READ (FBC_INT_EN + fbcbase) );
}

void
cosmo2_vid_ce_fbc1_setup (__uint32_t fbcbase, 
								UWORD horz, UWORD vert, 
								UBYTE *patrn, UBYTE padright, UBYTE padleft,
								UBYTE padtop, UBYTE padbot )
{

	UWORD tmp16; 
	UBYTE ypad, upad, vpad;

	ypad = *(patrn+0);
    upad = *(patrn+1);
    vpad = *(patrn+2);

    mcu_WRITE(FBC_MAIN + fbcbase, 0);

	/* setup the fbc registers */
    mcu_WRITE(FBC_Y_PAD + fbcbase, ypad);
    mcu_WRITE(FBC_U_PAD + fbcbase, upad);
    mcu_WRITE(FBC_V_PAD + fbcbase, vpad);
	
    mcu_WRITE_WORD (FBC_H_ACTIVE_START + fbcbase, padleft );

    mcu_WRITE_WORD (FBC_H_ACTIVEG_END + fbcbase, (horz - 1 + padleft) );

    tmp16 = horz - 1 + padleft + padright ;
    mcu_WRITE_WORD (FBC_H_PADDING_END + fbcbase,  tmp16);

    tmp16 = horz-1 + 16 +padleft + padright ;
    mcu_WRITE_WORD (FBC_H_BLANKING_END+fbcbase, tmp16);

    mcu_WRITE_WORD (FBC_V_ACTIVE_START + fbcbase, padtop );
    mcu_WRITE_WORD (FBC_V_ACTIVEG_END + fbcbase, (vert - 1 + padtop ) );

    tmp16 = vert - 1 + padtop + padbot;
    mcu_WRITE_WORD (FBC_V_PADDING_END + fbcbase, tmp16);

    tmp16 = vert - 1 + padtop + padbot;
    mcu_WRITE_WORD (FBC_V_BLANKING_END +fbcbase, tmp16) ;

    mcu_WRITE(FBC_VERT_CLIP_END + fbcbase, 0 );

	mcu_WRITE(FBC_INT_EN + fbcbase, 0);
	mcu_WRITE(FBC_INT_STAT + fbcbase, 0xff);

   	 	mcu_WRITE(FBC_FLOW + fbcbase, VIDEO_CONSTANT_EN  | MODE9 );
    	mcu_WRITE(FBC_MAIN + fbcbase, VIDEO_EN );
}

void
cosmo2_cbar_setup(UWORD horz, UWORD vert, UBYTE chnl)
{
	mcu_WRITE(CBAR_BASE +CBAR_SOFTRESET, 0x4);
	if (chnl == 0) {
    mcu_WRITE_WORD(CBAR_BASE+VC1_HOR_SYNC_START, 138 - 8);
    mcu_WRITE_WORD(CBAR_BASE+VC1_HOR_SYNC_LEN, horz + 8);
    mcu_WRITE_WORD(CBAR_BASE+VC1_HOR_DATA_LEN, horz );

    mcu_WRITE_WORD(CBAR_BASE+VC1_ODD_SYNC_START,20);
    mcu_WRITE_WORD(CBAR_BASE+VC1_ODD_DATA_START,20);

    mcu_WRITE_WORD(CBAR_BASE+VC1_EVEN_SYNC_START,19);
    mcu_WRITE_WORD(CBAR_BASE+VC1_EVEN_DATA_START,19);

    mcu_WRITE_WORD(CBAR_BASE+VC1_ODD_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC1_EVEN_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC1_VERT_DATA_LEN,vert+1);

    mcu_WRITE_WORD(CBAR_BASE+VC2_HOR_SYNC_START, 139 );
    mcu_WRITE_WORD(CBAR_BASE+VC2_HOR_SYNC_LEN, horz);
    mcu_WRITE_WORD(CBAR_BASE+VC2_HOR_DATA_LEN, horz);

    mcu_WRITE_WORD(CBAR_BASE+VC2_ODD_SYNC_START,20);
    mcu_WRITE_WORD(CBAR_BASE+VC2_ODD_DATA_START,20);

    mcu_WRITE_WORD(CBAR_BASE+VC2_EVEN_SYNC_START,19);
    mcu_WRITE_WORD(CBAR_BASE+VC2_EVEN_DATA_START,19);

    mcu_WRITE_WORD(CBAR_BASE+VC2_ODD_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC2_EVEN_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC2_VERT_DATA_LEN,vert);
	}else{
    mcu_WRITE_WORD(CBAR_BASE+VC2_HOR_SYNC_START, 138 - 8);
    mcu_WRITE_WORD(CBAR_BASE+VC2_HOR_SYNC_LEN, horz + 8);
    mcu_WRITE_WORD(CBAR_BASE+VC2_HOR_DATA_LEN, horz );

    mcu_WRITE_WORD(CBAR_BASE+VC2_ODD_SYNC_START,20);
    mcu_WRITE_WORD(CBAR_BASE+VC2_ODD_DATA_START,20);

    mcu_WRITE_WORD(CBAR_BASE+VC2_EVEN_SYNC_START,19);
    mcu_WRITE_WORD(CBAR_BASE+VC2_EVEN_DATA_START,19);

    mcu_WRITE_WORD(CBAR_BASE+VC2_ODD_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC2_EVEN_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC2_VERT_DATA_LEN,vert+1);

    mcu_WRITE_WORD(CBAR_BASE+VC1_HOR_SYNC_START, 139 );
    mcu_WRITE_WORD(CBAR_BASE+VC1_HOR_SYNC_LEN, horz);
    mcu_WRITE_WORD(CBAR_BASE+VC1_HOR_DATA_LEN, horz);

    mcu_WRITE_WORD(CBAR_BASE+VC1_ODD_SYNC_START,20);
    mcu_WRITE_WORD(CBAR_BASE+VC1_ODD_DATA_START,20);

    mcu_WRITE_WORD(CBAR_BASE+VC1_EVEN_SYNC_START,19);
    mcu_WRITE_WORD(CBAR_BASE+VC1_EVEN_DATA_START,19);

    mcu_WRITE_WORD(CBAR_BASE+VC1_ODD_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC1_EVEN_SYNC_LEN,vert);
    mcu_WRITE_WORD(CBAR_BASE+VC1_VERT_DATA_LEN,vert);

	}

	if (chnl == 0) {
	mcu_WRITE(CBAR_BASE+CBAR_VC1_SRC, SYNC_SRC_VO );
	mcu_WRITE(CBAR_BASE+CBAR_VC2_SRC, SYNC_SRC_VC1|DATA_SRC_VC1);
	mcu_WRITE(CBAR_BASE+CBAR_VO_SRC , SYNC_SRC_VC1|DATA_SRC_VC1); 
	}else
	if (chnl == 1) {
	mcu_WRITE(CBAR_BASE+CBAR_VC2_SRC,SYNC_SRC_VO );
	mcu_WRITE(CBAR_BASE+CBAR_VC1_SRC,SYNC_SRC_VC2|DATA_SRC_VC2);
	mcu_WRITE(CBAR_BASE+CBAR_VO_SRC, SYNC_SRC_VC2|DATA_SRC_VC2); 
	}

}


extern void i2cl_init(void);
extern void avi(void );
extern void vo_internal(void);

UBYTE cosmo2_vid_ce(  int argc, char **argv )
{

	UBYTE padtop, padbot, padright, padleft  ;
	UWORD Horz, Vert   ;
	__uint32_t fbcbase1, fbcbase2, chnl = 0, upcbase2;
	UBYTE flow_control;
	__uint32_t  i, iterations, j, size, tv_std = 1 ;
	UBYTE  *ptr, val8, chnl1;
	__uint32_t patrn1, mode = 0, line, pixel = 0, genlock = 0 ;
	__uint64_t val64, temp ;

	size = DMA_DATA_SIZE*sizeof(__uint64_t) ; 

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
            break ;
            case 's':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *)&tv_std);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *)&tv_std);
                }
            break ;
            case 'm':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *)&mode);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *)&mode);
                }
            break ;

            case 'p':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *)&patrn1);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *)&patrn1);
                }
            break ;
            case 'g':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *)&genlock);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *)&genlock);
                }
            break ;


            default :
                msg_printf(SUM, " usage: -c chnl -p (YUV)\n") ;
                return (0);
        }
        argc--; argv++;
    }

    if (chnl == 0) {
    	fbcbase1 = VIDCH1_FBC_BASE;
    	fbcbase2 = VIDCH2_FBC_BASE;
#if 0
    	upcbase1 = VIDCH1_UPC_BASE;
#endif
    	upcbase2 = VIDCH2_UPC_BASE;
		chnl1 = 3;
    }
    else 
	if (chnl == 1) {
    	fbcbase1 = VIDCH2_FBC_BASE;
    	fbcbase2 = VIDCH1_FBC_BASE;
#if 0
    	upcbase1 = VIDCH2_UPC_BASE;
#endif
    	upcbase2 = VIDCH1_UPC_BASE;
		chnl1 = 1;
    }

	G_errors = 0;
    	padtop = 0 ; padbot = 0 ; padright = 0 ; padleft = 0 ;

	ptr = ((UBYTE *) &patrn1) + 1;


	temp = *ptr << 16 | (*(ptr+1) << 24) |  *ptr  | *(ptr+2) << 8  ;
		temp = (temp & 0xffffffff) << 32 | (temp & 0xffffffff) ;
		msg_printf(DBG, " temp %llx channel %d \n", temp, cgi1_fifo_rw_o( chnl1)  );

	
		cosmo2_reset_chnl ();
		cosmo2_tv_std(tv_std, &Horz, &Vert, genlock);
	iterations = (__uint32_t)(Horz*Vert*2/(DMA_DATA_SIZE*sizeof(__uint64_t)));
	msg_printf(VRB, " Iterations %d \n", iterations);


#if 1
	cosmo2_cbar_vivo_setup (0, 0);
	cosmo2_cbar_chan_setup (0, 0);
#endif
		cosmo2_cbar_setup( Horz, Vert, chnl);

		cosmo2_vid_ce_fbc1_setup(fbcbase1, Horz, Vert, ptr,
					padright, padleft , padtop , padbot ) ;
		cosmo2_vid_ce_fbc2_setup(fbcbase2, Horz, Vert, 
					padright, padleft , padtop , padbot ) ;
#if 0
		print_fbc_all_reg(fbcbase1);
		print_fbc_all_reg(fbcbase2);
#endif
        val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
		while ((val8 & FIELD_SELECT) == 0) {
			msg_printf(SUM, " trying to flip the buffer \n");
        	mcu_WRITE(FBC_FLOW + fbcbase2, (FB_WR_EN | FIELD_SELECT | MODE3) ) ;
        	val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
		}

        cosmo2_fbc_poll_video_done_pio (fbcbase2) ;
        mcu_WRITE (FBC_INT_STAT + fbcbase2, 0xff) ;
        cosmo2_fbc_poll_video_done_pio (fbcbase2) ;
	 	

        val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
		while ((val8 & FIELD_SELECT) != 0) {
			DelayFor(100000);
			msg_printf(DBG, " trying to flip the buffer \n");
        	mcu_WRITE(FBC_FLOW + fbcbase2, MODE3 );
        	val8 = mcu_READ (FBC_FLOW + fbcbase2);
		}

        flow_control = UPC_DIR_TOGIO | UPC_UVY ;
        cosmo2_upc_setup (upcbase2, flow_control,
                        0, Horz+padleft+padright - 1, 0, 0);

        flow_control |= UPC_RUN_ENB ;
        mcu_WRITE(UPC_FLOW_CONTROL + upcbase2, flow_control);

        val8 = mcu_READ ( FBC_MAIN + fbcbase2 );
        mcu_WRITE( FBC_MAIN + fbcbase2, GO | val8 ) ;

		if (mode == 1) { 
			cos2_EnablePioMode();
			pixel = 0;
        	for ( i = 0 ; i < iterations ; i++) {
            	for ( j = 0 ; j < DMA_DATA_SIZE ; j++ , pixel+=8) {
					line = (i*size)/(Horz*2);
                	val64= cgi1_read_reg(cosmobase+cgi1_fifo_rw_o( chnl1) );
                	if (val64 != temp ) {
                   		msg_printf(ERR, "ERR i %3x j %3d recv %16llx exp %16llx \n",
                                line, pixel, val64, temp );
                   	 	G_errors++;
                	}else
                    msg_printf(DBG, "i %3x j %3d recv %16llx exp %16llx \n",
                                i, j, val64, temp );
             	}
			j = cosmo2_mod(pixel, (Horz*2) );
			pixel = j;
         }
		} else {
			pixel = 0 ;
        	for ( i = 0 ; i < iterations ; i++) {
           		cosmo2_recv_dma_data(chnl1, TO_GIO, dma_recv_data,
						DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
				line = (i*size)/(Horz*2);
				for ( j = 0 ; j < DMA_DATA_SIZE ; j++ , pixel+=8) {
					if (dma_recv_data[j] != temp ) {
						msg_printf(ERR, "ERR i %3x j %3d recv %16llx exp %16llx \n",
                                line, pixel, dma_recv_data[j], temp );
                   		 G_errors++;
					msg_printf(ERR, " constant video test from channel %d failed \n",
                chnl+1);

					return (0);

                	}else
                    msg_printf(DBG, "i %3x j %3d recv %16llx exp %16llx \n",
                                i, j, dma_recv_data[j], temp );
           		  }
			j = cosmo2_mod(pixel, (Horz*2) );
			pixel = j ;
			}
		}

    cosmo2_fbc_poll (fbcbase2);

	
	mcu_WRITE(FBC_MAIN + fbcbase1, 0 );
	mcu_WRITE(FBC_MAIN + fbcbase2, 0 );

	if (G_errors)  {
		msg_printf(ERR, " constant video test from channel %d failed \n",
				chnl+1);
		return (0);
		}
	else {
		msg_printf(SUM, " constant video test from channel %d passed \n", 
							chnl+1);
		return (1);
		}
}

/* video tests ; This tests captures in one channel  and plays back
 on the other channel 
*/

UBYTE cosmo2_vid(  int argc, char **argv )
{

	UBYTE padtop, padbot, padright, padleft  ;
	UWORD Horz, Vert   ;
	__uint32_t fbcbase1, fbcbase2, chnl = 0  , upcbase1, upcbase2, genlock = 0;
	UBYTE flow_control;
	__uint32_t  i, iterations, j, size;
	UBYTE  *ptr, val8, chnl1;
	__uint32_t patrn1 = 0xffffffff,  line, pixel = 0, tv_std = 0;

	size = DMA_DATA_SIZE*sizeof(__uint64_t) ; 

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
            break ;
			case 'g' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *)&genlock);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *)&genlock);
                }
            break ;
            case 's' :
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *)&tv_std);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *)&tv_std);
                }
            break ;

            default :
                msg_printf(SUM, " usage: -c chnl -g 0/1\n") ;
                return (0);
        }
        argc--; argv++;
    }

    if (chnl == 0) {
    	fbcbase1 = VIDCH1_FBC_BASE;
    	fbcbase2 = VIDCH2_FBC_BASE;
    	upcbase1 = VIDCH1_UPC_BASE;
    	upcbase2 = VIDCH2_UPC_BASE;
		chnl1 = 3;
    }
    else 
	if (chnl == 1) {
    	fbcbase1 = VIDCH2_FBC_BASE;
    	fbcbase2 = VIDCH1_FBC_BASE;
    	upcbase1 = VIDCH2_UPC_BASE;
    	upcbase2 = VIDCH1_UPC_BASE;
		chnl1 = 1;
    }

	G_errors = 0;
    	padtop = 0 ; padbot = 0 ; padright = 0 ; padleft = 0 ;

		cosmo2_reset_chnl ();
		cosmo2_tv_std(tv_std, &Horz, &Vert, genlock);
	iterations = (__uint32_t)(Horz*Vert*2/(DMA_DATA_SIZE*sizeof(__uint64_t)));

#if 1
	cosmo2_cbar_vivo_setup (0, 0);
	cosmo2_cbar_chan_setup (0, 0);
#endif
    ptr = ((UBYTE *) &patrn1) + 1;

		cosmo2_cbar_setup( Horz, Vert, chnl);

        cosmo2_upc_setup (upcbase1, UPC_UVY, 0, Horz-1, 0, 0);
        flow_control = mcu_READ(UPC_FLOW_CONTROL + upcbase1 );
        mcu_WRITE(UPC_FLOW_CONTROL + upcbase1, flow_control | UPC_RUN_ENB );

        cosmo2_vid_ce_fbc1_setup(fbcbase1, Horz, Vert, ptr,
                    padright, padleft , padtop , padbot ) ;
   	 	mcu_WRITE(FBC_FLOW + fbcbase1, MODE9 );
        mcu_WRITE(FBC_MAIN + fbcbase1, VIDEO_EN );

         val8 = mcu_READ (FBC_MAIN + fbcbase1) ;
         mcu_WRITE(FBC_MAIN + fbcbase1, GO | val8 ) ;
		if (chnl) chnl = 3; else chnl = 1;
         msg_printf(VRB, " Iterations %d \n", iterations);
         for ( i = 0 ; i < iterations ; i++) {
			
             cosmo2_1to2_dma_swap(dma_send_data, dma_data, i);
             cosmo2_recv_dma_data (chnl, FROM_GIO, dma_send_data,
                        DMA_DATA_SIZE*sizeof(__uint64_t));
            if (G_errors) {
				msg_printf(SUM, " FBC is not taking the data %d \n", i);
                return(0) ;
			}
         }

         cosmo2_fbc_poll (fbcbase1);

        val8 = mcu_READ (FBC_FLOW + fbcbase1) ;
        while ((val8 & FIELD_SELECT) == 0) {
            msg_printf(DBG, " trying to flip the buffer in channel %d \n",
                        (chnl) ? 1 : 0);
            mcu_WRITE(FBC_FLOW + fbcbase1, FIELD_SELECT | MODE9 ) ;
            val8 = mcu_READ (FBC_FLOW + fbcbase1) ;
        }

        cosmo2_vid_ce_fbc2_setup(fbcbase2, Horz, Vert,
                    padright, padleft , padtop , padbot ) ;

#if 0
        print_fbc_all_reg(fbcbase1);
        print_fbc_all_reg(fbcbase2);
#endif

        val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
        while ((val8 & FIELD_SELECT) == 0) {
			DelayFor(100000);
            msg_printf(DBG, " trying to flip the buffer in channel %d \n",
                        (chnl) ? 0 : 1);
            mcu_WRITE(FBC_FLOW + fbcbase2, (FB_WR_EN | FIELD_SELECT | MODE3) ) ;
            val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
		}

        cosmo2_fbc_poll_video_done_pio (fbcbase2) ;
        mcu_WRITE (FBC_INT_STAT + fbcbase2, 0xff) ;
        cosmo2_fbc_poll_video_done_pio (fbcbase2) ;
	 	
        val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
		while ((val8 & FIELD_SELECT) != 0) {
			DelayFor(100000);
			msg_printf(DBG, "2 trying to select the buffer \n");
        	mcu_WRITE(FBC_FLOW + fbcbase2, MODE3 );
        	val8 = mcu_READ (FBC_FLOW + fbcbase2);
		}

        flow_control = UPC_DIR_TOGIO | UPC_UVY ;
        cosmo2_upc_setup (upcbase2, flow_control,
                        0, Horz+padleft+padright - 1, 0, 0);

        flow_control |= UPC_RUN_ENB ;
        mcu_WRITE(UPC_FLOW_CONTROL + upcbase2, flow_control);

        val8 = mcu_READ ( FBC_MAIN + fbcbase2 );
        mcu_WRITE( FBC_MAIN + fbcbase2, GO | val8 ) ;

			pixel = 0 ;
        	for ( i = 0 ; i < iterations ; i++) {
             cosmo2_1to2_dma_swap(dma_send_data, dma_data, i);
           		cosmo2_recv_dma_data(chnl1, TO_GIO, dma_recv_data,
						DMA_DATA_SIZE*sizeof(__uint64_t) ) ;
				line = (i*size)/(Horz*2);
				for ( j = 0 ; j < DMA_DATA_SIZE ; j++ , pixel+=8) {
					if ( dma_recv_data[j] != dma_send_data[j] ) {
						msg_printf(ERR, "ERR i %3x j %3d recv %16llx exp %16llx \n",
                                line, pixel, dma_recv_data[j], dma_send_data[j]  );
                   		 G_errors++;
						msg_printf(ERR, " video test from channel %d failed \n", chnl+1);	
						return (0);
                	}else
                    msg_printf(DBG, "i %3x j %3d recv %16llx exp %16llx \n",
                                i, j, dma_recv_data[j], dma_send_data[j] );
           		  }
			j = cosmo2_mod(pixel, (Horz*2) );
			pixel = j ;
			}
		

    cosmo2_fbc_poll (fbcbase2);

	
	mcu_WRITE(FBC_MAIN + fbcbase1, 0 );
	mcu_WRITE(FBC_MAIN + fbcbase2, 0 );

	if (G_errors) { 
		msg_printf(ERR, " video test from channel %d failed \n", chnl+1);
		return (0);
		}
	else {
		msg_printf(SUM, " video test from channel %d passed \n", chnl+1);
		return (1);
	}

}


