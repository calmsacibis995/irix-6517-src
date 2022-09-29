
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

#define BYTES_PER_BLOCK 48

extern __uint64_t dma_data[];

extern void cc1_slave (void);
extern int cos2_EnablePioMode( void );

extern cosmo2_cbar_setup(UWORD , UWORD , UBYTE );


extern void Reset_SCALER_UPC(UWORD );
extern  void Reset_050_016_FBC_CPC(UWORD );

extern void READ_CC1_FRAME_BUFFER_SETUP(UBYTE, UWORD, UWORD );

extern void cosmo2_const_cbar_setup ();
extern void cosmo2_reset_chnl (void);

void cosmo2_cbar_vivo_setup (UBYTE ,UBYTE  );
void cosmo2_cbar_chan_setup (UBYTE , UBYTE );


void constant_enable_test_fbc_setup (__uint32_t fbcbase, 
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

    tmp16 = horz-1 + 32 +padleft + padright ;
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

	/* enable the cosntant and set mode playback/9 */
    mcu_WRITE(FBC_FLOW + fbcbase, VIDEO_CONSTANT_EN  | 0x09 );

    mcu_WRITE(FBC_MAIN + fbcbase, VIDEO_EN );
}

void setup_cc1_const_enable( UBYTE fld_frm_sel,
                            UWORD startline,
                            UWORD block)
{



        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x30);

#if 1
        mcu_WRITE (CC1_BASE + CC1_FB_SEL, 0 );
        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 49);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, ((startline >> 1)  & 0xFF) );
#endif

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  48);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,  0x4 );

#if 0
        mcu_WRITE (CC1_BASE + CC1_FB_CTL, (((startline & 0x1)<<5)|0xC0)|block);
        mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0x0 );
#endif
		mcu_WRITE(CC1_BASE + CC1_SYS_CTL, 0x40 );
#if 0
		value = mcu_READ(CC1_BASE + CC1_SYS_CTL);
		while (value & 0x40)
		value = mcu_READ(CC1_BASE + CC1_SYS_CTL);
#endif
}

#define BLOCKS 96 
extern void WAIT_FOR_TRANSFER_READY(void);
extern void recv_buf(UBYTE *);
extern void i2cl_init(void);
extern void avi(void );
extern void vo_internal(void);
extern void avi(void);
extern void cc1(void);
extern void cc1_ccir(void);
extern void avo_combo(UBYTE, UBYTE, UBYTE);
extern void avi_pal(void);
extern void cc1_ccir_pal(void);
extern void avi_pal(void);
extern void cc1_pal(void);



	UBYTE  *ptr;
__uint32_t patrn1;

void cosmo2_tv_std( UBYTE tv_std, UWORD *horz, UWORD *vert, UBYTE genlock)
{

    switch (tv_std) {
        case 0:                 /* ccir, ntsc */
			 *horz =  720; *vert = 243 ;
            i2cl_init();
            avi();
            cc1_ccir();
            avo_combo(0, 1, genlock);
            cosmo2_cbar_vivo_setup (0, 1);
            cosmo2_cbar_chan_setup (0, 1);
        break;
        case 1:                  /* sq, ntsc */
			*horz =  640; *vert = 243 ;
            i2cl_init();
            avi();
            cc1();
            avo_combo(0, 0, genlock);
            cosmo2_cbar_vivo_setup (0, 0);
            cosmo2_cbar_chan_setup (0, 0);
        break;
        case 2:                 /* ccir, pal */
			*horz =  720; *vert = 288 ;
            i2cl_init();
            avi_pal();
            cc1_ccir_pal();
            avo_combo(1, 1, genlock);
            cosmo2_cbar_vivo_setup (1, 1);
            cosmo2_cbar_chan_setup (1, 1);
        break;
        case 3:                 /*  sq ,  pal */
			*horz =  768; *vert = 288 ;
            i2cl_init();
            avi_pal();
            cc1_pal();
            avo_combo(1, 0, genlock);
            cosmo2_cbar_vivo_setup (1, 0);
            cosmo2_cbar_chan_setup (1, 0);
        break;
        default:
        msg_printf(SUM, " usage: incorrect tv standard \n");
         break;
    }

}

UBYTE constant_enable_test (  int argc, char **argv )
{

	UBYTE padtop, padbot, padright, padleft ;
	UWORD Horz, Vert ;
	
	__uint32_t fbcbase, chnl = 0  ;
	
	
	__uint32_t   tv_std = 1, genlock; /* sq, ntsc */

    argc--; argv++;

	if (argc < 2) { 
	msg_printf(SUM, "usage: c2_cc -c [1/2] -p (YUV) -s tvstd [0-3] -g genlock [1/yes, 0/no] \n");
	msg_printf(SUM, " -c channel 0 or 1 \n");
    msg_printf(SUM, " -p YUV (0xf08080) \n");
	msg_printf(SUM, " -s tvstd 0 [ccir_ntsc] 1 [sq ntsc] 2 [ccir pal] 3 [pal sq] \n");
    msg_printf(SUM, " -g genlock 1 [genlock mode], 0 [no internal mode] \n");
	return (0);
    }

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &chnl);
                }
            break ;
            case 'g':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &genlock);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &genlock);
                }
            break ;
            case 's':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &tv_std);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &tv_std);
                }
            break ;
            case 'p':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &patrn1);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &patrn1);
                }
            break ;

            default :
                msg_printf(SUM, " usage: -c chnl -p (YUV) -s tv_std[0-3] -g genlock[0/1]\n") ;
                return (0);
        }
        argc--; argv++;
    }

	chnl = chnl - 1 ;

    if (chnl == 0) {
    	fbcbase = VIDCH1_FBC_BASE;
    }
    else 
	if (chnl == 1) {
    fbcbase = VIDCH2_FBC_BASE;
    }

	cosmo2_tv_std(tv_std, &Horz, &Vert, genlock);

	cosmo2_reset_chnl ();

	cosmo2_cbar_setup(Horz, Vert, chnl);


