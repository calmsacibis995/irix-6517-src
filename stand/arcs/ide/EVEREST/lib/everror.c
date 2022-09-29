/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1993, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"$Revision: 1.110 $"

/*
 *        P L E A S E    R E A D
 *
 *   *****    IMPORTANT   NOTE     *******
 *
 *  READ THIS NOTE BEFORE MAKING ANY CHANGES IN THIS FILE.
 *
 *
 *  This file is being shared by IDE diagsnostics code and Kernel. Main
 *  Reason for this is to use the same code/technique to handle the errors
 *  both in Kenel and Standalone Mode. 
 *
 *  This necessitates certain precaution in adding/removing code from this 
 *  file.
 *
 *  If any code is specific to Kernel, and is not needed in the Standalone 
 *  mode please envelope the code with  
 *  #ifndef	_STANDALONE 
 *  #endif
 *
 * Similarly if some code has to be specific to Standaloce source then use
 *  #ifdef	_STANDALONE
 *  #endif
 *
 *
 * Just using #ifdef KERNEL would not be sufficient since IDE build does use
 * it.
 *
 */


#include <sys/types.h>
#include <sys/reg.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/clock.h>
#include <sys/pfdat.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/dang.h>
#ifndef _STANDALONE
#include <sys/EVEREST/mc3.h>
#include <sys/idbgentry.h>
#include <string.h>
#endif

/* To handle IA Rev-1 bug. Assumes only one IO4 board exists in a system  */
static int	IA_chiprev=0; 
static int 	ip_slot_print; 
char		error_dumpbuf[4096];
int		error_dumpindx=0;

#ifdef _STANDALONE
void adap_error_log	(int, int);
void adap_error_show	(int, int);
void adap_error_clear	(int, int);
void ip_error_clear	(int);
void ip_error_log	(int);
void ip_error_show	(int);
void mc3_ebus_error_log	(int);
void mc3_error_show	(int);
int  mc3_mem_error_log	(int);
#else
static void adap_error_log	(int, int);
static void adap_error_show	(int, int);
static void adap_error_clear	(int, int);
static void ip_error_clear	(int);
static void ip_error_log	(int);
static void ip_error_show	(int);
static void mc3_ebus_error_log	(int);
static void mc3_error_show	(int);
static int  mc3_mem_error_log	(int);
#endif /* _STANDALONE */
static void epc_error_clear	(int, int);
static void epc_error_log	(int, int);
static void epc_error_show	(int, int);
static void fcg_error_clear	(int, int);
static void fcg_error_log	(int, int);
static void fcg_error_show	(int, int);
static void dang_error_show	(int, int);
static void dang_error_clear	(int, int);
static void dang_error_log	(int, int);
static void fchip_error_log	(int, int);
static void fchip_error_show	(int, int);
static void scsi_error_clear	(int, int);
static void scsi_error_log	(int, int);
static void scsi_error_show	(int, int);

#ifndef _STANDALONE
extern time_t time;

#define MBYTES 1024*1024
int mc3_type_bank_size[] = {16*MBYTES,64*MBYTES,128*MBYTES,256*MBYTES,
			    256*MBYTES,1024*MBYTES,1024*MBYTES,0};

#define MC3_SBE_MAX_INTR_RATE (HZ)	/* min time interval btwn interrupts */
#define MC3_SBE_EXCESSIVE_RATE 3600	/* declare "excessive" if see err    */
					/*  within this many secs of previous */

mc3_array_err_t	mc3_errcount[EV_MAX_MC3S];

mc3error_t *last_mc3error;	/* for debugging purposes */
#endif	/* _STANDALONE */

#ifdef	CHIPREV_BUG
static char	IA_bugmsg[]={"( Possible IA Rev1 bug??? )"};
#define IAREV1_MSG 	(((IA_chiprev == 1)) ? IA_bugmsg : "")
#define	AREV2_MSG(x)    ((x==2) ? "( Possible A Rev2 bug??? )" : "")
#define	AREV3_MSG(x)    ((x==3) ? "( Possible A Rev3 bug??? )" : "")
#define	VMECC_REV1(x)	((!(x >> 8)) ? "( Possible VMECC Rev1 bug??? )" : "")
#endif

#define	make_ll(x, y)  (evreg_t)((evreg_t)x << 32 | y)	

int everest_error_intr_reason = 0;  

static int everror_udbe;	/* set to SPNUM on a user level DBE. Should
				 * happen only due to bad coding/hardware !!!!
				 */

extern void vsprintf(char *, char *, va_list);

void
bad_board_type(int type, int slot)
{
	printf("^??? Bad board type %d slot %d\n", type, slot);
}

void
bad_ioa_type(int type, int slot, int padap)
{
	printf("^??? Bad ioa type %d slot %d padap %d\n", type, slot, padap);
}

static caddr_t 
ioa_name(int type)
{
	switch(type){
	case IO4_ADAP_EPC  : return ("EPC");
	case IO4_ADAP_FCHIP: return ("Fchip");
	case IO4_ADAP_SCIP : return ("SCIP");
	case IO4_ADAP_SCSI : return ("S1");
	case IO4_ADAP_VMECC: return ("VMECC");
	case IO4_ADAP_HIPPI: return ("HIPPI");
	case IO4_ADAP_FCG  : return ("FCG");
	case IO4_ADAP_DANG : return ("DANG");
	default            : return ("Unknown");
	}
}


#ifdef	IDE 
#include <ide_msg.h> 
#endif

static char	everr_msg[256];
#define	NEST_DEPTH	2
static char	everr_line[256];

#define	NONE_LVL	0
#define	SLOT_LVL	1
#define	ASIC_LVL	2
#define	REG_LVL		3
#define	BIT_LVL		4

/*
 * ev_perr : Error printing Routine.
 *        Format the error based on the level and print it.
 */
void
ev_perr(int level, char *fmt, ...)
{
    
    va_list 	ap;
    char	*c, *p, *line;
    int		i;

    va_start(ap, fmt);
    vsprintf(everr_msg, fmt, ap);
    va_end(ap);

    /* Format message based on level. Take care of  multi-line output */
    for (c = everr_msg; *c; c = p) {
	for (p=c; (*p) && (*p != '\n'); p++); /* Get till eoln OR Null */

	if (*p)
	    p++;  /* beyond eoln */

	line = everr_line;
	if (level){
	    *line++ = '+';
	    for (i=0; i < level*NEST_DEPTH; i++)
	        *line++ = ' ';
	}

	if (*c){
	    bcopy(c, line, (ulong)(p-c));
	    line += (p-c);
	}
	else{
	    *line++ = '\n';
	}
	*line++ = 0;


#ifdef	IDE
	msg_printf(ERR, everr_line);
#else
	/*printf(everr_line);*/
	bcopy(everr_line, &error_dumpbuf[error_dumpindx], (line - everr_line));
	/* Dont count NULL at end */
	error_dumpindx += line - everr_line - 1;
#endif

    }
}

#define	FROM_KERNEL	1
#define	FROM_SYMMON	0

void dump_hwstate(int);

void
everest_error_clear(int mode)	/* 0 == clear local CC, 1 == clear all CC's */
{
	int slot;
	int padap;
	unchar cpubrdnum=0;
	static int first_error_clear = 1;

	everest_error_sendintr(EVERROR_INTR_CLEAR);
	cc_error_clear(mode);

	for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);

		if (eb->eb_enabled == 0)
			continue;

		switch (eb->eb_type) {
		case EVTYPE_IP21:
		case EVTYPE_IP19:
			ip_error_clear(slot);
			if (first_error_clear)
				/* Used as an index into EVERROR cpu array */
				eb->eb_cpu.eb_cpunum = cpubrdnum++;
			break;
		case EVTYPE_MC3:
			mc3_error_clear(slot);
			break;
		case EVTYPE_IO4:
			io4_error_clear(slot);
			for (padap = 1; padap < IO4_MAX_PADAPS; padap++) {
				if (eb->eb_ioarr[padap].ioa_enable == 0)
					continue;
				adap_error_clear(slot, padap);
			}
			break;
		default:
			break;
		}
	}
	everest_error_intr_reason = 0;
	first_error_clear = 0;
}


void
everest_error_log(int	from)
{
	int slot;
	int padap;

	if (from == FROM_KERNEL){
	    /* Needed only when called from kernel */
	    everest_error_sendintr(EVERROR_INTR_PANIC);
	    cc_error_log();			/* log private error state */
	}

	/*
	 * It is possible that not all cpu's private states are logged in time
	 * if the cpu did not get the interrupt . We will NOT wait.
	 */

	for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);

		if (eb->eb_enabled == 0)
			continue;

		switch (eb->eb_type) {
		case EVTYPE_IP21:
		case EVTYPE_IP19:
			ip_error_log(slot);
			break;
		case EVTYPE_MC3:
			mc3_ebus_error_log(slot);
			mc3_mem_error_log(slot);
			break;
		case EVTYPE_IO4:
			io4_error_log(slot);
			for (padap = 1; padap < IO4_MAX_PADAPS; padap++) {
				if (eb->eb_ioarr[padap].ioa_enable == 0)
					continue;
				adap_error_log(slot, padap);
			}
			break;
		default:
			break;
		}
	}
	everest_error_intr_reason = 0;
}

void
everest_error_show(void)
{
	int slot;
	int id;			/* virtual id of the board/adpter */
	int padap;

	ev_perr(NONE_LVL, "HARDWARE ERROR STATE:\n");
	for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);

		if (eb->eb_enabled == 0)
			continue;

		switch (eb->eb_type) {
		case EVTYPE_IP21:
		case EVTYPE_IP19:
			ip_slot_print=1;
			ip_error_show(slot);
			for (id = 0; id < EV_CPU_PER_BOARD; id++)
				if (eb->eb_cpuarr[id].cpu_enable)
				    cc_error_show(slot, id);
			ip_slot_print=0;
			break;
		case EVTYPE_MC3:
			mc3_error_show(slot);
			break;
		case EVTYPE_IO4:
			io4_error_show(slot);
			for (padap = 1; padap < IO4_MAX_PADAPS; padap++) {
				if (eb->eb_ioarr[padap].ioa_enable == 0)
					continue;
				adap_error_show(slot, padap);
			}
			break;
		default:
			break;
		}
	}
}


volatile int in_everest_error_handler;
static int everest_error_dump_done=0;

#ifndef	_STANDALONE 
extern int ev_wbad_inprog, ev_wbad_val;
#ifdef	DEBUG
extern  short kdebug;
#endif
#endif

#if TFP
/* This routine called to clear G-cache parity error before we actually
 * take an interrupt.  Used when probing for non-existent controllers,
 * which ends up returning bad read data to the CPU and hence to the
 * G-cache.  We get G-cache parity errors when this occurs, so we
 * clear them.
 */
void
tfp_clear_gparity_error()
{
	setcause(getcause() & ~(CAUSE_IP9 | CAUSE_IP10));
}


/* This handler called on TFP G-cache parity error.
 */
