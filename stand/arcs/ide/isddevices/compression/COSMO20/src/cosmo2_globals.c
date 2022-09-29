
/*
 * c2_globals.c
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
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */


#include "cosmo2_defs.h"
#include "cosmo2_dma_def.h"

__uint32_t GioSlot = 0, board = 0;
volatile __uint64_t *cosmobase;
volatile __uint64_t *cosmo2brbase;
cgi1_reg_t *cgi1ptr;

__uint32_t timeOut = 0xffffff ;

__uint32_t cmd_seq  = 0;

_CTest_Info cgi1_info[] = {
	"Product Id Test",			"CGI11",  0,
	"BOARD RESET Test",			"CGI12",  1,
	"CGI1 Connectivity Test",	 	"CGI13",  2,
	"Pattern Tests",			"CGI14",  3, 
	"MCU FIFO test",			"CGI15",  4,
	"Hardware Revision Test",                 "CGI16",  5,
	"Read Register Test with arg",		"CGI18",  6,
	"which_slot is it",			"CGI19",  7,
	"SetPioMode",				"CGI110", 8,
	"UnsetPioMode",				"CGI111", 9,
	"MCU_FIFO_LOOP_BACK_TEST",		"MCU1",	  10,
	"SRAM_TEST",				"SRAM1",  11,
	"upc patrn test",			"UpcPatrn",  	12,
	"upc addr uniq test",			"UpcUniqAddr",  	13,
	"cpc patrn test",			"CpcPatrn",  	14,
	"cpc  uniq addr test",			"CpcUniqAddr",  15	,
	"pio_loopback",				"PIOLOOP", 16,
	"z050 patrn test",			"z050patrn",	17,
	"z050 unique test",			"z050patrn",	18,
	"Hardware Revision Test",		"HRDWARE",	19,
	"z016 patrn test",			"z016patrn",	20,
	"z016 unique test",			"z016patrn",	21,
	"fifo_lb_c2u",				"fifo_lb_c2u", 22,
	"fifo_lb_u2c",				"fifo_lb_u2c", 23,
	"fifo_lb_mcu2cpc",			"fifo_lb_mcu2cpc", 24,
	"fifo_lb_mcu2upc",			"fifo_lb_mcu2upc", 25,
	"scaler_patrn",				"scaler_patrn", 26,
	"Table_pass",				"Table Pass", 27,
	"",					"",       1000
};

