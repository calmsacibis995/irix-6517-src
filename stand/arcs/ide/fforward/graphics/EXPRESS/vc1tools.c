/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  VC1 Diagnostics
 */

#include "ide_msg.h"
#include "sys/gr2hw.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsc.h"
#include "uif.h"

/* #define MFG_USED 1 */

extern struct gr2_hw *base;

struct vc1label {
	short numbits;
	char name[15];
};
struct vc1label GR2_VC1_length[2][0x7f];  /* 0x7f == 1 + GR2_MODE_REG1F_O (largest address)*/ 

#define ON 1
#define OFF 0 

void
VC1menu(void)
{
	int offset,i,incr;
	msg_printf(SUM,"CMD#1: XMAP_MODE \n");
	msg_printf(SUM,"CMD#2: VC1 SRAM (External memory)\n");
	msg_printf(SUM,"CMD#3: Test Registers\n");
	msg_printf(SUM,"CMD#4: Addr, low byte\n");
	msg_printf(SUM,"CMD#5: Addr, hi byte\n");
	msg_printf(SUM,"CMD#6: System Control\n");

	msg_printf(SUM,"\nCMD#0: VID_TIMING, CURSOR_CTRL, DID_CTRL, XMAP_CTRL \n");
	msg_printf(SUM,"For VC1 REV 1.0, you must write HOR_DIV and HOR_MOD as a 16 bit value\n");
	msg_printf(SUM,"VC1 Hex Addresses used with CMD#0::\n");
	for (offset = 0x0; offset <= 0x62; ) {
		for (i=0;(i < 4) && (offset <=0x62); i++) {

			incr = 0;
			if (GR2_VC1_length[0][offset].numbits < 0)
				offset = - GR2_VC1_length[0][offset].numbits;

			msg_printf(SUM,"%s = %#x	", GR2_VC1_length[0][offset].name, offset);

			if (GR2_VC1_length[0][offset].numbits % 8 != 0)
				incr = 1;
			incr += (GR2_VC1_length[0][offset].numbits / 8);
			offset += incr;
		}
		msg_printf(SUM,"\n");

	}

} 