void
tfp_gcache_intr(eframe_t *ep, void *arg)
{
	k_machreg_t	save_cause;
	evreg_t		slotproc;
	register int	slot; 
	register int	proc; 
	cpuerror_t	*ce = &(EVERROR->cpu[cpuid()]);

	save_cause = ep->ef_cause;
	ep->ef_cause &= ~(CAUSE_IP9 | CAUSE_IP10);
	setcause(getcause() & ~(CAUSE_IP9 | CAUSE_IP10));
	ce->external_intr = (save_cause & CAUSE_IPMASK) >> CAUSE_IPSHIFT;

	slotproc = EV_GET_LOCAL(EV_SPNUM);
	slot = (slotproc & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	proc = (slotproc & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;

	cmn_err(CE_WARN|CE_CPUID,
"G-cache (%s) parity error: hardware error (CPU %d in slot %d), CAUSE %x SR %x",
		(save_cause & CAUSE_IP9) ?  "even"  : "odd",
		proc, slot, save_cause, ep->ef_sr);

#ifndef _STANDALONE
	dump_hwstate(FROM_KERNEL);
#endif

	cmn_err(CE_PANIC,
"G-cache (%s) parity error: hardware error (CPU %d in slot %d), CAUSE %x SR %x",
		(save_cause & CAUSE_IP9) ?  "even"  : "odd",
		proc, slot, save_cause, ep->ef_sr);

}
#endif /* TFP */

/*
 * This handler is called to handle all everest error interrupts.
 * It may be called by boot master to handle global error interrupts or
 * called by any cpu to handle panic exceptions and error interrupts
 * that are sent to the local CPU only.
 */
static char *everest_errintr_level[] = {
	"EPC", "DANG", "VMECC", "FCHIP", "IO4", "MC3 Memory", "MC3 EBus", "CC" 
};

void
everest_error_handler(eframe_t *ep, void *arg)
{
	evreg_t	elevel, ev_ile;
#ifndef _STANDALONE
	extern int 	putbufndx;
#endif

	elevel = EV_GET_LOCAL(EV_HPIL);

#ifndef	_STANDALONE

	switch (elevel) {

	    case EVINTR_LEVEL_EPC_ERROR:
		epc_intr_error(ep, arg);
		break;

	    case EVINTR_LEVEL_MC3_EBUS_ERROR:
		mc3_intr_ebus_error();
		break;

	    case EVINTR_LEVEL_MC3_MEM_ERROR:
		if (mc3_intr_mem_error() == 0)	/* if non-fatal memory error */
			return;			/*  then leave quietly	     */
		break;

	    /* Not used anymore .. Moved to vmecc.c * * * * * 
	    case EVINTR_LEVEL_VMECC_ERROR:
		if (ev_wbad_inprog) {
			EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_VMECC_ERROR);
			ev_wbad_val = 1;
			everest_error_clear(0);
			return;
		}
		break;
	    * * * * * * * * * * * * *  * * * * * * * * * * */

	    default:
#if TFP
		cc_error_log(); /* for future use. */
#else
		/* We could enter everest_error_handler due to USER level VME
		 * probing generating a CC Timeout. So check for that and return
		 * If it is any other kind of error, we would blow out anyway 
		 */
        	if (elevel < EVINTR_LEVEL_ERR && (ep) &&
		    (ep->ef_cause & SRB_ERR) &&  /* CC ERTOIP interrupt */
		    ((ep->ef_cause & CAUSE_EXCMASK) == EXC_INT)){
			cc_error_log(); /* for future use. */
			everror_udbe = EV_GET_REG(EV_SPNUM);
			EV_SET_REG(EV_CERTOIP, 0xffff); /* Explicit clear */
			return;  
		}
#endif
		break;

	}  /* switch */

	if (in_everest_error_handler) {
#ifdef	DEBUG
	if (kdebug)
	    debug((char *)0);
#endif
	    return;
	}
	if (everest_error_dump_done) /* If it happens to come here again !! */
	    cmn_err(CE_PANIC,
		    "Hardware Error dump already done! arg=0x%x\n",arg);

#endif
	in_everest_error_handler = 1;

	ev_ile = EV_GET_LOCAL(EV_ILE);
	EV_SET_LOCAL(EV_ILE, (ev_ile & ~EV_EBUSINT_MASK)); 

	if (elevel > EVINTR_LEVEL_ERR){
	    sprintf(error_dumpbuf, 
		"Received interrupt at level 0x%x due to %s ERROR\n", 
		elevel, everest_errintr_level[elevel - EVINTR_LEVEL_ERR]);
	    error_dumpindx = strlen(error_dumpbuf);
	}

	/* also send nmi interrupt to other cpus to log private error state */
	everest_error_log(FROM_KERNEL);
	everest_error_show();			/* To system console */
	/* everest_error_fru(); */

#ifndef _STANDALONE
	/* Set the putbufndx to 0 to ensure the hardware error state is
	 * featured prominently at the top of the putbuf.
	 */
	putbufndx = 0;
#endif

	printf(error_dumpbuf);

	EV_SET_LOCAL(EV_ILE, ev_ile); /* disable interrupts */
	everest_error_dump_done  = 1;
	/* kick other cpu to continue */
	in_everest_error_handler = 0;

#ifndef	_STANDALONE
	/* usually should panic and not return unless the error is fixed */
	cmn_err(CE_PBPANIC,"Hardware error detected, arg=0x%x", arg);
#endif

}

#ifndef	_STANDALONE
void
dump_hwstate(int from)
	/* from=1 => Kernel called this. Otherwise got called from symmon */
{
    uint	ev_ile;

    if (in_everest_error_handler)
	return;

    ev_ile = EV_GET_LOCAL(EV_ILE);
    EV_SET_LOCAL(EV_ILE, (ev_ile & ~EV_EBUSINT_MASK)); 
    in_everest_error_handler = 1;
    error_dumpindx=0;
    everest_error_log(from==FROM_KERNEL? FROM_KERNEL:FROM_SYMMON);
    everest_error_show();
    if (from == 1)
	printf(error_dumpbuf);
    else qprintf(error_dumpbuf);	/* Called from symmon */

    in_everest_error_handler = 0;
    EV_SET_LOCAL(EV_ILE, ev_ile);
}
#endif

#ifndef _STANDALONE
static 
#endif
void
adap_error_clear(int slot, int padap)
{
	int	type  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	switch (type) {
	case IO4_ADAP_FCHIP:
		fchip_error_clear(slot, padap);
		break;
	case IO4_ADAP_VMECC:
		vmecc_error_clear(slot, padap);
		fchip_error_clear(slot, padap);
		break;
	case IO4_ADAP_HIPPI:
		io4hip_error_clear(slot, padap);
		fchip_error_clear(slot, padap);
		break;
	case IO4_ADAP_EPC:
		epc_error_clear(slot, padap);
		break;
	case IO4_ADAP_FCG:
		fcg_error_clear(slot, padap);
		fchip_error_clear(slot, padap);
		break;
	case IO4_ADAP_DANG:
		dang_error_clear(slot, padap);
		break;
	/* SABLE does not support scsi */
	case IO4_ADAP_SCSI:
	case IO4_ADAP_SCIP:
#ifndef SABLE
		scsi_error_clear(slot, padap);
#endif
		break;
	case IO4_ADAP_NULL:
		break;
	default:
		bad_ioa_type(type, slot, padap);
	}
}

#ifndef _STANDALONE
static 
#endif
void
adap_error_log(int slot, int padap)
{
	int	type  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	switch (type) {
	case IO4_ADAP_FCHIP:
		fchip_error_log(slot, padap);
#ifdef	_STANDALONE
		vmecc_error_log(slot, padap);
#endif
		break;
	case IO4_ADAP_HIPPI:
		io4hip_error_log(slot, padap);
		fchip_error_log(slot, padap);
		break;
	case IO4_ADAP_VMECC:
		vmecc_error_log(slot, padap);
		fchip_error_log(slot, padap);
		break;
	case IO4_ADAP_EPC:
		epc_error_log(slot, padap);
		break;
	case IO4_ADAP_FCG:
		fcg_error_log(slot, padap);
		fchip_error_log(slot, padap);
		break;
	case IO4_ADAP_DANG:
		dang_error_log(slot, padap);
		break;
	/* SABLE does not support scsi */
	case IO4_ADAP_SCSI:
	case IO4_ADAP_SCIP:
#ifndef SABLE
		scsi_error_log(slot, padap);
#endif
		break;
	case IO4_ADAP_NULL:
		break;	  /* nothing to do*/
	default:
		bad_ioa_type(type, slot, padap);
	}
}

#ifndef _STANDALONE
static 
#endif
void
adap_error_show(int slot, int padap)
{
	int	type  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	switch (type) {
	case IO4_ADAP_FCHIP:
		fchip_error_show(slot, padap);
#ifdef	_STANDALONE
		vmecc_error_show(slot, padap);
#endif
		break;
	case IO4_ADAP_HIPPI:
		fchip_error_show(slot, padap);
		io4hip_error_show(slot, padap);
		break;
	case IO4_ADAP_VMECC:
		fchip_error_show(slot, padap);
		vmecc_error_show(slot, padap);
		break;
	case IO4_ADAP_EPC:
		epc_error_show(slot, padap);
		break;
	case IO4_ADAP_FCG:
		fchip_error_show(slot, padap);
		fcg_error_show(slot, padap);
		break;
	case IO4_ADAP_DANG:
		dang_error_log(slot, padap);
		break;
	/* SABLE does not support scsi */
	case IO4_ADAP_SCSI:
	case IO4_ADAP_SCIP:
#ifndef SABLE
		scsi_error_show(slot, padap);
#endif
		break;
	case IO4_ADAP_NULL:
		break;	  /* nothing to do*/
	default:
		bad_ioa_type(type, slot, padap);
	}
}


/*
 * About these interface functions,
 *	xxx_error_clear	- clears the h/w error registers and EVERROR.
 *	xxx_error_log	- log the h/w error register into EVERROR.
 *	xxx_error_show	- print out the EVERROR value onto console.
 */

#if IP21
/*
 * Bits 8-11 of the A Chip on IP21 are encoded to identify the 
 * DB chip and CPU chip having the error.
 */
#define	DB0_MIN	10
#define	DB0_MAX	11
#define	DB1_MIN	8
#define	DB1_MAX	9
#define	DB_ERRMASK 0x3
#define	DB0_ERROR(err)	(((err)>>DB0_MIN)&DB_ERRMASK)
#define	DB1_ERROR(err)	(((err)>>DB1_MIN)&DB_ERRMASK)

char *db0_error[] = {
	"",					/* no error */
	"",					/* not valid */
	"   11:CPU 0 DB0->D parity error",
	"10,11:CPU 1 DB0->D parity error"
};
char *db1_error[] = {
	"",					/* no error */
	"",					/* not valid */
	"    9:CPU 0 DB1->D parity error",
	"  8,9:CPU 1 DB1->D parity error"
};
#endif
char *ip_error[] = {
#if IP21
/************************************ IP21 ***********************************/
	"    0:CPU 0 CC->A Channel 0 parity error",
	"    1:CPU 0 CC->A Wback Channel parity error",
	"    2:CPU 1 CC->A Channel 0 parity error",
	"    3:CPU 1 CC->A Wback Channel parity error",
	"    4:ADDR_ERROR on EBUS",
	"    5:My ADDR_ERROR on EBUS",
	"",
	"",
	"    8:CPU 0 CC->D parity error",
	"    9:CPU 0 CC->D parity error",
	"   10:CPU 1 CC->D parity error",
	"   11:CPU 1 CC->D parity error",
	"   12:CPU 0 ADDR_HERE not asserted",
	"   13:CPU 0 ADDR_HERE not asserted",
	"   14:CPU 1 ADDR_HERE not asserted",
	"   15:CPU 1 ADDR_HERE not asserted"
#else
/************************************ IP19 ***********************************/
	" 0:CPU 0 CC->A parity error",
	" 1:CPU 1 CC->A parity error",
	" 2:CPU 2 CC->A parity error",
	" 3:CPU 3 CC->A parity error",
	" 4:ADDR_ERROR on EBUS",
	" 5:My ADDR_ERROR on EBUS",
	"",
	"",
	" 8:CPU 0 CC->D parity error",
	" 9:CPU 1 CC->D parity error",
	"10:CPU 2 CC->D parity error",
	"11:CPU 3 CC->D parity error",
	"12:CPU 0 ADDR_HERE not asserted",
	"13:CPU 1 ADDR_HERE not asserted",
	"14:CPU 2 ADDR_HERE not asserted",
	"15:CPU 3 ADDR_HERE not asserted"
#endif
};

#define IP_ERROR_MAX	16

/* Set to 1 if IP19/IP21 slot no is to be printed in cc_error_show */

#ifndef _STANDALONE
static 
#endif
void
ip_error_show(int slot)
{
	int id    = EVCFGINFO->ecfg_board[slot].eb_cpu.eb_cpunum;
	int error = EVERROR->ip[id].a_error;
	int i;

	if (error == 0)
		return;

	ev_perr(SLOT_LVL, "Cpu Board in slot %d \n", slot);
	ip_slot_print = 0;
	ev_perr(REG_LVL, "A Chip Error Register: 0x%x\n", error);
	for (i = 0; i < IP_ERROR_MAX; i++) {
#ifdef IP21
		if (i >= DB0_MIN && i <= DB0_MAX) {
			if (DB0_ERROR(error))
				ev_perr(BIT_LVL, "%s\n",
						db0_error[DB0_ERROR(error)]);
		} else if (i >= DB1_MIN && i <= DB1_MAX) {
			if (DB1_ERROR(error))
				ev_perr(BIT_LVL, "%s\n",
						db1_error[DB1_ERROR(error)]);
		} else
#endif
		if (error & (1L << i))
#ifdef	CHIPREV_BUG
			if (A_ERROR_ADDR_ERR & (1L << i)) 
			    ev_perr(BIT_LVL,"%s %s\n",ip_error[i],IAREV1_MSG);
			else 
#endif
			    ev_perr(BIT_LVL, "%s\n",ip_error[i]);
	}
}

#ifndef _STANDALONE
static 
#endif
void
ip_error_clear(int slot)
{
	int id = EVCFGINFO->ecfg_board[slot].eb_cpu.eb_cpunum;

	EVERROR->ip[id].a_error = 0;
	EV_GETCONFIG_REG(slot, 0, EV_A_ERROR_CLEAR);
}


#ifndef _STANDALONE
static 
#endif
void
ip_error_log(int slot)
{
	int id = EVCFGINFO->ecfg_board[slot].eb_cpu.eb_cpunum;

	if (EVERROR->ip[id].a_error)
		return;

	EVERROR->ip[id].a_error = EV_GETCONFIG_REG(slot, 0, EV_A_ERROR);
}


/******************************** CC *****************************************/

#if IP21
char *dchip_error[] = {
	"0:D Chip 0 Data Receive Error",		/* 0 */
	"1:D Chip 1 Data Receive Error",		/* 1 */
	"2:D Chip 2 Data Receive Error",		/* 2 */
	"3:D Chip 3 Data Receive Error",		/* 3 */
};
#endif

char *cc_error[] = {
#if IP21
	" 0:DB chip Parity error DB0",                   /* 0 */
	" 1:DB chip Parity error DB1",                   /* 1 */
	" 2:A Chip Addr Parity",                         /* 2 */
	" 3:Time Out on MyReq on EBus Channel 0",        /* 3 */
	" 4:Time Out on MyReq on EBus Wback Channel ",   /* 4 */
	" 5:Addr Error on MyReq on EBus Channel 0",      /* 5 */
	" 6:Addr Error on MyReq on EBus Wback Channel ", /* 6 */
	" 7:Data Sent Error Channel 0",                  /* 7 */
	" 8:Data Sent Error Wback Channel ",             /* 8 */
	" 9:Data Receive Error",                         /* 9 */
	"10:Intern Bus vs. A_SYNC",                      /* 10 */
	"11:A Chip MyResponse Data Resources Time Out",  /* 11 */
	"12:A Chip MyIntrvention Data Resources Time Out",/* 12 */
	"13:Parity Error on DB - CC shift lines",        /* 13 */
#else
	" 0:ECC uncorrectable (multi bit) error in Scache",
	" 1:ECC correctable (single bit) error in Scache",
	" 2:Parity Error on TAG RAM Data",
	" 3:Parity Error on Address from A-chip",
	" 4:Parity Error on Data from D-chip",
	" 5:MyRequest TimeOut on EBUS",
	" 6:MyResponse D-Resource TimeOut in A chip",
	" 7:MyIntervention Response D-Resource TimeOut in A chip",
	" 8:Address Error on MyRequest on EBUS",
	" 9:Data Error on MyData on EBUS",
	"10:Internal Bus State is out of sync with A_SYNC"
#endif
};

#if IP21
#define CC_ERROR_MAX	14
#else
#define CC_ERROR_MAX	11
#endif


/*
 * The size of cache for each R4000 is in mega bytes.
 * We will only attempt to dump the cache during bring up stage.
 * The space allocated will be returned to the pool
 */
void
dump_local_cache(cachedump_t *cd)
{
	if (cd == 0)		/* is there space reserved?  NO..*/
		return;

	/* do dump */
}

void
cc_sbe_handler(void)
{
	/*
	 * TBD, there is state as to the address/syndrome of the error.
	 * All we know is the error was corrected.
	 * We will need to scrub the scache to cause a cache exception
	 * in order to identify the error and fix it.
	 */
}

#ifdef	CHIPREV_BUG
#define	AREV3_ERRBITS   (CC_ERROR_MYRES_TIMEOUT|CC_ERROR_MYINT_TIMEOUT)
#define	AREV2_ERRBITS   (CC_ERROR_MYREQ_TIMEOUT|AREV3_ERRBITS)
#endif

void
cc_error_show(int slot, int pcpuid)
{
	int  vcpuid = EVCFGINFO->ecfg_board[slot].eb_cpuarr[pcpuid].cpu_vpid;
	uint error  = EVERROR->cpu[vcpuid].cc_ertoip;
	int i;
#ifdef	CHIPREV_BUG
	int alevel;
#endif

	if (error) {
		if (ip_slot_print){
		    ip_slot_print=0;
		    ev_perr(SLOT_LVL, "Cpu Board in slot %d \n", slot);
		}
#ifdef	CHIPREV_BUG
		alevel = EV_GETCONFIG_REG(slot, 0, EV_A_LEVEL);
#endif
#if IP21 && _KERNEL && !_STANDALONE
		/*
		 * Print board rev number if non-zero so we can tell if
		 * hardware has D-chip error reporting fix.
		 */
		if (pdaindr[vcpuid].pda->p_dchip_err_hw)
		    ev_perr(ASIC_LVL,"CC in Cpu Slot %d, cpu %d (rev %d)\n",
			slot, pcpuid, pdaindr[vcpuid].pda->p_dchip_err_hw);
		else
#endif
		    ev_perr(ASIC_LVL,"CC in Cpu Slot %d, cpu %d\n",slot,pcpuid);

		ev_perr(REG_LVL, "CC ERTOIP Register: 0x%x \n", error);
		for (i = 0; i < CC_ERROR_MAX; i++) {
			if (error & (1L << i)){
#ifdef	CHIPREV_BUG
				if ((alevel==2)&&(AREV2_ERRBITS & (1 << i)))
				    ev_perr(BIT_LVL, "%s %s\n", cc_error[i], 
				    		AREV2_MSG(alevel));
				else if((alevel==3)&&(AREV3_ERRBITS & (1<<i)))
				    ev_perr(BIT_LVL, "%s %s\n", cc_error[i], 
				    		AREV3_MSG(alevel));
				else
#endif
				    ev_perr(BIT_LVL, "%s\n",cc_error[i]);
			}
		}
	}

#if IP21 && _KERNEL && !_STANDALONE
	if (pdaindr[vcpuid].pda->p_dchip_err_hw) {
		unchar derr;

		derr = pdaindr[vcpuid].pda->p_dchip_err_bits;
		if (derr) {
			ev_perr(REG_LVL, "D Chip Error Register: 0x%x\n", derr);
			for (i = 0; i < 4; i++)
				if (derr & (1L << i))
					ev_perr(BIT_LVL,"%s\n",dchip_error[i]);
		}
	}
#endif /* IP21 && _KERNEL */

}

/*
 * cpuid gets its value from the PDAPAGE to be setup by the kernel
 * In case of standalone, we call the function defined in libsk/ml/EVEREST.c
 */
#ifdef	_STANDALONE

#ifdef	cpuid
#undef	cpuid
extern 	int 	cpuid();
#endif
#endif	/*_STANDALONE*/
int
cc_error_log(void)
{
	cpuerror_t *ce = &(EVERROR->cpu[cpuid()]);

	/* If this CPU had a user level DBE earlier, dont read again. */
	if (everror_udbe == EV_GET_LOCAL(EV_SPNUM))
	    return(1);

	ce->cc_ertoip = EV_GET_REG(EV_ERTOIP);
#if R4000
	if (ce->cc_ertoip & IP19_CC_ERROR_SCACHE_SBE) {
		cc_sbe_handler();
		EV_SET_REG(EV_CERTOIP, IP19_CC_ERROR_SCACHE_SBE);

		if (ce->cc_ertoip == IP19_CC_ERROR_SCACHE_SBE) {
			cc_error_clear(0);
			return 0;
		}

		ce->cc_ertoip &= ~IP19_CC_ERROR_SCACHE_SBE;
	}
#endif

#if IP21 && _KERNEL
	if (private.p_dchip_err_hw) {
		volatile evreg_t derr;

		derr = *(volatile evreg_t *)EV_D_ERROR;
		private.p_dchip_err_bits = derr & EV_DERR_MASK;
	}
#endif /* IP21 && _KERNEL */

	/* Call dump_local_caches with ce->cachedump as parameter */

	return 1;
}

void
cc_error_clear(int mode)	/* 0 == clear local CC, 1 == clear all CC's */
{
	int	i;

	if (mode == 0) {
		EVERROR->cpu[cpuid()].cc_ertoip = 0;
#if IP21 && _KERNEL
		private.p_dchip_err_bits = 0;
#endif /* IP21 && _KERNEL */
	}
	else {
		/* Since this routine gets called early, dont use cpuid */
		for (i=0; i < EV_MAX_CPUS; i++)
			EVERROR->cpu[i].cc_ertoip = 0;
	}

	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_CC_ERROR);
	EV_SET_REG(EV_CERTOIP, 0xFFFF);			/* clear all bits */
#if IP21
	*(volatile evreg_t *)EV_D_ERROR = 0;
#endif /* IP21 */
	everror_udbe = 0;

}

void
cc_error_intr(eframe_t *ep, void *arg)
{
	if (cc_error_log() == 0)
		return;			/* do nothing if sbe only */

	everest_error_handler(ep, arg);
}

/*********************************** MC3 *************************************/

#ifndef _STANDALONE
/*
 *  This timeout handler gets entered a short while (MC3_SBE_MAX_INTR_RATE)
 *  after a singlebit error interrupt, in order to throttle the interrupt
 *  rate.  What we do here is to reenable MC3 SBE interrupts by doing a
 *  Read-Clear of the per-leaf error status registers.
 */
static void
mc3_enable_memerr_intr(unchar *slot)
{
	uint  error;

	/*
	 *  Clear the error register iff an SBE was pending
	 */
	error = MC3_GETLEAFREG(*slot, 0, MC3LF_ERROR);
	if (error  &&  (error & MC3_MEM_ERROR_MBE) == 0)
		MC3_GETLEAFREG_NOWAR(*slot, 0, MC3LF_ERRORCLR);

	error = MC3_GETLEAFREG(*slot, 1, MC3LF_ERROR);
	if (error  &&  (error & MC3_MEM_ERROR_MBE) == 0)
		MC3_GETLEAFREG_NOWAR(*slot, 1, MC3LF_ERRORCLR);
}


/*
 * This timeout handler gets entered when the mc3 logger determines that an
 * excessive number of singlebit errors have been seen, and that a warning
 * message should be displayed.
 */
static void
mc3_log_excessive(evbnkcfg_t *bnk)
{
	int bank_num;

	bank_num = bnk - &(EVCFGINFO->ecfg_board[bnk->bnk_slot].eb_banks[0]);

	cmn_err(CE_WARN,
		"MC3 Slot:%d Bank:%d has excessive Single Bit Errors",
		bnk->bnk_slot, bank_num);
}
#endif

static void
mc3_sbe_analyze(int slot, int mbid, int *bank_num, int *simm_num)
{
	mc3error_t  *mc3err = &(EVERROR->mc3[mbid]);
	evbrdinfo_t *board  = &(EVCFGINFO->ecfg_board[slot]);
	int	    bank, bank_base;
	uint	    physaddr;

	/* Establish "not found" return values */
	*bank_num = MC3_NUM_BANKS;
	*simm_num = MC3_SIMMS_PER_BANK;

	if (mc3err->mem_error[0]) {
		bank_base = 0;
		physaddr = ((mc3err->erraddrhi[0] & 0xFF) << 24) | 
			   (mc3err->erraddrlo[0] >> 8);
		if (mc3err->syndrome0[0])
			*simm_num = 0;
		else
		if (mc3err->syndrome1[0])
			*simm_num = 1;
		else
		if (mc3err->syndrome2[0])
			*simm_num = 2;
		else
		if (mc3err->syndrome3[0])
			*simm_num = 3;
	} else
	if (mc3err->mem_error[1]) {
		bank_base = 4;
		physaddr = ((mc3err->erraddrhi[1] & 0xFF) << 24) | 
			   (mc3err->erraddrlo[1] >> 8);
		if (mc3err->syndrome0[1])
			*simm_num = 0;
		else
		if (mc3err->syndrome1[1])
			*simm_num = 1;
		else
		if (mc3err->syndrome2[1])
			*simm_num = 2;
		else
		if (mc3err->syndrome3[1])
			*simm_num = 3;
	} else
		return;

#ifndef _STANDALONE
	for (bank = bank_base + 3; bank >= bank_base; bank--) {
		evbnkcfg_t *bnk = &(board->eb_mem.ebun_banks[bank]);

		if (bnk->bnk_size == MC3_NOBANK)
			continue;

		if (physaddr >= bnk->bnk_bloc   &&
		    physaddr <  (bnk->bnk_bloc + 
				   ((mc3_type_bank_size[bnk->bnk_size] 
					* bnk->bnk_count) >> 8))) {
			*bank_num = bank;
			return;
		}
	}
#endif
}

#ifndef _STANDALONE
static 
#endif
void
mc3_error_show(int slot)
{
	int	mbid = EVCFGINFO->ecfg_board[slot].eb_mem.eb_mc3num;
	mc3error_t	*mc3err = &(EVERROR->mc3[mbid]);
	uint	error, i;
	int	bank_num, simm_num;
	caddr_t vaddr;

	if ((mc3err->ebus_error == 0) && (mc3err->mem_error[0] == 0) &&
	    (mc3err->mem_error[1] ==0))
	    return;

	ev_perr(SLOT_LVL, "MC3 in slot %d \n", slot);

	error = mc3err->ebus_error;

	if (error){
	    ev_perr(REG_LVL, "MA EBus Error register: 0x%x\n", error);
	    if (error & MC3_EBUS_ERROR_DATA)
		ev_perr(BIT_LVL, "0: EBus Data Error\n");
	    if (error & MC3_EBUS_ERROR_ADDR)
#ifdef	CHIPREV_BUG
		ev_perr(BIT_LVL, "1: EBus Address Error %s\n", IAREV1_MSG);
#else
		ev_perr(BIT_LVL, "1: EBus Address Error\n");
#endif
	    if (error & MC3_EBUS_ERROR_SENDER_DATA)
		ev_perr(BIT_LVL, "2: My EBus Data Error\n");
	    if (error & MC3_EBUS_ERROR_SENDER_ADDR)
		ev_perr(BIT_LVL, "3: My EBus Address Error\n");
	}

	/* Dump memory leaves information */
	for (i=0; i < MC3_NUM_LEAVES; i++){
	    if ((error = mc3err->mem_error[i]) == 0)
		continue;

	    ev_perr(REG_LVL,"MA Leaf %d Error Status Register: 0x%x\n",i,error);

	    if (error & MC3_MEM_ERROR_PWRT_MBE) {
		mc3_sbe_analyze(slot,mbid,&bank_num,&simm_num);
		ev_perr(BIT_LVL, 
			"0: PartialWrite Uncorrectable (Multiple Bit) Error (Bank %d)\n",
			bank_num);
	    }
	    if (error & MC3_MEM_ERROR_READ_MBE)
		ev_perr(BIT_LVL,"1: Read Uncorrectable (Multiple Bit) Error\n");

	    if (error & MC3_MEM_ERROR_READ_SBE) {
		mc3_sbe_analyze(slot,mbid,&bank_num,&simm_num);
		ev_perr(BIT_LVL,
			"2: Read correctable (SINGLE BIT) Error\n");
	    }

	    if (error & MC3_MEM_ERROR_MULT)
		ev_perr(BIT_LVL, "3: Multiple occurence of Read correctable (SINGLE BIT) Error\n");

	    if (error && (mc3err->erraddrhi[i] || mc3err->erraddrlo[i])) {
		ev_perr(REG_LVL, "MA Leaf %d Bad Memory Address: 0x%llx\n", i,
			make_ll(mc3err->erraddrhi[i], mc3err->erraddrlo[i]));
		mc3_decode_addr((void (*)(char *,...))ev_perr, mc3err->erraddrhi[i],
				 mc3err->erraddrlo[i]);
	    }

            if (mc3err->syndrome0[i])
                ev_perr(REG_LVL,"MA Leaf %d Memory Syndrome Register 0: 0x%x\n",
                                i, mc3err->syndrome0[i]);
            if (mc3err->syndrome1[i])
                ev_perr(REG_LVL,"MA Leaf %d Memory Syndrome Register 1: 0x%x\n",
                                i, mc3err->syndrome1[i]);
            if (mc3err->syndrome2[i])
                ev_perr(REG_LVL,"MA Leaf %d Memory Syndrome Register 2: 0x%x\n",
                                i, mc3err->syndrome2[i]);
            if (mc3err->syndrome3[i])
                ev_perr(REG_LVL,"MA Leaf %d Memory Syndrome Register 3: 0x%x\n",
                                i, mc3err->syndrome3[i]);
        }
}
	
	
#ifndef _STANDALONE
static 
#endif
void
mc3_ebus_error_log(int slot)
{
	int	   mbid = EVCFGINFO->ecfg_board[slot].eb_mem.eb_mc3num;
	mc3error_t *me  = &(EVERROR->mc3[mbid]);

	if (me->ebus_error)
		return;

	me->ebus_error = MC3_GETREG(slot, MC3_EBUSERROR);
}

/*
 *  Return the MA revision on the MC3 board
 */
static int
mc3_ma_revision(int slot)
{
	uint mc3_revision, bist_result;

	mc3_revision = MC3_GETREG(slot, MC3_REVLEVEL);
	bist_result  = MC3_GETREG(slot, MC3_BISTRESULT);

	switch (mc3_revision) {

	    case 0:
		return(1);	/* MA1 */

	    case 1:
#ifdef WAR
		if ((bist_result & MC3_BIST_MA_REV) == MC3_BIST_MA_REV)
#endif
			return(3);
#ifdef WAR
		else
			return(2);
#endif

	    default:
		return(mc3_revision+1);		/* take a guess */
	}
}


/*
 *  For a specific slot and leaf, return 1 if a single bank is enabled,
 *  or 0 if multiple banks are enabled.
 */
static int
mc3_single_bank_enabled(int slot, int leaf)
{
	uint bank_enable;

	bank_enable = MC3_GETREG(slot, MC3_BANKENB);

	switch (bank_enable & ( (leaf == 0) ? MC3_BENB_LF0 : MC3_BENB_LF1) ) {
	    case MC3_BENB(0,0):
	    case MC3_BENB(0,1):
	    case MC3_BENB(0,2):
	    case MC3_BENB(0,3):
	    case MC3_BENB(1,0):
	    case MC3_BENB(1,1):
	    case MC3_BENB(1,2):
	    case MC3_BENB(1,3):	return(1);

	    default:		return(0);
	} /* switch */
}


/*
 * There is an mc3 bug doing singlebit ECC correction on multiple-bank leaves 
 * with MA rev0 or rev1:  the ECC error might be "scrubbed" by writing back to 
 * the wrong bank.  A board-level workaround is to disable scrubbing and
 * reporting of soft errors.  If we do see a singlebit error interrupt, and if 
 * this looks like an old mc3 with multiple banks, then we want to return to 
 * the interrupt handler, which will summarily panic.
 */
#define MC3_SINGLEBIT_FATAL(_slot)	\
	mc3_ma_revision(_slot) < 3 &&	\
	  (   (me->mem_error[0] && !mc3_single_bank_enabled(_slot,0)) \
	   || (me->mem_error[1] && !mc3_single_bank_enabled(_slot,1)) )

#if _STANDALONE
int ok_to_read_syndrome = 1;
static int panic_on_sbe = 1;	/* XXX standalone panics, until MC3 SBE fix */
#else
int ok_to_read_syndrome = 0;
extern int panic_on_sbe;
#endif

#ifndef _STANDALONE
static 
#endif
int
mc3_mem_error_log(int slot)
{
	int	   mbid = EVCFGINFO->ecfg_board[slot].eb_mem.eb_mc3num;
	mc3error_t *me  = &(EVERROR->mc3[mbid]);
	int	   bank_num, simm_num;
#ifndef _STANDALONE
	mc3_bank_err_t  *bank_info;
#endif

	/*
	 *  Exit now if we haven't dealt with the previous error
	 */
	if (me->mem_error[0] ||
	    me->mem_error[1])
		return 0;

#ifndef _STANDALONE
	last_mc3error = me;
#endif

	me->mem_error[0] = MC3_GETLEAFREG(slot,0,MC3LF_ERROR);
	me->mem_error[1] = MC3_GETLEAFREG(slot,1,MC3LF_ERROR);

	/*
	 *  Exit now if the mc3 has no memory of an error
	 */
	if (me->mem_error[0] == 0 &&
	    me->mem_error[1] == 0)
		return 0;

	/*
	 *  Retrieve error state information from the mc3
	 */
	me->erraddrhi[0] = MC3_GETLEAFREG(slot,0,MC3LF_ERRADDRHI);
	me->erraddrlo[0] = MC3_GETLEAFREG(slot,0,MC3LF_ERRADDRLO);

	if (panic_on_sbe)
		ok_to_read_syndrome = 1;

	if (ok_to_read_syndrome) {
		me->syndrome0[0] = MC3_GETLEAFREG(slot,0,MC3LF_SYNDROME0);
		me->syndrome1[0] = MC3_GETLEAFREG(slot,0,MC3LF_SYNDROME1);
		me->syndrome2[0] = MC3_GETLEAFREG(slot,0,MC3LF_SYNDROME2);
		me->syndrome3[0] = MC3_GETLEAFREG(slot,0,MC3LF_SYNDROME3);
	} else {
		/* An MA3/MC3 bug corrupts data if we read syndrome bits on a running system */
		me->syndrome0[0] = 0;
		me->syndrome1[0] = 0;
		me->syndrome2[0] = 0;
		me->syndrome3[0] = 0;
	}

	me->erraddrhi[1] = MC3_GETLEAFREG(slot,1,MC3LF_ERRADDRHI);
	me->erraddrlo[1] = MC3_GETLEAFREG(slot,1,MC3LF_ERRADDRLO);
	if (ok_to_read_syndrome) {
		me->syndrome0[1] = MC3_GETLEAFREG(slot,1,MC3LF_SYNDROME0);
		me->syndrome1[1] = MC3_GETLEAFREG(slot,1,MC3LF_SYNDROME1);
		me->syndrome2[1] = MC3_GETLEAFREG(slot,1,MC3LF_SYNDROME2);
		me->syndrome3[1] = MC3_GETLEAFREG(slot,1,MC3LF_SYNDROME3);
	} else {
		/* An MA3/MC3 bug corrupts data if we read syndrome bits on a running system */
		me->syndrome0[1] = 0;
		me->syndrome1[1] = 0;
		me->syndrome2[1] = 0;
		me->syndrome3[1] = 0;
	}

			
	if ( (!panic_on_sbe) && 
	     (me->mem_error[0] & MC3_MEM_ERROR_MBE) == 0 &&
	     (me->mem_error[1] & MC3_MEM_ERROR_MBE) == 0 ) {
		/* The only errors were sbe */
		mc3_sbe_analyze(slot,mbid,&bank_num,&simm_num);

#ifndef _STANDALONE

		/*
		 *  Analyze the error info into bank and simm,
		 *  and update the error counts.
		 */
		if (bank_num < 0  ||  bank_num >= MC3_NUM_BANKS) {
		    /* couldn't decipher to which bank -- keep count */
		    mc3_errcount[mbid].m_unk_bank_errcount++;
		} else {
		    bank_info = &(mc3_errcount[mbid].m_bank_errinfo[bank_num]);

		    bank_info->m_bank_errcount++;

		    if (simm_num >= 0  && simm_num < MC3_SIMMS_PER_BANK)
			    bank_info->m_simm_errcount[simm_num]++;

		    if (bank_info->m_first_err_time == 0)
			bank_info->m_first_err_time = time; 

		    if (bank_info->m_bank_errcount > 1  &&
		        ((time - bank_info->m_first_err_time)
					/ bank_info->m_bank_errcount
		    	 ) < MC3_SBE_EXCESSIVE_RATE 	&&
		        (time - bank_info->m_last_log_time) >= SECDAY ) {
			/*
			 * The error rate is building fast, so we
			 * conclude there is broken hardware.  Declare
			 * a warning the first time we detect there is
			 * a problem, and daily thereafter.
			 */
			timeout(mc3_log_excessive,
				&EVCFGINFO->ecfg_board[slot].eb_banks[bank_num],
				TIMEPOKE_NOW);
			bank_info->m_last_log_time = time;
		    }
		    bank_info->m_last_err_time = time;
		}
		/*
		 *  Disable memory error interrupts from this mc3 for awhile,
		 *  to avoid overloading the CPU with interrupts for the same
		 *  broken bank.  Interrupts are disabled until we do a
		 *  Read-Clear on the status register.
		 */
		timeout(mc3_enable_memerr_intr, 
			&EVCFGINFO->ecfg_board[slot].eb_slot, 
			MC3_SBE_MAX_INTR_RATE);

		/*
		 *  There is an mc3 bug doing singlebit ECC correction.
		 *  See the comment for MC3_SINGLEBIT_FATAL for details.
		 */
		if (MC3_SINGLEBIT_FATAL(slot)) {
			cmn_err(CE_WARN,
				"MC3 (slot %d) has MA2 and is missing rework.\n   Either bring uplevel, or depopulate to one bank per leaf.\n",
				slot);
			return 1;   /* if interrupt, just return and panic */
		} else if (panic_on_sbe) {
			return 1;
		} else {
			me->mem_error[0] = 0;
			me->mem_error[1] = 0;
			EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_MC3_MEM_ERROR);
			return 0;
		}
#endif
	} else
		return 1;
}


