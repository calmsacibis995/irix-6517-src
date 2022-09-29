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
 *      scsi_dmaintr.c - scsi dma interrupt test			*
 *		- Do a dma read of a mem location and then enable 	*
 *		  interrupts.						*
 *		  Verify that S1 and cpu recognize the error.		*
 *                                                                      *
 ************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everror.h>
#include <sys/dvh.h>
#include <sys/elog.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/scsi.h>
#include <sys/dkio.h>
#include <sys/dksc.h>
#include <arcs/io.h>
#include <sys/newscsi.h>
#include <sys/wd95a.h>
#include <sys/wd95a_struct.h>
#include <sys/invent.h>
#include <ide_msg.h>
#include <everr_hints.h>
#include <io4_tdefs.h>
#include <ide_wd95.h>
#include <ide_s1chip.h>
#include <setjmp.h>
#include "pattern.h"

#ifdef TFP
#include <sys/EVEREST/IP21.h>
#include <ip21.h>
#else
#include <sys/EVEREST/IP19.h>
#include <ip19.h>
#endif
#include <prototypes.h>

#define WORKLINE        PHYS_TO_K0(PHYS_CHECK_LO+0x1000)
#define NEWLINE        PHYS_TO_K0(0x1000000)
#define AMEG    0x100000
#define OUTADDR 0xa0001080 /* make stuff appear on the everest bus */
#define  ALINE   0x80    /* 2ndry cache line size in bytes */
#define _DEset	1 

/* from IP19/r4k/cache */
#define CACHEERR_LEVEL          0x40000000      /* cache level:
                                                   0=Primary, 1=Secondary */
#define CACHEERR_DATA           0x20000000      /* data field:
                                                   0=No error, 1=Error */
#define CACHEERR_REQ            0x08000000      /* request type:
                                                   0=Internal, 1=External */
#define SKIPWRITE 	0x5


/* Maximum memory size for this test, and the bad data buffer address */
#define MAXADDR 0x10000000
#define BADADDR 0xfffff80

#define IO4_ORIGINATING_IOA     16

jmp_buf dmaintr_buf;

extern int *Reportlevel;
extern wd95ctlrinfo_t  *wd95dev;
extern char *atob();
extern struct dksoftc *dksoftc[SC_MAXADAP][SC_MAXTARG][DK_MAXLU]; 

struct scsi_request *getreq();

static scsi_target_info_t *info;
static scsi_target_info_t realinfo;
static char *inv;
static uint octl;
static int wd95loc[2];
static int dkpart = PTNUM_FSSWAP; /* default to disk partition 1 */
static int force;
static uint *k0addr;
static caddr_t dp;
static int fd;
static unsigned long mem_bad;

