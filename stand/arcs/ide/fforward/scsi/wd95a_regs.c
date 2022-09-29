/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/************************************************************************
 *                                                                      *
 *wd95a_regtest.c - wd95a controller reg. read/write tests for scsimezz *
 *                                                                      *
 ************************************************************************/

#include "uif.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wd95a.h>
#include <sys/sbd.h>
#include <sys/hpc3.h>
#include <sys/mc.h>
#include <setjmp.h>

#define GET_WD_REG(sbp, reg) \
	( *(u_char *)((int) sbp +3 ) = reg, *(volatile unsigned char *)(sbp + 11))	
#define SET_WD_REG(sbp, reg, val) \
	 *(u_char *)((int) sbp +3) = reg; *(u_char *)(sbp + 11) = (val);

	
typedef struct wd_reglist {
        char            *rname;
        int             rnum;
        unchar          mask_val;
	int		read_write;
} wd_reglist_t;

#define hpc3_scsimezz	1		/* HPC3 id for SCSIMEZZ card */
#define HDS0		0x40000		/* offset from base to hd0.cs */
#define HDS1		0x48000		/* offset from base to hd1.cs */
#define HPC_HOLLY_2_LIO 0xBF9801C0
#define WD95_PIO_CFG_33MHZ   0x318
#define WD95_DMA_CFG_33MHZ   0x1841
#define CNTL_OFFSET	4	/* SCSIMEZZ disk controller start from 4 */

extern int hpc_probe(int);

u_int gio64_arb_reg, gio64_arb_reg_sav;

/* wd structure is "reg. name", "reg. address","data mask","read/write" */
wd_reglist_t reglist_setup[] = {
        "control",			WD95A_S_CTL,		0x3e,   1,
        "scsi configuration",           WD95A_S_SCNF,		0xff,   1,
        "Own ID",                       WD95A_S_OWNID,		0xf,    1,
        "Sel & Resel timout period",    WD95A_S_TIMOUT,		0xff,   1,
        "Sleep Countdown",              WD95A_S_SLEEP,		0xff,   1,
        "scsi timeout residue",	        WD95A_S_TIMER, 		0xff,	0, 
        "CDB Size reg (grp 3,4)",       WD95A_S_CDBSIZ1,	0xff,   1,
        "CDB Size reg (grp 6,7)",       WD95A_S_CDBSIZ2,	0xff,   1,
        "SCSI-ID flags (0..7)",         WD95A_S_IDFLAG0,	0xff,   1,
        "SCSI-ID flags (8..15)",      	WD95A_S_IDFLAG1,	0xff,   1,
        "DMA Configuration",            WD95A_S_DMACNF, 	0xff,   0,
        "DMA Timing control",           WD95A_S_DMATIM, 	0xff,   0,
        "Factory Test0",                WD95A_S_TEST0, 		0xff,   0,
        "SCSI Low-level control0",      WD95A_S_SC0,  		0x05,   1,
        "SCSI Low-level control1",      WD95A_S_SC1,    	0xff,   0,
        "SCSI Low-level control2",      WD95A_S_SC2,    	0xff,   0,
        "SCSI Low-level control3",      WD95A_S_SC3,    	0xff,   0,
        "WCS Address",                  WD95A_S_CSADR,  	0x7f,   1,
        "WCS port for window 0",        WD95A_S_CSPRT0, 	0xff,   0,
        "WCS port for window 1",        WD95A_S_CSPRT1, 	0xff,   0,
        "WCS port for window 2",        WD95A_S_CSPRT2, 	0xff,   0,
        "WCS port for window 3",        WD95A_S_CSPRT3, 	0x3f,   0,
        "WCS Sel reponse addr",         WD95A_S_SQSEL, 		0x7f,   1,
        "WCS ReSel reponse addr",       WD95A_S_SQRSL,  	0x7f,   1,
        "WCS Start addr for DMA req",   WD95A_S_SQDMA,  	0x7f,   1,
        "DPR address Pointer",          WD95A_S_DPRADD, 	0x1f,   0,
        "DPR Transfer Counter",         WD95A_S_DPRTC,  	0x0f,   0,
        "Physical:Logical Block Ratio", WD95A_S_PLR,    	0x0f,   1,
        "Physical Block Size0",         WD95A_S_PSIZE0, 	0xff,   1,
        "Physical Block Size1",         WD95A_S_PSIZE1, 	0x0f,   1,
        "Mirroring Ping-pong value",    WD95A_S_PING,   	0x0f,   1,
        "Data Compare Registers",       WD95A_S_CMPIX,  	0x07,   1,
        "Data Compare Value",           WD95A_S_CMPVAL, 	0xff,   1,
        "Data Compare Mask",            WD95A_S_CMPMASK,	0xff,   1,
        "Buffered BD[0..7]",            WD95A_S_BBDL,   	0xff,   0,
        "Buffered BD[8..15]",           WD95A_S_BBDH,   	0xff,   0,
        "Chip",                         WD95A_S_CVER,   	0x3f,   0,
        "Factory Test1",                WD95A_S_TEST1,  	0x3f,   0,
        "REQ",                          WD95A_S_OFFSET, 	0x3f,   0,
        "done",                                 -1,     	0x00,   1
};

