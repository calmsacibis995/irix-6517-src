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
 *      wd95a_regtest.c - wd95a chips register read/write tests         *
 *                                                                      *
 ************************************************************************/

#include <sys/types.h>

#include <sys/param.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/wd95a.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include <io4_tdefs.h>
#include <ide_wd95.h>
#include <ide_s1chip.h>
#include <setjmp.h>

#define TEST_VAL        TIMEOUT_INIT
#define TIMEOUT_INIT    77              /* timeout value */

uint safe_reg[256];
int wd95loc[2];

reglist_t reglist_setup[] = {
        "control",                              WD95A_S_CTL,            0x55,   0xaa,   1,
        "scsi configuration",                   WD95A_S_SCNF,           0x55,   0xaa,   1,
        "Own ID",                               WD95A_S_OWNID,          0x55,   0xaa,   1,
        "Sel & Resel timout period",            WD95A_S_TIMOUT,         0x55,   0xaa,   1,
        "Sleep Countdown",                      WD95A_S_SLEEP,          0x55,   0xaa,   0,
        "scsi timeout countdown residue",       WD95A_S_TIMER,          0x55,   0xaa,   1,
        "Special CDB Size reg (grp 3,4)",       WD95A_S_CDBSIZ1,        0x55,   0xaa,   1,
        "Special CDB Size reg (grp 6,7)",       WD95A_S_CDBSIZ2,        0x55,   0xaa,   1,
        "SCSI-ID specific flags (0..7)",        WD95A_S_IDFLAG0,        0x55,   0xaa,   1,
        "SCSI-ID specific flags (8..15)",       WD95A_S_IDFLAG1,        0x55,   0xaa,   1,
        "DMA Configuration",                    WD95A_S_DMACNF,         0x55,   0xaa,   1,
        "DMA Timing control",                   WD95A_S_DMATIM,         0x55,   0xaa,   1,
        "Factory Test0",                        WD95A_S_TEST0,          0x55,   0xaa,   1,
        "SCSI Low-level control0",              WD95A_S_SC0,            0x55,   0xaa,   1,
        "SCSI Low-level control1",              WD95A_S_SC1,            0x55,   0xaa,   1,
        "SCSI Low-level control2",              WD95A_S_SC2,            0x55,   0xaa,   1,
        "SCSI Low-level control3",              WD95A_S_SC3,            0x55,   0xaa,   1,
        "WCS Address",                          WD95A_S_CSADR,          0x55,   0xaa,   1,
        "WCS port for window 0",                WD95A_S_CSPRT0,         0x55,   0xaa,   1,
        "WCS port for window 1",                WD95A_S_CSPRT1,         0x55,   0xaa,   1,
        "WCS port for window 2",                WD95A_S_CSPRT2,         0x55,   0xaa,   1,
        "WCS port for window 3",                WD95A_S_CSPRT3,         0x55,   0xaa,   1,
        "WCS Sel reponse addr",                 WD95A_S_SQSEL,          0x55,   0xaa,   1,
        "WCS ReSel reponse addr",               WD95A_S_SQRSL,          0x55,   0xaa,   1,
        "WCS Start addr for DMA req",           WD95A_S_SQDMA,          0x55,   0xaa,   1,
        "DPR address Pointer",                  WD95A_S_DPRADD,         0x55,   0xaa,   1,
        "DPR Transfer Counter",                 WD95A_S_DPRTC,          0x55,   0xaa,   1,
        "Physical:Logical Block Ratio",         WD95A_S_PLR,            0x55,   0xaa,   1,
        "Physical Block Size0",                 WD95A_S_PSIZE0,         0x55,   0xaa,   1,
        "Physical Block Size1",                 WD95A_S_PSIZE1,         0x55,   0xaa,   1,
        "Mirroring Ping-pong value",            WD95A_S_PING,           0x55,   0xaa,   1,
        "Data Compare Index",                   WD95A_S_CMPIX,          0x55,   0xaa,   1,
        "Data Compare Value",                   WD95A_S_CMPVAL,         0x55,   0xaa,   1,
        "Data Compare Mask",                    WD95A_S_CMPMASK,        0x55,   0xaa,   1,
        "Buffered BD[0..7]",                    WD95A_S_BBDL,           0x55,   0xaa,   1,
        "Buffered BD[8..15]",                   WD95A_S_BBDH,           0x55,   0xaa,   1,
        "Chip",                                 WD95A_S_CVER,           0x55,   0xaa,   1,
        "Factory Test1",                        WD95A_S_TEST1,          0x55,   0xaa,   1,
        "REQ",                                  WD95A_S_OFFSET,         0x55,   0xaa,   1,
        "done",                                 -1,                     0x00,   0x00,   1
};

