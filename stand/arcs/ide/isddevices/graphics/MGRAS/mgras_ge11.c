/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <libsc.h>
#include <libsk.h>
#include "uif.h"
#include <sys/mgrashw.h>
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"

#ifndef _STANDALONE
#include <stdio.h>
#endif

#if HQ4
#undef WAIT_TIL_SYNC_FIFO_EMPTY
#define	WAIT_TIL_SYNC_FIFO_EMPTY DELAY(50);
#endif

static char *on[] = { NULL, "1" };
static char *off[] = { NULL, "0" };


/* Diagnostic Registers Address Map */

struct ge11reg {
	char *name;
	__uint32_t addr_offset;
	__uint32_t    mask;
};

struct ge11reg ge_diag_reg[] = {
{"BreakPoint Address Register",BREAKPT_ADDR,0x3ffff},
{"Write/Sync Fifi Control Register",WRT_SYNC_FIFO_CONTROL,0x3f},
{"Diag_Sel Register",DIAG_SEL,0xff},
{"CM/Ofifo Tag Config",CM_OFIFO_TAG_CONFIG,0x1fff1}
};

__uint32_t ge11ucode_addr_pat[] = {
	0x1, 0x2, 0x4, 0x8,
	0x10, 0x20, 0x40, 0x80,
	0x100, 0x200, 0x400, 0x800,
	0x1000, 0x2000, 0x4000, 0x8000,
	0xfffe, 0xfffd, 0xfffb, 0xfff7,
	0xffef, 0xffdf, 0xffbf, 0xff7f,
	0xfeff, 0xfdff, 0xfbff, 0xf7ff,
	0xefff, 0xdfff, 0xbfff, 0x7fff
};
__uint32_t	ucode_walk_1_0[] = {
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

/* Turn on GE11 #0 diagnostic acess mode */
__uint32_t hq3_config,gio_config,orig_gio_config,orig_hq3_config;


int   		activeGe = 0;	/* active GE in a 2 Ge system */

/* DOUBT : This seems to be a redundant statement.
mgras_set_active_ge function assigns the variable.
so, we do not assign here

__uint32_t  ge11_diag_a_addr = GE11_0_DIAG_A_ADDR;
__uint32_t  ge11_diag_d_addr = GE11_0_DIAG_D_ADDR;
*/
__uint32_t  ge11_diag_a_addr ;
__uint32_t  ge11_diag_d_addr ;




void
HQ3_DIAG_ON(void)
{
	/* Turn on HQ3 GE0 diag mode */
	/* Save HQ3_CONFIG & GIO_CONFIG */

	WAIT_TIL_SYNC_FIFO_EMPTY;

	HQ3_PIO_RD(HQ3_CONFIG_ADDR, HQ3_CONFIG_MASK, hq3_config);
	orig_hq3_config = hq3_config;
	HQ3_PIO_RD(GIO_CONFIG_ADDR, GIO_CONFIG_MASK, gio_config);
	orig_gio_config = gio_config;

	WAIT_TIL_SYNC_FIFO_EMPTY;

	if (activeGe == 1)	
		hq3_config = (hq3_config & ~HQ3_CONFIG_GEDIAG_MASK) |  HQ3_DIAG_READ_GE1 | HQ3_UCODE_STALL;
	else
		hq3_config = (hq3_config & ~HQ3_CONFIG_GEDIAG_MASK) |  HQ3_DIAG_READ_GE0 | HQ3_UCODE_STALL;
	HQ3_PIO_WR(HQ3_CONFIG_ADDR, hq3_config , HQ3_CONFIG_MASK);

	gio_config |= HQBUS_BYPASS_BIT;
	HQ3_PIO_WR(GIO_CONFIG_ADDR, gio_config, ~0x0);
msg_printf(5,"hq3_config = 0x%x, gio_config = 0x%x\n",hq3_config,gio_config);
}


void
HQ3_DIAG_OFF(void)
{
	orig_hq3_config &= ~(HQ3_UCODE_STALL | HQ3_DIAG_READ_GE0 | HQ3_DIAG_READ_GE1);

	WAIT_TIL_SYNC_FIFO_EMPTY;
	HQ3_PIO_WR(HQ3_CONFIG_ADDR, orig_hq3_config, HQ3_CONFIG_MASK);
	WAIT_TIL_SYNC_FIFO_EMPTY;
	HQ3_PIO_WR(GIO_CONFIG_ADDR, orig_gio_config, GIO_CONFIG_MASK);
}

void
GE11_DIAG_ON(void)
{
	HQ3_DIAG_ON();

	/* GE11 diag set-up */

	/* Disable GE11 OUTPUT FIFO first */
	HQ3_PIO_WR(ge11_diag_a_addr, OUTPUT_FIFO_CNTL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, DIAG_MODE | (OFIFO_WM << 1) , ~0x0);

	HQ3_PIO_WR(ge11_diag_a_addr, DIAG_SEL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, (GE11_DIAGSEL_INPUT) , ~0x0);

	/* Write RAAM Conf Register*/
        GE11_DIAG_WR_PTR(CM_OFIFO_TAG_CONFIG);
        DELAY(100);
        GE11_DIAG_WR_IND(GE11_COTM_CONFIG_VAL,0x1fff1);
}


/* Turn off GE11 diagnostic acess mode */

void
GE11_DIAG_OFF(void)
{
	/* enable GE OUTPUT FIFO */
	HQ3_PIO_WR(ge11_diag_a_addr, OUTPUT_FIFO_CNTL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, (OFIFO_WM << 1) & ~DIAG_MODE, ~0x0);

	/* Restore GIO_CONFIG & HQ3_CONFIG */
	HQ3_DIAG_OFF();
}


/*
 * Reroute the output of the GE to HQ instead of RE.
 */
void
GE11_ROUTE_GE_2_HQ(void)
{
	HQ3_DIAG_ON();

	/* GE11 diag set-up */
	/* Reroute GE output to HQ */
	HQ3_PIO_WR(ge11_diag_a_addr, OUTPUT_FIFO_CNTL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, (OFIFO_WM << 1) | ROUTE_RE_2_HQ, ~0x0);

	HQ3_PIO_WR(ge11_diag_a_addr, EXECUTION_CONTROL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, 0x1, ~0x0);

	/* remove bypass mode */
	HQ3_DIAG_OFF();
}



void
GE11_ROUTE_GE_2_RE(void)
{
	HQ3_DIAG_ON();

	/* GE11 diag set-up */

	/* Reroute GE output to HQ */
	HQ3_PIO_WR(ge11_diag_a_addr, OUTPUT_FIFO_CNTL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, (OFIFO_WM << 1), ~0x0);

	HQ3_PIO_WR(ge11_diag_a_addr, EXECUTION_CONTROL, 0x7fffffff);
	HQ3_PIO_WR(ge11_diag_d_addr, 0x1, ~0x0);

	HQ3_DIAG_OFF();
}


__uint32_t
mgras_set_active_ge(int argc, char** argv)
{
	__uint32_t newge = 0, errors = 0;
	unsigned char bdvers1;
	__uint32_t numges;

	if (argc > 1)
		atohex(argv[1], (int*)&newge);

	if (newge > 1 /* || newge < 0 usigned alwasy > 0 */) {
		msg_printf(ERR,"Invalid argument.\n");
		return(1);
	}

	MGRAS_GET_BV1(mgbase, bdvers1);
	numges = MGRAS_BV1_NUM_GE(bdvers1);

	if (newge > (numges-1)) {
		msg_printf(VRB,"GE %d non existant (only %d GEs)\n",
			   newge,numges);
		newge = 0;
	}

	if (newge == 1) {
		activeGe = 1;
		ge11_diag_a_addr = GE11_1_DIAG_A_ADDR;
		ge11_diag_d_addr = GE11_1_DIAG_D_ADDR;
		msg_printf(VRB,"GE11_1 is the active ge\n");	
	}
	else {
		activeGe = 0;
		ge11_diag_a_addr = GE11_0_DIAG_A_ADDR;
		ge11_diag_d_addr = GE11_0_DIAG_D_ADDR;
		msg_printf(VRB,"GE11_0 is the active ge\n");	
	}

	return (errors);
}


__uint32_t
GE11_Diag_read_Dbl(__uint32_t mask, __uint32_t *data_h, __uint32_t *data_l)
{
	__uint32_t  value, time_out;
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	/* Wait until Diag Data is available  or time-out*/
/* msg_printf(5,"FLAG_SET_PRIV_ADDR = %x,GE11_DIAG_READ= 0x%x\n",FLAG_SET_PRIV_ADDR,GE11_DIAG_READ); */

/* msg_printf(5,"waiting for flag to be set or time_out\n"); */ 
	WAIT_TIL_SYNC_FIFO_EMPTY;
	HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, value);
#if HQ4
	time_out = 2000;
#else
	time_out= 2000;
#endif
	do {
		WAIT_TIL_SYNC_FIFO_EMPTY;
		HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, value);
		DELAY(10);
		time_out--;
	} while (((value & GE11_DIAG_READ) == 0) && time_out > 0);