cgi1_t cgi1_reg_list[] = {
	"cgi1_product_id_reg",		cgi1_product_id_reg,		8,	_32bit_mask,	RDONLY,	CGI1_PRODUCT_ID_DEF,
	"cgi1_overview", 		cgi1_overview,		 	8,	_32bit_mask,	RDONLY,	CGI1_DMA_STAT_DEF,
	"cgi1_board_con_reg",		cgi1_board_con_reg, 		8,	_BCR_mask,	RDWR,	CGI1_BCR_DEF,
	"cgi1_ch0_tblptr",		cgi1_ch0_tblptr, 		8,	_32bit_mask,	RDWR,	CGI1_CH0_TBLPTR_DEF,	
	"cgi1_ch1_tblptr",		cgi1_ch1_tblptr, 		8,	_32bit_mask,	RDWR,	CGI1_CH1_TBLPTR_DEF,	
	"cgi1_ch2_tblptr", 		cgi1_ch2_tblptr, 		8,	_32bit_mask,    RDWR,   CGI1_CH2_TBLPTR_DEF,
	"cgi1_ch3_tblptr", 		cgi1_ch3_tblptr, 		8,	_32bit_mask,    RDWR,   CGI1_CH3_TBLPTR_DEF,
	"cgi1_dma_control", 		cgi1_dma_control, 		8,	_16bit_mask,	RDWR,	CGI1_DMA_CONTROL_DEF,
	"cgi1_interrupt_status",	cgi1_interrupt_status, 		8,	_32bit_mask,	RDWR,	CGI1_INTERRUPT_STAT_DEF,
	"cgi1_interrupt_mask", 		cgi1_interrupt_mask, 		8,	_32bit_mask,	RDWR,	CGI1_INTERRUPT_MASK_DEF,
	"cgi1_off_time", 		cgi1_off_time, 			8,	_16bit_mask,	RDWR,	CGI1_DMA_OFF_TIME_DEF,
	"cgi1_fifo_fg_ae_0", 		cgi1_fifo_fg_ae_offset(0), 	8,	_9BIT_MASK,	RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_fg_af_0", 		cgi1_fifo_fg_af_offset(0), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_ae_0", 		cgi1_fifo_tg_ae_offset(0), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_af_0", 		cgi1_fifo_tg_af_offset(0), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_fg_ae_1", 		cgi1_fifo_fg_ae_offset(1), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_fg_af_1", 		cgi1_fifo_fg_af_offset(1), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_ae_1", 		cgi1_fifo_tg_ae_offset(1), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_af_1", 		cgi1_fifo_tg_af_offset(1), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_fg_ae_2", 		cgi1_fifo_fg_ae_offset(2), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_fg_af_2", 		cgi1_fifo_fg_af_offset(2), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_ae_2", 		cgi1_fifo_tg_ae_offset(2), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_af_2", 		cgi1_fifo_tg_af_offset(2), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_fg_ae_3", 		cgi1_fifo_fg_ae_offset(3), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_fg_af_3", 		cgi1_fifo_fg_af_offset(3), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_ae_3", 		cgi1_fifo_tg_ae_offset(3), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_tg_af_3", 		cgi1_fifo_tg_af_offset(3), 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_fifo_rw_0", 		cgi1_fifo_rw_offset(0), 	8,	(__uint64_t) _64bit_mask,	RDWR,	CGI1_FIFO_RW_0_DEF,
	"cgi1_fifo_rw_1", 		cgi1_fifo_rw_offset(1), 	8,	(__uint64_t) _64bit_mask,    RDWR,   CGI1_FIFO_RW_1_DEF,
	"cgi1_fifo_rw_2", 		cgi1_fifo_rw_offset(2), 	8,	(__uint64_t) _64bit_mask,    RDWR,   CGI1_FIFO_RW_2_DEF,
	"cgi1_fifo_rw_3", 		cgi1_fifo_rw_offset(3), 	8,	(__uint64_t) _64bit_mask,    RDWR,   CGI1_FIFO_RW_3_DEF,
	"cgi1_mcu_fifo_rw_4", 		cgi1_mcu_fifo_rw_offset, 	8,	_16bit_mask,    RDWR,   CGI1_FIFO_RW_2_DEF,
	"cgi1_mcu_fifo_fg_ae_4",	cgi1_mcu_fifo_fg_ae_offset, 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_mcu_fifo_fg_af_4", 	cgi1_mcu_fifo_fg_af_offset, 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_mcu_fifo_tg_ae_4", 	cgi1_mcu_fifo_tg_ae_offset, 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"cgi1_mcu_fifo_tg_af_4", 	cgi1_mcu_fifo_tg_af_offset, 	8,	_9BIT_MASK,     RDWR,	CGI1_FIFO_FLAGS_RW__DEF,
	"", 				0, 				0,	0,		0,	0
};

__uint64_t *cgi1base ;

#if defined(IP30)

gio_slot_t gio_slot[] = {
        { 0, cgi1_slot0_base } ,
        { 0, cgi1_slot1_base } ,
        { 0, cgi1_slot_gfx_base }
};
	
#else

gio_slot_t gio_slot[] = {
        { GIO_SLOT_0, (unsigned long long *) cgi1_slot0_base } ,
        { GIO_SLOT_1, (unsigned long long *) cgi1_slot1_base } ,
        { GIO_SLOT_GFX, (unsigned long long *) cgi1_slot_gfx_base }
};
#endif

