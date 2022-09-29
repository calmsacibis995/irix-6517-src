/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
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
 *      scsi_intr.c - scsi interrupt test				*
 *		- Cause an interrupt in the wd95a and verify that the 	*
 *		  S1 and cpu recognize the error.			*
 *                                                                      *
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/evintr.h>
#include <sys/wd95a.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include <io4_tdefs.h>
#include <ide_wd95.h>
#include <ide_s1chip.h>
#include <setjmp.h>

jmp_buf intr_buf;
extern int nofault;
extern int *Reportlevel;
uint octl, oisrm, oueim;
static int wd95loc[2];


do_intr(caddr_t swin, int ioa_num, int cont_num, uint slot, int contn)
{
    uint dummy, oile, osr, tmpword;
    int retval=0;
    int s1num;
    long long shift, tmp;
    int i, ipri, readback;

    wd95loc[0] = slot;
    wd95loc[1] = ioa_num;

    /* Enable level 0 intrs. */
    oile = EV_GET_LOCAL(EV_ILE);
    EV_SET_LOCAL(EV_ILE, oile | EV_EBUSINT_MASK);

    /* Make sure there are currently no pending interrupts */

    osr = GetSR();

    SetSR(osr | SR_IBIT3 | SR_IE);

    if (setjmp(intr_buf))
    {
	s1num = s1num_from_slotadap(slot, ioa_num);
	ipri = EVINTR_LEVEL_S1_CHIP(s1num);

	if (ipri < 64)
            tmp = EV_GET_LOCAL(EV_IP0);
        else
            tmp = EV_GET_LOCAL(EV_IP1);
        shift = 1;
        if (tmp != (shift << (ipri % 64))) {
            	retval = 1;
		err_msg(WD95_INTR_LEV, wd95loc, ipri);
		msg_printf(ERR,"Got:(IP0) 0x%llx, (IP1) 0x%llx\n", 
			EV_GET_LOCAL(EV_IP0), EV_GET_LOCAL(EV_IP1));
        }

	msg_printf(DBG,"ipri is 0x%x\n", ipri);		

	tmp = EV_GET_LOCAL(EV_HPIL);
        if (tmp != ipri) {
		err_msg(WD95_INTR_HP, wd95loc, ipri, tmp);
            	retval = 1;
        }

        /* Verify R4000 Cause -- IP2 bit is set for level 0 intr. */
        tmpword = get_cause();
        if (!(tmpword & CAUSE_IP3)) {
		err_msg(WD95_INTR_NOCAUSE, wd95loc, tmpword);
            	retval = 1;
        }
        if ((retval) || (DBG <= *Reportlevel)) {
		msg_printf(ERR,"About to show the fault: \n\n");
            	show_fault();
		msg_printf(SUM,"\n");
	}

	tmp = EV_GET_LOCAL(EV_ERTOIP);
	msg_printf(DBG,"ev_ertoip before clearing is 0x%llx\n", tmp);
	
	wd95a_clear_intr(swin, ioa_num, cont_num, slot);
        /* Clear pending interrupt. */
        EV_SET_LOCAL(EV_CIPL0, ipri);
	msg_printf(DBG,"Setting CIPL0 to %d\n", ipri);

	/* clear interrupts */
	EV_SET_LOCAL(EV_CERTOIP, tmp);

	tmp = EV_GET_LOCAL(EV_ERTOIP);
	msg_printf(DBG,"ev_ertoip after ev_certoip is 0x%llx\n", tmp);

        /* Wait a cycle (is this necessary?). */
        tmp = EV_GET_LOCAL(EV_RO_COMPARE);

        /* Verify that EV_IP? is now zero and EV_HPIL is now zero. */
        if (ipri < 64)
            tmp = EV_GET_LOCAL(EV_IP0);
        else
            tmp = EV_GET_LOCAL(EV_IP1);
        if (tmp != 0) {
		err_msg(WD95_INTR_IPNZ, wd95loc, EV_GET_LOCAL(EV_IP0), 
				EV_GET_LOCAL(EV_IP1));
            	retval = 1;
        }

        tmp = EV_GET_LOCAL(EV_HPIL);
        if (tmp != 0) {
            	err_msg(WD95_INTR_HPNZ, wd95loc, tmp);
            	retval = 1;
        }

        /* Verify R4000 cause -- IP2 bit should now be zero. */
        tmpword = get_cause();
        if (tmpword & CAUSE_IP3) {
            	err_msg(WD95_INTR_CANZ, wd95loc, tmpword);
            	retval = 1;
        }
    }
    else
    {
	set_nofault(intr_buf);

	tmp = EV_GET_LOCAL(EV_ERTOIP);
	msg_printf(DBG,"ev_ertoip before interrupt is 0x%llx\n", tmp);

	if (DBG <= *Reportlevel) {
		msg_printf(DBG,"Showing fault before any faults. Ctlr %d\n\n",
			contn);
		show_fault();
	}	

        /* Send an interrupt from the wd95a */
	wd95a_intr(swin, ioa_num, cont_num, slot, contn);

        /* Wait a bus cycle for intr to propagate. */
        tmp = EV_GET_LOCAL(EV_RO_COMPARE);

        retval = 1;
	err_msg(WD95_INTR_NOINT, wd95loc);
	if (cpu_intr_pending() == 0) {
            	readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
		err_msg(WD95_INTR_NOEBUS, wd95loc, readback);
	}
	else {
            	readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
		err_msg(WD95_INTR_ENOCPU, wd95loc, readback);
	}
	/* Read S1 IBUS Err Status Reg */
	dummy = S1_GET_REG(swin, S1_IBUS_ERR_STAT);
	msg_printf(DBG,"IBUS Err Status Reg: 0x%x\n", dummy);

	clear_nofault();
	wd95a_clear_intr(swin, ioa_num, cont_num, slot);
	msg_printf(ERR, "About to show the fault whiched was not seen\n");
        show_fault();
    }
    /* Leave things tidy */
    SetSR(osr);
    EV_SET_LOCAL(EV_CIPL0, 0);

    /* Disable level 0 interrupts */
    oile = EV_GET_LOCAL(EV_ILE);
    EV_SET_LOCAL(EV_ILE, oile | ~EV_EBUSINT_MASK);

    return(retval);
}

