#if NOT_USED

/**************************************************************************
 *                                                                        *
 *             Copyright (C) 1992, Silicon Graphics, Inc.                 *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/reg.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/s1chip.h>
/*
 * The analysis of everest error data to determine a fru is not an easy job
 * and may take years of field experience to make it good enough.
 *
 * No matter how good a job we did, the result is still a guess!.
 */

static eframe_t	*evfru_ep;

/*
 * Data structures used within evfru.c to keep track of error propogation 
 * upstream/downstream 
 */

static int Ignore_data_error;
static int my_data_err, my_addr_err;
static int Expect_adap_cmd_err, Expect_adap_data_err;

static char io4_ebus_timeout[EV_MAX_SLOTS], ibus_par_err[EV_MAX_SLOTS];

static char Ignore_ibus_cmd_err[EV_MAX_SLOTS]; 
static char Ignore_ibus_data_err[EV_MAX_SLOTS];
static char ia_ebus_timeout[EV_MAX_SLOTS];
static char ia_addrerr[EV_MAX_SLOTS];
static char fruline[128]; /* tmp buf to format a line */

static int Expect_fci_par_err, Ignore_fci_par_err;

#define	FRU_LVL			1
#define	fru_msg_s1(s,p,c,m)	ev_perr(FRU_LVL, "FRU: S1 Chnl %d, adap %d, IO4 slot %d. Reason: %s\n",c,p,s,m)
#define	fru_msg_cpu(s,c,m)	ev_perr(FRU_LVL, "FRU: CPU %d in slot %d. Reason: %s\n",c,s,m)
#define fru_msg_mem(s,l,b,m)	ev_perr(FRU_LVL, "FRU: Simm in Bank %d leaf %d MC3 slot %d. Reason: %s\n", b,l,s,m)
#define	fru_msg_md(s,m)		ev_perr(FRU_LVL, "FRU: MC3 in slot %d. Reason: %s\n",s,m)
#define	fru_msg_fchip(s,p,m)	ev_perr(FRU_LVL, "FRU: Fchip adap %d, IO4 slot %d. Reason: %s\n", p, s, m)
#define	fru_msg_vme(s,p,m)	ev_perr(FRU_LVL, "FRU: vmecc adap %d, IO4 slot %d. Reason: %s\n", p, s, m)
#define	fru_msg_hip		fru_msg_vme
#define	fru_msg_fci(s,p,m)	ev_perr(FRU_LVL, "FRU: FCI in adap %d IO4 slot %d. Reason: %s\n", p, s, m)
#define	fru_msg_epc(s,p,m)	ev_perr(FRU_LVL, "FRU: EPC adap %d IO4 slot %d. Reason: %s\n", p, s, m)
#define	fru_msg_ibus(s,m)	ev_perr(FRU_LVL, "FRU: IBus in IO4 slot %d. Reason: %s\n",s, m)
#define	fru_msg_io4(s,m)	ev_perr(FRU_LVL, "FRU: IO4 in slot %d. Reason: %s\n",s,m)
#define	fru_msg_ebus(m)		ev_perr(FRU_LVL, "FRU: Ebus. Reason: %s\n", m)
#define	fru_msg_sw(m)		ev_perr(FRU_LVL, "Software Error???. Reason: %s\n",m);
#define	fru_msg_mapram(s,m)	ev_perr(FRU_LVL, "FRU: Mapram in IO4 slot %d. Reason: %s\n", s, m);


extern void bad_board_type(int, int);
extern void bad_ioa_type(int, int, int);
extern void ebus_fru_check(void);

#if R4000
#define	CC_ERROR_TIMEOUT  (CC_ERROR_MYREQ_TIMEOUT|CC_ERROR_MYRES_TIMEOUT| \
				CC_ERROR_MYINT_TIMEOUT)
#endif
#if TFP
#define	CC_ERROR_TIMEOUT  (CC_ERROR_MYREQ_TIMEOUT|CC_ERROR_MYRES_TIMEOUT)
#endif
/*
 * List of hardware which are considered as FRUs
 *
 * Each of the boards (IP19, IO4, MC3)
 * One of 4 CPUs in IP19 board, Tagram, and Secondary Cache
 * Any Leaf, Bank and SIMM in Memory Board.
 * Mezz boards in IO4, Flat cable, VCAM, Any SCSI channels
 *
 * On-board ASICs in IO4 are not taken as FRUs. If there is some problem in 
 * them, entire IO4 board is considered as FRU.
 * 
 * When software tries to analyze the error state, it tries to point to one
 * or more of the above mentioned hardware to be broken.
 *
 */