/*	mcu_WRITE(CC1_BASE + CC1_SYS_CTL, 0x0); */
	if (chnl == 0) {
	mcu_WRITE(CBAR_BASE+CBAR_VO_SRC, SYNC_SRC_VC1|DATA_SRC_VC1);
	mcu_WRITE(CBAR_BASE+CBAR_VC1_SRC, SYNC_SRC_VO);
	}
	if (chnl == 1) {
	mcu_WRITE(CBAR_BASE+CBAR_VO_SRC, SYNC_SRC_VC2|DATA_SRC_VC2);
	mcu_WRITE(CBAR_BASE+CBAR_VC2_SRC, SYNC_SRC_VO);
	}

#if 0
	patrn1 = (0xffffffff) ;
	ptr = ((UBYTE *) dma_data) + index ;
#else
	ptr = ((UBYTE *) &patrn1) + 1;
#endif

    padtop = 0 ; 
	padbot = 0 ; 
  padright = 0 ; 
   padleft = 0 ;

   G_errors = 0;

	constant_enable_test_fbc_setup ( fbcbase, Horz, Vert, ptr, 
					padright, padleft , padtop , padbot ) ;

	return (0);

}

const_cc1_read ( int argc, char **argv )
{

 
    __uint32_t field = 0, line = 0;
    UBYTE buf[BYTES_PER_BLOCK];
    UBYTE print = 0;
    __uint32_t blk, i;

	msg_printf(SUM,"YUV values %x %x %x \n",*ptr,*(ptr+1),*(ptr+2));

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'f':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &field);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &field);
                }
            break ;
        case 'l':
         if (argv[0][2]=='\0') {
         atob(&argv[1][0], (int *) &line);
         argc--; argv++;
         } else {
         atob(&argv[0][2], (int *) &line);
         }
         break ;
            default :
                msg_printf(SUM, " usage: -l line -f field\n") ;
                return (0);
        }
        argc--; argv++;
    }
    /* setup cc1 to read */

#if 1
    msg_printf(SUM, " setup Read from line %x \n", line);
	setup_cc1_const_enable ( (UBYTE) field , (UWORD)line, 0) ;
#endif
	
    DelayFor(0xffff);

    for ( blk = 0 ; blk < BLOCKS ; blk++ ) {
        	WAIT_FOR_TRANSFER_READY () ;

     		for ( i = 0 ; i < BYTES_PER_BLOCK  ; i++ )
				buf[i] = 0;

        	recv_buf(buf) ;

		print = 0;

     	for ( i = 0 ; i < BYTES_PER_BLOCK  ; i+=4 )
        	if ( ( buf[i]  != *(ptr+1) ) || ( buf[i+1] != *(ptr) )
         	|| ( buf[i+2] != *(ptr+2) ) || ( buf[i+3] != *(ptr) ) ) {
			print = 1;
        	G_errors++ ;
     	}

		if (print)
		    msg_printf(ERR, "block %d \n", blk);
		else
		    msg_printf(SUM, "block %d\n", blk);

		for ( i = 0; i < 24 ; i++)
		    msg_printf(SUM, "%x ", buf[i]) ; msg_printf(SUM, "\n") ;
		for ( i = 24; i < 48 ; i++)
		    msg_printf(SUM, "%x ", buf[i]) ;  msg_printf(SUM, "\n") ;
		print = 0;
    }

    if (G_errors) {
        msg_printf (SUM, "Constant Enable Test failed \n");
        return ( 0 );
        }
    else {
        msg_printf (SUM, "Constant Enable Test Passed \n");
        return ( 1 );

	}

}

void cosmo2_cc1_readfb()
{
    UBYTE buf[BYTES_PER_BLOCK];
    __uint32_t i;


            WAIT_FOR_TRANSFER_READY () ;

            for ( i = 0 ; i < BYTES_PER_BLOCK  ; i++ )
                buf[i] = 0 ;

            recv_buf(buf)  ;


        for ( i = 0; i < 24 ; i++)
            msg_printf(SUM, "%x ", buf[i]) ; msg_printf(SUM, "\n") ;
        for ( i = 24; i < 48 ; i++)
            msg_printf(SUM, "%x ", buf[i]) ;  msg_printf(SUM, "\n") ;
    
}


UBYTE cosmo2_cc1_indread (  int argc, char **argv )
{


	__uint32_t regoff; 
	UBYTE val8;

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'o':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &regoff);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &regoff);
                }
            break ;

            default :
                msg_printf(SUM, " usage: -o indreg\n") ;
                return (0);
        }
        argc--; argv++;
    }

    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, (UBYTE) regoff);
	val8 = mcu_READ(CC1_BASE+CC1_INDIR_DATA);
	msg_printf(SUM, " regoff %d 0x%x \n", regoff, val8);

	return (0);

}

UBYTE cosmo2_cc1_indwrite (  int argc, char **argv )
{


	__uint32_t regoff, value; 


    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'o':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &regoff);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &regoff);
                }
            break ;
            case 'v':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &value);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &value);
                }
			break;	

            default :
                msg_printf(SUM, " usage: -i indreg\n") ;
                return (0);
        }
        argc--; argv++;
    }

    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, (UBYTE) regoff);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, (UBYTE) value);

	return (0);
}