dmaintr(caddr_t swin, int ioa_num, int cont_num, int slot, uint targ, int lun, 
	int contn)
{
    uint dummy, oile, osr, reg, tmpword;
    int retval=0;
    int s1num;
    long long shift, tmp, goal;
    int i, ipri, j, readback;
    register struct dksoftc *dk;	
    int        window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
    io4error_t *ie = &(EVERROR->io4[window]);

    wd95loc[0] = slot;
    wd95loc[1] = ioa_num;

    /* Enable level 0 intrs. */
    oile = EV_GET_LOCAL(EV_ILE);
    /* EV_SET_LOCAL(EV_ILE, oile | EV_EBUSINT_MASK); */

    /* Make sure there are currently no pending interrupts */

    osr = GetSR();
	msg_printf(DBG,"osr loc 1 is 0x%x\n", osr);

    /*
     * NEW - cleans and sets up
     */
    setup_err_intr(slot, ioa_num);
    
    /* SetSR(osr | SR_IBIT3 | SR_IE); */
	msg_printf(DBG,"sr loc 2 is 0x%x\n", GetSR());
	msg_printf(DBG,"Loc 1:(IP0) 0x%llx, (IP1) 0x%llx\n", 
			EV_GET_LOCAL(EV_IP0), EV_GET_LOCAL(EV_IP1));

    if (setjmp(dmaintr_buf))
    {
	s1num = s1num_from_slotadap(slot, ioa_num);
	ipri = EVINTR_LEVEL_S1_CHIP(s1num);

        tmp = EV_GET_LOCAL(EV_IP1);
        shift = 1;
	/* looking for 0x0800 0000 0001 0000 in IP1 */
	goal = ((shift << (ipri % 64)) | (shift << 59));
	msg_printf(DBG,"goal is 0x%llx, tmp: 0x%llx\n", goal, tmp);
        if (tmp != goal) {
            	retval = 1;
		err_msg(WD95_INTR_LEV, wd95loc, tmp);
		msg_printf(ERR,"Got:(IP0) 0x%llx, (IP1) 0x%llx\n", 
			EV_GET_LOCAL(EV_IP0), EV_GET_LOCAL(EV_IP1));
        }

	msg_printf(DBG,"ipri is 0x%x\n", ipri);		

	tmp = EV_GET_LOCAL(EV_HPIL);
        if (tmp != EVINTR_LEVEL_IO4_ERROR) {
		err_msg(WD95_INTR_HP, wd95loc, EVINTR_LEVEL_IO4_ERROR, tmp);
            	retval = 1;
        }

        /* Verify R4000 Cause -- IP2 bit is set for level 0 intr. */
        tmpword = get_cause();
        if (!(tmpword & CAUSE_IP3)) {
		err_msg(WD95_INTR_NOCAUSE, wd95loc, tmpword);
            	retval = 1;
        }

        /* Verify CacheErr register sees error */
        tmpword = GetCacheErr();
	msg_printf(DBG,"CacheErr reg: 0x%x\n", tmpword);

        /* defined in r4k/cache/cpu.h */
        /* CACHEERR_LEVEL = 1 ==> secondary cache
         * CACHEERR_DATA = 1  ==> data field error occurred
         * CACHEERR_REQ  = 1  ==> external request
         */
	/*
        if (!(tmpword & CACHEERR_LEVEL) || !(tmpword & CACHEERR_DATA) ||
                !(tmpword & CACHEERR_REQ)) {
                err_msg(WD95_INTR_NOCACHE, wd95loc, (CACHEERR_LEVEL |
                        CACHEERR_DATA | CACHEERR_REQ), tmpword & 0xff000000);
                msg_printf(DBG,"CacheErr reg: 0x%x\n", tmpword);
                retval = 1;
        }
	*/
/*
	if (DBG <= *Reportlevel) {
		msg_printf(DBG,"io err show:\n");
		io_err_log(slot, (1 << ioa_num));
		io_err_show(slot, (1 << ioa_num));
	}
*/

	/* verify the IA error bus reg, IA ebus error address */
	reg = IO4_GETCONF_REG(slot, IO4_CONF_EBUSERROR);
	msg_printf(DBG,"reg is 0x%x\n", reg);

	if (!(reg & IO4_EBUSERROR_STICKY) || !(reg & IO4_EBUSERROR_ADDR_ERR)
		|| !(reg & IO4_EBUSERROR_MY_ADDR_ERR)) 
	{
		err_msg(WD95_INTR_IAEBUSREG, wd95loc, (IO4_EBUSERROR_STICKY | 
			IO4_EBUSERROR_ADDR_ERR | IO4_EBUSERROR_MY_ADDR_ERR),
			reg);
		retval = 1;
	}

/* no sticky bit if we go too high on BADADDR - bumped to >256 megs
	if (!(reg & IO4_EBUSERROR_ADDR_ERR) ||
	    !(reg & IO4_EBUSERROR_MY_ADDR_ERR)) 
	{
		err_msg(WD95_INTR_IAEBUSREG, wd95loc,
			(IO4_EBUSERROR_ADDR_ERR | IO4_EBUSERROR_MY_ADDR_ERR),
			reg);
		retval = 1;
	}
*/

	/* Print the Ebus Address register only if proper bits are set in
         * Ebus error register !!
         */

	/* verify IA ebus address */
	if (ie->ebus_error1) {
		msg_printf(DBG,"IA EBUS ADDR: 0x%x\n", ie->ebus_error1);
		if (ie->ebus_error1 != mem_bad) {
			err_msg(WD95_INTR_IOADDR, wd95loc, ie->ebus_error1,
				mem_bad);
			retval = 1;
		}	
	}

	/* verify IA originating ioa number */		
	if (j = ((ie->ebus_error2 >> IO4_ORIGINATING_IOA) & 0x7)) {
		msg_printf(DBG,"originating io4 adapter: %d\n", j);
		if (j != ioa_num) {
			err_msg(WD95_INTR_IOAORIG, wd95loc, j, ioa_num);
			retval = 1;
		}
	}

        if (retval)  {
		msg_printf(DBG,"About to show the fault: (retval- %d)\n\n",
				retval);
		msg_printf(ERR,"IO Error log: \n");
		io_err_log(slot, (1 << ioa_num));
                io_err_show(slot, (1 << ioa_num));
            	show_fault();
		msg_printf(ERR,"\n");
	}

	tmp = EV_GET_LOCAL(EV_ERTOIP);
	msg_printf(DBG,"ev_ertoip before clearing is 0x%llx\n", tmp);

	if (!_DEset) {
		dk = dev_to_softc(contn, targ, lun);
		msg_printf(DBG,"dmaintr: dk is 0x%x, dksoftc: 0x%x\n", 
			*dk, *dksoftc[contn][targ][lun]);
		msg_printf(DBG,"dmaintr: dk is 0x%x, dksoftc: 0x%x\n", 
			dk, dksoftc[contn][targ][lun]);
        	drive_release(dk);

		DoErrorEret();  
	}
	wd95a_clear_dmaintr(swin, ioa_num, cont_num, slot, targ, lun, contn);

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
	set_nofault(dmaintr_buf);

	tmp = EV_GET_LOCAL(EV_ERTOIP);
	msg_printf(DBG,"ev_ertoip before interrupt is 0x%llx\n", tmp);

	/*
	 * clear error interrupt that might be pending
	 */
	wd95a_clear_dmaintr(swin, ioa_num, cont_num, slot, targ, lun, contn);

	if (DBG <= *Reportlevel) {
		msg_printf(DBG,"Showing fault before any faults. Ctlr %d\n\n",
			contn);
		show_fault();
	}	

        /* Set up the ecc error */
	/* setbadecc(); */

	/* do a dma access to hit the ecc location */
	/* was cont_num */
	retval = dmadoit(ioa_num, contn, slot, targ); 

	/* if no swap partition to xfer to, exit - was not a failure */
	if (retval == TEST_SKIPPED)
	    goto TEST_END;

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

	if (DBG <= *Reportlevel)
		show_fault();

	/* DoErrorEret();   */
	wd95a_clear_dmaintr(swin, ioa_num, cont_num, slot, targ, lun, contn);
    }

TEST_END:    
    clear_nofault();
    /* clearbadecc(); */

    /* Leave things tidy */
/*
    SetSR(osr);
*/
    /*
     * NEW - cleans up
     */
    clear_err_intr(slot, ioa_num);
    
    EV_SET_LOCAL(EV_CIPL0, 0); 

    /* Disable level 0 interrupts */
    oile = EV_GET_LOCAL(EV_ILE);
    EV_SET_LOCAL(EV_ILE, oile | ~EV_EBUSINT_MASK);

    return(retval);
}