void
mc3_error_clear(int slot)
{
	int	   mbid = EVCFGINFO->ecfg_board[slot].eb_mem.eb_mc3num;
	mc3error_t *me  = &(EVERROR->mc3[mbid]);

	me->ebus_error   = 0;
	me->mem_error[0] = 0;
	me->mem_error[1] = 0;

	MC3_GETLEAFREG_NOWAR(slot, 0, MC3LF_ERRORCLR);
	MC3_GETLEAFREG_NOWAR(slot, 1, MC3LF_ERRORCLR);
	MC3_GETREG(slot, MC3_EBUSERROR); /* Clear on Read */

	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_MC3_MEM_ERROR);
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_MC3_EBUS_ERROR);
}


/*
 *  Scan all the MC3 slots for errors.  If any MC3 reports a fatal error,
 *  then return nonzero, else return zero.
 */
int
mc3_intr_mem_error(void)
{
        int slot;
        int ret = 0;

        for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
                evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);

                if (eb->eb_type == EVTYPE_MC3  &&
                    mc3_mem_error_log(slot)) {
                        ret++;
		}
        }

        return(ret);
}


void
mc3_intr_ebus_error(void)
{
        cmn_err(CE_NOTE, "MC3 Ebus Error Interrupt\n");
}



/***************************************** IO4 *******************************/