#if !HQ4
	if (time_out == 0) {
		msg_printf(1,"Time out\n");
		return (1);
	}
	msg_printf(5,"flag = 0x%x\n",value);
#endif
/*	msg_printf(5,"Reading(DIAG_RD_WD0 << 2 ) + REIF_CTX_BASE = 0x%x\n",(DIAG_RD_WD0 << 2 ) + REIF_CTX_BASE);  */
		WAIT_TIL_SYNC_FIFO_EMPTY;

		HQ3_PIO_RD((DIAG_RD_WD0 << 2 ) + REIF_CTX_BASE, mask, *data_h);	
		HQ3_PIO_RD((DIAG_RD_WD1 << 2 ) + REIF_CTX_BASE, mask, *data_l);	
		return(0);


}

/* GE diagnostic single data read */

__uint32_t
GE11_Diag_read(__uint32_t mask)
{
	__uint32_t  value, time_out;

	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	/* Wait until Diag Data is available  or time-out */

msg_printf(5,"FLAG_SET_PRIV_ADDR = %x,GE11_DIAG_READ= 0x%x\n",FLAG_SET_PRIV_ADDR,GE11_DIAG_READ);

	WAIT_TIL_SYNC_FIFO_EMPTY;
	HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, value);

/* Bump this value up for R12K 300M HZ and up */
#if HQ4
	time_out = 5000;