clearbadecc()
{
	uint *addr;
	uint tmp;

	msg_printf(DBG,"clearbadecc\n");
	addr = (uint *)NEWLINE;

#ifndef TFP
	/* prevent ECC exceptions in the write back below */
        SetSR(GetSR() | SR_DE);
#endif

	/*
         * Force a write back of a line
         */
        tmp = load_double((long long*)(k0addr + AMEG)); /* scache is 1 mb */
        *(k0addr + AMEG) = tmp;

#ifndef TFP
	SetSR(GetSR() & ~SR_DE);	
#endif
}

/*
 * this needs to be looked at VERY carefully - the code is being altered to
 * compile, and bad ecc forcing needs to WORK to get this test to function
 */
setbadecc()
{
	int data = 0x1;
	uint   tag[2];
	long long tmp;

	k0addr = (uint *)(WORKLINE + ALINE);
	
	/* dirty a cache line -- (write to) a memory location, cached */
	*(uint*)k0addr = data;

#if _MIPS_SIM != _ABI64
	/* set C0_ECC for this cache line -- 2ndary index load tag */
	_read_tag(CACH_SD, k0addr, tag); 

	/* force bad ecc value in C0_ECC */
	set_ecc(~(get_ecc() ^ 1)); /* this generates a double bit ECC */

	/* CE bit must be on for CACHE op to use C0_ECC */
        SetSR(GetSR() | SR_CE);

        /* write the line back from pdcache to scache */
        pd_hwbinv(k0addr);

        /* turn off CE */
        SetSR(GetSR() & ~SR_CE);  
#else
	set_cp0_badvaddr((long)k0addr);
#endif

	/* prevent ECC exceptions in the write back below */
        /* SetSR(GetSR() | SR_DE); */

#if 0
	/* 
         * Force a write back of this line with bad ecc from scache to memory
         * and, hence, an interrupt at CC level 3.
         */
        tmp = load_double((long long*)(k0addr + AMEG)); /* scache is 1 mb */
        *(k0addr + AMEG) = tmp;
#endif
} 