char *io4_ibuserror[] = {
	" 0: Sticky Error",
	" 1: First Level Map Error for 2-Level Mapping",
	" 2: 2-Level Address Map Response Command Error",
	" 3: 1-Level Map Data Error",
	" 4: 1-Level Address MapResponse Command Error",
	" 5: IA Response Data Bad On IBUS",
	" 6: DMA Read Response Command Error",
	" 7: GFX Write Command Error",
	" 8: PIO Read Command Error",
	" 9: PIO Write Data Bad",
	"10: PIO write Command Error",
	"11: PIO ReadResponse Data Error",
	"12: DMA Write Data Error (Data From IOA)",
	"13: DMA Write Command Error from IOA",
	"14: PIO Read Response Command Error from IOA",
	"15: Command Error on OP from IOA",
};

#define IO4_IBUSERROR_MAX	16


char *io4_ebuserror[] = {
	" 0: Sticky Error",
	" 1: My DATA_ERROR Received",
	" 2: ADDR_ERROR Detected",
	" 3: Non Existent IOA",
	" 4: Illegal PIO",
	" 5: ADDR_HERE not asserted or My ADDR_ERROR Received",
	" 6: EBUS_TIMEOUT Received",
	" 7: Invalidate Dirty Exclusive Cache Line",
	" 8: Read Resource Time Out",
	" 9: DATA_ERROR Received"
};