reglist_t reglist_normal[] = {
        "Control",                              WD95A_N_CTL,            0x55,   0xaa,   1,
        "Auxilliary Control",                   WD95A_N_CTLA,           0x55,   0xaa,   1,
        "Interrupt Status",                     WD95A_N_ISR,            0x55,   0xaa,   1,
        "Unexpected Stop Status",               WD95A_N_STOPU,          0x55,   0xaa,   1,
        "Unexpected Event Intr",                WD95A_N_UEI,            0x55,   0xaa,   1,
        "ISR Mask",                             WD95A_N_ISRM,           0x55,   0xaa,   1,
        "STOPU Mask",                           WD95A_N_STOPUM,         0x55,   0xaa,   1,
        "UEI Mask",                             WD95A_N_UEIM,           0x55,   0xaa,   1,
        "Automatic Response Control",           WD95A_N_RESPONSE,       0x55,   0xaa,   1,
        "WCS Sequencer Interrupt Addr",         WD95A_N_SQINT,          0x55,   0xaa,   1,
        "Current addr WCS is executing",        WD95A_N_SQADR,          0x55,   0xaa,   1,
        "SCSI Transfer Control",                WD95A_N_STC,            0x55,   0xaa,   1,
        "SCSI Pulse Width Control Reg",         WD95A_N_SPW,            0x55,   0xaa,   1,
        "Destination ID",                       WD95A_N_DESTID,         0x55,   0xaa,   1,
        "Source ID",                            WD95A_N_SRCID,          0x55,   0xaa,   1,
        "Flag",                                 WD95A_N_FLAG,           0x55,   0xaa,   1,
        "Transfer Count 0..7",                  WD95A_N_TC_0_7,         0x55,   0xaa,   1,
        "Transfer Count 8..15",                 WD95A_N_TC_8_15,        0x55,   0xaa,   1,
        "Transfer Count 16..23",                WD95A_N_TC_16_23,       0x55,   0xaa,   1,
        "Data Access Port",                     WD95A_N_DATA,           0x55,   0xaa,   1,
        "Status",                               WD95A_N_SR,             0x55,   0xaa,   1,
        "Fifo Status",                          WD95A_N_FIFOS,          0x55,   0xaa,   1,
        "Physical Block Residue",               WD95A_N_PBR,            0x55,   0xaa,   1,
        "Byte Residue0",                        WD95A_N_BR0,            0x55,   0xaa,   1,
        "Byte Residue1",                        WD95A_N_BR1,            0x55,   0xaa,   1,
        "PING Residue",                         WD95A_N_PINGR,          0x55,   0xaa,   1,
        "LRC Residue (Low)",                    WD95A_N_LRCR0,          0x55,   0xaa,   1,
        "LRC Residue (High)",                   WD95A_N_LRCR1,          0x55,   0xaa,   1,
        "Odd-Byte Reconnect",                   WD95A_N_ODDR,           0x55,   0xaa,   1,
        "DPR_0",                                WD95A_N_DPR,            0x55,   0xaa,   1,
        "done",                                 -1,                     0x00,   0x00,   1
};

my_get_wd_reg(uint sbp, uint cont_num, uint reg)
{
	uint foo;

	foo = S1_CTRL(cont_num);
	msg_printf(DBG,"sbp: 0x%x, foo: 0x%x, reg: 0x%x, tot: 0x%x\n",
		sbp, foo, reg, (sbp + foo) + (reg << 3) +4);
}