__uint32_t walk1s0s_32[WALK_32_SIZE] = {
	0x00000001, 0x00000002, 0x00000004, 0x00000008,
	0x00000010, 0x00000020, 0x00000040, 0x00000080,
	0x00000100, 0x00000200, 0x00000400, 0x00000800,
	0x00001000, 0x00002000, 0x00004000, 0x00008000,
	0x00010000, 0x00020000, 0x00040000, 0x00080000,
	0x00100000, 0x00200000, 0x00400000, 0x00800000,
	0x01000000, 0x02000000, 0x04000000, 0x08000000,
	0x10000000, 0x20000000, 0x40000000, 0x80000000,
	0xfffffffe, 0xfffffffd, 0xfffffffb, 0xfffffff7,
	0xffffffef, 0xffffffdf, 0xffffffbf, 0xffffff7f,
	0xfffffeff, 0xfffffdff, 0xfffffbff, 0xfffff7ff,
	0xffffefff, 0xffffdfff, 0xffffbfff, 0xffff7fff,
	0xfffeffff, 0xfffdffff, 0xfffbffff, 0xfff7ffff,
	0xffefffff, 0xffdfffff, 0xffbfffff, 0xff7fffff,
	0xfeffffff, 0xfdffffff, 0xfbffffff, 0xf7ffffff,
	0xefffffff, 0xdfffffff, 0xbfffffff, 0x7fffffff,
} ;

__uint32_t     G_errors = 0 ;

__uint64_t walk1s0s_64[WALK_64_SIZE] = {
	0x0000000000000001LL, 0x0000000000000002LL, 0x0000000000000004LL, 0x0000000000000008LL,
	0x0000000000000010LL, 0x0000000000000020LL, 0x0000000000000040LL, 0x0000000000000080LL,
	0x0000000000000100LL, 0x0000000000000200LL, 0x0000000000000400LL, 0x0000000000000800LL,
	0x0000000000001000LL, 0x0000000000002000LL, 0x0000000000004000LL, 0x0000000000008000LL,
	0x0000000000010000LL, 0x0000000000020000LL, 0x0000000000040000LL, 0x0000000000080000LL,
	0x0000000000100000LL, 0x0000000000200000LL, 0x0000000000400000LL, 0x0000000000800000LL,
	0x0000000001000000LL, 0x0000000002000000LL, 0x0000000004000000LL, 0x0000000008000000LL,
	0x0000000010000000LL, 0x0000000020000000LL, 0x0000000040000000LL, 0x0000000080000000LL,
	0x0000000100000000LL, 0x0000000200000000LL, 0x0000000400000000LL, 0x0000000800000000LL,
	0x0000001000000000LL, 0x0000002000000000LL, 0x0000004000000000LL, 0x0000008000000000LL,
	0x0000010000000000LL, 0x0000020000000000LL, 0x0000040000000000LL, 0x0000080000000000LL,
	0x0000100000000000LL, 0x0000200000000000LL, 0x0000400000000000LL, 0x0000800000000000LL,
	0x0001000000000000LL, 0x0002000000000000LL, 0x0004000000000000LL, 0x0008000000000000LL,
	0x0010000000000000LL, 0x0020000000000000LL, 0x0040000000000000LL, 0x0080000000000000LL,
	0x0100000000000000LL, 0x0200000000000000LL, 0x0400000000000000LL, 0x0800000000000000LL,
	0x1000000000000000LL, 0x2000000000000000LL, 0x4000000000000000LL, 0x8000000000000000LL,
	0xfffffffffffffffeLL, 0xfffffffffffffffdLL, 0xfffffffffffffffbLL, 0xfffffffffffffff7LL,
	0xffffffffffffffefLL, 0xffffffffffffffdfLL, 0xffffffffffffffbfLL, 0xffffffffffffff7fLL,
	0xfffffffffffffeffLL, 0xfffffffffffffdffLL, 0xfffffffffffffbffLL, 0xfffffffffffff7ffLL,
	0xffffffffffffefffLL, 0xffffffffffffdfffLL, 0xffffffffffffbfffLL, 0xffffffffffff7fffLL,
	0xfffffffffffeffffLL, 0xfffffffffffdffffLL, 0xfffffffffffbffffLL, 0xfffffffffff7ffffLL,
	0xffffffffffefffffLL, 0xffffffffffdfffffLL, 0xffffffffffbfffffLL, 0xffffffffff7fffffLL,
	0xfffffffffeffffffLL, 0xfffffffffdffffffLL, 0xfffffffffbffffffLL, 0xfffffffff7ffffffLL,
	0xffffffffefffffffLL, 0xffffffffdfffffffLL, 0xffffffffbfffffffLL, 0xffffffff7fffffffLL,
	0xfffffffeffffffffLL, 0xfffffffdffffffffLL, 0xfffffffbffffffffLL, 0xfffffff7ffffffffLL,
	0xffffffefffffffffLL, 0xffffffdfffffffffLL, 0xffffffbfffffffffLL, 0xffffff7fffffffffLL,
	0xfffffeffffffffffLL, 0xfffffdffffffffffLL, 0xfffffbffffffffffLL, 0xfffff7ffffffffffLL,
	0xffffefffffffffffLL, 0xffffdfffffffffffLL, 0xffffbfffffffffffLL, 0xffff7fffffffffffLL, 
	0xfffeffffffffffffLL, 0xfffdffffffffffffLL, 0xfffbffffffffffffLL, 0xfff7ffffffffffffLL,
	0xffefffffffffffffLL, 0xffdfffffffffffffLL, 0xffbfffffffffffffLL, 0xff7fffffffffffffLL,
	0xfeffffffffffffffLL, 0xfdffffffffffffffLL, 0xfbffffffffffffffLL, 0xf7ffffffffffffffLL,
	0xefffffffffffffffLL, 0xdfffffffffffffffLL, 0xbfffffffffffffffLL, 0x7fffffffffffffffLL
} ;