#define IO4_EBUSERROR_MAX	10

char *io4_ebuserror_op[] = {
	"51: Write Back",
	"52: DMA Write",
	"53: Read Exclusive",
	"54: DMA Read",
	"55: Address Map Match",
	"56: Interrupt from IA"
};

/*
#define IO4_EBUSERROROP_MIN	51
#define IO4_EBUSERROROP_MAX	57
*/
#define	IO4_EBUSOUTG_CMD	8
#define	IO4_EBUS_CMD_SIZE	8
#define	IO4_ORIGINATING_IOA	16
#define IO4_EBUSERROROP_MIN	19
#define IO4_EBUSERROROP_MAX	25

void
io4_error_show(int slot)
{
	int	   window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	io4error_t *ie = &(EVERROR->io4[window]);
	int 	i, error;

	if ((ie->ibus_error== 0) && (ie->ebus_error == 0))
	    return;

	ev_perr(SLOT_LVL, "IO4 board in slot %d \n",slot);
	if (ie->ibus_error) {
	    ev_perr(REG_LVL,"IA IBUS Error Register: 0x%x\n", ie->ibus_error);
		for (i = 0; i < IO4_IBUSERROR_MAX; i++) {
			if (ie->ibus_error & (1L << i))
				ev_perr(BIT_LVL, "%s\n",io4_ibuserror[i]);
		}
	    if (i = ((ie->ibus_error >> IO4_IBUSERROR_MAX) & 0x7)){
		error = EVCFGINFO->ecfg_board[slot].eb_ioarr[i].ioa_type;
		ev_perr(BIT_LVL, "18..16: IOA number of Transaction: %d (%s)\n",
			i, ioa_name(error));
	    }
	}

	if (ie->ebus_error == 0)
	    return;

	ev_perr(REG_LVL,"IA EBUS Error Register: 0x%x\n", ie->ebus_error);
	for (i = 0; i < IO4_EBUSERROR_MAX; i++) {
		if (ie->ebus_error & (1L << i))
#ifdef	CHIPREV_BUG
			if (IO4_EBUSERROR_ADDR_ERR & (1L << i))
			    ev_perr(BIT_LVL,"%s %s\n", io4_ebuserror[i],IAREV1_MSG);
			else
#endif
			    ev_perr(BIT_LVL,"%s\n", io4_ebuserror[i]);
	}

	for (i=1, error = (ie->ebus_error >> IO4_EBUSERROR_PIOQFULL_SHIFT);
	     i < IO4_MAX_PADAPS; i++)
	     if (error & (1 << i))
		 ev_perr(BIT_LVL,"%d: PIO Queue full for Adapter %d\n", 
		     (IO4_EBUSERROR_PIOQFULL_SHIFT + i), i);

	/* Note: Ebus physical address is in two IO4 IA config registers */
	/* Print the Ebus Address register only if proper bits are set in 
	 * Ebus error register !!
	 */
	if (!(ie->ebus_error & IO4_EBUSERROR_MY_ADDR_ERR))
	    return;

	if(ie->ebus_error2 || ie->ebus_error1)
	        ev_perr(REG_LVL,"IA Error EBus Address: 0x%llx\n",
	          make_ll((ie->ebus_error2&0xff), ie->ebus_error1)); 

	if (i = ((ie->ebus_error2 >> IO4_EBUSOUTG_CMD) & 0x7f))
	        ev_perr(BIT_LVL,"%d..%d: EBus Outgoing Command: 0x%x\n", 
			(32+IO4_EBUSOUTG_CMD+IO4_EBUS_CMD_SIZE-1), 
			 (32+IO4_EBUSOUTG_CMD), i);

	if (i = ((ie->ebus_error2 >> IO4_ORIGINATING_IOA) & 0x7))
	    ev_perr(BIT_LVL, "%d: Originating IOA number = %d\n", 
			(32+IO4_ORIGINATING_IOA), i);
	
	for (i = 0; i <= (IO4_EBUSERROROP_MAX-IO4_EBUSERROROP_MIN); i++) {
		if (ie->ebus_error2 & (1 << (i + IO4_EBUSERROROP_MIN)))
			ev_perr(BIT_LVL,"%s\n",io4_ebuserror_op[i]);
	}

}

void
io4_error_log(int slot)
{
	int	   window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	io4error_t *ie = &(EVERROR->io4[window]);

	if (ie->ibus_error || ie->ebus_error)
		return;

	/* IA Chip Rev bits are 7..4 */
	IA_chiprev = ((unsigned)IO4_GETCONF_REG(slot, IO4_CONF_SW) >> 4) & 0xf;
	ie->ibus_error  = IO4_GETCONF_REG(slot, IO4_CONF_IBUSERROR);
	ie->ebus_error  = IO4_GETCONF_REG(slot, IO4_CONF_EBUSERROR);
	ie->ebus_error1 = IO4_GETCONF_REG(slot, IO4_CONF_EBUSERROR1);
	ie->ebus_error2 = IO4_GETCONF_REG(slot, IO4_CONF_EBUSERROR2);

}
			
void
io4_error_clear(int slot)
{
	int	   window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	io4error_t *ie = &(EVERROR->io4[window]);

	ie->ibus_error = 0;
	ie->ebus_error = 0;
	ie->ebus_error1 = 0;
	ie->ebus_error2 = 0;

	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_IO4_ERROR);

	IO4_GETCONF_REG_NOWAR(slot, IO4_CONF_IBUSERRORCLR);
	IO4_GETCONF_REG_NOWAR(slot, IO4_CONF_EBUSERRORCLR);
}