#else
	time_out = 5000;
#endif
	do {
		WAIT_TIL_SYNC_FIFO_EMPTY;
		HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, value);
		DELAY(10);
		time_out--;
	} while (((value & GE11_DIAG_READ) == 0) && time_out > 0);
#if !HQ4
	if (time_out == 0) {
		msg_printf(ERR,"HQ-GE communication time out\n");
		return 1;
	}
#endif
	msg_printf(DBG,"flag = 0x%x\n",value);

	msg_printf(DBG,"Reading(DIAG_RD_WD0 << 2 ) + REIF_CTX_BASE = 0x%x\n",(DIAG_RD_WD0 << 2 ) + REIF_CTX_BASE);  
		WAIT_TIL_SYNC_FIFO_EMPTY;

		HQ3_PIO_RD((DIAG_RD_WD0 << 2 ) + REIF_CTX_BASE, mask, value);	
		HQ3_PIO_RD((DIAG_RD_WD1 << 2 ) + REIF_CTX_BASE, mask, value);	

	msg_printf(DBG,"Finish Reading\n");  
	msg_printf(DBG,"value = 0x%x\n",value);

	return(value);
}

#if 0
/* GE11 Diagnostic block read -
   Assumeing the Diagnostic Acess is on */   

GE11_Diag_blk_read(__uint32_t n, __uint32_t buff[])
{
	__uint32_t  time_out, value, mask, i ;

	/* Poll the flag and read the data in */

	for (i =0; i < n; i++)
	{

		time_out = 20; /* KY: Set it to be 20 for now */
		do {
		   HQ3_PIO_RD(FLAG_SET_PRIV_ADDR, HQ3_FLAGS_MASK, value);
		   DELAY(1000);
		   time_out--;
		} while (((value & GE11_DIAG_READ) != GE11_DIAG_READ) && (time_out > 0));

	        /* Wait until Diag Data is available  or time-out*/
		if (time_out != 0){
		   HQ3_PIO_RD((DIAG_RD_WD0 << 2 ) + REIF_CTX_BASE, mask, value);	
	           buff[i] = value;
		}
	   	else 
	      	   msg_printf(ERR,"GE0_DIAG_BLK_READ Timeout\n");	

	}
	return;
}
#endif



__uint32_t
mgras_hq3_ge11_diagpath(int argc, char** argv)
{
	__uint32_t i,offset, expected,actual, errors = 0;

	if (argc != 2) {
		msg_printf(SUM, "usage: %s mg_diag_path  register_offset \n" , argv[0]);
		return -1;
	}	

	atohex(argv[1], (int*)&offset);

	for (i = 0, expected = 1; i < 32; i++, expected <<= 1 ) 
	{
	   /* Write pattern to the first GE11 RAMscan Control register */

	      GE11_DIAG_WR_PTR(RAMSCAN_CONTROL);

	      GE11_DIAG_WR_IND(expected,RAMSCAN_CNTROL_MASK);

	      /* write a compliment data to the bus */

	      GE11_DIAG_WR_IND(~expected,RAMSCAN_CNTROL_MASK);
	   
	   /* Read back and verify */

	      GE11_DIAG_WR_PTR(RAMSCAN_CONTROL);

	      actual = GE11_Diag_read(RAMSCAN_CNTROL_MASK);
	      COMPARE("HQ3 to GE Diagpath test", RAMSCAN_CONTROL, expected, actual, RAMSCAN_CNTROL_MASK, errors);
	} 
	return errors;
}

