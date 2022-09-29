

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

extern UBYTE checkDataInFifo( UBYTE );

extern __uint64_t dma_data[];
extern UBYTE huff_tbl[];
extern UBYTE quant_tbl[];
extern int cos2_EnablePioMode( void );
extern int cos2_DisEnablePioMode( void );
extern __uint32_t cosmo2_recv_dma_data(UBYTE,UBYTE,__uint64_t *,
						__uint32_t); 
extern void cosmo2_dma_flush (UBYTE );
extern cosmo2_reset_chnl (void);


extern UBYTE comp_data[];




#define SIZE_OF_FRAME 19
#define SIZE_OF_SCAN  14

extern UBYTE scan_headers_loss[SIZE_OF_SCAN] ; 

extern UBYTE scan_headers[SIZE_OF_SCAN] ;

extern UBYTE frame_headers_loss[SIZE_OF_FRAME] ;

extern UBYTE frame_headers[SIZE_OF_FRAME] ;

#define SIZE_OF_HUFF_LOSS 62 

extern UBYTE huff_tbl_loss[SIZE_OF_HUFF_LOSS] ;

extern __uint32_t chk_sum_array[] ;

#define CHK_SUM_TBL_SIZE 80 

void
cosmo2_comp_slavesetup_050 ( __uint32_t addr, UBYTE lossless)
{
	UBYTE  value; 
	
	value = Z50_SLAVE_8 ; /* slave mode */
	mcu_WRITE(addr+Z50_HARDWARE, value);

	value = Z50_Baseline_Compression ;
	mcu_WRITE(addr+Z50_MODE, value);
	value = 0;
	mcu_WRITE(addr+Z50_OPTIIONS, value); 
	value = 0;
	mcu_WRITE(addr+Z50_INT_REQ_0, value);
	mcu_WRITE(addr+Z50_INT_REQ_1, END);
	value =  DHT | DQT | DHTI ;	/* don't include markers.We won't decompress */
	mcu_WRITE(addr+Z50_MARKERS_EN, value);
	value = 1;
	mcu_WRITE(addr+Z50_SF_H, value);
	value = 0;
	mcu_WRITE(addr+Z50_SF_L, value);

#if 0
	mcu_WRITE(addr+Z50_ACV_H, 0);
	mcu_WRITE(addr+Z50_ACV_MH, 0);
	mcu_WRITE(addr+Z50_ACV_ML, 0);
	mcu_WRITE(addr+Z50_ACV_L, 0);
#endif

	mcu_WRITE(addr+Z50_AF_H, 0xff);
	mcu_WRITE(addr+Z50_AF_M, 0xff);
	mcu_WRITE(addr+Z50_AF_L, 0xff);

    msg_printf(DBG, " Sending the frame headers to the 050 \n");

	if (lossless == 0) {
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x40, 
			SIZE_OF_FRAME, frame_headers );
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x7a, 
			SIZE_OF_SCAN, scan_headers );
    	msg_printf(DBG, " Sending the quantization tables to the 050 \n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0xcc, 
			SIZE_OF_QUANT_TABLE ,  quant_tbl );
    	msg_printf(DBG, " Sending the huffman tables to the 050 \n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x1d4, 
			SIZE_OF_HUFF_TABLE , huff_tbl );
	}
	else {
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
						addr+0x40, SIZE_OF_FRAME,	frame_headers_loss );
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
						addr+0x7a, SIZE_OF_SCAN, scan_headers_loss );
    	msg_printf(DBG, " Sending the huffman tables to the 050 \n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
						addr+0x1D4, SIZE_OF_HUFF_LOSS , huff_tbl_loss );
	}

}

extern cosmo2_comp_setup_016 ( __uint32_t  , UWORD , UWORD , UBYTE );
extern cosmo2_comp_setup_upc ( __uint32_t  , UWORD );
extern cosmo2_comp_setup_fbc ( __uint32_t , UWORD , UWORD , 
							UBYTE *, UBYTE , UBYTE ,
							UBYTE , UBYTE );

extern put_in_fifo( UBYTE , UWORD , __uint64_t );

extern fbc_init( __uint32_t  );

extern print_fbc_regs(__uint32_t  ) ;

UBYTE cos2_patcomp_slave ( int argc, char **argv  )
{

	UWORD Horz, Vert, timeout = 0xff, polled = 0, end_timeout=0xffff, end_polled=0 ;
	UBYTE padtop, padbot, padright, padleft, val8, *ptr, datardy;
	/*REFERENCED*/
	UBYTE mask;
	__uint64_t value64 ;
	__uint32_t chk_sum = 0,  data = 0xeb8080, Auto = 1, verbose=0,
		fbcbase, upcbase, z50base, z16base, chnl = 0, mode = 0;
	static UBYTE index = 0 ;

	UBYTE buf[3] = {0x80, 0x80, 0x80};

	G_errors = 0;

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &chnl);
                }
            break;
            case 'm':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &mode);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &mode);
                }
            break;
            case 'd':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &data);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &data);
                }
            break;
            case 'a':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &Auto);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &Auto);
                }
            break;
            case 'v':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &verbose);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &verbose);
                }
            break;
			default:
                break ;
        }
        argc--; argv++;
    }

	if (chnl == 0) {
	fbcbase = VIDCH1_FBC_BASE; upcbase = VIDCH1_UPC_BASE;
	z50base = VIDCH1_050_BASE; z16base = VIDCH1_016_BASE ;
	mask = FIFO_02_EMPTY_IRQ ;
	chnl = 1 ;
	}
	else if (chnl == 1) {
    fbcbase = VIDCH2_FBC_BASE; upcbase = VIDCH2_UPC_BASE;
    z50base = VIDCH2_050_BASE; z16base = VIDCH2_016_BASE ;
	mask = FIFO_13_EMPTY_IRQ;
	chnl = 3 ;
    }

	cosmo2_reset_chnl();