/**************************************** EPC ********************************/

static char *epc_ierr_ia[] = {
	"no error",
	"PIO Write Request Data",
	"DMA Read Response Data",
	"Unexpected DMA Read Response Data or Unexpected IGNT",

	"Undetermined Command",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",
};

static char *epc_ierr_np[] = {
	"no error",
	"PIO Write Request Data Error",
	"GFX Write Request Data Error",
	"DMA Read Response Data Error",

	"Address Map Response Data Error",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",
};

static char *epc_ierr_epc[] = {
	"no error",
	"PIO (to EPC) Read Response Command Error",
	"PIO (to EPC) Read Response Data Error",
	"DMA (Enet) Read Request Command Error",

	"DMA (Enet) Write Request Command Error",
	"DMA (Enet) Write Request Data Error",
	"Interrupt Request Command Error",
	"? not used ?",

	"? not used ?",
	"PIO (thru EPC) Read Response Command Error",
	"PIO (thru EPC) Read Response Data Error",
	"DMA (PPort) Read Request Command Error",

	"DMA (Pport) Write Request Command Error",
	"DMA (Pport) Write Request Data Error",
	"? not used ?",
	"? not used ?",
};

static char *epc_ierr[] = {
	"16: Parity Error on IBusAD[15:0]",
	"17: Parity Error on IBusAD[31:16]",
	"18: Parity Error on IBusAD[47:32]",
	"19: Parity Error on IBusAD[63:48]",
	"20: Error Overrun",
	"21: DMA Read Response 1 msec Timeout Error",
	"22: EPC in PIO Drop Mode: Detected a PIO Write Data Error"
};
#define EPC_IERR_MIN	16
#define EPC_IERR_MAX	22
	
static void
epc_error_show(int slot, int padap)
{
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	x, i;
	epcerror_t *epce  = (epcerror_t*)&(EVERROR->epc[vadap]);

 	if ((epce->ibus_error == 0) && (epce->ia_np_error2 == 0))
	    return;

	ev_perr(ASIC_LVL,"EPC in IO4 slot %d adapter %d\n", slot, padap);

	if (epce->ibus_error == 0)
	    goto check_epc_np;

	ev_perr(REG_LVL,"Ibus Error Register: 0x%x\n", epce->ibus_error);

	x = EPC_IERR_IA(epce->ibus_error);
	if (x)
	    ev_perr(BIT_LVL,"  3..0: EPC Detected- %s Error from IA\n",
			epc_ierr_ia[x]);

	x = EPC_IERR_NP(epce->ibus_error);
	i = EPC_IERR_NP_IOA(epce->ibus_error);
	if (x)
	    ev_perr(BIT_LVL," 11..4: EPC Observed- %s from IA to IOA %d\n",
			epc_ierr_np[x], i);
#ifdef	NOTNEEDED		
	x = EPC_IERR_NP_IOA(epce->ibus_error);
	if (x > 0 && x < 8)
	    ev_perr(BIT_LVL," 11..8: Bad IOA Destination: %d\n",x);
	else if (x == 8)
	    ev_perr(BIT_LVL," 11..8: Bad IOA unknown due to Command Error\n");
	else
	    ev_perr(BIT_LVL," 11..8: EPC reports Invalid IOA number: %x\n", x);
#endif
	x = EPC_IERR_EPC(epce->ibus_error);
	if (x) {
	    ev_perr(BIT_LVL,"15..12: EPC Sent- %s to IA\n",
			epc_ierr_epc[x]);
	}

	for (i = EPC_IERR_MIN; i < EPC_IERR_MAX; i++) {
		if (epce->ibus_error & (1L << i))
			ev_perr(BIT_LVL,"%s\n", epc_ierr[i - EPC_IERR_MIN]);
	}
check_epc_np:
 	if (epce->ia_np_error2)
	    ev_perr(REG_LVL,"IBus Opcode+Address: 0x%llx\n",
		make_ll(epce->ia_np_error1, epce->ia_np_error2));
}

static void
epc_error_clear(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	epcerror_t *epce  = (epcerror_t*)&(EVERROR->epc[vadap]);

	epce->ibus_error = 0;
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_EPC_ERROR);
	EV_GET_REG(swin + EPC_IERRC);
}

static void
epc_error_log(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	epcerror_t *epce  = (epcerror_t*)&(EVERROR->epc[vadap]);

	if (epce->ibus_error)
		return;

	epce->ibus_error   = EV_GET_REG(swin + EPC_IERR);
	epce->ia_np_error1 = EV_GET_REG(swin + EPC_EINFO1);
	epce->ia_np_error2 = EV_GET_REG(swin + EPC_EINFO2);
}


/************************************* VMECC *********************************/

static char *vmecc_error[] = {
	"0: VME Bus Error on PIO Write (interrupts)",
	"1: VME Bus Error on PIO Read (no interrupt), return bad parity data",
	"2: VME Slave Got Parity Error (no interrupt)",
	"3: VME Acquisition Timeout by PIO Master (interrupt, set dropmode)",
	"4: FCIDB Timeout (no interrupt)", 
	"5: FCI PIO Parity Error (interrupt)",
	"6: Overrun among bit 0,1,3,4,5",
	"7: in Dropmode"
};
#define VMECC_ERROR_MAX		8

static char *vmecc_errxtra_glvl[] = {
	"0: VME backplane levels",
	"1: VME backplane levels",
	"2: VME backplane levels",
	"3: VME backplane levels",
	"4: DMA Engine",
	"5: PIO Master",
	"6: Interrupt Master",
	"7: ???"
};

/*
 * This table is indexed by a 4-bit integer composed of
 * DS1:DS0:A01:~LWORD 
 * Data here is picked up from the VME specification document.
 */
static char *vme_data_strings[] = {	/* ~DS1  ~DS0  A01  ~LWORD	*/
      /* 0 */ "Address Only",		/*   H	  H     x      x	*/
      /* 1 */ "Address Only", 
      /* 2 */ "Address Only",
      /* 3 */ "Address Only",
      /* 4 */ "D24: B1-3",		/*   H	  L	L	L	*/
      /* 5 */ "D8: B1",			/*   H	  L	L	H	*/
      /* 6 */ "Illegal",              
      /* 7 */ "D8: B3",			/*   H	  L	H	H	*/
      /* 8 */ "D24: B0-2",		/*   L	  H	L	L	*/
      /* 9 */ "D8: B0",			/*   L	  H	L	H	*/
      /* a */ "Illegal",
      /* b */ "D8: B2",			/*   L	  H	H	H	*/
      /* c */ "Quadword",		/*   L	  L	L	L	*/
      /* d */ "D16: B0-1",		/*   L	  L	L	H	*/
      /* e */ "D16: B1-2",		/*   L	  L	H	L	*/
      /* f */ "D16: B2-3"		/*   L	  L	H	H	*/
};


void
vmecc_error_clear(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;

	EVERROR->vmecc[vadap].error = 0;
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_VMECC_ERROR);
	EV_GET_REG(swin + VMECC_ERRCAUSECLR);
	EV_SET_REG(swin + VMECC_ERRXTRAVME, 0);
}

void
vmecc_error_show(int slot, int padap)
{
	int	i;
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	vmeccerror_t *ve = &(EVERROR->vmecc[vadap]);
	uint	address_size;

	ve->error &= ~VMECC_ERROR_FCIDB_TO; /* Buggy bit in VMEcc */
	if (ve->error == 0)
	    return;

	ev_perr(ASIC_LVL, "VMECC in IO4 slot %d adapter %d \n",
		slot, padap, swin);

	ev_perr(REG_LVL, "Error Cause Register: 0x%x\n", ve->error);
	for (i = 0; i < VMECC_ERROR_MAX; i++) {
		if (ve->error & (1L << i))
			/* VMECC_ERROR_FCIDB_TO gets set due to a bug. */
			if (VMECC_ERROR_FCIDB_TO != ( 1 << i))
				ev_perr(BIT_LVL, "%s\n", vmecc_error[i]);
			
	}

	address_size = ve->xtravme & VMECC_ERRXTRAVME_AM;

	if (address_size < 8)
		address_size = 64;
	else if (address_size < 16)
		address_size = 32;
	else if ((address_size == 0x29) || (address_size == 0x2d))
		address_size = 16;
	else if ((address_size >= 0x38) && (address_size <= 0x3f))
		address_size = 24;
	else
		address_size = 0;

	if (ve->xtravme & VMECC_ERRXTRAVME_AVALID){
		/* LSB is the ~LWORD VME signal.
		 * Use following table to define LSB of address
		 *
		 * ~LWORD  ~DS0  ~DS1  LSB
		 *    0      1     1    0    => 32bit operation
		 *    1      1     0    0    => 8bit transfer
		 *    1      0     1    1    => 8bit odd byte transfer
		 *    1      1     1    0    => 16bit transfer
		 * So only cases where it needs to be zeroed is when ~DSO=1
		 */
		if ((ve->addrvme & 1) &&		/* ~LWORD = 1 */
		    (ve->xtravme & VMECC_ERRXTRAVME_XVALID) && /* XTRA Valid */
		    (ve->xtravme & VMECC_ERRXTRAVME_DS0))
				ve->addrvme &= ~1; /* Clear LSB */

		if (address_size == 16)
			ve->addrvme &= 0xffff;
		else if (address_size == 24)
			ve->addrvme &= 0xffffff;
	
		ev_perr(REG_LVL, "VME Address Error Register: 0x%x\n", 
			ve->addrvme);
	}

	if (ve->xtravme & VMECC_ERRXTRAVME_XVALID) {
		int data_access =
			(ve->xtravme & VMECC_ERRXTRAVME_DATA_MASK)
			>> VMECC_ERRXTRAVME_DATA_SHIFT;
#define	A01MASK		2

		/* data_access is AS:DS1:DS0:LWORD. Strip AS, Move DS1:DS0
		 * left by one bit, and copy A01 in bit 1, and put ~LWORD in 0.
		 * This will make data_access as  DS1:DS0:A01:~LWORD
		 */
		data_access = ((data_access & 0x6) << 1) |
				(ve->addrvme & A01MASK) | ( ~data_access & 1);

		ev_perr(REG_LVL,"Extra VME Bus signal register: 0x%x\n",
			ve->xtravme);
		ev_perr(BIT_LVL, "AM: 0x%x %s %s AS:%d DS0:%d DS1:%d LWORD:%d\n",
			ve->xtravme & VMECC_ERRXTRAVME_AM,
			ve->xtravme & VMECC_ERRXTRAVME_IACK  ? "IACK" : "",
			ve->xtravme & VMECC_ERRXTRAVME_WR    ? "Write" : "Read",
			ve->xtravme & VMECC_ERRXTRAVME_AS    ? 1 : 0,
			ve->xtravme & VMECC_ERRXTRAVME_DS0   ? 1 : 0,
			ve->xtravme & VMECC_ERRXTRAVME_DS1   ? 1 : 0,
			ve->xtravme & VMECC_ERRXTRAVME_LWORD ? 1 : 0);
		ev_perr(BIT_LVL, "    (A%d%s/%s)\n",
			address_size,
			ve->xtravme & VMECC_ERRXTRAVME_AMSUPER ? "S" : "NP",
			vme_data_strings[data_access]);
		i = VMECC_ERRXTRAVME_GLVL(ve->xtravme);
		ev_perr(BIT_LVL, "VME-Grant-Level: %s\n",
			vmecc_errxtra_glvl[i]);
	}
}