#ifdef MFG_USED
__uint32_t
mgras_hq3_ge11_diag_wr(int argc, char** argv)
{
	__uint32_t offset, expected, errors = 0;

	if (argc != 3) {
		msg_printf(SUM, "usage: %s register_offset data \n" , argv[0]);
		return 1;
	}	

	atohex(argv[1], (int*)&offset);
	atohex(argv[2], (int*)&expected);
	/* Write data into register */

	msg_printf(1,"offset = 0x%x, data = 0x%x\n",offset,expected);
	GE11_DIAG_WR_PTR(offset);
	DELAY(100);
	GE11_DIAG_WR_IND(expected,~0);
	DELAY(100);
	return(errors);
}

__uint32_t
mgras_hq3_ge11_ucode_wr(int argc, char** argv)
{
	__uint32_t offset, expected, errors = 0;

	if (argc != 3) {
		msg_printf(SUM, "usage: %s ucode_offset data \n" , argv[0]);
		return 1;
	}	

	atohex(argv[1], (int*)&offset);
	atohex(argv[2], (int*)&expected);
	/* Write data into register */

	GE11_DIAG_ON();

	GE11_DIAG_WR_PTR(GE11_UCODE_BASE + offset);

	GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	DELAY(10);
	GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	DELAY(10);
	GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	DELAY(10);
	GE11_DIAG_OFF();
	return(errors);
}

__uint32_t
mgras_hq3_ge11_ucode_rd(int argc, char** argv)
{
	__uint32_t base, i,offset, expected1, expected2, expected3, errors = 0;
	__uint32_t ucode0, ucode1, ucode2, ucode3, ucode4, ucode5;

	if (argc != 5) {
		msg_printf(SUM, "usage: %s register_offset data1 data2 data3 \n" , argv[0]);
		return 1;
	}	

	atohex(argv[1], (int*)&offset);
	atohex(argv[2], (int*)&expected1);
	atohex(argv[3], (int*)&expected2);
	atohex(argv[4], (int*)&expected3);
        msg_printf(1,"ucode offset = 0x%x \n",offset);

	GE11_DIAG_ON();

	base = GE11_UCODE_BASE;
	GE11_DIAG_WR_PTR(base + offset);

	for (i = 0 ; i < 1; i++) /* only the first address (0) is tested */
	{

	   GE11_DIAG_WR_IND(expected1, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected2, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected3, GE11_UCODE_MASK);

	} 

	/* Read back and verify */
	msg_printf(DBG, "Ucode Write completed\n");
	DELAY(5000);

	GE11_DIAG_WR_PTR(base + offset);
	for (i = 0 ; i < 1; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   }

	msg_printf(DBG,"\nUcodeWalking_Bit: First pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((base + + offset + 4));
	for (i = 1 ; i < 1; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode2 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode3 = GE11_Diag_read(GE11_UCODE_MASK);

	}
	msg_printf(DBG,"\nUcodeWalking_Bit: Second pass ucode read complete\n");
	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode4 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode5 = GE11_Diag_read(GE11_UCODE_MASK);

	GE11_DIAG_OFF();
#if 0
	GE11_DIAG_ON();

	GE11_DIAG_WR_PTR(GE11_UCODE_BASE + offset);

	GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	DELAY(10);
	GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	DELAY(10);
	GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	DELAY(10);


	GE11_DIAG_WR_PTR(GE11_UCODE_BASE + offset);
	GE11_DIAG_WR_PTR((1 << 31) | 0x2);
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);

        ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	GE11_DIAG_WR_PTR(GE11_UCODE_BASE + offset + 4);
	GE11_DIAG_WR_PTR((1 << 31) | 0x2);
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);

        ucode2 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode3 = GE11_Diag_read(GE11_UCODE_MASK);

	GE11_DIAG_OFF();
#endif
        msg_printf(VRB,"GE11 ucode Data0 = 0x%x, Data1 = 0x%x,Data2=0x%x,data3 = 0x%x\n",ucode0,ucode1,ucode2,ucode3);

	return(errors);
}
#endif /* MFG_USED */