/*
 * FRU analysis logic..
 *
 * Aim of this FRU analyzer is to weed out the stray error messages (which 
 * would have occured due to propogation of errors), and identify the root
 * cause for a particular system crash. Once a set of error messages are 
 * identified to be the main cause, it finds out the FRUs which could have
 * caused this error and flags them as broken.
 *
 * This is a fairly simplistic approach to the problem of analyzing the
 * errors, but one which I believe is most useful and implementable without
 * much complexity.
 *
 */


/*
 * Some support routines needed 
 */

/* ARGSUSED */
Analyze_address(eframe_t *ep)
{
    fru_msg_sw("Bad Address used by cpu..");
    return 1;
}

/*
 * Logic for cpu_fru_check: 
 * 
 * For each CPU
 * Look at the bits set in CC, and see if any truly local problems could be 
 * identified. If so, flag the appropriate CPU as FRU.
 *
 *  If there is an Scache problem... this CPU is a FRU.
 *  If there is a Tagram problem... this CPU is a FRU.
 *     in both these cases, set Ignore_data_error to be true.
 *
 *  If CC says it's out of Sync.. Dont know what causes it.. 
 *     Flag this CPU as FRU.
 *  
 *  If CC sees an Addr(data) parity error from A(D), but no other boards see
 *     it, should be a local problem, Flag this CPU as FRU
 *
 *  If CC encounters a Timeout error....it could be due to a bad external
 *     board. Try to analyze address found in eframe.badvaddr, and if it turns 
 *     out to be bad..flag this CPU to be bad..
 */
void
cpu_fru_check(int slot)
{
	int id, vcpuid;
	int aerror, error;
	ulong slotmask = (1 << slot);
	
	evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);

	aerror = EVERROR->ip[eb->eb_cpu.eb_cpunum].a_error;

	for (id = 0; id < EV_MAX_CPUS_BOARD; id++) {
		if (eb->eb_cpuarr[id].cpu_enable == 0)
			continue;

		vcpuid = eb->eb_cpuarr[id].cpu_vpid;
		error = EVERROR->cpu[vcpuid].cc_ertoip;

#if R4000
		if (error & (CC_ERROR_SCACHE_MBE|CC_ERROR_PARITY_TAGRAM)){
		    if (error & CC_ERROR_SCACHE_MBE)
			fru_msg_cpu(slot, id, "Bad Scache");
		    else
			fru_msg_cpu(slot, id, "Bad Tagram in CC");

		    Ignore_data_error |= slotmask;
		    
		    if (!(my_data_err & slotmask))
			/* Why my_data_err is not set?? */
			my_data_err |= slotmask;
		}
#endif /* R4000 */
		if (error & CC_ERROR_ASYNC)
		    fru_msg_cpu(slot, id, "Internal Bus Out of Sync");

		/* If CC sees an Addr/data error, but no other boards see
		 * it, should be a local problem 
		 */
		if (((error & CC_ERROR_PARITY_A) && !my_addr_err) ||
		    ((error & CC_ERROR_PARITY_D) && !my_data_err))
		    fru_msg_cpu(slot, id, "Broken A->CC Path");

		if (error &  CC_ERROR_TIMEOUT){
		    if(Analyze_address(evfru_ep) == 0)
		        fru_msg_cpu(slot, id, "Ebus Protocol Timeout");
		}

		if (error & CC_ERROR_MY_ADDR){
		    /* Could be due to ADDR_HERE timeout or 
		     * A chip seeing a parity error in the address sent
		     */
		    if (aerror & (A_ERROR_CC2A_PARITY << id))
			fru_msg_cpu(slot, id, "Broken CC->A path");

		    if ((aerror & (A_ERROR_ADDR_HERE_TIMEOUT << id)) &&
		        (Analyze_address(evfru_ep) == 0))
			fru_msg_cpu(slot, id, "Broken CC->A path");
		}

		/* Check if Data error was due to bad scache */
		if (((error & CC_ERROR_MY_DATA) || 
		     (aerror & (A_ERROR_CC2D_PARITY << id))) && 
		    !(Ignore_data_error & slotmask)){
		    /* Should be some broken path between CC and D chip */
		    fru_msg_cpu(slot, id, "Broken CC->D data path");
		    Ignore_data_error |= slotmask;
		}

	}
}

