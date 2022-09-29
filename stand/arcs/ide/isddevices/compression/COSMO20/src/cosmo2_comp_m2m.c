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

UBYTE scan_headers_loss[SIZE_OF_SCAN] = {
0xFF, 0xDA, 0x0, 0xc, 0x3, 0x1, 0x0, 0x2, 0x10, 0x3, 0x10, 0x1, 0x0, 0x0
};

UBYTE scan_headers[SIZE_OF_SCAN] = {
0xFF, 0xDA, 0x0, 0xc, 0x3, 0x1, 0x0, 0x2, 0x11, 0x3, 0x11, 0x0, 0x3f, 0x0
};

UBYTE frame_headers_loss[SIZE_OF_FRAME] = {
0xFF, 0xC3, 0x00, 0x11, 0x12, 
							0xff, 0xff, /* # of lines */
                            0xff, 0xff, /* # of samples */
                            0x3,        /* # of components */
                            0x1, 0x21, 0x0,
                            0x2, 0x11, 0x0,
                            0x3, 0x11, 0x0
};

UBYTE frame_headers[SIZE_OF_FRAME] = {
0xFF, 0xC0, 0x00, 0x11, 0x8, 
							0xff, 0xff, /* # of lines */
                            0xff, 0xff, /* # of samples */
                            0x3,        /* # of components */
                            0x1, 0x21, 0x0,
                            0x2, 0x11, 0x01,
                            0x3, 0x11, 0x01
};

#define SIZE_OF_HUFF_LOSS 62 

UBYTE huff_tbl_loss[SIZE_OF_HUFF_LOSS] = {
    0xff,   0xc4,
    0,
    60,
    0x0,        /* id of Luminance DC coefficients */
    0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0B,

    0x1,        /* id of   chrominance DC coefficients */
    0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b

};

__uint32_t chk_sum_array[] = {
0xd8c5, 0xd8c7, 0xd8c9, 0xd8cb, 0xd7ee, 0xd7f6, 0xd531, 0xd724, 0xd533, 0xd7f7,
0xd7ff, 0xd80c, 0xd80e, 0xd810, 0xd812, 0xd945
};

#define CHK_SUM_TBL_SIZE 80 

void cosmo2_comp_setup_050 ( __uint32_t addr, UBYTE lossless)
{
	UBYTE  value; 
	
	value = Z50_MSTR ;
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

    msg_printf(DBG, " Sending the frame headers to the 050\n");

	if (lossless == 0) {
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x40, 
			SIZE_OF_FRAME, frame_headers );
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x7a, 
			SIZE_OF_SCAN, scan_headers );
    	msg_printf(DBG, " Sending the quantization tables to the 050\n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0xcc, 
			SIZE_OF_QUANT_TABLE ,  quant_tbl );
    	msg_printf(DBG, " Sending the huffman tables to the 050\n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, addr+0x1d4, 
			SIZE_OF_HUFF_TABLE , huff_tbl );
	}
	else {
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
						addr+0x40, SIZE_OF_FRAME,	frame_headers_loss );
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
						addr+0x7a, SIZE_OF_SCAN, scan_headers_loss );
    	msg_printf(DBG, " Sending the huffman tables to the 050\n");
        mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
						addr+0x1D4, SIZE_OF_HUFF_LOSS , huff_tbl_loss );
	}

}

void cosmo2_comp_setup_016 ( __uint32_t  addr , UWORD horz, UWORD vert, UBYTE lossless)
{

	UBYTE value;

	value = DSPY_422PXOUT | MODE_422PXIN_422050 | MODE_COMPRESS ;
	mcu_WRITE(addr + Z16_MODE_REG , value);

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_SETUP_PARAMS1);

	if (lossless)
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, SETUP_RSTR );
	else
	mcu_WRITE (addr + Z16_INDIR_DATA_REG,  SETUP_CNTI );

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_SETUP_PARAMS2);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, SETUP2_SYEN );

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAX_LO);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);	

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAX_HI);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAY_LO);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);	

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAY_HI);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);	

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAX_LO);
	value = horz & 0xff ;
	msg_printf(VRB, "horz LO %x\n", value);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);	

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAX_HI);
	value = (horz & 0xff00) >> 8 ; ;
	msg_printf(VRB, "horz HI %x\n", value);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);	

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAY_LO);
	value = vert & 0xff ;
	msg_printf(VRB, "vert LO %x\n", value);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);	

	us_delay(1000);
	mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAY_HI);
	value = (vert & 0xff00) >> 8 ;
	msg_printf(VRB, "vert HI %x\n", value);
	mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);	