my_getreg95(uint reg, uint window)
{
	uint region, padap;
	
	region = window << 19;
	padap = 4 << 16;
	msg_printf(DBG,"my_getreg95: 0x%x\n", 0xb0000000+region + padap + reg);
}
	

ide_wd95_present(register volatile caddr_t sbp, int slot, int adap) {
        int id;

        if ( ! sbp )
                return 0;

        SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
        SET_WD_REG(sbp, WD95A_S_TIMOUT, TEST_VAL);

        SET_WD_REG(sbp, WD95A_S_CTL, W95_CTL_SETUP);
        id = GET_WD_REG(sbp, WD95A_S_TIMOUT) & 0xff;

	wd95loc[0] = slot;
        wd95loc[1] = adap;
        if (id != TEST_VAL) {
		err_msg(WD95_PRESNT1, wd95loc, TEST_VAL, id);
                return 0;
        } else {
                int res_info;

                res_info = GET_WD_REG(sbp, WD95A_S_SC1);
#define SC1_PROB        (W95_SC1_REQ | W95_SC1_ACK | W95_SC1_MSG |\
                                 W95_SC1_CD | W95_SC1_IO | W95_SC1_ATNL)
                if (res_info & SC1_PROB) {
                        init_wd95a(sbp, 0);
			err_msg(WD95_PRESNT2, wd95loc, sbp);
			err_msg(WD95_PRESNT3, wd95loc, res_info);
                        return 0;
                }

                return 1;
        }
}