wd_reglist_t reglist_normal[] = {
        "Control",                      WD95A_N_CTL,            0x3f,   1,
        "Auxilliary Control",           WD95A_N_CTLA,           0xff,   1,
        "Interrupt Status",             WD95A_N_ISR,            0xff,   0,
        "Unexpected Stop Status",       WD95A_N_STOPU,          0x3f,   0,
        "Unexpected Event Intr",        WD95A_N_UEI,            0xff,   0,
        "ISR Mask",                     WD95A_N_ISRM,           0x7f,   1,
        "STOPU Mask",                   WD95A_N_STOPUM,         0x3f,   1,
        "UEI Mask",                     WD95A_N_UEIM,           0xff,   1,
        "Automatic Response Control",   WD95A_N_RESPONSE,       0x7f,   1,
        "WCS Sequencer Interrupt Addr", WD95A_N_SQINT,          0xff,   0,
        "Current addr WCS is executing",WD95A_N_SQADR,          0xff,   0,
        "SCSI Transfer Control",        WD95A_N_STC,            0xbf,   1,
        "SCSI Pulse Width Control Reg", WD95A_N_SPW,            0x77,   1,
        "Destination ID",               WD95A_N_DESTID,         0x0f,   1,
        "Source ID",                    WD95A_N_SRCID,          0x1f,   0,
        "Flag",                         WD95A_N_FLAG,           0xff,   1,
        "Transfer Count 0..7",          WD95A_N_TC_0_7,         0xff,   0,
        "Transfer Count 8..15",         WD95A_N_TC_8_15,        0xff,   0,
        "Transfer Count 16..23",        WD95A_N_TC_16_23,       0xff,   0,
        "Data Access Port",             WD95A_N_DATA,           0xff,   0,
        "Status",                       WD95A_N_SR,             0x07,   0,
        "Fifo Status",                  WD95A_N_FIFOS,          0xff,   0,
        "Physical Block Residue",       WD95A_N_PBR,            0x0f,   1,
        "Byte Residue0",                WD95A_N_BR0,            0xff,   1,
        "Byte Residue1",                WD95A_N_BR1,            0x0f,   1,
        "PING Residue",                 WD95A_N_PINGR,          0x0f,   1,
        "LRC Residue (Low)",            WD95A_N_LRCR0,          0xff,   1,
        "LRC Residue (High)",           WD95A_N_LRCR1,          0xff,   1,
        "Odd-Byte Reconnect",           WD95A_N_ODDR,           0xff,   1,
        "DPR_0",                        WD95A_N_DPR,            0xff,   0,
        "done",                         -1,                     0xff,   1
};


ide_wd95_init(register volatile unsigned int  base) {

int adap;
	/*
	 * Set up to talk to second HPC3.
	 * If a GIO-32 SCSI or Enet board is in expansion slot 1, steal
	 *   DMA from slot 0.  Otherwise steal from slot one.
	 * Program gio64_arb register on MC chip.
	 * Then set up pbus pio cfg register so that we can set
	 *   up the config register on Challenge S I/O board.
	 */
	gio64_arb_reg_sav = *(volatile u_int *)PHYS_TO_K1(GIO64_ARB);
	/* EISA options have to be set, otherwise it won't work 
	   UNIX did the same thing, reason unknown */
	gio64_arb_reg = gio64_arb_reg_sav | GIO64_ARB_EISA_SIZE | GIO64_ARB_EISA_MST;
	if (badaddr((char *) HPC_HOLLY_2_LIO, sizeof(int)) == 0) { 
		gio64_arb_reg |= GIO64_ARB_EXP0_MST | GIO64_ARB_EXP0_SIZE_64 |
				 GIO64_ARB_HPC_EXP_SIZE_64;
		adap = 0x20;
	}
	else {
		gio64_arb_reg |= GIO64_ARB_EXP1_MST | GIO64_ARB_EXP1_SIZE_64 |
			         GIO64_ARB_HPC_EXP_SIZE_64;
		adap = 0x30;
	} 

	writemcreg(GIO64_ARB, gio64_arb_reg);
	/*   *((uint *) PHYS_TO_K1(GIO64_ARB)) = gio64_arb_reg;  */

	*((uint *) PHYS_TO_K1(HPC31_PBUS_PIO_CFG_0)) =  0x3ffff;
	*((uint *) PHYS_TO_K1(HPC31_INTRCFG)) =  adap;
	*((uint *) PHYS_TO_K1(HPC31_SCSI_PIOCFG0)) = WD95_PIO_CFG_33MHZ;
	*((uint *) PHYS_TO_K1(HPC31_SCSI_PIOCFG1)) = WD95_PIO_CFG_33MHZ;
	*((uint *) PHYS_TO_K1(HPC31_SCSI_DMACFG0)) = WD95_DMA_CFG_33MHZ;
	*((uint *) PHYS_TO_K1(HPC31_SCSI_DMACFG1)) = WD95_DMA_CFG_33MHZ;
	*((uint *) PHYS_TO_K1(HPC31_SCSI_CONTROL0)) = 0;
	*((uint *) PHYS_TO_K1(HPC31_SCSI_CONTROL1)) = 0;
}