#if 0
	mcu_WRITE (addr + Z16_ADDR_PTR_REG,Z16_SETUP_PARAMS1);
	value = mcu_READ(addr + Z16_INDIR_DATA_REG);
	msg_printf(VRB, "Z16_SETUP_PARAMS1 %x\n", value);

    mcu_WRITE (addr + Z16_ADDR_PTR_REG,Z16_SETUP_PARAMS2);
    value = mcu_READ(addr + Z16_INDIR_DATA_REG);
    msg_printf(VRB, "Z16_SETUP_PARAMS2 %x\n", value);

	mcu_WRITE (addr + Z16_ADDR_PTR_REG,Z16_NLINES_LO);
	value = mcu_READ(addr + Z16_INDIR_DATA_REG);
    msg_printf(VRB, "Z16_NLINES_LO %x\n", value);

	mcu_WRITE (addr + Z16_ADDR_PTR_REG,Z16_NLINES_HI);
	value = mcu_READ(addr + Z16_INDIR_DATA_REG);
    msg_printf(VRB, "Z16_NLINES_HI %x\n", value);
#endif

}

void cosmo2_comp_setup_upc ( __uint32_t  addr, UWORD Horz )
{

	UBYTE value;
	UWORD horz;

	value = CPC_DIR_TOGIO  | 0x1 ;
	mcu_WRITE (addr + CPC_CTL_STATUS, value);
	value = CPC_RUN_ENB | CPC_DIR_TOGIO | 0x1 ;
	mcu_WRITE (addr + CPC_CTL_STATUS, value);

	value = 0 ;
	mcu_WRITE (addr + UPC_FLOW_CONTROL, value);
	value = 0 ;
	mcu_WRITE(addr + UPC_GENESIS_ACCESS, value);

	horz = Horz - 1 ;
	mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq++, 
				addr + UPC_PIXEL_PER_LINE_REG,  2, (UBYTE *) &horz);

	horz = mcu_READ_WORD(addr + UPC_PIXEL_PER_LINE_REG );
	msg_printf(VRB, "UPC_PXL_LINE %x%x\n", (horz&0xff00)>>8, (horz&0xff)); 

	value = mcu_READ(addr + UPC_FLOW_CONTROL );
	us_delay(1000);
	value = UPC_YUV ;
	mcu_WRITE (addr + UPC_FLOW_CONTROL, value);

	msg_printf(DBG, "UPC_FLOW_CONTROL %x\n", value); 

	value = mcu_READ (addr + UPC_FLOW_CONTROL);
	us_delay(1000);
	value = value | UPC_RUN_ENB ;
	mcu_WRITE (addr + UPC_FLOW_CONTROL, value);

}

void cosmo2_comp_setup_fbc ( __uint32_t  fbcbase, UWORD Horz, UWORD Vert, 
							UBYTE *pad, UBYTE padright, UBYTE padleft,
							UBYTE padtop, UBYTE padbot )
{

	UWORD value;
	UBYTE tmp8;
	
    value = padleft;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq,
            FBC_H_ACTIVE_START + fbcbase, 2, (UBYTE *) &value);

    value = padtop;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq,
            FBC_V_ACTIVE_START + fbcbase, 2, (UBYTE *) &value);

    mcu_WRITE(FBC_Y_PAD + fbcbase, *pad);
    mcu_WRITE(FBC_U_PAD + fbcbase, *(pad+1) );
    mcu_WRITE(FBC_V_PAD + fbcbase, *(pad+2) );

	msg_printf(VRB, " padding values %x%x%x\n", *pad, *(pad+1), *(pad+2) );

    value = Horz - 1 + padleft  ;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq,
            FBC_H_ACTIVEG_END + fbcbase, 2, (UBYTE *) &value);

    value = Vert - 1  + padtop;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq,
            FBC_V_ACTIVEG_END + fbcbase, 2, (UBYTE *) &value);

    value = Horz - 1 + padleft + padright;
    mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq,
            FBC_H_PADDING_END + fbcbase, 2, (UBYTE *) &value);

    value = Vert - 1  + padtop + padbot ;
    mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq,
            FBC_V_PADDING_END + fbcbase, 2, (UBYTE *) &value);

    value = Horz-1 + 16 +padleft + padright ;
     mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq,
        FBC_H_BLANKING_END+fbcbase, 2, (UBYTE *) &value);

    value = Vert - 1 + padtop  + padbot ;
        mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq,
            FBC_V_BLANKING_END +fbcbase, 2, (UBYTE *) &value);

    tmp8 = mcu_READ (FBC_MAIN + fbcbase) ;
	us_delay(1000);
    mcu_WRITE(FBC_MAIN + fbcbase, tmp8 ) ;

    mcu_WRITE(FBC_FLOW + fbcbase, 5 ) ; /* compress */
	
}