void
mc3_fru_check(int slot)
{
	int	   mbid = EVCFGINFO->ecfg_board[slot].eb_mem.eb_mc3num;
	mc3error_t *me = &(EVERROR->mc3[mbid]);
	int	   bank;

	if((me->ebus_error & (MC3_EBUS_ERROR_SENDER_DATA)) && 
	   !((me->mem_error[0] & MC3_MEM_ERROR_MBE ) ||
	    (me->mem_error[1] & MC3_MEM_ERROR_MBE ))){
	       fru_msg_md(slot, "Bad MD chip"); 
	       return;
	}

	if (me->mem_error[0] & MC3_MEM_ERROR_MBE) {
		bank  =	me->syndrome0[0] ? 0 :
			me->syndrome1[0] ? 1 :
			me->syndrome2[0] ? 2 : 3;
		fru_msg_mem(slot, 0, bank, "Bad SIMMs causing Data error");
	}
	if (me->mem_error[1] & MC3_MEM_ERROR_MBE) {
		bank =	me->syndrome0[0] ? 0 :
			me->syndrome1[0] ? 1 :
			me->syndrome2[0] ? 2 : 3;
		fru_msg_mem(slot, 1, bank, "Bad SIMMs causing Data error");
	}
}

/*****************************************************************************/

void
fchip_fru_check(ulong ferror, int slot, int padap)
{

	/* io4_fru_check and epc_fru_check should have been executed 
	 * before calling this routine 
	 */

	/* F to IBus command error */
	if (ferror & F2IBUS_CMND_ERR){
	    fru_msg_fchip(slot,padap,"Command On IBus had Bad parity");
	    if (Expect_adap_cmd_err == padap)
		Expect_adap_cmd_err = 0;
	}

	if (ferror & FCI2F_PAR_ERR)
		Expect_fci_par_err = 1;

	/* F to IBus data error */
	if (ferror & F2IBUS_DATA_ERR){
	    if (!Expect_fci_par_err){
	        fru_msg_fchip(slot,padap,"Data from F on IBus had Bad parity");
	        if (Expect_adap_data_err == padap)
		    Expect_adap_data_err = 0;
	    }
	}

	if (ferror & FCHIP_TIMEOUT_ERR){
	    if (!io4_ebus_timeout[slot] && !ibus_par_err[slot])
		fru_msg_fchip(slot,padap,"Command timeout");
	}

	if (ferror & FCHIP_ERROR_FR_IBUS_CMND){
	    if (!Ignore_ibus_cmd_err[slot] && !ibus_par_err[slot])
		fru_msg_fchip(slot,padap,"Broken Internal command path");
	}

	if (ferror & IBUS2F_DATA_ERR){
	    if (!Ignore_ibus_data_err[slot] && !ibus_par_err[slot]){
		fru_msg_fchip(slot,padap,"Broken Internal Data path");
		Ignore_fci_par_err = 1;
	    }
	}

	if (ferror & FCHIP_ERROR_FR_IBUS_PIOW_INTRNL)
	    fru_msg_fchip(slot,padap,"Internal PIO write error");

}

/*
 * vme_dmaaddr:
 *   This routine accepts the slot number
 *   Assumptions:
 *	ebus_error1 and ebus_error2 in IO4 error register are valid. 
 *	The value got set due to a DMA request from Fchip.
 *   Return:
 *	It returns the IO address which maps to the physical address
 *	found in ebus_error1 and ebus_error2
 *  
 */
#define	EBUS_PG_MASK	0xFFFFFFF /* 40 - 12 = 28 bits  */