dmadoit(int adap, int ctlr, int slot, int targ)
{
        int i, j, result;
        char device[12];
        uint count, firstblock, oile, osr;
        IDESTRBUF s;
        char answ[4];
        char name[SCSI_DEVICE_NAME_SIZE];
        int fail = 0;

        wd95loc[0] = slot;
        wd95loc[1] = adap;

        clearvh();

        fd = gopen(ctlr,targ,PTNUM_VOLUME,SCSI_NAME,OpenReadWrite);
        if (fd < 0) {
                return(1);
        }
        else {
                sprintf(s.c,"scsi(%d)disk(%d)part(%d)",ctlr,targ,PTNUM_VOLUME);
                msg_printf(DBG,"%s opened!, fd: %d\n", s.c, fd);
                if (!scsi_dname(name, sizeof (name), fd))
                        msg_printf(DBG,"Scsi drive type == %s\n", name);
        }

        if (init_label(fd)) {
                return (1);
        }

	/*
	 * if no space on partition to test, it should be a skip, not a fail
	 */
	if (firstblock_ofpart(dkpart, force, &firstblock) == -1) {
		msg_printf(INFO, "Skipping partition %d\n", dkpart);
		return (TEST_SKIPPED);
	}

    	/* Enable level 0 intrs. */
    	oile = EV_GET_LOCAL(EV_ILE);
    	EV_SET_LOCAL(EV_ILE, oile | EV_EBUSINT_MASK);

    	/* Make sure there are currently no pending interrupts */

    	osr = GetSR();
    	msg_printf(DBG,"osr loc 1 is 0x%x\n", osr);
	
	cpu_err_clear();

        /* write buffer to device */
        msg_printf(INFO, "Reading from partition %d\n", dkpart);
        msg_printf(DBG,"firstblock is %d\n", firstblock);
	
	msg_printf(DBG,"before wr: CacheErr reg: 0x%x\n", GetCacheErr());

#ifndef TFP
	/* disable ECC exceptions */
	if (_DEset)
		SetSR(GetSR() | SR_DE);
#endif
        for ( count = 0; count < 1; count++ ) {
		msg_printf(VRB, "count is %d\r", count);
                /* result = gwrite(firstblock,k0addr,1,slot,adap,fd); */
		result = gread(firstblock,(void *)mem_bad, 1, slot,adap,fd); 
		msg_printf(DBG,"cause: 0x%x, ERTOIP: 0x%x\n", get_cause(), 
				(uint)EV_GET_LOCAL(EV_ERTOIP));
		if ((get_cause() & SR_IBIT3) && (EV_GET_LOCAL(EV_ERTOIP)))
			msg_printf(DBG,"Got the expected Intr from CC+R4K\n");
		msg_printf(DBG,"returned from gread, result: %d\n", 
			result);
    		msg_printf(DBG,"sr loc 2 is 0x%x\n", GetSR());
                if (result) {
                        /* err_msgs in gread definition */
                        gclose(fd);
                        fail = 1;
                        return(fail);
                }
	}
	msg_printf(INFO,"Read completed ok\n");
	gclose(fd);
	msg_printf(DBG,"after wr a: CacheErr reg: 0x%x\n", GetCacheErr());
#ifndef TFP
	if (_DEset)
		SetSR(GetSR() & ~SR_DE);
#endif
	msg_printf(DBG,"after wr b: CacheErr reg: 0x%x\n", GetCacheErr());
	if (DBG <= *Reportlevel)
		show_fault();
	msg_printf(DBG,"Loc c:(IP0) 0x%llx, (IP1) 0x%llx\n", 
			EV_GET_LOCAL(EV_IP0), EV_GET_LOCAL(EV_IP1));
	msg_printf(DBG,"about to set sr\n");
    	SetSR(GetSR() | SR_IBIT3 | SR_IE); 
} /* dmadoit */