void
initvc1size(void) {
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_EP].numbits = 16;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_EP].name, "VID_EP");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LC].numbits = 12;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LC].name,  "VID_LC");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_SC].numbits = 10;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_SC].name , "VID_SC");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_TSA].numbits = 7;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_TSA].name , "VID_TSA");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_TSB].numbits = 7;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_TSB].name , "VID_TSB");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_TSC].numbits = 7;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_TSC].name , "VID_TSC");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LP].numbits = 16;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LP].name,  "VID_LP");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LS_EP].numbits = 16;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LS_EP].name,  "VID_LS_EP");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LR].numbits = 8;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_LR].name,  "VID_LR");
    GR2_VC1_length[GR2_VC1_VID_TIMING][0xE].numbits = -0x10;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][0xE].name , "DUMMY");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_FC].numbits = 32;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_FC].name,  "VID_FC");
    GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_ENAB].numbits = 6;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][GR2_VID_ENAB].name,  "VID_ENAB");
    GR2_VC1_length[GR2_VC1_VID_TIMING][0x15].numbits = -0x20;
    strcpy(GR2_VC1_length[GR2_VC1_VID_TIMING][0x15].name , "DUMMY");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_EP].numbits = 16;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_EP].name , "CUR_EP");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_XL].numbits = 11;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_XL].name , "CUR_XL");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_YL].numbits = 12;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_YL].name , "CUR_YL");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_MODE].numbits = 8 + 8; /* tmp fix*/
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_MODE].name , "CUR_MODE and GR2_CUR_BX");
/*
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_BX].numbits = 8;
*/
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_LY].numbits = 11;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_LY].name	, "CUR_LY");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_YC].numbits = 12;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_YC].name	, "CUR_YC");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][0x2C].numbits = -0x2E;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][0x2C].name	, "DUMMY");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_CC].numbits = 8;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_CC].name	, "CUR_CC");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][0x2F].numbits = -0x30;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][0x2F].name	, "DUMMY");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_RC].numbits = 12;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][GR2_CUR_RC].name	, "CUR_RC");
    GR2_VC1_length[GR2_VC1_CURSOR_CTRL][0x32].numbits  = -0x40;
    strcpy(GR2_VC1_length[GR2_VC1_CURSOR_CTRL][0x32].name , "DUMMY");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_EP].numbits  = 16;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_EP].name , "DID_EP");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_END_EP].numbits  = 16;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_END_EP].name , "DID_END_EP");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_HOR_DIV].numbits  = 8;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_HOR_DIV].name , "DID_HOR_DIV");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_HOR_MOD].numbits  = 3;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_HOR_MOD].name , "DID_HOR_MOD");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_LP].numbits  = 16;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_LP].name , "DID_LP");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_LS_EP].numbits  = 16;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_LS_EP].name , "DID_LS_EP");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_WC].numbits  = 16;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_WC].name , "DID_WC");
    GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_RC].numbits  = 16;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][GR2_DID_RC].name , "DID_RC");
    GR2_VC1_length[GR2_VC1_DID_CTRL][0x4E].numbits= -0x60;
    strcpy(GR2_VC1_length[GR2_VC1_DID_CTRL][0x4E].name , "DUMMY");
    GR2_VC1_length[GR2_VC1_XMAP_CTRL][GR2_BLKOUT_EVEN].numbits= 8 + 8; /* tmp fix*/ 
    strcpy(GR2_VC1_length[GR2_VC1_XMAP_CTRL][GR2_BLKOUT_EVEN].name , "BLKOUT_EVEN and ODD"); 
    GR2_VC1_length[GR2_VC1_XMAP_CTRL][GR2_AUXVIDEO_MAP].numbits = 8;
    strcpy(GR2_VC1_length[GR2_VC1_XMAP_CTRL][GR2_AUXVIDEO_MAP].name , "AUXVIDEO_MAP"); 
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_0_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_2_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_3_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_4_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_5_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_6_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_7_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_8_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_9_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_A_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_B_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_C_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_D_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_E_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_F_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_10_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_11_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_12_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_13_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_14_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_15_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_16_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_17_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_18_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_19_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1A_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1B_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1C_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1D_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1E_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1F_E].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_0_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_2_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_3_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_4_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_5_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_6_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_7_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_8_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_9_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_A_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_B_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_C_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_D_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_E_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_F_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_10_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_11_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_12_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_13_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_14_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_15_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_16_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_17_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_18_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_19_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1A_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1B_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1C_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1D_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1E_O].numbits = 16;
    GR2_VC1_length[GR2_VC1_XMAP_MODE][GR2_MODE_REG_1F_O].numbits = 16;

}

/*
 * wvc1() - write VC1 register
 *
 *	This is a tool to allow writing of arbitrary values to the various
 *	VC1 registers from the IDE command line.
 */