#define	IO_ADDR(x,y)	(((x << 1)|y) << 12)
int
vme_dmaaddr(int slot, int window, int padap)
{

	uint	phys_addr;
	uint	io_addr, i;
	evreg_t ftlb_addr;
	int   vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	__psunsigned_t swin   = SWIN_BASE(window, padap);

	/* phys_addr holds the physical address as would be available in 
	 * F Tlb 
	 */
	/* place the appropriate page no in bits 0-27  */
	phys_addr = ((EVERROR->io4[window].ebus_error2 & 0xff) << 20) |
		     ((EVERROR->io4[window].ebus_error1 >> 12) & 0xfffff);

	for (i=0; i < 8; i++){
	    /* Out of 21 addr bits in io_addr, upper two correspond to 
	     * Mapram-id. Strip them while returning the io-address
	     */
	    io_addr = EV_GET_REG(swin+FCHIP_TLB_IO0+(i*8));
	    if (!(io_addr & 0x200000)) /* Not a valid entry */
		continue;

	    ftlb_addr = EV_GET_REG(swin+FCHIP_TLB_EBUS0+(i*8));

	    if ((ftlb_addr & EBUS_PG_MASK) == phys_addr)
		/* Lower IO page */
	        return IO_ADDR(io_addr, 0);

	    if(((ftlb_addr >> 32) & EBUS_PG_MASK) == phys_addr)
		/* Higher IO page */
	        return (IO_ADDR(io_addr, 1));
	}

	return 0;
}

void
vmecc_fru_check(int slot, int padap)
{
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	vmeccerror_t *vmecc = &EVERROR->vmecc[vadap];
	ulong	vmecc_error = vmecc->error;
	ulong	ferror = EVERROR->fvmecc[vadap].error & FCHIP_ERROR_MASK;
	char	board[32];

	/* Check for any Fchip originated Errors. Otherwise, it should just
	 * set some flags to inform VMECC to ignore data errors 
	 */
	Expect_fci_par_err = Ignore_fci_par_err = 0;
	fchip_fru_check(ferror, slot, padap);


	if (vmecc_error & VMECC_ERROR_VMEBUS_TO ||
	    vmecc_error & VMECC_ERROR_VMEBUS_PIOW) {
		fru_msg_vme(slot, padap, "Broken VME Bus or Bad Board");
	}

	/* PIOR path, VME -> VMECC -> F */
	if (vmecc_error & VMECC_ERROR_VMEBUS_PIOR){
		vme_ioaddr(vmecc->addrvme, slot, padap, board);
		sprintf(fruline,"PIO Read from VME Board %s failed", board);
		fru_msg_vme(slot, padap, fruline);

		if (Expect_adap_data_err == padap)
		    Expect_adap_data_err = 0;

		if (Expect_fci_par_err)
		    Expect_fci_par_err = 0;
	}

	/* Ignore VMECC_ERROR_FCIDB_TO since it gets set in a bogus way */

	if (vmecc_error & VMECC_ERROR_VMEBUS_SLVP ){
	    /* VME did not get a response for a DMA request... */
	    if (ia_addrerr[slot] == padap){
		fru_msg_vme(slot,padap,"Bad DMA request from a VME controller");
	    }
	    else 
		fru_msg_fci(slot,padap,"Problem in Flat Cable");
	}

	if ((vmecc_error & (VMECC_ERROR_VMEBUS_SLVP|VMECC_ERROR_FCI_PIOPAR)) &&
	    !Ignore_fci_par_err)
		fru_msg_vme(slot, padap, "Problem in Flat Cable..");

	
	if (Expect_fci_par_err)
	    fru_msg_fci(slot, padap, "F Got bad parity data from FCI");
}

void
io4hip_fru_check(int slot, int padap)
{

	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	vmeccerror_t *vmecc = &EVERROR->vmecc[vadap];
	ulong	vmecc_error = vmecc->error;
	ulong	ferror = EVERROR->fvmecc[vadap].error & FCHIP_ERROR_MASK;
	char	board[32];

	/* Check for any Fchip originated Errors. Otherwise, it should just
	 * set some flags to inform VMECC to ignore data errors 
	 */
	Expect_fci_par_err = Ignore_fci_par_err = 0;

	fchip_fru_check(ferror, slot, padap);

	if (vmecc_error & VMECC_ERROR_VMEBUS_TO ||
	    vmecc_error & VMECC_ERROR_VMEBUS_PIOW) {
		fru_msg_hip(slot, padap, "Broken HIPPI(vmecc) connection");
	}

	/* PIOR path, VME -> VMECC -> F */
	if (vmecc_error & VMECC_ERROR_VMEBUS_PIOR){
		vme_ioaddr(vmecc->addrvme, slot, padap, board);
		sprintf(fruline,"PIO Read from HIPPI(vmecc) failed", board);
		fru_msg_hip(slot, padap, fruline);

		if (Expect_adap_data_err == padap)
		    Expect_adap_data_err = 0;

		if (Expect_fci_par_err)
		    Expect_fci_par_err = 0;
	}

	/* Ignore VMECC_ERROR_FCIDB_TO since it gets set in a bogus way */

	if ((vmecc_error & (VMECC_ERROR_VMEBUS_SLVP|VMECC_ERROR_FCI_PIOPAR)) &&
	    !Ignore_fci_par_err)
		fru_msg_hip(slot, padap, "Bad parity data sent on FCI");

	if (Expect_fci_par_err){
	    fru_msg_fci(slot, padap, "F Got bad parity data on FCI");
	    Expect_adap_data_err = 0;
	}
}