void put_in_fifo( UBYTE chnl, UWORD cnt, __uint64_t val64)
{
  UWORD i;
	for ( i = 0; i < cnt; i++ ) {
	cgi1_write_reg (val64, cosmobase + cgi1_fifo_rw_o(chnl));
	us_delay(100);
	}
	
}

void fbc_init( __uint32_t  addr )
{
	mcu_WRITE(addr+FBC_FLOW, 0);
	mcu_WRITE(addr+FBC_MAIN, 0);
	mcu_WRITE(addr+FBC_INT_EN, 0xff);

}

void print_fbc_regs(__uint32_t  addr ) 
{
	UBYTE val8;
	val8 = mcu_READ (addr+FBC_INT_STAT);
	msg_printf(VRB, "FBC_INT_STAT 0x%x\n", val8);
	val8 = mcu_READ (addr+FBC_STATUS);
	msg_printf(VRB, "FBC_STATUS 0x%x\n", val8);
	val8 = mcu_READ (addr+FBC_FLOW);
	msg_printf(VRB, "FBC_FLOW 0x%x\n", val8);
	val8 = mcu_READ (addr+FBC_MAIN);
	msg_printf(VRB, "FBC_MAIN 0x%x\n", val8);
}

__uint32_t  collect_stat_compression (__uint64_t  value, UBYTE IVal )
{
	__uint32_t  i ;
	static __paddr_t check_sum = 0;
	static __paddr_t i7 = 0, i6 = 0, i5 = 0, i4 = 0, i3 = 0, i2 = 0, i1 = 0, i0 = 0;
	UBYTE *ptr, flag = 1;

	if (IVal == 0) {
		i0 = 0; i1 = 0; i2 = 0; i3 = 0; i4 = 0; i5 = 0; i6 = 0; i7 = 0;
		check_sum = 0;
		msg_printf(VRB, "%d,%d,%d,%d,%d,%d,%d,%d\n", i0,i1,i2,i3,i4,i5, i6,i7);
		msg_printf(VRB, "check sum %x\n", check_sum);
		return (0) ; 
	}

	if (IVal == 2) {
	msg_printf(VRB, "check sum %x\n", check_sum);
	msg_printf(VRB, "%d,%d,%d,%d,%d,%d,%d,%d,\n", i0,i1,i2,i3,i4,i5, i6,i7);
	return (check_sum);
	}
			
		/* look for 0xd9ffffffffffffff */
		if ((value & 0xffffffffffffff) == 0xffffffffffffff )
				flag = 0;
			else
				flag = 1;

		for ( i = 0; i < 8; i++) {
		ptr = ((UBYTE *) &value) + i ;

			i0 += ((*ptr >> 0) & 0x1)	;
			i1 += ((*ptr >> 1) & 0x1)	;
			i2 += ((*ptr >> 2) & 0x1)	;
			i3 += ((*ptr >> 3) & 0x1)	;
			i4 += ((*ptr >> 4) & 0x1)	;
			i5 += ((*ptr >> 5) & 0x1)	;
			i6 += ((*ptr >> 6) & 0x1)	;
			i7 += ((*ptr >> 7) & 0x1)	;

			check_sum = check_sum + (*ptr) ;
			msg_printf(VRB, "check sum 0x%x ptr 0x%x\n", check_sum, *ptr);

			if (flag == 0)
				return (-1);
			
			if ((*ptr == 0xff) && (*(ptr+1) == 0xd9))
				flag = 0;
		}

	return 0;
}