/*ARGSUSED*/
int
gr2_wvc1(int argc, char **argv)
{
#ifdef MFG_USED
    int cmd;		/* command for configsel */
    long offset;	/* VC1 register offset */
    long data,wr_data;  	/* VC1 register data */
    long i, count;  	/* Times to write to register*/
    int size;
    int intr;

    initvc1size();

    if (argc < 4) {
	msg_printf(ERR,"usage: %s CMD reg_offset data <count> <intr>)\n", argv[0]);
	VC1menu();
	return -1;
    }
    argv++;
    atob((*argv), &cmd);
    argv++;
    atob((*argv), &offset);
    argv++;
    atob((*argv), &data);

    if (argc >= 5) { 
	argv++;
    	count = atoi(*argv);
    }
    else  count = 1;

    if (argc == 6) {
	intr = ON; /*Wait for char from console before writing again*/
    }
    else intr = OFF;

	
    for (i = 0; i < count; i++,offset +=2) {
    	switch(cmd) {
		
   		case GR2_VC1_CMD0: /*  Frequently used control setting*/	
    			base->vc1.addrhi = (offset >> 8)  & 0x00ff;
    			base->vc1.addrlo = offset & 0x00ff;

    			size = GR2_VC1_length[cmd][offset].numbits;
    			wr_data = data & (0x1 << size) - 1 ;
		/*SW workaround cases */
			if (offset == GR2_VID_ENAB) { /* 0x14*/
                                wr_data = data & 0x3f00;
                        }
                        else if (offset == GR2_CUR_XL) { /* 0x22,0x23, 0x24, 0x25*/

                                /*Write to GR2_CUR_XL */
                                base->vc1.cmd0 = wr_data;
			msg_printf(DBG, "VC1 SW workaround, writing same value to GR2_CUR_YL\n");
                                /* Set up mask for writing to GR2_CUR_YL*/
                                size = GR2_VC1_length[cmd][GR2_CUR_YL].numbits;
                                wr_data = data & ((0x1 << size) - 1);
                        }
                        else if (offset == GR2_DID_HOR_DIV) {  /*0x44, 0x45*/
                                        wr_data = data & 0xff07;
                        }
                        else if (offset == GR2_AUXVIDEO_MAP)  /*0x62*/
                                        wr_data = data & 0xff00;

			base->vc1.cmd0 = wr_data; 
    			msg_printf(DBG,"Loop %d: Writing VC1 addr %#08x: %#08x\n", 
				i, offset, wr_data);
			break;
   		case GR2_VC1_XMAP_MODE:	
    			base->vc1.addrhi = (offset >> 8)  & 0x00ff;
    			base->vc1.addrlo = offset & 0x00ff;
			base->vc1.xmapmode = (data >> 8) & 0xff; 
			base->vc1.xmapmode = data & 0xff; 
    			msg_printf(DBG,"Loop %d: Writing XMAP_MODE addr %#08x: %#08x\n", 
				i, offset, data & 0xffff);
			break;
		case GR2_VC1_EXT_MEMORY:
    			base->vc1.addrhi = (5 >> 8)  & 0x00ff;
    			base->vc1.addrlo = 5 & 0x00ff;
			if ((base->vc1.testreg & 0x7) == 1) {
				msg_printf(ERR,"NOTE: In Rev 1 of VC1, can't read SRAM\n");
			}
    			base->vc1.addrhi = (offset >> 8)  & 0x00ff;
    			base->vc1.addrlo = offset & 0x00ff;
			base->vc1.sram = data; 
    			msg_printf(DBG,"Loop %d: Writing VC1_SRAM addr %#08x: %#08x\n", 
				i, offset, data & 0xffff);
			break;
		case GR2_VC1_TEST_REGS:
			if (offset != 3) {
				msg_printf(ERR,"Can only write to addr 0x3 of test reg\n");
				return -1;
			}
    			base->vc1.addrhi = (offset >> 8)  & 0x00ff;
    			base->vc1.addrlo = offset & 0x00ff;

			base->vc1.testreg = data & 0xff; 
			break;
		case GR2_VC1_ADDR_LOW:
			base->vc1.addrlo = data & 0xff; 
			break;
		case GR2_VC1_ADDR_HIGH:
			base->vc1.addrhi = data & 0xff; 
			break;
		case GR2_VC1_SYS_CTRL:
			base->vc1.sysctl = data & 0xff; 
			break;
    	}

    	if (intr == ON) {
		if (getchar() == 0x03) {
			base->vc1.sysctl = 0x3c;
			return 0;
		}
    	}
    }
/*
	 base->vc1.sysctl = 0x3c;
*/ 
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
#endif
    return 0;
}


/*
 * rvc1() - read VC1 register
 *
 *	This is a tool to allow reading of values from the various
 *	VC1 registers from the IDE command line.
 */