UBYTE comp_data[DMA_DATA_SIZE*2] = {
	0x10, 0x80, 0x10, 0x80, 0x20, 0x80, 0x20, 0x80,
	0x30, 0x80, 0x30, 0x80, 0x40, 0x80, 0x40, 0x80,
	0x50, 0x80, 0x50, 0x80, 0x60, 0x80, 0x60, 0x80,
	0x70, 0x80, 0x70, 0x80, 0x80, 0x80, 0x80, 0x80,

	0x90, 0x80, 0x90, 0x80, 0xa0, 0x80, 0xa0, 0x80,
	0xb0, 0x80, 0xb0, 0x80, 0xc0, 0x80, 0xc0, 0x80,
	0xd0, 0x80, 0xd0, 0x80, 0xe0, 0x80, 0xe0, 0x80,
	0xf0, 0x80, 0xf0, 0x80, 0x80, 0xf0, 0x80, 0xf0,

};


#if 0
__uint64_t dma_data[DMA_DATA_SIZE] = {
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,
	0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL, 0xa2a1a3a1a2a1a3a1LL,

} ;

#else
__uint64_t dma_data[DMA_DATA_SIZE] = {
    0x1111222233334444LL, 0x5555666677778888LL, 0x9999aaaabbbbccccLL, 0xddddeeeeffff0000LL,
    0x2222333344445555LL, 0x6666777788889999LL, 0xaaaabbbbccccddddLL, 0xeeeeffff00001111LL,
    0x3333444455556666LL, 0x777788889999aaaaLL, 0xbbbbccccddddeeeeLL, 0xffff000011112222LL,
    0x4444555566667777LL, 0x88889999aaaabbbbLL, 0xcccceeeeffff0000LL,

    0x5555666677778888LL, 0x9999aaaabbbbccccLL, 0xddddeeeeffff0000LL, 0x1111222233334444LL,
    0x6666777788889999LL, 0xaaaabbbbccccddddLL, 0xeeeeffff00001111LL, 0x2222333344445555LL,
    0x777788889999aaaaLL, 0xbbbbccccddddeeeeLL, 0xffff000011112222LL, 0x3333444455556666LL,
    0x777788889999aaaaLL, 0x1111222233334444LL, 0x5555666677778888LL, 
	0x9999aaaabbbbccccLL, 0xddddeeeeffff0000LL
} ;