void
vmecc_error_log(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;

	vmeccerror_t *ve = &(EVERROR->vmecc[vadap]);

	if (ve->error)
		return;		/* do not bother if error already logged */

	ve->error = EV_GET_REG(swin + VMECC_ERRORCAUSES);

	if (ve->error == 0)
		return;		/* do not bother if no error */

	/*ve->addrebus	= EV_GET_REG(swin + VMECC_ERRADDREBUS); */
	ve->addrvme	= EV_GET_REG(swin + VMECC_ERRADDRVME);
	ve->xtravme	= EV_GET_REG(swin + VMECC_ERRXTRAVME);
}

/************************************* HIPPI *********************************/

void
io4hip_error_clear(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	hadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;

	EVERROR->io4hip[hadap].error = 0;
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_VMECC_ERROR);
	EV_GET_REG(swin + VMECC_ERRCAUSECLR);
	EV_SET_REG(swin + VMECC_ERRXTRAVME, 0);
}

void
io4hip_error_show(int slot, int padap)
{
	int	i;
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	hadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	io4hiperror_t *he = &(EVERROR->io4hip[hadap]);

	he->error &= ~VMECC_ERROR_FCIDB_TO; /* Buggy bit in VMEcc */
	if (he->error == 0)
	    return;

	ev_perr(ASIC_LVL, "HIPPI in IO4 slot %d adapter %d \n",
		slot, padap, swin);

	ev_perr(REG_LVL, "Error Cause Register: 0x%x\n", he->error);
	for (i = 0; i < VMECC_ERROR_MAX; i++) {
		if (he->error & (1L << i))
#ifdef	CHIPREV_BUG
			if ( VMECC_ERROR_FCIDB_TO & ( 1 << i))
			    ev_perr(BIT_LVL,"%s %s\n", vmecc_error[i], 
						VMECC_REV1(he->error));
			else
#endif
			    ev_perr(BIT_LVL, "%s\n", vmecc_error[i]);
	}

	if (he->xtravme & VMECC_ERRXTRAVME_AVALID) 
		ev_perr(REG_LVL, "HIPPI-VMECC Address Error Register: 0x%x\n", 
			he->addrvme);

	if (he->xtravme & VMECC_ERRXTRAVME_XVALID) {
		ev_perr(REG_LVL,"Extra HIPPI-VMECC Bus signal register: 0x%x\n",
			he->xtravme);
		ev_perr(BIT_LVL, "AM: 0x%x %s %s %s %s %s\n",
			he->xtravme & VMECC_ERRXTRAVME_AM,
			he->xtravme & VMECC_ERRXTRAVME_IACK  ? "IACK" : "",
			he->xtravme & VMECC_ERRXTRAVME_WR    ? "Write" : "Read",
			he->xtravme & VMECC_ERRXTRAVME_AS    ? "AS" : "",
			he->xtravme & VMECC_ERRXTRAVME_DS0   ? "DS0" : "",
			he->xtravme & VMECC_ERRXTRAVME_DS1   ? "DS1" : "",
			he->xtravme & VMECC_ERRXTRAVME_LWORD ? "LWORD" : " ");
		i = VMECC_ERRXTRAVME_GLVL(he->xtravme);
		ev_perr(BIT_LVL, "VMECC-Grant-Level: %s\n",
			vmecc_errxtra_glvl[i]);
	}
}

void
io4hip_error_log(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	hadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;

	io4hiperror_t *he = &(EVERROR->io4hip[hadap]);

	if (he->error)
		return;		/* do not bother if error already logged */

	he->error = EV_GET_REG(swin + VMECC_ERRORCAUSES);

	if (he->error == 0)
		return;		/* do not bother if no error */

	/*he->addrebus	= EV_GET_REG(swin + VMECC_ERRADDREBUS); */
	he->addrvme	= EV_GET_REG(swin + VMECC_ERRADDRVME);
	he->xtravme	= EV_GET_REG(swin + VMECC_ERRXTRAVME);
}


/************************************ FCHIP **********************************/

char *fchip_error[] = {
	" 0: OverWrite",
	" 1: Loopback Received",
	" 2: Loopback Error",
	" 3: F to IBus Command Error - non-interruptable",
	" 4: PIO Read Response IBus Data Error - non-interruptable",
	" 5: DMA Read Request Timeout Error",
	" 6: Unknown IBus Command Error",
	" 7: DMA Read Response Ibus Data Error",
	" 8: DMA Write Data FCI Error",
	" 9: PIO Read Response FCI Data Error",
	"10: PIO/GFX Write IBus Data Error",
	"11: Load Address Read FCI Error",
	"12: DMA Write IBus Command Error",
	"13: Address Map Request IBus Command Error",
	"14: Interrupt IBus Command Error",
	"15: Load Address Write FCI Error",
	"16: Unknown FCI Command Error",
	"17: Address Map Request Timeout Error",
	"18: Address Map Response Data Error",
	"19: PIO F Internal Write IBus Data Error",
	"20: IBus Surprise",
	"21: DMA write IBus data Error",
	"22: System FCI Reset",
	"23: Software FCI Reset",
	"24: Master Reset",
	"25: F Error FCI Reset",
	"26: F Chip Reset In Progress",
	"27: Drop PIO Write Mode",
	"28: Drop DMA Write Mode",
	"29: Fake DMA Read Mode"
};
#define FCHIP_ERROR_MAX 30

void
fchip_error_clear(int slot, int padap)
{

	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	type   = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	if (type == IO4_ADAP_VMECC)
		EVERROR->fvmecc[vadap].error = 0;
	else if (type == IO4_ADAP_HIPPI)
		EVERROR->fio4hip[vadap].error = 0;
	else
		EVERROR->ffcg[vadap].error = 0;
	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_FCHIP_ERROR);
	EV_GET_REG(swin + FCHIP_ERROR_CLEAR);
}

static char *fci_master[]={"VMECC", "FCG", "HIPPI", "NONE" };

#define	FCHIP_IBUS_CMD_ERR	((1 << 3)|(1 << 12)|(1 << 13)|(1 << 14))
#define	FCHIP_FCI_CMD_ERR	(1<<16) 

static void
fchip_error_show(int slot, int padap)
{
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	type   = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	fchiperror_t	*fe;
	int		i;

	if (type == IO4_ADAP_VMECC) {
		fe = &(EVERROR->fvmecc[vadap]);
		i = 0;
	} else if (type == IO4_ADAP_HIPPI) {
		fe = &(EVERROR->fio4hip[vadap]);
		i = 2;
	} else if (type == IO4_ADAP_FCG){
		fe = &(EVERROR->ffcg[vadap]);
		i = 1;
	}
	else    i = 3;

	if (fe->error == 0)
	    return;
	ev_perr(ASIC_LVL, "Fchip in IO4 slot %d adapter %d, FCI master: %s\n", 
		slot, padap, fci_master[i]);
	ev_perr(REG_LVL, "Error Status Register: 0x%x\n", fe->error);

	for (i = 0; i < FCHIP_ERROR_MAX; i++) {
		if (fe->error & (1L << i))
			ev_perr(BIT_LVL, "%s\n", fchip_error[i]);
	}

	/* Qualify the Error bits whlie printing addresses */
	if (fe->error & FCHIP_IBUS_CMD_ERR)
		ev_perr(REG_LVL,"IBus Opcode+Address: 0x%llx\n",fe->ibus_addr);
	if (fe->error & FCHIP_FCI_CMD_ERR)
		ev_perr(REG_LVL,"FCI  Error Command: 0x%x\n", fe->fci);
}

static void
fchip_error_log(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	type   = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	fchiperror_t	*fe;

	if (type == IO4_ADAP_VMECC) {
		fe = &(EVERROR->fvmecc[vadap]);
	} else if (type == IO4_ADAP_HIPPI) {
		fe = &(EVERROR->fio4hip[vadap]);
	} else {
		fe = &(EVERROR->ffcg[vadap]);
	}

	if (fe->error)
		return;

	fe->error = EV_GET_REG(swin + FCHIP_ERROR);

	fe->ibus_addr = EV_GET_REG(swin + FCHIP_IBUS_ERROR_CMND);
	fe->fci   = EV_GET_REG(swin + FCHIP_FCI_ERROR_CMND);
}


/************************************* FCG ***********************************/

/* ARGSUSED */
void
fcg_error_clear(int slot, int padap)
{
}

/* ARGSUSED */
void
fcg_error_log(int slot, int padap)
{
}

/* ARGSUSED */
void
fcg_error_show(int slot, int padap)
{
}

/************************************* DANG **********************************/

static char *dang_ierr_p[] = {
	"IBus Data Parity Error",
	"IBus Command Error",
	"IBus Suprise",
	"Bus Error for Command or Data",

	"Bus Error on Command",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",
};

static char *dang_ierr_md[] = {
	"DMA Master transaction killed by PIO",
	"IBus Read Timeout",
	"Incoming IBus Data Parity Error ",
	"Incoming GIO Bus Data Parity Error ",

	"Incoming Control Block Parity Error ",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",
};

static char *dang_ierr_sd[] = {
	"Incoming IBus Data Parity Error",
	"Incoming GIO Bus Command Parity Error",
	"Incoming GIO Bus Byte Count Parity Error",
	"Incoming GIO Bus Data Parity Error",

	"IBus Read Timeout",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",

	"? not used ?",
	"? not used ?",
	"? not used ?",
	"? not used ?",
};

void
dang_error_clear(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t swin = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	long long *adptr = (long long *)swin;
	dangerror_t *de = (dangerror_t*)&(EVERROR->dang[vadap]);
	volatile int    i;

	de->p_error  = 0;
	de->md_error = 0;
	de->sd_error = 0;
	de->md_cmplt = 0;

	EV_SET_LOCAL(EV_CIPL0, EVINTR_LEVEL_DANG_ERROR);

	/*
	 * currently, clears existing errors and mdma complete info
	 */
	i = (int)load_double((long long *)&adptr[DANG_PIO_ERR_CLR]);
	i = (int)load_double((long long *)&adptr[DANG_DMAM_ERR_CLR]);
	i = (int)load_double((long long *)&adptr[DANG_DMAS_ERR_CLR]);
	i = (int)load_double((long long *)&adptr[DANG_DMAM_COMPLETE_CLR]);
}

void
dang_error_log(int slot, int padap)
{
    int     window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
    __psunsigned_t swin = SWIN_BASE(window, padap);
    int     vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
    dangerror_t *de = (dangerror_t*)&(EVERROR->dang[vadap]);
    int	    inuse  = 0;
    long long *adptr = (long long *)swin;

    inuse = de->p_error | de->md_error | de->sd_error | de->md_cmplt;
    if (inuse)
	    return;

    de->p_error = (ushort) load_double((long long *)&adptr[DANG_PIO_ERR]);
    de->md_error = (ushort) load_double((long long *)&adptr[DANG_DMAM_ERR]);
    de->sd_error = (ushort) load_double((long long *)&adptr[DANG_DMAS_ERR]);
    de->md_cmplt = (ushort) load_double((long long *)&adptr[DANG_DMAM_COMPLETE]);

}

/* All three dang error registers are currently 5 bits */
#define DANG_ERRBIT_MIN	0
#define DANG_ERRBIT_MAX 5