__uint32_t
mgras_hq3_ge11_diag_rd(int argc, char **argv)
{
	__uint32_t offset, actual, errors = 0;

	if (argc != 2) {
		msg_printf(SUM, "usage: %s register_offset \n" , argv[0]);
		return 1;
	}	

	atohex(argv[1], (int*)&offset);
        msg_printf(1,"reg. offset = 0x%x \n",offset);
	WAIT_TIL_SYNC_FIFO_EMPTY;
	GE11_DIAG_WR_PTR(offset);
	GE11_DIAG_WR_PTR((1 << 31) | 0x1);

	DELAY(200);
        actual = GE11_Diag_read(~0);
        msg_printf(VRB,"Register 0x%x = 0x%x\n",offset,actual);

	return(errors);
}

#ifdef MFG_USED
__uint32_t
mgras_hq3_ge11_reg_test(int argc, char** argv)
{
	__uint32_t errors = 0;
	__uint32_t i, reg_index, pattern, actual;

	GE11_DIAG_ON();

	/* Walking bit test */
	for (reg_index = 0; reg_index < GE11_REGS_SIZE ; reg_index++)
	{
	   /* Walking 1 */
	   for (i = 0, pattern = 1; i < 18; i++, pattern <<= 1)
	   {
	      /* Write */
	      GE11_DIAG_WR_PTR(ge_diag_reg[reg_index].addr_offset);
              DELAY(100);
	      GE11_DIAG_WR_IND(pattern,ge_diag_reg[reg_index].mask);
	      DELAY(100);

	      /* Read and Compare */
	      WAIT_TIL_SYNC_FIFO_EMPTY;
	      GE11_DIAG_WR_PTR(ge_diag_reg[reg_index].addr_offset);
	      GE11_DIAG_WR_PTR((1 << 31) | 0x1);
	      actual = GE11_Diag_read(ge_diag_reg[reg_index].mask);
	      COMPARE("GE11 Reg. test - word", ge_diag_reg[reg_index].addr_offset, pattern, actual, ge_diag_reg[reg_index].mask, errors);
	   }
	   /* Walking 0 */
	   for (i = 0, pattern = 0x3fffe; i < 18; i++, (pattern <<= 1) | 1)
	   {
	      /* Write */
	      GE11_DIAG_WR_PTR(ge_diag_reg[reg_index].addr_offset);
              DELAY(100);
	      GE11_DIAG_WR_IND(pattern,ge_diag_reg[reg_index].mask);
	      DELAY(100);

	      /* Read and Compare */
	      WAIT_TIL_SYNC_FIFO_EMPTY;
	      GE11_DIAG_WR_PTR(ge_diag_reg[reg_index].addr_offset);
	      GE11_DIAG_WR_PTR((1 << 31) | 0x1);
	      actual = GE11_Diag_read(ge_diag_reg[reg_index].mask);
	      COMPARE("GE11 Reg. test - word", ge_diag_reg[reg_index].addr_offset, pattern, actual, ge_diag_reg[reg_index].mask, errors);
	   }
	}

	GE11_DIAG_OFF();
	return errors;
}

__uint32_t
mgras_GE11_ucode_DataBus(void)
{
	__uint32_t i ,  pattern_index, errors =0;
	__uint32_t ucode0, ucode1;
	__uint32_t expected;
	__uint32_t ram_size;

	msg_printf(SUM, "MGRAS GE11 Ucode Data Bus Test\n");

 	SET_RAM_SIZE(ram_size); 
	/* setup diagnostic acess first */

	GE11_DIAG_ON();


    for (pattern_index = 0; pattern_index < 64; pattern_index++)
    {
	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);

	expected = ucode_walk_1_0[pattern_index]; 
	msg_printf(DBG,"\npattern = 0x%x\n",expected);

	for (i = 0 ; i < 1; i++) /* only the first address (0) is tested */
	{

	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);

	} 

	/* Read back and verify */
	msg_printf(DBG, "Ucode Write completed\n");
	DELAY(5000);

	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	for (i = 0 ; i < 1; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   /* Compare data */
	   if ((i&0x1) ==  0)   {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	}

	msg_printf(DBG,"\nUcodeWalking_Bit: First pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((GE11_UCODE_BASE + 4));
	for (i = 1 ; i < 1; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   if ((i & 0x1) == 0)  { 
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }

	}
	msg_printf(DBG,"\nUcodeWalking_Bit: Second pass ucode read complete\n");
	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);
    }
	GE11_DIAG_OFF();
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_DATA_BUS_TEST]), errors);
}