int wd95a_intr(caddr_t swin, int ioa_num, int cont_num, uint slot, int contn)
{
	register uint dummy;
	register int fail = 0;
	uint ctrl_off;

	/*
	 * offset of the selected controller's regs in the s1 chip space
	 */
	ctrl_off = S1_CTRL_BASE + (cont_num * S1_CTRL_OFFSET);
	msg_printf(DBG,"ctrl_off is 0x%x\n", ctrl_off);

	dummy = S1_GET_REG(swin, S1_DMA_INTERRUPT+cont_num*S1_DMA_INT_OFFSET);
	msg_printf(DBG,"DMA chan %d intr reg is 0x%x\n", contn, dummy);
	dummy = S1_GET_REG(swin, S1_INTERRUPT);
	msg_printf(DBG,"S1 interrupt reg is 0x%x\n", dummy);
	
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"S1 CSR before reset is 0x%x\n", dummy);

	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISR);
	msg_printf(DBG,"ISR before reset is 0x%x\n", dummy);

	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI);
	msg_printf(DBG,"UEI before reset is 0x%x\n", dummy);

	/* put the wd95a into normal mode */
	octl = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL);
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, 0);
	msg_printf(DBG,"octl is 0x%x\n", octl);

	oisrm = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISRM);
	msg_printf(DBG,"Old ISRM is 0x%x\n", oisrm);

	/* unmask the uei bit in the ISRM reg to enable uei's */
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISRM, W95_ISRM_UEIM);
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISRM);
	msg_printf(DBG,"New isrm is 0x%x\n", dummy);

	oueim = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEIM);
	msg_printf(DBG,"Old UEIM is 0x%x\n", oueim);

	/* unmask the rstint bit in the UEIM reg - enables interrupt on reset*/
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEIM, W95_UEIM_RSTINTM);
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEIM);
	msg_printf(DBG,"New ueim is 0x%x\n", dummy);

	/*
	 * Reset scsi 
	 */
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, W95_CTL_RSTO);

	/* check wd95a status */
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISR);
	msg_printf(DBG,"ISR after reset is 0x%x\n", dummy);

	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI);
	msg_printf(DBG,"UEI after reset is 0x%x\n", dummy);

	/* check s1 status */
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"S1 CSR after reset is 0x%x\n", dummy);

	msg_printf(DBG,"debug: loc 1\n");

	/* un-reset the 95a */
	/* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, octl); */
	/* octl = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL); */
	/* msg_printf(DBG,"reset ctl is 0x%x\n", octl); */

	DELAY(200);

	msg_printf(DBG,"debug: loc 2\n");

	return(fail);
}

int wd95a_clear_intr(caddr_t swin, int ioa_num, int cont_num, uint slot)
{
	register uint dummy;
	register int fail = 0;
	uint ctrl_off;
	uint isr_intr, uei_intr;

	/*
	 * offset of the selected controller's regs in the s1 chip space
	 */
	ctrl_off = S1_CTRL_BASE + (cont_num * S1_CTRL_OFFSET);
	msg_printf(DBG,"ctrl_off is 0x%x\n", ctrl_off);
	
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"S1 CSR before clearing is 0x%x\n", dummy);

	isr_intr = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISR);
	msg_printf(DBG,"ISR before clearing is 0x%x\n", isr_intr);

	uei_intr = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI);
	msg_printf(DBG,"UEI before clearing is 0x%x\n", uei_intr);

	/* put the wd95a into normal mode */
	/* octl = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL); */
	msg_printf(DBG,"octl is 0x%x\n", octl);
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, 0);

	/* reset ISRM to original value */
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISRM, oisrm);
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISRM);
	msg_printf(DBG,"Original isrm is 0x%x\n", oisrm);
	msg_printf(DBG,"New isrm is 0x%x\n", dummy);

	/* reset UEIM to original value */
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEIM, oueim);
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEIM);
	msg_printf(DBG,"Original ueim is 0x%x\n", oueim);
	msg_printf(DBG,"New ueim is 0x%x\n", dummy);

	/* write to UEI to clear interrupt */
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI, uei_intr);

	/* write to ISR to clea interrupts */
	 SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISR, isr_intr);

	/*
	 * Reset scsi 
	 */
	/* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, W95_CTL_RSTO);*/

	/* check wd95a status */
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISR);
	msg_printf(DBG,"ISR after clearing is 0x%x\n", dummy);

	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI);
	 msg_printf(DBG,"UEI after clearing is 0x%x\n", dummy);

	/* check s1 status */
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"S1 CSR after clearing is 0x%x\n", dummy);

	msg_printf(DBG,"debug: loc 3\n");

	/* un-reset the 95a */
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, octl);
	octl = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL);
	msg_printf(DBG,"reset ctl is 0x%x\n", octl);

	DELAY(200);

	msg_printf(DBG,"debug: loc 4\n");

	return(fail);
}


int scsi_intr(int argc, char**argv)
{
	int fail = 0;

    	msg_printf(INFO, "scsi_intr - testing scsi interrupts\n");
	scsi_setup();
        fail = test_scsi(argc, argv, do_intr);
	return(fail);
}