void
dang_error_show(int slot, int padap)
{
    int     window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
    __psunsigned_t swin = SWIN_BASE(window, padap);
    int     vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
    dangerror_t *de = (dangerror_t*)&(EVERROR->dang[vadap]);
    int	    inuse  = 0;
    long long *adptr = (long long *)swin;
    int     i;

    inuse = de->p_error | de->md_error | de->sd_error | de->md_cmplt;
    if (inuse)
	    return;

    ev_perr(ASIC_LVL,"DANG in IO4 slot %d adapter %d\n", slot, padap);

    if (de->p_error) {
	ev_perr(REG_LVL,"PIO Error Register: 0x%x\n", de->p_error);

	for (i = DANG_ERRBIT_MIN; i < DANG_ERRBIT_MAX; i++) {
	    if (de->p_error & (1 << i))
		ev_perr(BIT_LVL,"%s\n", dang_ierr_p[i - EPC_IERR_MIN]);
	}
    }
    if (de->md_error) {
	ev_perr(REG_LVL,"Master DMA Error Register: 0x%x\n", de->md_error);

	for (i = DANG_ERRBIT_MIN; i < DANG_ERRBIT_MAX; i++) {
	    if (de->md_error & (1 << i))
		ev_perr(BIT_LVL,"%s\n", dang_ierr_md[i - EPC_IERR_MIN]);
	}
    }
    if (de->sd_error) {
	ev_perr(REG_LVL,"Slave DMA Error Register: 0x%x\n", de->md_error);

	for (i = DANG_ERRBIT_MIN; i < DANG_ERRBIT_MAX; i++) {
	    if (de->sd_error & (1 << i))
		ev_perr(BIT_LVL,"%s\n", dang_ierr_md[i - EPC_IERR_MIN]);
	}
    }

    if (de->md_cmplt)
	ev_perr(REG_LVL,"Master DMA Complete Register: 0x%x\n",de->md_cmplt);
}

/************************************ SCSI ***********************************/

#ifndef SABLE
/* SABLE does not support S1 chip */
#define	S1_CSR_ERR_MASK	(S1_SC_ERR_MASK(0)|S1_SC_ERR_MASK(1)|S1_SC_ERR_MASK(2))
#define S1_IBUSERR_MASK (S1_IERR_ERR_ALL(0) | S1_IERR_ERR_ALL(1) | S1_IERR_ERR_ALL(2))

void
scsi_error_clear(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	s1error_t *s1  = (s1error_t*)&(EVERROR->s1[vadap]);

	s1->ibus_error = s1->csr = s1->ierr_loglo = s1->ierr_loghi = 0;

	/* Clear all the error register bits */
	EV_SET_REG(swin + S1_IBUS_ERR_STAT, S1_IBUSERR_MASK);
	EV_SET_REG(swin + S1_STATUS_CMD, S1_CSR_ERR_MASK);
}

void
scsi_error_log(int slot, int padap)
{
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	__psunsigned_t	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	s1error_t *s1  = (s1error_t*)&(EVERROR->s1[vadap]);

	if (s1->ibus_error || s1->csr) /* already set. let's not do it again */
		return;

	s1->csr        = EV_GET_REG(swin + S1_STATUS_CMD);

	if (s1->csr & S1_SC_IBUS_ERR){
	    s1->ibus_error = EV_GET_REG(swin + S1_IBUS_ERR_STAT);
	    s1->ierr_loglo = EV_GET_REG(swin + S1_IBUS_LOG_LOW);
	    s1->ierr_loghi = EV_GET_REG(swin + S1_IBUS_LOG_HIG);
	}
}

static char *s1_csr_errors[] = {
	/* First 6 bits are command bits */
	"6 : Parity Error in SCSI data DMA channel 0",
	"7 : Parity Error in SCSI data DMA channel 1",
	"8 : Parity Error in SCSI data DMA channel 2",
	"9 : PIO Read Error on SCSI Bus 0",
	"10: PIO Read Error on SCSI Bus 1",
	"11: PIO Read Error on SCSI Bus 2",
	"12: PIO Write data Overrun due to PIO FIFO Full",
	"13: Missing Write data during PIO write",
	"14: PIO with an invalid address",
	"15: PIO Drop mode active"
};

static char *s1_ibus_errors[] = {
	"",
	" 1: Error on Incoming Data to S1",
	" 2: Error on Incoming command to S1",
	" 3: Error on Outgoing data from S1",
	" 4: Error on Outgoing command from S1",
	" 5: Error in DMA translation",
	" 6: Error in Channel 0",
	" 7: Error in Channel 1",
	" 8: Error in Channel 2",
	" 9: Surprising DMA Read/Ibus Grant",
	"10: PIO Read response data Error",
	"11: PIO Write request data Error",
	"12: DMA Read response data Error",
	"13: Interrupt Error"
	/* Bits 14-26 indicate overrun of errors for bits 1-13 */
};

void
scsi_error_show(int slot, int padap)
{
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	s1error_t *s1  = (s1error_t*)&(EVERROR->s1[vadap]);
	int     type   = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	if ((s1->csr & (S1_CSR_ERR_MASK | S1_SC_IBUS_ERR)) == 0)
	    return;

	ev_perr(ASIC_LVL, "%s in IO4 slot %d adapter %d\n", 
		(type == IO4_ADAP_SCIP ? "SCIP": "S1"), slot, padap);

	if (s1->csr){
	    ev_perr(REG_LVL, "S1 Command Status Register: 0x%x\n", s1->csr);
	    for (vadap=6; vadap < 16; vadap++)
		if (s1->csr & (1 << vadap))
		    ev_perr(BIT_LVL, "%s\n",s1_csr_errors[vadap - 6]);
	    
	}

	if ((s1->csr & S1_SC_IBUS_ERR) && (s1->ibus_error & S1_IBUSERR_MASK)){
	    ev_perr(REG_LVL, "S1 Ibus error Register: 0x%x\n", s1->ibus_error);
	    
	    for (vadap=1; vadap < 14; vadap++)
		if (s1->ibus_error & (1 << vadap))
		    ev_perr(BIT_LVL, "%s %s \n", s1_ibus_errors[vadap],
		      (s1->ibus_error & (1<<(vadap+14)) ? "- multiple occurances" : ""));
	    ev_perr(REG_LVL, "IBus Opcode+Address: 0x%llx\n",
		make_ll(s1->ierr_loghi, s1->ierr_loglo));
	}

}
#endif /* !SABLE */

/*****************************************************************************/


void
everest_error_intr(void)
{
	switch (everest_error_intr_reason) {
	case 0:
		return;
	default:
		cmn_err(CE_PANIC,"Bad Everest Error Intr Reason %d\n",
			everest_error_intr_reason);
		/* NOTREACHED */
	case EVERROR_INTR_CLEAR:
		cc_error_clear(0);
		break;
	case EVERROR_INTR_PANIC:
		cc_error_log();

		/* wait till the 'faulted' cpu dumps errors. */
		for (;;)
			if (in_everest_error_handler == 0)
				return;
		/* NOTREACHED */
	}
}	     

void	  
everest_error_sendintr(int reason)
{
	/*      
	 * This is used in two cases only so no protection is really needed.
	 */       
	if (maxcpus == 1)
	    return;

	everest_error_intr_reason = reason;
	sendintr(EVINTR_GLOBALDEST, DOACTION);
}  




/*
	int	window = EVCFGINFO->ecfg_board[slot].eb_io.eb_winnum;
	ulong	swin   = SWIN_BASE(window, padap);
	int	vadap  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	type  = EVCFGINFO->ecfg_board[slot].eb_ioarr[padap].ioa_type;
*/


extern unsigned int MemSizes[];		/* lib/libsk/ml/businfo.c */


/* in_bank returns 1 if the given address is in the same leaf position
 * as the bank whose interleave factor and position are provided
 * and the address is in the correct range.
 */
int
in_bank(uint bloc, uint offset, uint base_bloc, uint i_factor,
					uint i_position, uint simm_type)
{
	uint end_bloc;
	uint cache_line_num;

	end_bloc = base_bloc + MemSizes[simm_type] * MC3_INTERLEAV(i_factor);

#ifdef DECODE_DEBUG
	printf("bloc == 0x%x, base = %x, if = %d, ip = %d, type = %d ",
		bloc, base_bloc, i_factor, i_position, simm_type);
#endif

	/* If it's out of range, return 0 */
	if (bloc < base_bloc || bloc >= end_bloc) {
#ifdef DECODE_DEBUG
		printf("Not here.\n");
		/* Nope. */
#endif
		return 0;
	}

#ifdef DECODE_DEBUG
	printf("End == %x\n", end_bloc);
#endif
	/* Is this bank the right position in the "interleave"?
	 * See if the "block" address falls in _our_ interleave position
	 */
	cache_line_num = (bloc << 1) + (offset >> 7);

	if ((cache_line_num % MC3_INTERLEAV(i_factor)) == i_position) {
#ifdef DECODE_DEBUG
	printf("This bank!\n");
#endif
		/* We're the one */
		return 1;
	}

#ifdef DECODE_DEBUG
	printf("In range, wrong IP.\n");
#endif
	/* Nope. */
	return 0;
}


char
bank_letter(uint leaf, uint bank) {

	if ((leaf > 1) || (bank > 3))
		return '?';
	
	return 'A' + leaf + (bank << 1);
}

static int
addr2sbs(uint bloc, uint offset, uint *slot, uint *bank, uint *simm)
{
    int s, l, b;
    int index;
    short enable, simm_type, i_factor, i_position;
    uint base_bloc;
    evbrdinfo_t *brd;
    evbnkcfg_t *bnk;

    for (s = 0; s < EV_MAX_SLOTS; s++) {
	brd = &(EVCFGINFO->ecfg_board[s]);

	if (brd->eb_type == EVTYPE_MC3) {
#ifdef DECODE_DEBUG
	    printf("Board %d is memory\n", s);
#endif
	    /* It's a memory board, do leaf loop */
	    for (l = 0; l < MC3_NUM_LEAVES; l++) {
#ifdef DECODE_DEBUG
		printf("leaf %d\n", l);
#endif
	        for (b = 0; b < MC3_BANKS_PER_LEAF; b++) {

		    bnk = &(brd->eb_banks[l * MC3_BANKS_PER_LEAF + b]);
#ifdef DECODE_DEBUG
		    printf("bank %d\n", b);
#endif
		/* Nonzero means enabled */
		    enable = bnk->bnk_enable;
#ifdef DECODE_DEBUG
		    printf("Enable = %x\n", enable);
#endif
		    simm_type = bnk->bnk_size;
#ifdef DECODE_DEBUG
		    printf("Type = %x\n", simm_type);
#endif
		    i_factor = bnk->bnk_if;
		    i_position = bnk->bnk_ip;
		    base_bloc = bnk->bnk_bloc;
#ifdef DECODE_DEBUG
		    printf("Base bloc = %x\n", base_bloc);
#endif
		    if (enable && in_bank(bloc, offset, base_bloc, i_factor,
					i_position, simm_type)) {
			/* We have a winner! */
			*slot = s;
			/* Banks are in order within the slots */
#ifdef DECODE_DEBUG
			printf("Winner is slot %d, leaf %d, bank %d\n",
						s, l, b);
#endif
			*bank = b + (l * MC3_BANKS_PER_LEAF);
			return 0;
#ifdef DECODE_DEBUG
		    } else {
			printf("Disabled!\n");
#endif
		    } /* if enable... */
		}
	    }
	} /* If mem_slots... */
    } /* for s */

    return -1;
}


void
mc3_decode_addr(void (*print)(char *, ...), unsigned int paddrhi, unsigned int paddrlo)
{
	uint slot, leaf, bank, simm;
	uint bloc_num;

	bloc_num = (paddrlo >> 8) | ((paddrhi & 0xff) << 24);

	if (addr2sbs(bloc_num, paddrlo & 0xff, &slot, &bank, &simm)) {
		if ((__psint_t)print == (__psint_t)ev_perr)
			ev_perr(REG_LVL, "  Unable to decode address\n");
		else
			print(" - Unable to decode address\n");
	} else {
		leaf = bank >> 2;
		bank &= 3;
		if ((__psint_t)print == (__psint_t)ev_perr)
			ev_perr(REG_LVL, "  Slot %d, leaf %d, bank %d (%c)\n",
				slot, leaf, bank,
				bank_letter(leaf, bank));
		else
			print("slot %d, leaf %d, bank %d (%c)\n",
				slot, leaf, bank,
				bank_letter(leaf, bank));
	}
}