/*ARGSUSED*/
int
gr2_rvc1(int argc, char **argv)
{
#ifdef MFG_USED
    int cmd;		/* command for configsel */
    long offset;	/* VC1 register offset */
    long sh_offset,data;	/* offset used as an index*/
    long i,j, count,end_offset;
    int size,reg,incr;
    int intr;
    short rd_data[0x62*25];          /* VC1 register data */
    unsigned char lo_byte, hi_byte ;
	
    
    initvc1size();

    if (argc < 2) {
	msg_printf(ERR, "usage: %s CMD <start_offset (default = 0)> <offset of last byte read (default = 0x62)> <repeat (max = 50 for all CMD#0 addr)> <intr- any val>)\n", argv[0]);
	VC1menu();
	return -1;
    }
    argv++;
    atob((*argv), &cmd);

    if (argc >= 3) {
    argv++;
    atob((*argv), &offset);
    } else offset= 0x0;
    

    if (argc >= 4) {
    argv++;
    atob((*argv), &end_offset);
    } else end_offset= 0x62;
     
    if (argc >= 5) {
	argv++;
	count = atoi(*argv);
    }
    else count = 1;

    if (argc == 6) {
	intr = ON;	/*Wait for char from console before reading again*/
    }
    else intr = OFF;

    msg_printf(DBG, "Reading VC1 registers\n");

    for (i= j = 0; j < count; j++) {
    	base->vc1.addrhi = 0;
    	base->vc1.addrlo = offset;
    	for(sh_offset = offset; sh_offset <= end_offset;sh_offset+=2) {
		if (GR2_VC1_length[0][sh_offset].numbits < 0) {
                        sh_offset = - GR2_VC1_length[0][sh_offset].numbits;
    			base->vc1.addrhi = 0;
    			base->vc1.addrlo = sh_offset;
		}
		rd_data[i++] = base->vc1.cmd0;
	}
    }

    for (i = j =  0; j < count; j++) {
      for (reg = 0, sh_offset = offset; sh_offset <= end_offset;) {

    	   switch(cmd) {

                case GR2_VC1_CMD0: /*  Frequently used control setting*/
    			base->vc1.addrhi = 0;
    			base->vc1.addrlo = offset;
    	   		size = GR2_VC1_length[cmd][sh_offset].numbits;
			if (size > 24) {	
				data = 	(rd_data[i] << 24) | 
					(rd_data[i+1] << 16) |
					(rd_data[i+2] <<  8) |
					rd_data[i+3];
				i+=4;
				incr=4;
			}
			else if (size > 16) {	
				data =  (rd_data[i] << 16) |
					(rd_data[i+1] <<  8) |
					rd_data[i+2];
				i += 3;
				incr =3;
			}
			else if (size > 8) { 	
				data = (rd_data[i] <<  8) | rd_data[i+1];
				incr=2;
				i += 2;
			}
			else if (size > 0) { 
				data = rd_data[i++];
				incr=1;
			} else {
			 /* Addr of next readable register is (- size) */ 
				sh_offset = - size;
				base->vc1.addrhi = 0;
                                base->vc1.addrlo = - size;
				incr=0;
			}
			if (size > 0) { /*For all readable registers*/
        			msg_printf(DBG, "%s: %#x     ", GR2_VC1_length[cmd][sh_offset].name, data);
				reg++;
			} 
			sh_offset += incr;
                        break;
                case GR2_VC1_XMAP_MODE:
			base->vc1.addrhi = (sh_offset >> 8)  & 0x00ff;
                        base->vc1.addrlo = sh_offset & 0x00ff;
    			data =  (base->vc1.xmapmode << 8) | (base->vc1.xmapmode & 0xff);
			if (sh_offset < 0x40)
        			msg_printf(DBG, "XMAP_MODE REG%x_E: %#x     ", sh_offset/2, data);
			else
        			msg_printf(DBG, "XMAP_MODE REG%x_O: %#x     ", (sh_offset-0x40)/2, data);
			reg++;
			sh_offset+=2;
                        break;
                case GR2_VC1_EXT_MEMORY:
			base->vc1.addrhi = (5 >> 8)  & 0x00ff;
                        base->vc1.addrlo = 5 & 0x00ff;
			if ((base->vc1.testreg & 0x7) == 1) {
				msg_printf(ERR,"Can't read SRAM in VC1 REV 1.0\n");
				break;
			}
			base->vc1.addrhi = (sh_offset >> 8)  & 0x00ff;
                        base->vc1.addrlo = sh_offset & 0x00ff;
			data = base->vc1.sram ;
        		msg_printf(DBG, "VC1 SRAM[%#x]: %#x	",sh_offset, data & 0xffff);
			reg++;
			sh_offset+=2;
                        break;
		case GR2_VC1_TEST_REGS:
                        base->vc1.addrhi = (sh_offset >> 8)  & 0x00ff;
                        base->vc1.addrlo = sh_offset & 0x00ff;
                        data = base->vc1.testreg;
        		msg_printf(DBG, "Test register, addr %#x: %#x	",sh_offset, data);
			reg++; sh_offset++;
                        break;
                case GR2_VC1_ADDR_LOW:
			data = base->vc1.addrlo;
        		msg_printf(DBG, "Low addr byte: %#x	", data);
			reg++;
                        break;
                case GR2_VC1_ADDR_HIGH:
			data = base->vc1.addrhi;
        		msg_printf(DBG, "High addr byte: %#x	", data);
			reg++;
                        break;
                case GR2_VC1_SYS_CTRL:
			data = base->vc1.sysctl;
        		msg_printf(DBG, "System control: %#x	", data);
			reg++;
                        break;
    	}
	if ( (reg+4) % 4 == 0) {
		msg_printf(DBG, "\n"); 
	}

	/* Pause after writing 16 lines */
	if ((( reg + 4) % (4*9)) == 0) { 
		if (getchar() == 0x03) { 
			msg_printf(VRB, "Press any key to continue. \n");
			return 0;
		}
	}
 
      } /*inner  loop*/
    }   /* count loop*/	

	msg_printf(DBG, "\n"); 
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
#endif
    return 0;
}