#if 1	
	fbc_init(fbcbase );
#endif

	msg_printf(DBG, " Regs of FBC Before the test:\n");
	print_fbc_regs(fbcbase);

	/* Let the size of the active image be small */
	padtop = 15 ; padbot = 15 ; Vert = 2 ;
	padright = 14; padleft = 14; Horz = 4 ; 

	/* setup the fromae sizes */
	frame_headers[5] = ((Vert+padtop+padbot) & 0xff00) >> 8 ;
	frame_headers[6] = (Vert+padtop+padbot) & 0xff ;
	frame_headers[7] = (((Horz+padright+padleft) & 0xff00 ) >> 8) ;
	frame_headers[8] = ((Horz+padright+padleft) & 0xff ) ;

	/************ get data from user or from dma data structure */

	buf[0] = (UBYTE)((data & 0xff0000) >> 16); 
	buf[1] = (UBYTE)((data & 0xff00) >> 8);
	buf[2] = (UBYTE)(data & 0xff) ;
	ptr = 	((UBYTE *) buf)  ;

	data = ((__uint32_t)buf[0] << 24) |
		   ((__uint32_t)buf[1] << 16) |
		   ((__uint32_t)buf[0] << 8) |
		   ((__uint32_t)buf[2] ) ;

	if (Auto) {
	data = *(((__uint32_t  *)comp_data) + index) ; 

	buf[0] = (UBYTE)((data & 0xff000000) >> 24);
	buf[1] = (UBYTE)((data & 0x00ff0000) >> 16);
	buf[2] = (UBYTE)(data & 0xff) ;
		ptr = ((UBYTE *) buf)  ;

		index += 1;
	if (index >= (DMA_DATA_SIZE*2/4))
		{
			index = 0; 
			if (G_errors) {
				msg_printf(SUM, "COMPRESSION TEST FAILED in slave mode\n");
				return (0);
				}
			else {
				msg_printf(SUM, "COMPRESSION TEST Passed in slave mode\n");
			    return (1); 
			}
		}

	}

	value64 = ((__uint64_t)data << 32) | data ;

	cos2_EnablePioMode();
	put_in_fifo(chnl, Vert, value64);
	cos2_DisEnablePioMode ( );

	/* setup the 050 and UPC  registers */
	cosmo2_comp_setup_upc(upcbase, Horz);
	cosmo2_comp_slavesetup_050(z50base, mode) ;

	/* setup the 016 registers */
	cosmo2_comp_setup_016(z16base, (Horz+padright+padleft), 
			(Vert+padtop+padbot), mode);
   	mcu_WRITE(z16base+Z16_GO_STOP_REG, Z16_GO) ;

	/* setup the FBC registers */
	cosmo2_comp_setup_fbc(fbcbase, Horz, Vert,
                            ptr, padright, padleft , padtop , padbot);

	/* kick off all the chips */
	mcu_WRITE(z50base+Z50_GO, 0);
	val8 = mcu_READ(fbcbase + FBC_MAIN);
	mcu_WRITE(fbcbase + FBC_MAIN, val8 | GO | C_PAD_EN ); 
	
	/* I have to poll here for (FBC INT STAT & IRQ50) */

		chk_sum = 0; datardy = 0;

		val8 = mcu_READ(fbcbase+FBC_INT_STAT) ;
		while ((end_timeout != end_polled) && (!(val8 & IRQ_050))) {

			val8 = mcu_READ( z50base + Z50_STATUS_1 );	

				datardy = 0; timeout = 0xff ; polled = 0;
			while (( timeout != polled) && (datardy == 0) ) {
				if (val8 & DATRDY) {
					datardy = 1;
					val8 = mcu_READ(z50base+Z50_COMPRESSED_DATA); 
				msg_printf(VRB, " val8 %x \n", val8);
				chk_sum += val8;
				}else
					val8 = mcu_READ(z50base+Z50_STATUS_1);  

					polled++;
			}
	
			if (!datardy) {
				msg_printf(SUM, " data ready bit is not set, timeout occurred %d \n",
					polled);
				G_errors++;
				return (0);
				}

		val8 = mcu_READ(fbcbase+FBC_INT_STAT) ;
		end_polled++;
		}

				msg_printf(VRB, " chk_sum %x \n", chk_sum);
		
		if (Auto) {
			if (chk_sum_array[index-1] != chk_sum){
				msg_printf(SUM, "COMPRESSION TEST FAILED %x index %x \n",
							chk_sum_array[index-1], index-1);
				G_errors++;
				return (0);
			}
		}

    val8 = mcu_READ(z50base+Z50_STATUS_0);
    msg_printf(VRB, "Z50_STATUS_0 %x\n", val8);
    val8 = mcu_READ(z50base+Z50_STATUS_1);
    msg_printf(VRB, "Z50_STATUS_1 %x\n", val8);


	return (1);
}