#endif

unsigned short  walk1s0s_16[WALK_16_SIZE] = {
	0x0001, 0x0002, 0x0004, 0x0008, 
	0x0010, 0x0020, 0x0040, 0x0080,
	0x0100, 0x0200, 0x0400, 0x0800,
	0x1000, 0x2000, 0x4000, 0x8000,
	0xfffe, 0xfffd, 0xfffb, 0xfff7,
	0xffef, 0xffdf, 0xffbf, 0xff7f,
	0xfeff, 0xfdff, 0xfbff, 0xf7ff,
	0xefff, 0xdfff, 0xbfff, 0x7fff,
} ;

unsigned char  walk1s0s_8[WALK_8_SIZE] = {
	0x01, 0x02, 0x04, 0x08,
	0x10, 0x20, 0x40, 0x80,
	0xfe, 0xfd, 0xfb, 0xf7,
	0xef, 0xdf, 0xbf, 0x7f,
} ;

__uint64_t patrn64[PATRNS_64_SIZE] = {
	0x1111222233334444LL, 0xdeadbeefabbaeffeLL, 0x5555666677778888LL, 
    0x9999aaaabbbbccccLL, 0xddddeeeeffff0000LL,
    0x0000000000000001LL, 0x0000000000000002LL, 0x0000000000000004LL, 0x0000000000000008LL,
    0x0000000000000010LL, 0x0000000000000020LL, 0x0000000000000040LL, 0x0000000000000080LL,
    0x0000000000000100LL, 0x0000000000000200LL, 0x0000000000000400LL, 0x0000000000000800LL,
    0x0000000000001000LL, 0x0000000000002000LL, 0x0000000000004000LL, 0x0000000000008000LL,
    0x0000000000010000LL, 0x0000000000020000LL, 0x0000000000040000LL, 0x0000000000080000LL,
    0x0000000000100000LL, 0x0000000000200000LL, 0x0000000000400000LL, 0x0000000000800000LL,
    0x0000000001000000LL, 0x0000000002000000LL, 0x0000000004000000LL, 0x0000000008000000LL,
    0x0000000010000000LL, 0x0000000020000000LL, 0x0000000040000000LL, 0x0000000080000000LL,
    0x0000000100000000LL, 0x0000000200000000LL, 0x0000000400000000LL, 0x0000000800000000LL,
    0x0000001000000000LL, 0x0000002000000000LL, 0x0000004000000000LL, 0x0000008000000000LL,
    0x0000010000000000LL, 0x0000020000000000LL, 0x0000040000000000LL, 0x0000080000000000LL,
    0x0000100000000000LL, 0x0000200000000000LL, 0x0000400000000000LL, 0x0000800000000000LL,
    0x0001000000000000LL, 0x0002000000000000LL, 0x0004000000000000LL, 0x0008000000000000LL,
    0x0010000000000000LL, 0x0020000000000000LL, 0x0040000000000000LL, 0x0080000000000000LL,
    0x0100000000000000LL, 0x0200000000000000LL, 0x0400000000000000LL, 0x0800000000000000LL,
    0x1000000000000000LL, 0x2000000000000000LL, 0x4000000000000000LL, 0x8000000000000000LL,
    0xfffffffffffffffeLL, 0xfffffffffffffffdLL, 0xfffffffffffffffbLL, 0xfffffffffffffff7LL,
    0xffffffffffffffefLL, 0xffffffffffffffdfLL, 0xffffffffffffffbfLL, 0xffffffffffffff7fLL,
    0xfffffffffffffeffLL, 0xfffffffffffffdffLL, 0xfffffffffffffbffLL, 0xfffffffffffff7ffLL,
    0xffffffffffffefffLL, 0xffffffffffffdfffLL, 0xffffffffffffbfffLL, 0xffffffffffff7fffLL,
    0xfffffffffffeffffLL, 0xfffffffffffdffffLL, 0xfffffffffffbffffLL, 0xfffffffffff7ffffLL,
    0xffffffffffefffffLL, 0xffffffffffdfffffLL, 0xffffffffffbfffffLL, 0xffffffffff7fffffLL,
    0xfffffffffeffffffLL, 0xfffffffffdffffffLL, 0xfffffffffbffffffLL, 0xfffffffff7ffffffLL,
    0xffffffffefffffffLL, 0xffffffffdfffffffLL, 0xffffffffbfffffffLL, 0xffffffff7fffffffLL,
    0xfffffffeffffffffLL, 0xfffffffdffffffffLL, 0xfffffffbffffffffLL, 0xfffffff7ffffffffLL,
    0xffffffefffffffffLL, 0xffffffdfffffffffLL, 0xffffffbfffffffffLL, 0xffffff7fffffffffLL,
    0xfffffeffffffffffLL, 0xfffffdffffffffffLL, 0xfffffbffffffffffLL, 0xfffff7ffffffffffLL,
    0xffffefffffffffffLL, 0xffffdfffffffffffLL, 0xffffbfffffffffffLL, 0xffff7fffffffffffLL,
    0xfffeffffffffffffLL, 0xfffdffffffffffffLL, 0xfffbffffffffffffLL, 0xfff7ffffffffffffLL,
    0xffefffffffffffffLL, 0xffdfffffffffffffLL, 0xffbfffffffffffffLL, 0xff7fffffffffffffLL,
    0xfeffffffffffffffLL, 0xfdffffffffffffffLL, 0xfbffffffffffffffLL, 0xf7ffffffffffffffLL,
    0xefffffffffffffffLL, 0xdfffffffffffffffLL, 0xbfffffffffffffffLL, 0x7fffffffffffffffLL

};