/* ARGSUSED */
int
gr2_check_vc1(int argc, char *argv[])
{
#ifdef MFG_USED
    long hi_byte, wr_data, rd_data, rd_data2, rd_hi; 
    int size;

    int cmd; 
    unsigned long offset, data, rd_addr;
    long i,count;

    initvc1size();

    if (argc < 4) {
	printf("usage: %s CMD VC1_reg_offset data (optional count)\n", argv[0]);
	VC1menu();
        return -1;
    }
    argv++;
    atob(*argv,&cmd );
    argv++;
    atob(*argv,&offset );
    argv++;
    atob(*argv,&data);

    if (argc == 5) {
    argv++;
     count = atoi(*argv);
    }
    else count = 1;

    
    size = GR2_VC1_length[cmd][offset].numbits;
    wr_data = data & ((0x1 << size) - 1);
 
    for (i = 0; i < count; i++) {

    base->vc1.addrhi = (offset >> 8) & 0xff;
    
    base->vc1.addrlo = offset & 0x00ff;
    

    switch(cmd) {

                case GR2_VC1_CMD0: /*  Frequently used control setting*/
			if (offset == GR2_VID_ENAB) { /* 0x14*/
                                wr_data = data & 0x3f00;
                        }
                        else if (offset == GR2_CUR_XL) { /* 0x22,0x23, 0x24, 0x25*/

                                /*Write to GR2_CUR_XL */
                                base->vc1.cmd0 = wr_data;
    
                                /* Set up mask for writing to GR2_CUR_YL*/
                                size = GR2_VC1_length[cmd][GR2_CUR_YL].numbits;
                                wr_data = data & ((0x1 << size) - 1);
                        }
                        else if (offset == GR2_DID_HOR_DIV) {  /*0x44, 0x45*/
                                        wr_data = data & 0xff07;
                        }
                        else if (offset == GR2_AUXVIDEO_MAP)  /*0x62*/
                                        wr_data = data & 0xff00;

                        base->vc1.cmd0 = wr_data;
    
                        break;
                case GR2_VC1_XMAP_MODE:
                        base->vc1.xmapmode = (wr_data >> 8) & 0xff;
    
                        base->vc1.xmapmode = wr_data & 0xff;
    
                        break;
                case GR2_VC1_EXT_MEMORY:
                        base->vc1.sram = data;
    
                        break;
    }

    base->vc1.addrhi = (offset >> 8) & 0xff;
    
    base->vc1.addrlo = offset & 0x00ff;
    

    switch(cmd) {

                case GR2_VC1_CMD0: /*  Frequently used control setting*/
                        rd_data =  base->vc1.cmd0;
    			rd_data &= (0x1 << size) - 1;
                        break;
                case GR2_VC1_XMAP_MODE:
                        rd_data =  (base->vc1.xmapmode  << 8) | (base->vc1.xmapmode & 0xff) ;
    
    			rd_data &= (0x1 << size) - 1;
                        break;
                case GR2_VC1_EXT_MEMORY:
                        rd_data =  base->vc1.sram; 
			wr_data = data & 0xffff;
                        break;
    }

    if (rd_data  != wr_data)
    {
    
	rd_hi = (base->vc1.addrhi << 8) & 0xff00;
	rd_addr = (base->vc1.addrlo & 0xff) | rd_hi;

	msg_printf(ERR, "Loop #%d: Error reading from VC1 address %#x, returned: %#x, expected: %#x\n",
		 i, offset, rd_data, wr_data);
	msg_printf(ERR, "Address now set to %#x\n", rd_addr);
    }
  }
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif /* MFG_USED */
}