void
epc_fru_check(int slot, int padap)
{
	/* There is no error protection on the PBUS side at all */
	/* All errors reported by epc are fatal */
	/* We will check errors as two different kinds, parity and timeout. */
	/* Parity errors could be found by IA and/or EPC */

	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	ulong	epc_error = EVERROR->epc[vadap].ibus_error;

	if (epc_error == 0)
	    return;

	ibus_par_err[slot] = EPC_IBUS_PAR(epc_error);

	/* Error in Command/Data from IA to IOA */
	switch(EPC_IERR_IA(epc_error)){

	case 0  : break; /* No error if Zero */
	case EPC_IERR_IA_PIOW_DATA:
	case EPC_IERR_IA_DMAR_DATA:
	    if (Ignore_ibus_data_err[slot] == 0){ /* IA saw no error !!*/
		if(ibus_par_err[slot] == 0)
		    fru_msg_epc(slot,padap,"Broken EPC data path");
		else
		    fru_msg_ibus(slot,"EPC detected Bad Parity on IBus data");
		
	    }
	    break;
	case EPC_IERR_IA_UNXPCTD_DMA:
	    /* EPC received Unexpected DMA Read resp */
	    if ((epc_error & EPC_DMA_RDRSP_TOUT) == 0)
		fru_msg_io4(slot,"EPC Received unexpected DMA response");
	    break;
	
	case EPC_IERR_IA_BAD_CMND:
	    if (Ignore_ibus_cmd_err[slot] == 0){ /* IA saw no error !!! */
		if (ibus_par_err[slot] == 0)
		    fru_msg_epc(slot,padap,"Broken EPC command path");
		else
		    fru_msg_ibus(slot,"EPC detected Bad Parity on IBus cmd");
		
	    }
	    break;
	default:
	    fru_msg_epc(slot,padap,"Unexpected Error value from EPC");
	} /* switch */

	/* 
	 * Non participation Errors observed by EPC are being ignored now..
	 * Need to find a way to make use of it.
	 */

	/* Errors in data/cmd sent by EPC */
	switch(EPC_IERR_EPC(epc_error)){

	case 0:	break;		/* No error if Zero */
	case EPC_IERR_EPC_PIOR_CMND:
	case EPC_IERR_EPC_DMAR_CMND:
	case EPC_IERR_EPC_DMAW_CMND:
	case EPC_IERR_EPC_INTR_CMND: 
	case EPC_IERR_EPC_PIOR_CMNDX: /* Cmd from some device on Pbus */
	case EPC_IERR_EPC_DMAR_CMNDX: /* DMA rd/wr req from Pport */
	case EPC_IERR_EPC_DMAW_CMNDX:
	    /*
	     * XXXXXXXXXXXXXXXXXXXXXXXX
	     * Not comfortable with this yet.
	     * Is it possible that EPC has command error bits set but
	     * no parity error is seen on Ibus and by IA??
	     * Is it possible that EPC and IBus see errors and not IA ??
	     * Is it possible that IA sees it and not EPC or IBus ??
	     */
	    fru_msg_epc(slot,padap,"Sent commands with bad parity");
	    if (Expect_adap_cmd_err == padap)
		Expect_adap_cmd_err = 0;
	    break;
	case EPC_IERR_EPC_PIOR_DATA:
	case EPC_IERR_EPC_DMAW_DATA:
	case EPC_IERR_EPC_PIOR_DATAX: /* response from some device on Pbus */
	case EPC_IERR_EPC_DMAW_DATAX: /* DMA write data from Pport */
	    /* 
	     * XXXXXXXXXXXXXXXXXXXXXXXXXXX
	     * Similar questions as above case ???
	     */
	    fru_msg_epc(slot,padap,"Data with bad parity from EPC");
	    if (Expect_adap_data_err == padap)
		Expect_adap_data_err = 0;
	    break;
	default:
	    fru_msg_epc(slot,padap,"Invalid Error in EPC Ibus error register");
	    break;
	} /* switch */


}