int wd95a_clear_dmaintr(caddr_t swin, int ioa_num, int cont_num, uint slot, 
		uint targ, int lun, int contn)
{
	register uint dummy;
	register int fail = 0;
	uint ctrl_off;
	uint tmp;
	uint isr_intr, uei_intr;
	wd95ctlrinfo_t  *ci;
	wd95unitinfo_t  *ui;
	wd95luninfo_t 	*li;
	struct wd95request      *wreq;
	scsi_request_t          *req;
	dmamap_t                *dmamap;
	int val;

	ci = &wd95dev[contn];
	ui = ci->unit[targ];
	li = ui->lun_info[lun];
	wreq = li->wqueued;
	dmamap = ci->d_map;

	if (!_DEset) {
	msg_printf(DBG,"clr:ci: 0x%x, ui:0x%x, wreq:0x%x\n", ci, ui, wreq);
	msg_printf(DBG,"li->wqueued: 0x%x, ui->wrfree: 0x%x\n", 
		li->wqueued, ui->wrfree);
	msg_printf(DBG,"ci->page: %x, &wreq->sreq->sr_buffer[sr_buflen]:%x\n",
		ci->page, &wreq->sreq->sr_buffer[wreq->sreq->sr_buflen]);

	if (IS_KSEG1(dmamap))
                    flush_iocache(s1adap2slot(dmamap->dma_adap));

	wreq->ctlr = contn;
	wreq->unit = targ;

	wd95timeout(wreq);
	} /* !_DEset */

	/*
	 * offset of the selected controller's regs in the s1 chip space
	 */
	ctrl_off = S1_CTRL_BASE + (cont_num * S1_CTRL_OFFSET);
	msg_printf(DBG,"ctrl_off is 0x%x\n", ctrl_off);
	
	
	val=GET_WD_REG(swin + S1_CTRL(cont_num),WD95A_S_CTL) & W95_CTL_SETUP;
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, 0);
	
	dummy = S1_GET_REG(swin, S1_STATUS_CMD);
	msg_printf(DBG,"S1 CSR before clearing is 0x%x\n", dummy);

	isr_intr = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISR);
	msg_printf(DBG,"ISR before clearing is 0x%x\n", isr_intr);

	uei_intr = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI);
	msg_printf(DBG,"UEI before clearing is 0x%x\n", uei_intr);

	msg_printf(DBG,"STOPU before is 0x%x\n",
	GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI));

	/* put the wd95a into normal mode */
	/* octl = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL); */
	/* msg_printf(DBG,"octl is 0x%x\n", octl);
	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, 0);
	*/

        /* flush dma channel */
        SET_DMA_REG(dp, S1_DMA_FLUSH, 0);
        DELAY(200);
        SET_DMA_REG(dp, S1_DMA_CHAN_RESET, 0);
        tmp = S1_GET_REG(swin, S1_STATUS_CMD);
        msg_printf(DBG,"csr loc ab is 0x%x, c:%d\n", tmp, contn);

	/* reset ISRM to original value */
	/* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISRM, oisrm);
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_ISRM);
	msg_printf(DBG,"Original isrm is 0x%x\n", oisrm);
	msg_printf(DBG,"New isrm is 0x%x\n", dummy);
	*/

	/* reset UEIM to original value */
	/* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEIM, oueim);
	dummy = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEIM);
	msg_printf(DBG,"Original ueim is 0x%x\n", oueim);
	msg_printf(DBG,"New ueim is 0x%x\n", dummy);
	*/

	/* write to UEI to clear interrupt */
	/* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_N_UEI, uei_intr); */

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
	/* SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, octl); */
	octl = GET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL);
	msg_printf(DBG,"reset ctl is 0x%x\n", octl);

	DELAY(200);
	io_err_clear(slot, (1 << ioa_num));

	msg_printf(DBG,"debug: loc 4\n");

	SET_WD_REG(swin + S1_CTRL(cont_num), WD95A_S_CTL, val);

	/* msg_printf(DBG,"About to call gclose\n");
	gclose(fd); 
	*/
	
	return(fail);
}