int
testvc1(short cmd, unsigned short offset, unsigned int data)
{
    int hi_byte, wr_data, rd_data;
    int rd_data2, wr_data2; /* used to test GR2_CUR_XL*/                                     
    int size, error;



    error = 0; /* used to test GR2_CUR_XL*/

    
    initvc1size();
    size = GR2_VC1_length[cmd][offset].numbits;
    wr_data = data & ((0x1 << size) - 1);

    base->vc1.addrhi = (offset >> 8) & 0xff;
    
    base->vc1.addrlo = offset & 0x00ff;
    
    switch(cmd) {

                case GR2_VC1_CMD0: /*  Frequently used control setting*/

			if (offset == GR2_VID_ENAB) { /* 0x14*/
		       		wr_data = data & 0x3f00; 
			}
			else if (offset == GR2_CUR_XL) { /* 0x22,0x23, 0x24, 0x25*/

				/*Write to GR2_CUR_XL */
                        	base->vc1.cmd0 = wr_data;
    
				/* Set up mask for writing to GR2_CUR_YL*/
				size = GR2_VC1_length[cmd][GR2_CUR_YL].numbits;
		       		wr_data = data & ((0x1 << size) - 1); 
			}
			else if (offset == GR2_DID_HOR_DIV) {  /*0x44, 0x45*/  
		       			wr_data = data & 0xff07; 
			}
			else if (offset == GR2_AUXVIDEO_MAP)  /*0x62*/
		       			wr_data = data & 0xff00; 

                        base->vc1.cmd0 = wr_data;
	
                        break;
                case GR2_VC1_XMAP_MODE:
                        base->vc1.xmapmode = (wr_data >> 8) & 0xff;
    
                        base->vc1.xmapmode = wr_data & 0xff;
    
                        break;
                case GR2_VC1_EXT_MEMORY:
                        base->vc1.sram = data;
    
                        break;
    }

    
    base->vc1.addrhi = (offset >> 8) & 0xff;
    
    base->vc1.addrlo = offset & 0x00ff;
    
    

    switch(cmd) {

                case GR2_VC1_CMD0: /*  Frequently used control setting*/
                        rd_data = base->vc1.cmd0 ;

			if (offset == GR2_VID_ENAB) /* 0x14*/
    					rd_data &= 0x3f00;
			else if (offset ==  GR2_CUR_XL) { /* 0x22,0x23, 0x24, 0x25*/
                                        /* read data from GR2_CUR_XL */
				size = GR2_VC1_length[cmd][offset].numbits;
				rd_data &= (0x1 << size) - 1;

                                        /* Read GR2_CUR_YL*/
                        	rd_data2 = base->vc1.cmd0 ;
    
                                size = GR2_VC1_length[cmd][GR2_CUR_YL].numbits;
			         rd_data2 &= (0x1 << size) - 1;	
			}
			else if (offset ==  GR2_DID_HOR_DIV)  /*0x44, 0x45*/
			{
                                        rd_data &= 0xff07;
			}
			else if (offset ==  GR2_AUXVIDEO_MAP)  /*0x62*/
			{
                                        rd_data &= 0xff00;
			}
			else
			{
					rd_data &= (0x1 << size) - 1;
			}
			break;

                case GR2_VC1_XMAP_MODE:
                        hi_byte =  (base->vc1.xmapmode << 8) & 0xff00 ;
    
                        rd_data =  (base->vc1.xmapmode & 0xff) ;
    
                        rd_data |= hi_byte; 
    
    			rd_data &= (0x1 << size) - 1;
                        break;

                case GR2_VC1_EXT_MEMORY:
                        rd_data =  base->vc1.sram;
		        wr_data = data & 0xffff;
                        break;
    }

    if ((cmd == GR2_VC1_CMD0) && (offset == GR2_CUR_XL ))
    {
	wr_data = data & 0x07ff;  /* 11 bits for GR2_CUR_XL */
    	if (rd_data  != wr_data)
    	{
	
		msg_printf(ERR, "\nError reading from VC1 address %#x, returned: %#x, expected: %#x\n",
 offset, rd_data, wr_data);
		 error++;
    	}
	wr_data2 = data & 0x0fff;  /* 12 bits for GR2_CUR_YL */
    	if (rd_data2  != wr_data2)
    	{
		msg_printf(ERR, "\nError reading from VC1 address %#x, returned: %#x, expected: %#x\n",
GR2_CUR_YL, rd_data2, wr_data2);
		 error++;
    	}
    }
    else {  
	if (rd_data  != wr_data) {
	msg_printf(ERR, "\nError reading from VC1 address %#x, returned: %#x, expected: %#x\n",
		 offset, rd_data, wr_data);
	error++;
    	}
    }
    return error;
}