__uint32_t
mgras_GE11_ucode_AddrBus(void)
{
	__uint32_t i ,  pattern_index, errors =0;
	__uint32_t ucode0, ucode1;
	__uint32_t expected;
	__uint32_t ram_size;
	__uint32_t j, base;

	msg_printf(SUM, "MGRAS GE11 Ucode Address Bus Test\n");

 	SET_RAM_SIZE(ram_size); 
	/* setup diagnostic acess first */

	GE11_DIAG_ON();

	base = GE11_UCODE_BASE;
  for ( j = 0,base = GE11_UCODE_BASE | 1<<j++; base < (GE11_UCODE_BASE + ram_size); base = GE11_UCODE_BASE | 1<<j++) 
  {
    for (pattern_index = 0; pattern_index < 64; pattern_index++)
    {
	GE11_DIAG_WR_PTR(base);

	expected = ucode_walk_1_0[pattern_index]; 
	msg_printf(DBG,"\npattern = 0x%x\n",expected);

	for (i = 0 ; i < 1; i++) /* only the first address (0) is tested */
	{

	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);

	} 

	/* Read back and verify */
	msg_printf(DBG, "Ucode Write completed\n");
	DELAY(5000);

	GE11_DIAG_WR_PTR(base);
	for (i = 0 ; i < 1; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   /* Compare data */
	   if ((i&0x1) ==  0)   {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	}

	msg_printf(DBG,"\nUcodeWalking_Bit: First pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((base + 4));
	for (i = 1 ; i < 1; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	   if ((i & 0x1) == 0)  { 
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }

	}
	msg_printf(DBG,"\nUcodeWalking_Bit: Second pass ucode read complete\n");
	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);
    }
  }
	GE11_DIAG_OFF();
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_ADDR_BUS_TEST]), errors);
}

__uint32_t
mgras_GE11_ucode_Walking_bit(void)
{
	__uint32_t i ,led,  pattern_index, errors =0;
	__uint32_t ucode0, ucode1;
	__uint32_t expected;
	__uint32_t ram_size;

	msg_printf(SUM, "MGRAS GE11 Ucode Walking Bit Test\n");

	SET_RAM_SIZE(ram_size);
	/* setup diagnostic acess first */

	GE11_DIAG_ON();


    for (pattern_index = 0; pattern_index < 64; pattern_index++)
    {
	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);

	expected = ucode_walk_1_0[pattern_index]; 
	msg_printf(DBG,"\npattern = 0x%x\n",expected);

	for (i = 0 ; i < ram_size; i++)
	{

	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);

	} 

	/* Read back and verify */
	msg_printf(DBG, "Ucode Write completed\n");
	DELAY(5000);

	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	for (i = 0 ; i < ram_size; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   if ((i % 1024) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
		busy(1);
	   }

	   /* Compare data */
	   if ((i&0x1) ==  0)   {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	}

	msg_printf(DBG,"\nUcodeWalking_Bit: First pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((GE11_UCODE_BASE + 4));
	for (i = 1 ; i < ram_size; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	   if ((i % 1024) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
		busy(1);
	   }

	   if ((i & 0x1) == 0)  { 
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 2- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }

	}
	msg_printf(DBG,"\nUcodeWalking_Bit: Second pass ucode read complete\n");
	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);
    }
	GE11_DIAG_OFF();
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_WALKING_BIT_TEST]), errors);
}
#endif /* MFG_USED */

__uint32_t
mgras_GE11_ucode_AddrUniq(void)
{
	__uint32_t i , led, errors =0;
	__uint32_t ucode0, ucode1;
	__uint32_t ram_size;
	
	msg_printf(SUM, "MGRAS GE11 Ucode Address Uniqueness Test\n");

	SET_RAM_SIZE(ram_size);
	/* setup diagnostic acess first */

	GE11_DIAG_ON();


	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	for (i = 0 ; i < ram_size; i++)
	{

	   GE11_DIAG_WR_IND(((i)), GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(((i+1)), GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(((i+2)), GE11_UCODE_MASK);

	} 

	/* Read back and verify */
	msg_printf(DBG, "Ucode Write completed\n");
	DELAY(5000);

	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	for (i = 0 ; i < ram_size; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   /* msg_printf(1,"R1"); */
	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   /* msg_printf(1,"R2\n"); */
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   if ((i % 1024) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
		busy(1);
	   }

	   /* Compare data */
	   if ((i&0x1) ==  0)   {
	   	COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 1- word #1", i, ((i)), ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 1- word #3", i, ((i+2)), ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 1- word #2", i, ((i+1)), ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 1- word #3", i, ((i+2)), ucode1, GE11_UCODE2_MASK, errors);
	   }
	}

	msg_printf(DBG,"\nUcodeAddrUniq: First pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((GE11_UCODE_BASE + 4));
	for (i = 1 ; i < ram_size; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	   if ((i % 1024) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
		busy(1);
	   }


	   if ((i & 0x1) == 0)  { 
	   	COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 2- word #2", i, ((i+1)), ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 2- word #3", i, ((i+2)), ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 2- word #1", i, ((i)), ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrUniq test pass 2- word #3", i, ((i+2)), ucode1, GE11_UCODE2_MASK, errors);
	   }

	}
	msg_printf(DBG,"\nUcodeAddrUniq: Second pass ucode read complete\n");

	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	GE11_DIAG_OFF();
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_ADDRUNIQ_TEST]), errors);
}