#if 0
__uint64_t patrn64[PATRNS_64_SIZE] = {
	((__uint64_t) 0xa5a5a5a5a5a5a5a5L), ((__uint64_t) 0x0123456789abcdefL), 
        ((__uint64_t) 0x12344321abcddcbaL), ((__uint64_t) 0x1122334455667788L), ((__uint64_t) 0xdeadbeefabbaeffeL)
};
#endif


__uint32_t patrn32[PATRNS_32_SIZE] = {
	0xffffffff, 0x00000000, 0xa5a5a5a5, 0x5a5a5a5a, 0xaaaaaaaa,
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800,
    0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000,
    0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0xfffffffe, 0xfffffffd, 0xfffffffb, 0xfffffff7,
    0xffffffef, 0xffffffdf, 0xffffffbf, 0xffffff7f,
    0xfffffeff, 0xfffffdff, 0xfffffbff, 0xfffff7ff,
    0xffffefff, 0xffffdfff, 0xffffbfff, 0xffff7fff,
    0xfffeffff, 0xfffdffff, 0xfffbffff, 0xfff7ffff,
    0xffefffff, 0xffdfffff, 0xffbfffff, 0xff7fffff,
    0xfeffffff, 0xfdffffff, 0xfbffffff, 0xf7ffffff,
    0xefffffff, 0xdfffffff, 0xbfffffff, 0x7fffffff

};

unsigned short  patrn16[PATRNS_16_SIZE] = {
	0x1234, 0x5678, 0xa5a5, 0x9abc, 0xdef0,
    0x0001, 0x0002, 0x0004, 0x0008,
    0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800,
    0x1000, 0x2000, 0x4000, 0x8000,
    0xfffe, 0xfffd, 0xfffb, 0xfff7,
    0xffef, 0xffdf, 0xffbf, 0xff7f,
    0xfeff, 0xfdff, 0xfbff, 0xf7ff,
    0xefff, 0xdfff, 0xbfff, 0x7fff
};


UBYTE patrn8[PATRNS_8_SIZE] = {
	0x33, 0x44, 0x55, 0x66, 0x77,
    0x01, 0x02, 0x04, 0x08,
    0x10, 0x20, 0x40, 0x80,
    0xfe, 0xfd, 0xfb, 0xf7,
    0xef, 0xdf, 0xbf, 0x7f

};

