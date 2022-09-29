/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.39 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/reg.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/debug.h>
#include <sys/ktime.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/epc.h>

/*
 * REACT/Pro
 */
#include <sys/rtmon.h>

#define EPC_IERR_INIT_VALUE	0x1000000

epc_t	epcs[EV_MAX_IO4S];
int	numepcs=0;

extern void fastick_maint(eframe_t *);

/* ARGSUSED */
void
epc_intr_proftim(eframe_t *ep, void *arg)
{
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_EPC_PROFTIM);

	/*
	 * Frame Scheduler
	 */
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_PROFCOUNTER_INTR,
			 NULL, NULL, NULL, NULL);
	fastick_maint(ep);

	/* at last, re-enable the interrupt */
	if (cpuid() == 0)
	    EV_SET_REG(epcs[0].ioadap.swin + EPC_IMSET, EPC_INTR_PROFTIM);
	LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT,
			 TSTAMP_EV_PROFCOUNTER_INTR, NULL, NULL, NULL);
}

#ifdef	NOT_NEEDED
void
epc_intr_spare(eframe_t *ep, int arg)
{
	cmn_err(CE_PANIC, "Interrupt from EPC spare\n");

	/* at last, re-enable the interrupt */
	EV_SET_REG(epcs[0].ioadap.swin + EPC_IMSET, EPC_INTR_SPARE);
}

void
epc_intr_pport(eframe_t *ep, int arg)
{

	/* at last, re-enable the interrupt */
	EV_SET_REG(epcs[arg].ioadap.swin + EPC_IMSET, EPC_INTR_PPORT);
}
#endif

/* XXX NOT called if EPC_ERROR > INTR_ERR -- intr() is "smart" */
/* ARGSUSED */
void
epc_intr_error(eframe_t *ep, void *arg)
{
	int	i;
	/*
	 * The EPC2's reset function has been disabled,
	 * i.e., this should never happen
	 */
	/* 
	 * Since same ebus intr level is being used for all EPC error 
	 * interrupts, probe and find out which EPC sent this intr.
	 */
	
	for (i=0; i < numepcs; i++){
	    if ((unsigned)EV_GET_REG(epcs[i].ioadap.swin + EPC_IERR))
        	cmn_err(CE_NOTE, "EPC (Slot %d adap %d) Error Interrupt\n",
				epcs[i].ioadap.slot, epcs[i].ioadap.padap);

            if (EV_GET_REG(epcs[i].ioadap.swin + EPC_TBASELO) == 0)
                cmn_err(CE_PANIC, "epc reset");
	}
}

void
epc_init(int slot, int padap, int window)
{
	__psunsigned_t swin;

	epcs[numepcs].ioadap.slot	= slot;
	epcs[numepcs].ioadap.padap 	= padap;
	epcs[numepcs].ioadap.type	= IO4_ADAP_EPC;
	epcs[numepcs].ioadap.swin = swin = SWIN_BASE(window,padap);
	epcs[numepcs].ioadap.lwin	= 0;

	epc_intr_init(swin, numepcs);

	numepcs++;
}

static __psunsigned_t save_swin;
void
epc_intr_init(__psunsigned_t swin, int epcno)
{
	/* initialize the interrupt mapping registers */
	if (epcno == 0){
		save_swin = swin;
		/* evintr_connect to be done only for the Master EPC */
		evintr_connect((evreg_t *)(swin + EPC_IIDPROFTIM),
			EVINTR_LEVEL_EPC_PROFTIM,
			SPLPROF,
			EVINTR_DEST_EPC_PROFTIM,
			epc_intr_proftim,
			0);
	}

	/* Reset the Pbus devices on EPC of non-master IO4s */
	if (epcno){
		EV_SET_REG(swin + EPC_PRSTSET, 0x3ff);
		us_delay(10);
		EV_SET_REG(swin + EPC_PRSTCLR, 0x3ff);
		us_delay(10);
	}
	evintr_connect((evreg_t *)(swin + EPC_IIDERROR),
			EVINTR_LEVEL_EPC_ERROR,
			SPLMAX,
			EVINTR_DEST_EPC_ERROR,
			epc_intr_error,
			0);

	/* clear the error state */
	EV_SET_REG(swin + EPC_IERR, EPC_IERR_INIT_VALUE);
	EV_GET_REG(swin + EPC_IERRC);


#define	EPC_INTR_MAST	(EPC_INTR_PROFTIM) 
#define	EPC_INTR_ENBL	(EPC_INTR_ERROR)

	if (epcno == 0)
		EV_SET_REG(swin + EPC_IMSET, EPC_INTR_ENBL|EPC_INTR_MAST);
	else 	EV_SET_REG(swin + EPC_IMSET, EPC_INTR_ENBL); 
}
void
epc_intr_reinit(int type)
{

	/* initialize the interrupt mapping registers */
	if (type == 1) {
		/* direct the profiling intr at cpu 0 only
		 * this is needed for audio on isolated cpus
		 */
		evintr_connect((evreg_t *)(save_swin + EPC_IIDPROFTIM),
			       EVINTR_LEVEL_EPC_PROFTIM,
			       SPLPROF,
			       EVINTR_DEST_EPC_PROFTIM,
			       epc_intr_proftim,
			       0);
		return;
	}
	if (type == 2) {
		/* direct the profiling intr at all cpus */
		evintr_connect((evreg_t *)(save_swin + EPC_IIDPROFTIM),
			       EVINTR_LEVEL_EPC_PROFTIM,
			       SPLPROF,
			       0,
			       epc_intr_proftim,
			       0);
		return;
	}
}
/*
 * Convert the xternal adapter representation to EPC slot/adap no
 * Return 0 if successful, non-zero if error
 */
int
epc_xadap2slot(int adap, int *slot, int *padap)
{
	int	i;

	*slot = *padap = 0;
        if (adap){
                *slot  = adap >> IBUS_SLOTSHFT;
                *padap = (adap >> IBUS_IOASHFT) & IBUS_IOAMASK;
	}

	if ((adap == 0) || 
	    ((*slot <= 0) || (*slot >= EV_MAX_SLOTS))){
                *slot  = epc_slot(0);
                *padap = epc_adap(0);
		return 0;
	}

	/* adap != 0 && 0 < *slot < EV_MAX_SLOTS */
	if ((*padap == 0) || (*padap >= IO4_MAX_PADAPS)){
		/* search epcs array to get the appropriate adap */

		*padap = IO4_MAX_PADAPS;
	     	for (i=0; i < numepcs; i++){
	 		if (epc_slot(i) != *slot)
		     		continue;
		 	if (epc_adap(i) < *padap)
				*padap = epc_adap(i);
		}

		if (*padap == IO4_MAX_PADAPS)
			return 1;	/* ERROR */
	}

	/* Final check: validate if the IO4/EPC exists in *slot and  *padap
	 */

	 for (i=0; i < numepcs; i++)
		 if ((epc_slot(i) == *slot) && (epc_adap(i) == *padap))
			 return 0;

	 return 1;
}


int
count_xadaps(unsigned module)
{
        int i;
        int numfound = 0;

#if !SABLE
        for (i = 0; i < ibus_numadaps; i++)
                if (ibus_adapters[i].ibus_module == module)
                        numfound++;
#endif

        return numfound;
}