__uint32_t
mgras_GE11_ucode_mem_pat(void)
{
	__uint32_t i ,led,  pattern_index, errors =0;
	__uint32_t ucode0, ucode1;
	__uint32_t expected;
	__uint32_t ram_size;

	msg_printf(SUM, "MGRAS GE11 Ucode Memory Patterns  Test\n");

	SET_RAM_SIZE(ram_size);
	/* setup diagnostic acess first */

	GE11_DIAG_ON();


    for (pattern_index = 0; pattern_index < 1; pattern_index++)
    {
	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);

	expected = patrn[pattern_index]; 
	msg_printf(DBG,"\npattern = 0x%x\n",expected);

	for (i = 0 ; i < ram_size; i++)
	{

	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);
	   DELAY(10);
	   GE11_DIAG_WR_IND(expected, GE11_UCODE_MASK);

	} 

	/* Read back and verify */
	msg_printf(DBG, "Ucode Write completed\n");
	DELAY(5000);

	GE11_DIAG_WR_PTR(GE11_UCODE_BASE);
	for (i = 0 ; i < ram_size; i++)  
	{
	    if ((i&0x1) == 0) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	   }													  

	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);


	   if ((i % 1024) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
		busy(1);
	   }

	   /* Compare data */
	   if ((i&0x1) ==  0)   {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE11 UcodeAddrPat test pass 1- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }
	}

	msg_printf(DBG,"\nUcodeAddrPat: First pass ucode read complete\n");
	DELAY(100);

        GE11_DIAG_WR_PTR((GE11_UCODE_BASE + 4));
	for (i = 1 ; i < ram_size; i++)
	{
	    if ((i&0x1) == 1) {
		GE11_DIAG_WR_PTR((1 << 31) | 0x2);
		DELAY(5);
                MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT);
		DELAY(5);
	        MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
	    }


	   ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	   ucode1 = GE11_Diag_read(GE11_UCODE_MASK);

	   if ((i % 1024) == 0)
	   {
		led ? setled(2,off) : setled(2,on);
		led = ~led & 0x1;
		msg_printf(VRB, ".");
		busy(1);
	   }

	   if ((i & 0x1) == 0)  { 
	   	COMPARE("HQ3 to GE0 UcodeAddrPat test pass 2- word #2", i, expected, ucode0, GE11_UCODE_MASK, errors);
	   }
	   else {
	   	COMPARE("HQ3 to GE0 UcodeAddrPat test pass 2- word #1", i, expected, ucode0, GE11_UCODE_MASK, errors);
		COMPARE("HQ3 to GE0 UcodeAddrPat test pass 2- word #3", i, expected, ucode1, GE11_UCODE2_MASK, errors);
	   }

	}
	msg_printf(DBG,"\nUcodeAddrPat: Second pass ucode read complete\n");
	/* check  if any data is left over */
	/* following  read may time-out but its ok. It will flush any left over data that is not read in the loop */
	ucode0 = GE11_Diag_read(GE11_UCODE_MASK);
	ucode1 = GE11_Diag_read(GE11_UCODE_MASK);
    }
	GE11_DIAG_OFF();
	REPORT_PASS_OR_FAIL((&GE11_err[GE11_UCODE_PATTERN_TEST]), errors);
}