void
fcg_fru_check(int slot, int padap)
{
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	ulong	ferror = EVERROR->ffcg[vadap].error & FCHIP_ERROR_MASK;

	/* Check for any Fchip originated Errors. Otherwise, it should just
	 * set some flags to inform VMECC to ignore data errors 
	 */
	Expect_fci_par_err = Ignore_fci_par_err = 0;
	fchip_fru_check(ferror, slot, padap);

	if (Expect_fci_par_err){
	    fru_msg_fci(slot, padap, "F Got bad parity data on FCI");
	    Expect_adap_data_err = 0;
	}

}

/*
 * Analyze the Error information given out by the SCSI chips.
 * This should be executed after io4_fru_check and epc_fru_check
 */
void
scsi_fru_check(int slot, int padap)
{
#if SABLE
	/* SABLE does not support S1 chip emulation */
	return;
#else
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	ulong	s1_error = EVERROR->s1[vadap].ibus_error;
	int	sc_chnl;

	if (s1_error == 0)
	    return;
	
	sc_chnl = (s1_error >> 6) & 7; /* Bit0-> Chnl 0, .. bit2-> chnl 2 */

	if (((s1_error & (S1_IERR_IN_DATA|S1_IERR_DMA_READ)) && 
	      !Ignore_ibus_data_err[slot]) ||
	    ((s1_error & (S1_IERR_IN_CMD|S1_IERR_WRITE_REQ)) && 
	      !Ignore_ibus_cmd_err[slot]))
	    fru_msg_ibus(slot,"S1 received cmd/data with Bad parity");
	
	if (s1_error & (S1_IERR_OUT_DATA)){
	    Expect_adap_data_err = 0;
	    fru_msg_s1(slot,padap,sc_chnl,"Sent Bad Parity data on IBus");
	}

	if (s1_error & S1_IERR_OUT_CMD) {
	    Expect_adap_cmd_err = 0;
	    fru_msg_s1(slot,padap,sc_chnl,"Sent Bad Parity command on IBus");
	}

	if ((s1_error & S1_IERR_SURPRISE) && !ia_ebus_timeout[slot])
	    fru_msg_io4(slot,"S1 received an unexpected DMA response");

	if (s1_error & S1_IERR_PIO_READ){
	    Expect_adap_data_err = 0;
	    fru_msg_s1(slot,padap,sc_chnl,"Bad parity on PIO data");
	}
#endif /* !SABLE */
}


void
adap_fru_check(int slot)
{
	int	padap;
	int	type;
	evbrdinfo_t *eb   = &(EVCFGINFO->ecfg_board[slot]);

	for (padap = 1; padap < IO4_MAX_PADAPS; padap++) {
		if (eb->eb_ioarr[padap].ioa_enable == 0)
			continue;

		type = eb->eb_ioarr[padap].ioa_type;
		switch (type) {
		case IO4_ADAP_VMECC:
			vmecc_fru_check(slot, padap);
			break;
		case IO4_ADAP_HIPPI:
			io4hip_fru_check(slot, padap);
			break;
		case IO4_ADAP_EPC:
			epc_fru_check(slot, padap);
			break;
		case IO4_ADAP_FCG:
			fcg_fru_check(slot, padap);
			break;
		case IO4_ADAP_SCSI:
		case IO4_ADAP_SCIP:
			scsi_fru_check(slot, padap);
			break;
		case IO4_ADAP_NULL:
			break;
		default:
			bad_ioa_type(type, slot, padap);
		}
	}
}
	

#define	BAD_DATA_FRIOA	(IO4_IBUSERROR_DMAWDATA|IO4_IBUSERROR_PIORESPDATA)