UBYTE cos2_patcomp ( int argc, char **argv  )
{

	UWORD Horz, Vert  ;
	UBYTE padtop, padbot, padright, padleft, val8  , *ptr;
	__uint64_t value64 ;
	__uint32_t    data = 0xeb8080, Auto = 1, verbose=0,
		fbcbase, upcbase, z50base, z16base, chnl = 0, mode = 0;
	static UBYTE index = 0 ;

	UBYTE buf[3] = {0x80, 0x80, 0x80};

	if (G_errors)
	return(0);

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
	
	chnl = 1 ;
	}
	else if (chnl == 1) {
    fbcbase = VIDCH2_FBC_BASE; upcbase = VIDCH2_UPC_BASE;
    z50base = VIDCH2_050_BASE; z16base = VIDCH2_016_BASE ;
	
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
				msg_printf(SUM, "COMPRESSION TEST FAILED\n");
				return (0);
				}
			else {
				msg_printf(SUM, "COMPRESSION TEST Passed\n");
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
	cosmo2_comp_setup_050(z50base, mode) ;

	/* setup the 016 registers */
	cosmo2_comp_setup_016(z16base, (Horz+padright+padleft), 
			(Vert+padtop+padbot), mode);
   	mcu_WRITE(z16base+Z16_GO_STOP_REG, Z16_GO) ;

	/* setup the FBC registers */
	cosmo2_comp_setup_fbc(fbcbase, Horz, Vert,
                            ptr, padright, padleft , padtop , padbot);

	/* kick off all the chips */
	mcu_WRITE(z50base+Z50_GO, 0);
	us_delay(1000); 
	val8 = mcu_READ(fbcbase + FBC_MAIN);
	mcu_WRITE(fbcbase + FBC_MAIN, val8 | GO | C_PAD_EN ); 
	
	/* I have to poll here for (FBC INT STAT & IRQ50) */

#if 1
		cos2_EnablePioMode();
#ifdef IP30		
		{
		int comp_timeout = 0xff;
		val8 = mcu_READ(fbcbase+FBC_INT_STAT) ;
		while (comp_timeout-- && (!(val8 & IRQ_050))) {
			val8 = mcu_READ(fbcbase+FBC_INT_STAT) ;
		}
		if (comp_timeout == 0) {
			msg_printf(SUM, " IRQ_050 bit is not high %x\n", val8);
			G_errors++;
			return (0);
		}
		}
#else
		val8 = mcu_READ(fbcbase+FBC_INT_STAT) ;
		if (!(val8 & IRQ_050)) {
			msg_printf(SUM, " IRQ_050 bit is not high %x\n", val8);
			G_errors++;
			return (0);
		}

#endif
		   collect_stat_compression(value64, 0 );

			while ( checkDataInFifo(chnl-1) ) {
				value64 = cgi1_read_reg(cosmobase + cgi1_fifo_rw_o((chnl-1)) );
				if (verbose) 
				msg_printf(SUM, "%16llx\n", value64 ) ;
				collect_stat_compression(value64, 1 );
				value64 = 0;
			}

			cos2_DisEnablePioMode ( );
		
		if (Auto) {
			if (chk_sum_array[index-1] != collect_stat_compression(value64, 2)){
				msg_printf(SUM, "COMPRESSION TEST FAILED %x index %x\n",
							chk_sum_array[index-1], index-1);
				G_errors++;
				return (0);
			}
		}
		else collect_stat_compression (value64, 2 ) ;	
#endif

    val8 = mcu_READ(z50base+Z50_STATUS_0);
    msg_printf(VRB, "Z50_STATUS_0 %x\n", val8);
    val8 = mcu_READ(z50base+Z50_STATUS_1);
    msg_printf(VRB, "Z50_STATUS_1 %x\n", val8);


	return (1);
}
