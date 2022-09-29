
/*
 *
 * Cosmo2 video tests 
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

#include <arcs/io.h>
#include <arcs/errno.h>

extern int cos2_EnablePioMode( void );

extern void cosmo2_reset_chnl (void);

void cosmo2_cbar_vivo_setup (UBYTE ,UBYTE  );
void cosmo2_cbar_chan_setup (UBYTE , UBYTE );
extern __uint64_t dma_data[];
extern cosmo2_1to2_dma_swap (__uint64_t *, __uint64_t *, __uint32_t);

extern cosmo2_fbc_poll_video_done_pio (__uint32_t ) ;
extern cosmo2_vid_ce_fbc2_setup (__uint32_t , UWORD , UWORD ,
                                UBYTE , UBYTE , UBYTE , UBYTE );

extern cosmo2_fbc_poll (__uint32_t);
extern print_fbc_all_reg(__uint32_t );


extern cosmo2_vid_ce_fbc1_setup (__uint32_t , UWORD , UWORD , UBYTE *, 
							UBYTE , UBYTE , UBYTE , UBYTE );
extern cosmo2_cbar_setup(UWORD , UWORD , UBYTE );

extern cosmo2_mod(__uint32_t , __uint32_t );

extern void i2cl_init(void);
extern void avi(void );
extern void avo(void );
extern void avo_genlock(void);
extern void cc1(void );
extern void vo_internal(void);
extern __uint64_t dma_send_data[DMA_DATA_SIZE], dma_recv_data[DMA_DATA_SIZE];

UBYTE cosmo2_mode9(  int argc, char **argv )
{

	UBYTE padtop, padbot, padright, padleft  ;
	UWORD Horz, Vert;
	__uint32_t fbcbase1, fbcbase2, chnl = 0, upcbase1;
	/*REFERENCED*/
	__uint32_t size;
	/*REFERENCED*/
	__uint32_t upcbase2;
	/*REFERENCED*/
	UWORD chnl1;
	UBYTE flow_control;
	__uint32_t  i ;
	UBYTE  *ptr, val8  /*, flag = 0 */;
	__uint32_t patrn1 = 0xffffffff,  genlock = 0 /* , iterations */;
	char *bp;
	UBYTE buf[10] ;
	ULONG fd, cc;

	char fname[100] , server[100] , str[300];
	__uint32_t lderr=0;
	__uint64_t value64  ;



	size = DMA_DATA_SIZE*sizeof(__uint64_t) ; 

    argc-- ; argv++ ;

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
        case 's' :
        if (argv[0][2]=='\0') { /* has white space */
            sprintf(server, "%s", &argv[1][0]);
            argc--; argv++;
        }
        else { /* no white space */
            sprintf(server, "%s", &argv[0][2]);
        }
#if 0
        flag=1;
#endif
        break;

        case 'f' :
        if (argv[0][2]=='\0') { /* has white space */
            sprintf(fname, "%s", &argv[1][0]);
            argc--; argv++;
        }
        else { /* no white space */
            sprintf(fname, "%s", &argv[0][2]);
        }
        break;
            case 'g':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int*)&genlock);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int*)&genlock);
                }
            break;

        default  :
        break;
        }
        argc--; argv++;
    }

#if 0
    strcpy(str, "bootp()");

    if (flag) {
        strcat(str, server);
        strcat(str, ":");
        }

    strcat(str, fname);

#else
    strcat(str, fname);
#endif


	
    reset_chnl0(); reset_chnl1(); reset_chnl2(); reset_chnl3();
    DelayFor(10000);
    unreset_chnl0(); unreset_chnl1(); unreset_chnl2(); unreset_chnl3();

    if (Open ((CHAR *)str, OpenReadOnly, &fd) != ESUCCESS) {
    msg_printf(ERR, "Error: Unable to access file %s\n", str);
    return lderr ;
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

    cos2_EnablePioMode();

	G_errors = 0;
    	padtop = 0 ; padbot = 0 ; padright = 0 ; padleft = 0 ;

    Horz =  640; Vert = 240 ;
#if 0
	iterations = (__uint32_t)(Horz*Vert*2/(DMA_DATA_SIZE*sizeof(__uint64_t)));
#endif

		cosmo2_reset_chnl ();

		i2cl_init();
		avi();

        cc1();
        if (genlock)
         avo_genlock();
        else
         avo();


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

		/* send the data to the fifo */

    bp = (char*)buf; i = 0; value64  = 0;
    while ((Read(fd, bp, 1, &cc)==ESUCCESS) && cc) {

        value64 = (value64 << 8) | (UBYTE) *bp ;
        i++;

        if ((i % 8) == 0) {
            msg_printf(VRB, "%16llx\n", value64  );
            cgi1_write_reg(value64, cosmobase + cgi1_fifo_rw_o((chnl)) ) ;
            value64 = 0;
            i = 0;
        }
    }

	    if (( i < 8) && ( i > 0))
    	    {
       	     value64 = value64 << (8-i)*8 ;
       	     msg_printf(VRB, "%16llx\n", value64  );
       	     cgi1_write_reg(value64, cosmobase + cgi1_fifo_rw_o((chnl)) ) ;
       		 }
	     Close(fd);


         cosmo2_fbc_poll (fbcbase1) ;

        val8 = mcu_READ (FBC_FLOW + fbcbase1) ;
        while ((val8 & FIELD_SELECT) == 0) {
            msg_printf(VRB, " trying to flip the buffer in channel %d \n",
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
            msg_printf(VRB, " trying to flip the buffer in channel %d \n",
                        (chnl) ? 0 : 1);
            mcu_WRITE(FBC_FLOW + fbcbase2, (FB_WR_EN | FIELD_SELECT | MODE3) ) ;
            val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
		}

        cosmo2_fbc_poll_video_done_pio (fbcbase2) ;
        mcu_WRITE (FBC_INT_STAT + fbcbase2, 0xff) ;
        cosmo2_fbc_poll_video_done_pio (fbcbase2) ;
#if 0	
        val8 = mcu_READ (FBC_FLOW + fbcbase2) ;
		while ((val8 & FIELD_SELECT) != 0) {
			DelayFor(100000);
			msg_printf(VRB, "2 trying to select the buffer \n");
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
						msg_printf(SUM, "ERR i %3x j %3d recv %16llx exp %16llx \n",
                                line, pixel, dma_recv_data[j], dma_send_data[j]  );
                   		 G_errors++;
                	}else
                    msg_printf(VRB, "i %3x j %3d recv %16llx exp %16llx \n",
                                i, j, dma_recv_data[j], dma_send_data[j] );
           		  }
			j = cosmo2_mod(pixel, (Horz*2) );
			pixel = j ;
			}
		

    cosmo2_fbc_poll (fbcbase2);

	if (G_errors) { 
		msg_printf(ERR, " video test from channel %d failed \n", chnl+1);
		return (0);
		}
	else
		msg_printf(SUM, " video test from channel %d passed \n", chnl+1);
/*
	cosmo2_reset_chnl();
	mcu_WRITE(FBC_MAIN + fbcbase1, 0 );
	mcu_WRITE(FBC_MAIN + fbcbase2, 0 );
*/
#endif
	return(1);
}