#ifdef MFG_USED
static struct ge11reg ge11_diag_reg[] = {
{"EXECUTION_CONTROL",EXECUTION_CONTROL,0x3f},
{"BREAKPT_ADDR",BREAKPT_ADDR,0x3ffff},
{"RAMSCAN_CONTROL",RAMSCAN_CONTROL,0x7ffff},
{"WRT_SYNC_FIFO_CONTROL",WRT_SYNC_FIFO_CONTROL,0x3f},
{"DIAG_SEL",DIAG_SEL,0x1ff},
{"ENABLE_MASK",ENABLE_MASK,0x77},
{"CM_OFIFO_TAG_CONFIG",CM_OFIFO_TAG_CONFIG,0x1},
{"MICROCODE_ERROR",MICROCODE_ERROR,0xfffff},
{"OFIFO_0_TH_A_STATUS",OFIFO_0_TH_A_STATUS,0xffff},
{"OFIFO_0_TH_B_STATUS",OFIFO_0_TH_B_STATUS,0xffff},
{"OFIFO_1_TH_A_STATUS",OFIFO_1_TH_A_STATUS,0xffff},
{"OFIFO_1_TH_B_STATUS",OFIFO_1_TH_B_STATUS,0xffff},
{"OFIFO_2_TH_A_STATUS",OFIFO_2_TH_A_STATUS,0xffff},
{"OFIFO_2_TH_B_STATUS",OFIFO_2_TH_B_STATUS,0xffff},
{"OUTPUT_FIFO_CNTL",OUTPUT_FIFO_CNTL,0xfff},
{"ERAM",0x100000,0xffffffff}
};

void
mgras_GE11_diag_reg_dump(void)
{
	__uint32_t i, actual;
	/* allocate read data buffer first */

	/* setup diagnostic acess first */

	GE11_DIAG_ON();

	/* Write pointer set to first location of Diag. Registers */

	msg_printf(SUM,"GE11 Diagnostic Registers:\n");

	for (i = 0; i < sizeof(ge11_diag_reg)/sizeof(struct ge11reg) ; i++)
	{
	/* Read and Compare */
    	   GE11_DIAG_WR_PTR(ge11_diag_reg[i].addr_offset);
	   GE11_DIAG_WR_PTR((1 << 31) | 0x1);
	   actual = GE11_Diag_read(ge11_diag_reg[i].mask);
	   msg_printf(VRB,"%s Register : 	0x%x\n",ge11_diag_reg[i].name,actual);
	}
	GE11_DIAG_OFF();
}
#endif /* MFG_USED */

/*ARGSUSED2*/
__uint32_t
mgras_ge_diag_ramscan(int argc, char** argv)
{
    __uint32_t		i, errors = 0;
    __uint32_t		dataout, datain;


    datain = (argc > 1) ? 0x0 : 0xffffffff;

    GE11_DIAG_ON();

    HQ3_PIO_WR(ge11_diag_a_addr, WRT_SYNC_FIFO_CONTROL, 0x7fffffff);
    HQ3_PIO_WR(ge11_diag_d_addr, (WSYNC_FIFO_DISABLE | (WSYNC_FIFO_WM << 1)), ~0x0);

    HQ3_PIO_WR(ge11_diag_a_addr, RAMSCAN_CONTROL, 0x7fffffff);
    HQ3_PIO_WR(ge11_diag_d_addr, (RAMSCAN_FASTMODE | RAMSCAN_SCANMODE), ~0x0);

    HQ3_PIO_WR(ge11_diag_a_addr, GE11_RAMSCAN_BASE, 0x7fffffff);

    for(i=0; i<78; i++) {
        HQ3_PIO_WR(ge11_diag_d_addr, datain, ~0x0);
	
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
        dataout = GE11_Diag_read(~0);
	msg_printf(1,"Ramscan 1 pass %d : write = 0x%08x, read = 0x%08x\n", i, datain, dataout);
    }

    /* second pass */
    HQ3_PIO_WR(ge11_diag_a_addr, GE11_RAMSCAN_BASE, 0x7fffffff);

    for(i=0; i<78; i++) {
        HQ3_PIO_WR(ge11_diag_d_addr, datain, ~0x0);
	
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT | MGRAS_BC1_FORCE_DCB_STAT_BIT); 
	DELAY(5);
	MGRAS_SET_BC1(mgbase, MGRAS_BC1_DEFAULT);
        dataout = GE11_Diag_read(~0);
	msg_printf(1,"Ramscan 2 pass %d : write = 0x%08x, read = 0x%08x\n", i, datain, dataout);
    }

    HQ3_PIO_WR(ge11_diag_a_addr, RAMSCAN_CONTROL, 0x7fffffff);
    HQ3_PIO_WR(ge11_diag_d_addr, 0, ~0x0);

    HQ3_PIO_WR(ge11_diag_a_addr, WRT_SYNC_FIFO_CONTROL, 0x7fffffff);
    HQ3_PIO_WR(ge11_diag_d_addr,  (WSYNC_FIFO_WM << 1), ~0x0);

    GE11_DIAG_OFF();

    return(errors);
}