int regcheck_95a(caddr_t swin, int ioa_num, int cont_num, uint slot, int contn)
{
	register unchar orig_regval, new_regval, regval;
	reglist_t *rp;
	register int i;
	register uint dummy;
	register int fail = 0;
	char dprstr[5] = "DPR_";
	uint dummy2;
	int ctrl_off;
	uint ctrl_offset;
	int orig_ctl, orig_setup_m, setup_m;
	int j, k;

	/*
	 * offset of the selected controller's regs in the s1 chip space
	 */
	ctrl_off = S1_CTRL_BASE + (cont_num * S1_CTRL_OFFSET);
	msg_printf(DBG,"ctrl_off is 0x%x\n", ctrl_off);

	/*
	 * Reset scsi 
	 */
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"orig status is 0x%x\n", dummy);
	/* reset the 95a */
	msg_printf(DBG,"swin at loc 1 is 0x%x\n", swin);
	/* S1_PUT_REG(swin, S1_STATUS_CMD, (S1_SC_S_RESET(cont_num) | 0)); */
	msg_printf(DBG,"swin at loc 2 is 0x%x\n", swin);
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"second status is 0x%x\n", dummy);
	msg_printf(DBG,"debug: loc 1\n");
	DELAY(30);
	/* un-reset the 95a */
	/* S1_PUT_REG(swin, S1_STATUS_CMD, (S1_SC_S_RESET(cont_num) | 1));*/
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"thirdstatus is 0x%x\n", dummy);

	/* Wait for the chip to get itself into a known state */

	DELAY(200);

	msg_printf(DBG,"debug: loc 2\n");
	i = 0;
	/* dummy = S1_SC_INT_CNTRL(cont_num); */
	msg_printf(DBG,"debug: loc 2a, dummy: %x\n", dummy);
	/* dummy2 = getregs1(S1_STATUS_CMD); */
	dummy2 = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"third status is 0x%x\n", dummy2);
	msg_printf(DBG,"debug: loc 3\n");

	orig_ctl = GET_WD_REG(swin + S1_CTRL(cont_num),WD95A_S_CTL);
	orig_setup_m = orig_ctl & 0x1;
		/* setup_m == 0 => normal */
	msg_printf(DBG,"debug: loc 4\n");
	/* save_reg(setup_m, window, ioa_num, ctrl_off); */
	setup_m = orig_setup_m; 
	/* for (k = 0; k < 2; k++) { */
  	/* k=0, setup_m = orig_setup_m, k=1, setup_m = ~orig_setup_m */
	   msg_printf(DBG,"setup_m is %d\n", setup_m);
	   rp = (setup_m ? &reglist_setup[0] : &reglist_normal[0]);
	   while (rp->rnum >= 0) {
		msg_printf(DBG,"rnum is %d\n", rp->rnum);
		/* orig_regval = getreg95(rp->rnum); */
		orig_regval = GET_WD_REG(swin + S1_CTRL(cont_num), rp->rnum);
		my_get_wd_reg((uint)swin , cont_num, rp->rnum);
		if (rp->read_only) {
			msg_printf(VRB,"%s Register (read only): 0x%x\n",
				rp->rname, orig_regval);
		}
		else { /* writable */
		  for (i = 0; i < 3; i++) {
		    SET_WD_REG(swin + S1_CTRL(cont_num), rp->rnum, 
				(i==2 ? orig_regval : rp->rval[i]));
		    regval = GET_WD_REG(swin + S1_CTRL(cont_num), rp->rnum);
		    if (regval != (i == 2 ? orig_regval : rp->rval[i])) {
			   wd95loc[0] = slot;
			   wd95loc[1] = ioa_num;
			err_msg(WD95_REG_RW, wd95loc, contn, rp->rname,
				(i==2 ? orig_regval : rp->rval[i]), regval);
			fail = 1;
		    }
		    else
		    	msg_printf(VRB,"%s Register: write/read OK (0x%x)\n", 
					rp->rname, regval);
		  } /* for  i < 3 */
		} /* writable */
		rp++;
	   } /* while rnum >= 0 */
	   /* if normal mode, test the DPR regs */
	   if (setup_m == 0) {
	     for (j = 1; j <= 31; j++) {
		my_get_wd_reg((uint)swin ,cont_num, W95_DPR_R(j));
		/* orig_regval = getreg95(W95_DPR_R(i)); */
		orig_regval = GET_WD_REG(swin+S1_CTRL(cont_num),W95_DPR_R(j));
		for (i = 0; i < 3; i++) { 
			switch (i) {
				case 0: new_regval = 0x55; break;
				case 1: new_regval = 0xaa; break;
				case 2: new_regval = orig_regval; break;
			};
			SET_WD_REG(swin + S1_CTRL(cont_num), W95_DPR_R(i), 
				new_regval); 
			regval = GET_WD_REG(swin + S1_CTRL(cont_num),
					W95_DPR_R(i));
			if (regval != new_regval) {
			   wd95loc[0] = slot;
			   wd95loc[1] = ioa_num;
			   err_msg(WD95_REG_RW2, wd95loc, contn, dprstr, j,
				new_regval, regval);
                           fail = 1;
			}
			else
				msg_printf(VRB,
				"%s%d Register: write/read OK (0x%x)\n",
                                        dprstr, j, regval);
		} /* for i */
	     } /* for j */
	   } /* setup_m == 0 */
	   if (setup_m ==1) {
		setup_m = 0;
	   }
	   else if (setup_m == 0) {
		setup_m = 1;
	   }
	   /* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, setup_m); */
/*	} /* k < 2 */
	/* return ctl reg to original state */
	/* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, orig_ctl); */
	return(fail);
}

/*
 *  regs_95a - wd95a scsi controller register test
 *
 *  called with no parameters, checks all 95a scsi controllers under
 *  all s1 chips on all io4 boards present.
 *
 *  called with numerical parameters, checks controllers only under
 *  the enumerated s1 chips. ie "regs_95a" checks all present, while
 *  "regs_95a 1 3" checks 95a chips under s1's 1 and 3 (implies dual
 *  io4 system)
 */
int regs_95a(int argc, char**argv)
{
	int fail = 0;

    	msg_printf(INFO, "regs_95a - 95a chip register read/write test\n");
	scsi_setup();
	fail = test_scsi(argc, argv, regcheck_95a);
    	return (fail);
}