void
io4_fru_check(int slot)
{
	int	padap;
	evbrdinfo_t *eb   = &(EVCFGINFO->ecfg_board[slot]);
	int	window    = eb->eb_io.eb_winnum;
	ulong	ia_ierror = EVERROR->io4[window].ibus_error;
	ulong	ia_eerror = EVERROR->io4[window].ebus_error;
	ulong	ia_eerror2 = EVERROR->io4[window].ebus_error2;

	ia_addrerr[slot] = 0;
	Ignore_ibus_cmd_err[slot] = 0;
	Ignore_ibus_data_err[slot] = 0;

	padap = IO4_IBUSERROR_IOA(ia_ierror);
	if (!padap)
		padap = IO4_MAX_PADAPS;

	if (ia_eerror & IO4_EBUSERROR_INVDIRTYCACHE)
		fru_msg_ebus("IO4 Received Invalidate for dirty cache line");
	if (ia_eerror & IO4_EBUSERROR_PIO)
		fru_msg_sw("IO4 detected an illegal PIO");
	if (ia_eerror & IO4_EBUSERROR_BADIOA)
		fru_msg_sw("IO4 detected PIO to Non-existant IOA");

	if (ia_eerror & IO4_EBUSERROR_MY_DATA_ERR){
	    if((ia_ierror & BAD_DATA_FRIOA) == 0)
		fru_msg_io4(slot,"Bad IA/ID connection to Ebus");
	}

	if (ia_eerror & IO4_EBUSERROR_MY_ADDR_ERR){

	    /* Complicated Case... 
	     * Could happen due to two reasons..
	     *
	     * 1- Broken software. IOA was programmed to 
	     * 	do DMA from a bad address..OR mapram programmed incorrectly
	     *  or 2nd level Map table got trashed...
	     *
	     * 2- Broken ASIC.. sent out a wrong DMA address
	     *
	     * I dont know how to handle first case... It may be possible
	     * to findout the IO address which caused DMA request to this
	     * Ebus address (assuming it came from FCG/VMECC via Fchip.
	     * But how to map the IO address to the driver  ????
	     *
	     * Second case should become obvious once the adapter error is 
	     * processed..
	     *
	     * For now just set a flag
	     */
	    
	    padap = (ia_eerror2 & IO4_EBUSERROR2_IOA_ID) >> 16;
	    ia_addrerr[slot] = padap;
	}

	if (ia_eerror & (IO4_EBUSERROR_TRANTIMEOUT|IO4_EBUSERROR_TIMEOUT)){
	    ia_ebus_timeout[slot] = 1;

	    if (ia_eerror & IO4_EBUSERROR_TRANTIMEOUT)
	        fru_msg_io4(slot,"IO4 timed out trying to access EBus");

	    if (ia_eerror & IO4_EBUSERROR_TIMEOUT)
	        fru_msg_ebus("IO4 got no response from some board(Memory??)");
	}

	if (ia_eerror & IO4_EBUSERROR_DATA_ERR)
		/* Ignore all data errors down the line */
		Ignore_data_error |= ( 1 << slot);

	
	/* Processing IA Ibus error register contents */

	if (ia_ierror & IO4_IBUSERROR_MAPRAM)
	    fru_msg_mapram(slot,"Parity Error in Data from Mapram");

	/* Command sent from IA to IOA was flagged to have Bad parity */
	if (ia_ierror & IO4_IBUS_CMDERR_TO_IOA){
		/* Broken Ibus/IA. Replace IO4 */
		fru_msg_io4(slot,"Broken IBus Connection to IA");
		Ignore_ibus_cmd_err[slot] = 1;
	}

	if (ia_ierror & (IO4_IBUSERROR_IARESP|IO4_IBUSERROR_PIOWDATA)){
	    /* Bad data sent onto Ibus by IA.. Check if it came from Ebus */
	    if (Ignore_data_error == 0){
		fru_msg_io4(slot,"Broken IBus Connection to IA");
	    }
	    Ignore_ibus_data_err[slot] = 1;
	}

	/* 
	 * Some Command or Data sent from IOA to IA was found to have bad 
	 * parity. Check if the adapter also says so. If adapter thinks
	 * everything is fine, we have a broken IBus. Otherwise handle
	 * the error from adapter perspective.
	 */
	if (ia_ierror & (IO4_IBUSERROR_PIORESPDATA|IO4_IBUSERROR_DMAWDATA))
	    /* Ensure that the Adapter 'padap' did send bad data to IA */
	    Expect_adap_data_err = padap;

	if (ia_ierror & IO4_IBUS_CMDERR_FR_IOA)
	    Expect_adap_cmd_err = padap;
		    

	adap_fru_check(slot);

	/* Some of the 'Expect*' variables set by IO4 should have been 
	 * reset during adapter_fru processing. If they are not turned
	 * off, then the adapters did not see the errors which IO4 saw,
	 * indicating a broken path
	 */
	if (Expect_adap_data_err){
	    /* Adapter did not send down any bad parity data. So the 
	     * problem should be in ID
	     */
	    fru_msg_io4(slot,"ID chip got bad data, but NOT sent by padaps");
	    Expect_adap_data_err = 0;
	}

	if (Expect_adap_cmd_err){
	    /* Adapter did not get flagged by IBus that a command it sent
	     * was bad, but IA received bad command !!!!
	     */
	    fru_msg_io4(slot,"IA chip got bad command, but NOT sent by padaps");
	    Expect_adap_cmd_err = 0;
	}

	if (ia_addrerr[slot]){
	    /* There was no related error bit set in any other adapter .
	     * eg.. DMA error/timeout. So this should be due to broken
	     * IA
	     */
	    sprintf(fruline,"IA failed in Ebus read for data requested by %s",
			ioa_name(eb->eb_ioarr[padap].ioa_type));
	    fru_msg_io4(slot,fruline);
	    ia_addrerr[slot] = 0;
	}

}