/*ARGSUSED*/
int
gr2_testvc1(int argc, char *argv[])
{
#ifdef MFG_USED
    unsigned long pattern,addr;


    int part;  /* Portion of  vc1 to test */
    int all;  /* Check all parts of test*/
    int err,loop;

    err = 0;
    all = 1;
 

    if (argc < 2)
    {
	msg_printf(VRB, "Can also test w/r registers as separate categories.\n");
	msg_printf(VRB, "by specifing an arg  0=VID_TIMING, 1=CURS_CTRL, 2=DID_CTL");
	msg_printf(VRB, "3=XMAP_CTRL, 4=XMAP_MODE, 5=EXTERNAL SRAM\n");
    }
    if (argc == 2)
    {
    	argv++;
	part = atoi(*argv);
	all = 0;
    }
    msg_printf(SUM,"Testing VC1\n");
    for (loop = 0,pattern = 0x00000000; loop < 4; pattern += 0x55555555,loop++) {
	msg_printf(VRB,"Writing %#x pattern to VC1 registers\n",pattern);
	
	if (all | (part == 0))  {
	msg_printf(VRB,"Testing video timing registers\n");
	err += testvc1(GR2_VC1_VID_TIMING, GR2_VID_EP, pattern);
	err += testvc1(GR2_VC1_VID_TIMING, GR2_VID_ENAB, pattern);
	}
	if (all | (part == 1)) {
	msg_printf(VRB,"Testing cursor control registers\n");
	err += testvc1(GR2_VC1_CURSOR_CTRL, GR2_CUR_EP, pattern);
	err += testvc1(GR2_VC1_CURSOR_CTRL, GR2_CUR_XL, pattern);

/* Won't be updated unless after write to GR2_CUR_XL
 *	err += testvc1(GR2_VC1_CURSOR_CTRL, GR2_CUR_YL, pattern);
*/
	err += testvc1(GR2_VC1_CURSOR_CTRL, GR2_CUR_MODE, pattern);

/* Won't be updated unless after write to GR2_CUR_MODE
 *	err += testvc1(GR2_VC1_CURSOR_CTRL, GR2_CUR_BX, pattern);
*/
	err += testvc1(GR2_VC1_CURSOR_CTRL, GR2_CUR_LY, pattern);
	}
	if (all | (part == 2)) {
	msg_printf(VRB,"Testing DID control registers\n");
	err += testvc1(GR2_VC1_DID_CTRL, GR2_DID_EP, pattern);
	err += testvc1(GR2_VC1_DID_CTRL, GR2_DID_END_EP, pattern);
	err += testvc1(GR2_VC1_DID_CTRL, GR2_DID_HOR_DIV, pattern);

/* Won't be updated unless after write to GR2_DID_HOR_DIV 
 *	err += testvc1(GR2_VC1_DID_CTRL, GR2_DID_HOR_MOD, pattern);
*/
	} 
	if (all | (part == 3)) {
	msg_printf(VRB,"Testing XMAP control registers\n");
	err += testvc1(GR2_VC1_XMAP_CTRL, GR2_BLKOUT_EVEN, pattern);
/* Won't be updated unless after write to GR2_BLKOUT_EVEN 
 *err += testvc1(GR2_VC1_XMAP_CTRL, BLKOUT_ODD, pattern);
*/
	err += testvc1(GR2_VC1_XMAP_CTRL, GR2_AUXVIDEO_MAP, pattern);
	}
	if (all | (part == 4)) {
	msg_printf(VRB,"Testing XMAP mode registers\n");
		for(addr = 0; addr < 0x80; addr+=2)
		err += testvc1(GR2_VC1_XMAP_MODE, addr, pattern);
	}
	if (all | (part == 5)) {
		base->vc1.addrlo = 5;
		base->vc1.addrhi = 0;
	if ((base->vc1.testreg & 0x7) == 1) 
		msg_printf(VRB,"Using VC1 Rev. 1, skip SRAM test\n");
        else {
	msg_printf(VRB,"Testing External VC1 SRAM\n");
		for (addr = 0; addr < 0x10000 ; addr+=2)
        		err += testvc1(GR2_VC1_EXT_MEMORY, addr, pattern);
	}
	}
    }
    msg_printf(DBG, "\n");
    if (!err)
    	msg_printf(SUM, "VC1 tests --  PASSED\n"); 
    return err; 
#else
msg_printf(SUM, "%s is available in manufacturing scripts only\n", argv[0]);
return 0;
#endif /* MFG_USED */
}