ide_wd95_present(register volatile unsigned int  sbp) {
unsigned char setup_readback,cver,id;
        SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
        setup_readback = GET_WD_REG(sbp, WD95A_S_CTL);
	if (setup_readback !=  W95_CTL_SETUP ) {
		msg_printf(VRB,"WD95a setup should be %d, actual = %d\n",W95_CTL_SETUP,setup_readback);
		return 1;
	}

	cver = GET_WD_REG(sbp, WD95A_S_CVER);
	if (cver < 0x10 ) { 
		msg_printf(VRB,"Wrong WD95a version  %d\n",cver);
		return 1;
	} 
	return 0;	
}

/*
 *  regs_wd95a - wd95a scsi controller register test
 *
 *  called with no parameters, checks all 95a scsi controllers under
 *  hpc3.
 *
 */
int regs_wd95a(int argc, char**argv)
{
	int base, cntl, sbp;
	int i, j;
	int fail = 0;
	wd_reglist_t *rp;
	u_char actual, original_val;
	u_int	test_val;

    msg_printf(INFO, "WD95a_regs - SCSIMEZZ WD95a controller registers read/write test\n");

    if (!hpc_probe(hpc3_scsimezz)) {
	  msg_printf (VRB, "Please run hpc3 test first and see if HPC3 is there\n");
		   return 0;
    }
    base = (unsigned int)hpc_base(hpc3_scsimezz);
    /* Initialize wd95 chips */
    ide_wd95_init(base);

    for (cntl = 0, sbp = base + HDS0; cntl < 2; cntl++, sbp = sbp + (HDS1-HDS0)) 
    {
	    /* check to see if WD95a chip is present first */
	    if (ide_wd95_present(sbp) == 0) 
   	       msg_printf(VRB,"WD95a controller %d is present \n",cntl+CNTL_OFFSET);
    	    else {
      		msg_printf(VRB,"WD95a controller %d is not present \n",cntl+CNTL_OFFSET);
      		 return 1; 
    	    }
	    /* Set to Set-up mode first */
            SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
	    rp = &reglist_setup[1]; /* skip set_up register */

	    msg_printf(VRB,"Testing Controller %d...\n",cntl+CNTL_OFFSET);
	    while (rp->rnum >= 0)
	    {
		original_val = GET_WD_REG(sbp, rp->rnum) & rp->mask_val;
		if (rp->read_write) 
		{
		   for (test_val = 0; test_val <= 0xff; test_val = test_val + 0x55)
		   {
			SET_WD_REG(sbp, rp->rnum,test_val & rp->mask_val);
			actual = GET_WD_REG(sbp, rp->rnum) & rp->mask_val;
			if ((test_val & rp->mask_val) != actual)
			{
			  msg_printf(ERR,"WD95a setup register %s test failed: expected = 0x%x, actual = 0x%x\n", rp->rname, test_val, actual);
			  return 1; 
			}
			/* restore original value */
			SET_WD_REG(sbp, rp->rnum,original_val);
		   }
	     	}			
		else msg_printf(DBG,"WD95a register %s is read only\n",rp->rname);

		rp++;
	    }
	    /* Set to Normal mode */
            SET_WD_REG(sbp, WD95A_S_CTL, 0);
	    rp = &reglist_normal[1]; /* skip set_up register */

	    while (rp->rnum >= 0)
	    {
		original_val = GET_WD_REG(sbp, rp->rnum) & rp->mask_val;
		if (rp->read_write) 
		{
		   for (test_val = 0; test_val <= 0xff; test_val = test_val + 0x55)
		   {
			SET_WD_REG(sbp, rp->rnum,test_val & rp->mask_val);
			actual = GET_WD_REG(sbp, rp->rnum) & rp->mask_val;
			if ((test_val & rp->mask_val) != actual)
			{
			  msg_printf(VRB," WD95a normal register %s test failed: expected = 0x%x, actual = 0x%x\n", rp->rname, test_val, actual);
			  return 1; 
			}
			/* restore original value */
		      	SET_WD_REG(sbp, rp->rnum,original_val);
		   }
	     	}			
		else msg_printf(DBG,"WD95a register %s is read only\n",rp->rname);
		rp++;
	    }

    }
    msg_printf(VRB,"WD95a controller registers test passed.\n");
    /* restore original GIO64_ARB register */
 /*   writemcreg(GIO64_ARB, gio64_arb_reg_sav); */
    return 0;
}