/*****************************************************************************/


/*
 *								 VMECC
 *	   ---------						 |
 *	  ---------|						--	FCI
 *	 ---------|						|
 *	---------|				EPC	S1	F
 *	|  CC	|				|	|	|
 *	---------				---------------------	IBUS
 *	    |						    |
 *      ---------		---------		---------
 *	| A + D |...		| MA+MD |...		| IA+ID |...
 *	---------		---------		---------
 *	    |			    |			    |
 *	========================================================	EBUS
 *
 */

/*
 * The backplane check is done first. These error state are kept global thus
 * to be used later on by other module's xxx_fru_check().
 */

void
ebus_fru_check(void)
{
	int	slot, pcpuid;
	ulong	mask, error, slotmask=0;

	my_addr_err = my_data_err = 0;

	/*
	 * only checks ADDR_ERR and DATA_ERR state on all cards.
	 */
	for (slot = 1, mask = 2; slot < EV_MAX_SLOTS; slot++, mask <<= 1) {
		evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);
		if (eb->eb_enabled == 0)
			continue;

		slotmask |= mask;

		switch (eb->eb_type) {
		case EVTYPE_IP19:
			pcpuid = eb->eb_cpu.eb_cpunum;
			error = EVERROR->ip[pcpuid].a_error;
			if (error & A_ERROR_MY_ADDR_ERR)
				my_addr_err |= mask;
			if (error & (A_ERROR_CC2D_PARITY << pcpuid))
				my_data_err |= mask;

			for (pcpuid = 0; pcpuid < 4; pcpuid++) {
				error = EVERROR->cpu[eb->eb_cpuarr[pcpuid].cpu_vpid].cc_ertoip;
				if (error & CC_ERROR_MY_ADDR)
					my_addr_err |= mask;
				if (error & CC_ERROR_MY_DATA)
					my_data_err |= mask;
			}

			break;

		case EVTYPE_MC3:
			error = EVERROR->mc3[eb->eb_mem.eb_mc3num].ebus_error;
			if (error & MC3_EBUS_ERROR_SENDER_ADDR)
				my_addr_err |= mask;
			if (error & MC3_EBUS_ERROR_SENDER_DATA)
				my_data_err |= mask;
			break;

		case EVTYPE_IO4:
			error = EVERROR->io4[eb->eb_io.eb_winnum].ebus_error;
			if (error & IO4_EBUSERROR_MY_ADDR_ERR)
				my_addr_err |= mask;
			if (error & IO4_EBUSERROR_MY_DATA_ERR)
				my_data_err |= mask;
			break;

		default:
			bad_board_type(eb->eb_type, slot);
		}
	}

}

void
everest_error_fru(eframe_t *ep)
{
	int slot;

	ev_perr(0, "\nEVEREST FRU Analysis:\n");

	evfru_ep = ep;
	/* Check for Address/Data Errors seen on Ebus. */
	ebus_fru_check();

	/*
	 * Next check each board for error within the board.
	 */
	for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);
		if (eb->eb_enabled == 0)
			continue;
		switch (eb->eb_type) {
		case EVTYPE_IP19:
			cpu_fru_check(slot);
			break;
		case EVTYPE_MC3:
			mc3_fru_check(slot);
			break;
		case EVTYPE_IO4:
			io4_fru_check(slot);
			break;
		default:
			bad_board_type(eb->eb_type, slot);
		}
	}
}
#endif /* NOT_USED */