/*ARGSUSED*/
int
gr2_test_vc1_sram(int argc, char **argv)
{
	int i, write_data,read_data;
	int start_offset ,end_offset,offset;
	int err = 0 ;

    	if (argc >= 2) {
    		argv++;
    		atob((*argv), &start_offset );
    	} else start_offset= 0x0;

    	if (argc >= 3) {
    		argv++;
    		atob((*argv), &end_offset );
    	} else end_offset= 0x8000;

	/*
    	if (argc >= 4) {
    		argv++;
    		atob((*argv), &write_data);
    	} else write_data= 0xff;

	*/

    	initvc1size();

	base->vc1.addrhi = (5 >> 8)  & 0x00ff;
	base->vc1.addrlo = 5 & 0x00ff;
	if ((base->vc1.testreg & 0x7) == 1) {
		msg_printf(ERR,"NOTE: In Rev 1 of VC1, can't read SRAM\n");
	}


	for ( i = 0 ; i < 4; i ++)
	{
		write_data = 0x5555 *i ;

		for (  offset = start_offset  ; offset < end_offset ; offset += 2)
		{
			base->vc1.addrhi = (offset >> 8)  & 0x00ff;
			base->vc1.addrlo = offset & 0x00ff;
			base->vc1.sram = write_data; 
			msg_printf(DBG,"Writing VC1_SRAM addr %#08x: %#08x\n", 
				 offset, write_data & 0xffff);

		}
		for ( offset = start_offset  ; offset < end_offset ; offset += 2)
		{
			base->vc1.addrhi = (offset >> 8)  & 0x00ff;
			base->vc1.addrlo = offset & 0x00ff;
			read_data = base->vc1.sram;
			msg_printf(DBG, "VC1 SRAM[%#x]: %#x\n",offset, read_data & 0xffff);

			if ( write_data != read_data ) {
				err ++ ;
				msg_printf(ERR,"VC1 SRAM: Mismatch at %#x expected %#x actual %#x\n",
						offset,write_data, read_data);
			}
		}
	}

	return err ;
}