int
do_dmaintr(caddr_t swin, int ioa_num, int cont_num, uint slot, int contn)
{
        register uint dummy;
        register int fail = 0;
        uint ctrl_off;
        int adap, i, result, targ, tmp;
        /* scsi_target_info_t realinfo; */
        /* scsi_target_info_t *info;
        char *inv; */
        wd95ctlrinfo_t  *ci, *debugci;
        int lun = 0;
        int nodevice = 1;
	int tested = 0;
        char id[26];            /* 8 + ' ' + 16 + null */
        char *extra;

	io_err_clear(slot, (1 << ioa_num)); 
        info = &realinfo;

        for (i = 0; i < 26; i++)
                id[i] = 'b';

        /*
         * offset of the selected controller's regs in the s1 chip space
         */
        ctrl_off = S1_CTRL_BASE + (cont_num * S1_CTRL_OFFSET);
        msg_printf(DBG,"ctrl_off is 0x%x\n", ctrl_off);
        ci = &wd95dev[contn];
        dp = ci->ha_addr;
        ci->present = 1;
        debugci = &wd95dev[contn];
        msg_printf(DBG,"debugci->present is %d\n", debugci->present);

        /* determine if there are devices on this channel  -from scsiinstall */
        msg_printf(INFO,"\n\t\tDevices connected to controller %d: \n", contn);
        msg_printf(DBG,"cont_num is %d, contn is %d\n", cont_num, contn);
        tmp = S1_GET_REG(swin, S1_STATUS_CMD);
        msg_printf(DBG,"csr loc a is 0x%x\n", tmp);
        for (targ = 1; targ < WD95_LUPC; targ++) {
                msg_printf(DBG,"debug selftest, targ: %d\n", targ);
                if ((info = (scsi_target_info_t *)wd95info(contn,targ,lun)) == NULL) {
                        tmp = S1_GET_REG(swin, S1_STATUS_CMD);
                        msg_printf(DBG,"csr loc aa is 0x%x, t:%d, c:%d\n",
                                        tmp, targ, contn);
                        SET_DMA_REG(dp, S1_DMA_FLUSH, 0);
                        DELAY(200);
                        SET_DMA_REG(dp, S1_DMA_CHAN_RESET, 0);
                        tmp = S1_GET_REG(swin, S1_STATUS_CMD);
                        msg_printf(DBG,"csr loc ab is 0x%x, t:%d, c:%d\n",
                                        tmp, targ, contn);
                        continue;
                }
                inv = (char *)info->si_inq;
                /* ARCS Identifier is "Vendor Product" */
                strncpy(id,inv+8,8); id[8] = '\0';
                msg_printf(DBG,"id1 is %s\n", id);
                for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
                strcat(id," ");
                strncat(id,inv+16,16); id[25] = '\0';
                msg_printf(DBG,"id2 is %s\n", id);
                for (extra=id+strlen(id)-1; *extra == ' '; *extra-- = '\0') ;
                msg_printf(DBG,"id3 is %s\n", id);
                msg_printf(VRB,"inv[0] is %d\n", inv[0]);

                nodevice = 0;
                /* tmp = S1_GET_REG(swin, S1_STATUS_CMD);
                msg_printf(DBG,"csr loc b is 0x%x\n", tmp); */
                msg_printf(DBG,"debug: selftest, ioa_num: %d\n", ioa_num);
                msg_printf(DBG,"inv is %s\n", inv);
                switch(inv[0]) {
                   case 0:
                        /* sometimes CD ROM returns status of disk */
                        if ((strncmp(id, "TOSHIBA CD-ROM", 14) == 0) ||
                                (strncmp(id, "SONY    CD", 10) == 0) ||
                                (strncmp(id, "LMS     CM ", 11) == 0)) {
                                msg_printf(INFO, "\t\t   CD ROM, ");
                                msg_printf(INFO,"unit: %d ", targ);
                                msg_printf(INFO,"- %s\n", id);
                                msg_printf(INFO,
                           "Test is for disks only. Skipping CD ROM.\n");
                        }
                        else { /* normal disk */
			msg_printf(INFO,"\t\t   Hard disk, ");
                        msg_printf(INFO,"unit: %d ", targ);
                        msg_printf(INFO,"- %s\n", id);
                        result = dmaintr(swin,ioa_num,cont_num,slot,targ, lun,contn);
                        msg_printf(DBG,"dmaxfer loc 1, result: %d\n",result);
			ide_wd95busreset(ci);
			if (result != TEST_SKIPPED)
			    tested = 1;
			} /* else normal disk */
                        break;
                   case 1: msg_printf(INFO,"\t\t   SCSI Tape, ");
                        msg_printf(INFO,"unit: %d - %s\n",
                                targ, id);
                        msg_printf(INFO,
                           "Test is for disks only. Skipping tape drive.\n");
                        break;

                   default:
                        if ((inv[0]&0x1f) == INV_CDROM) {
                                msg_printf(INFO, "\t\t   CD ROM, ");
                                msg_printf(INFO,"unit: %d ",targ);
                                msg_printf(INFO,"- %s\n", id);
                                msg_printf(INFO,
                           "Test is for disks only. Skipping CD ROM.\n");
                        }
                        else {
                                msg_printf(INFO,"\t\t   Device type %u ",
                                        inv[0]&0x1f);
                                msg_printf(INFO,"unit: %d ",targ);
                                msg_printf(INFO,"- %s\n", id);
                                msg_printf(INFO,
                           "Test is for disks only. Skipping this device.\n");
                        }
                        break;
                }
                /* output results */
		if (!fail || (fail == TEST_SKIPPED))
		    fail = result;
		else if (result != TEST_SKIPPED)
		    fail += result;
                msg_printf(DBG,"dmaxfer loc 2\n");
                /* tmp = S1_GET_REG(swin, S1_STATUS_CMD);
                msg_printf(DBG,"csr loc c is 0x%x\n", tmp); */
                msg_printf(DBG,"dmaxfer loc 3, targ: %d\n", targ);
        } /* for targ */
        /* tmp = S1_GET_REG(swin, S1_STATUS_CMD);
        msg_printf(DBG,"csr loc d is 0x%x\n", tmp); */
        if (nodevice) {
               msg_printf(INFO,"\t\t   None\n");
	       if (!fail)
		    fail = TEST_SKIPPED;
	}

	if (!tested) 
           msg_printf(INFO,"\nNo SCSI disks tested on controller %d\n",contn);

        msg_printf(DBG,"debug: loc 2\n");

        return(fail);
}


int scsi_dmaintr(int argc, char**argv)
{
	int fail = 0;
	unsigned long memsize;
	char answ[4];

	dkpart = PTNUM_FSSWAP;
	force = 0;
	
    	msg_printf(INFO, "scsi_dmaintr - testing scsi dma interrupts\n");

	/*
	 * check to see if the maximum memory size this test can use
	 * (MAXADDR) is exceeded - if it is, skip; if not, position
	 * the memory buffer (mem_bad) to straddle the end of the
	 * installed memory
	 */
        memsize = readconfig_reg(RDCONFIG_MEMSIZE,0);
	if (memsize > MAXADDR) {
	    msg_printf(INFO,
		"memory size too large for this test - skipping\n");
	    return(TEST_SKIPPED);
	}
	mem_bad = (--memsize) & BADADDR;

	while (--argc > 0) {
                if (**++argv == '-') {
                        msg_printf(DBG,"argv is -\n");
                        msg_printf(DBG,"*argv[1] is %c\n", (*argv)[1]);
                        msg_printf(DBG,"argc is %d\n", argc);
                        switch ((*argv)[1]) {
                           case 'f':
                                force = 1; break;
                           case 'p':
                                if (argc-- <= 1) {
                                   msg_printf(SUM,
                                      "The -p switch requires a number\n");
                                    return (1);
                                }
                                msg_printf(DBG,"argc is %d\n", argc);
                                if (*atob(*++argv, &dkpart)) {
                                   msg_printf(SUM,
                                      "Unable to understand the -p number\n");
                                   msg_printf(SUM,
                                      "The -p switch requires a number\n");
                                   return (1);
                                }
                                break;
                        }
                }
        }
        msg_printf(DBG,"force: %d, dkpart: %d\n", force,dkpart);
questloop:
	/* leave like this for now with force instead of !force */
        if (force) {
                   msg_printf(SUM,"Do you want to continue? (y=1, n=0): ");
                   gets(answ);
                   if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                        if (!strcmp(answ, "0")) {
                                msg_printf(SUM,"Skipping the test...\n");
                                return (0);
                        }
                   }
                   else {
                        msg_printf(SUM,"Please enter a 1 or a 0\n");
                        goto questloop;
                   }
questloop2:
                   msg_printf(SUM,"Are you sure? (y=1, n=0): ");
                   gets(answ);
                   if (!strcmp(answ, "1") || !strcmp(answ, "0")) {
                        if (!strcmp(answ, "0")) {
                                msg_printf(SUM,"Skipping the test...\n");
                                return (0);
                        }
                   }
                   else {
                        msg_printf(SUM,"Please enter a 1 or a 0\n");
                        goto questloop2;
                   }
	}
	scsi_setup();
        fail = test_scsi(argc, argv, do_dmaintr);
	return(fail);
}
