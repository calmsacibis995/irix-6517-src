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

/* NOTE:  This is a pared down version of kern/ml/EVEREST/everror.c
intended for use only by the FRU analyzer and icrash. */

#ident	"$Revision: 1.1 $"

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


#define _KERNEL 1
#include <sys/types.h>
#undef _KERNEL

#include <stdio.h>

#include <sys/sbd.h>
#include <sys/immu.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/vmecc.h>
#include <sys/EVEREST/fchip.h>
#include <sys/EVEREST/s1chip.h>
#include <sys/EVEREST/dang.h>

#include "evfru.h"

/* To handle IA Rev-1 bug. Assumes only one IO4 board exists in a system  */
static int	IA_chiprev=0; 
static char	IA_bugmsg[]={"( Possible IA Rev1 bug??? )"};
static int 	ip_slot_print; 
char		error_dumpbuf[4096];
int		error_dumpindx=0;

#ifdef STANDALONE
void adap_error_show	(int, int);
void ip19_error_show	(int);
void ip21_error_show	(int);
void mc3_error_show	(int);
#else
static void adap_error_show	(int, int);
static void ip19_error_show	(int);
static void ip21_error_show	(int);
static void mc3_error_show	(int);
#endif /* STANDALONE */
static void ip19_cc_error_show	(int slot, int pcpuid);
static void ip21_cc_error_show	(int slot, int pcpuid);
static void epc_error_show	(int, int);
static void dang_error_show	(int, int);
static void fcg_error_show	(int, int);
static void fchip_error_show	(int, int);
static void scsi_error_show	(int, int);
void   fru_mc3_decode_addr	(void (*)(), uint, uint, evcfginfo_t *);
extern int addr2sbs(uint bloc, uint offset, uint *slot, uint *bank,
		    uint *simm, evcfginfo_t *ec);

#ifndef _STANDALONE

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
#define IAREV1_MSG 	(((IA_chiprev == 1)) ? IA_bugmsg : "")
#define	AREV2_MSG(x)    ((x==2) ? "( Possible A Rev2 bug??? )" : "")
#define	AREV3_MSG(x)    ((x==3) ? "( Possible A Rev3 bug??? )" : "")
#define	VMECC_REV1(x)	((!(x >> 8)) ? "( Possible VMECC Rev1 bug??? )" : "")
#endif

#define	make_ll(x, y)  (evreg_t)((evreg_t)x << 32 | y)	

static int everror_udbe;	/* set to SPNUM on a user level DBE. Should
				 * happen only due to bad coding/hardware !!!!
				 */

#define	IP19_CPUNAME	"IP19"
#define	IP21_CPUNAME	"IP21"
#define	MEMNAME		"MC3"
#define	IONAME		"IO4"

/* Undefined prototypes */
extern void qprintf(char *);
extern int	vsprintf(char *, const char *, /* va_list */ char *);
extern char bank_letter(uint leaf, uint bank);

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

everror_t *errbuf;
evcfginfo_t *ecbuf;

void show_hardware_state(everror_t *pass_errbuf, evcfginfo_t *pass_ecbuf)
{
	errbuf = pass_errbuf;
	ecbuf = pass_ecbuf;

	everest_error_show();
}


void
everest_error_show()
{
	int slot;
	int id;			/* virtual id of the board/adpter */
	int padap;

	ev_perr(NONE_LVL, "HARDWARE ERROR STATE:\n");
	for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
		evbrdinfo_t *eb = &(ecbuf->ecfg_board[slot]);

		if (eb->eb_enabled == 0)
			continue;

		switch (eb->eb_type) {
		case EVTYPE_IP19:
			ip_slot_print=1;
			ip19_error_show(slot);
			for (id = 0; id < 4; id++)
				if (eb->eb_cpuarr[id].cpu_enable)
				    ip19_cc_error_show(slot, id);
			ip_slot_print=0;
			break;
		case EVTYPE_IP21:
			ip_slot_print=1;
			ip21_error_show(slot);
			for (id = 0; id < 4; id++)
				if (eb->eb_cpuarr[id].cpu_enable)
				    ip21_cc_error_show(slot, id);
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


/*
 * ev_perr : Error printing Routine.
 *        Format the error based on the level and print it.
 */
void
ev_perr(int level, char *fmt, ...)
{

    va_list     ap;
    unchar      *c, *p, val=0;
    char        *line;
    int         i;

    va_start(ap, fmt);
    vsprintf(everr_msg, fmt, ap);
    va_end(ap);

    for (i = 0; i < level * 2; i++)
	putchar(' ');

    printf("  %s", everr_msg);

}


#ifndef STANDALONE
static 
#endif
void
adap_error_show(int slot, int padap)
{
	int	type  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_type;

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
		dang_error_show(slot, padap);
		break;
	case IO4_ADAP_SCSI:
	case IO4_ADAP_SCIP:
		scsi_error_show(slot, padap);
		break;
	case IO4_ADAP_NULL:
		break;	  /* nothing to do*/
	default:
		bad_ioa_type(type, slot, padap);
	}
}


/*
 * About these interface functions,
 *	xxx_error_show	- print out the errbuf value onto console.
 */

/*
 * Bits 8-11 of the A Chip on IP21 are encoded to identify the
 * DB chip and CPU chip having the error.
 */
#define DB0_MIN 10
#define DB0_MAX 11
#define DB1_MIN 8
#define DB1_MAX 9
#define DB_ERRMASK 0x3
#define DB0_ERROR(err)  (((err)>>DB0_MIN)&DB_ERRMASK)
#define DB1_ERROR(err)  (((err)>>DB1_MIN)&DB_ERRMASK)

char *db0_error[] = {
        "",                                     /* no error */
        "",                                     /* not valid */
        "   11:CPU 0 DB0->D parity error",
        "10,11:CPU 1 DB0->D parity error"
};

char *db1_error[] = {
        "",                                     /* no error */
        "",                                     /* not valid */
        "    9:CPU 0 DB1->D parity error",
        "  8,9:CPU 1 DB1->D parity error"
};

char *ip21_error[] = {
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
};
#define IP21_ERROR_MAX	16

/************************************ IP19 ***********************************/

char *ip19_error[] = {
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
};
#define IP19_ERROR_MAX	16

/* Set to 1 if IP19 slot no is to be printed in cc_error_show */

#ifndef STANDALONE
static 
#endif
void
ip19_error_show(int slot)
{
	int id    = ecbuf->ecfg_board[slot].eb_cpu.eb_cpunum;
	int error = errbuf->ip[id].a_error;
	int i;

	if (error == 0)
		return;

	ev_perr(SLOT_LVL, "%s in slot %d \n", IP19_CPUNAME, slot);
	ip_slot_print = 0;
	ev_perr(REG_LVL, "A Chip Error Register: 0x%x\n", error);
	for (i = 0; i < IP19_ERROR_MAX; i++) {
		if (error & (1L << i))
#ifdef	CHIPREV_BUG
			if (A_ERROR_ADDR_ERR & (1L << i)) 
			    ev_perr(BIT_LVL,"%s %s\n",ip19_error[i],IAREV1_MSG);
			else 
#endif
			    ev_perr(BIT_LVL, "%s\n",ip19_error[i]);
	}
}

#ifndef STANDALONE
static 
#endif
void
ip21_error_show(int slot)
{
	int id    = ecbuf->ecfg_board[slot].eb_cpu.eb_cpunum;
	int error = errbuf->ip[id].a_error;
	int i;

	if (error == 0)
		return;

	ev_perr(SLOT_LVL, "%s in slot %d \n", IP21_CPUNAME, slot);
	ip_slot_print = 0;
	ev_perr(REG_LVL, "A Chip Error Register: 0x%x\n", error);
	for (i = 0; i < IP21_ERROR_MAX; i++) {
		if (error & (1L << i))
#ifdef	CHIPREV_BUG
			if (A_ERROR_ADDR_ERR & (1L << i)) 
			    ev_perr(BIT_LVL,"%s %s\n",ip21_error[i],IAREV1_MSG);
			else 
#endif
			    ev_perr(BIT_LVL, "%s\n",ip21_error[i]);
	}
}


/******************************** CC *****************************************/

char *ip19_cc_error[] = {
	"0: ECC uncorrectable (multi bit) error in Scache",
	"1: ECC correctable (single bit) error in Scache",
	"2: Parity Error on TAG RAM Data",
	"3: Parity Error on Address from A-chip",
	"4: Parity Error on Data from D-chip",
	"5: MyRequest TimeOut on EBUS",
	"6: MyResponse D-Resource TimeOut in A chip",
	"7: MyIntervention Response D-Resource TimeOut in A chip",
	"8: Address Error on MyRequest on EBUS",
	"9: Data Error on MyData on EBUS",
	"10: Internal Bus State is out of sync with A_SYNC"
};
#define IP19_CC_ERROR_MAX	11

char *ip21_cc_error[] = {
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
};

#define IP21_CC_ERROR_MAX	14


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

#ifdef	CHIPREV_BUG
#define	AREV3_ERRBITS   (CC_ERROR_MYRES_TIMEOUT|CC_ERROR_MYINT_TIMEOUT)
#define	AREV2_ERRBITS   (CC_ERROR_MYREQ_TIMEOUT|AREV3_ERRBITS)
#endif

static void
ip19_cc_error_show(int slot, int pcpuid)
{
	int  vcpuid = ecbuf->ecfg_board[slot].eb_cpuarr[pcpuid].cpu_vpid;
	uint error  = errbuf->cpu[vcpuid].cc_ertoip;
	int i, alevel;

	if (error) {
		if (ip_slot_print){
		    ip_slot_print=0;
		    ev_perr(SLOT_LVL, "%s in slot %d \n", IP19_CPUNAME, slot);
		}
		ev_perr(ASIC_LVL,"CC in %s Slot %d, cpu %d\n",
				IP19_CPUNAME,slot, pcpuid);
		ev_perr(REG_LVL, "CC ERTOIP  Register: 0x%x \n", error);

		for (i = 0; i < IP19_CC_ERROR_MAX; i++) {
			if (error & (1L << i)){
#ifdef	CHIPREV_BUG
				if ((alevel==2)&&(AREV2_ERRBITS & (1 << i)))
				    ev_perr(BIT_LVL,"%s %s\n",ip19_cc_error[i], 
				    		AREV2_MSG(alevel));
				else if((alevel==3)&&(AREV3_ERRBITS & (1<<i)))
				    ev_perr(BIT_LVL,"%s %s\n",ip19_cc_error[i], 
				    		AREV3_MSG(alevel));
				else
#endif
				    ev_perr(BIT_LVL, "%s\n",ip19_cc_error[i]);
			}
		}
	}

}


static void
ip21_cc_error_show(int slot, int pcpuid)
{
	int  vcpuid = ecbuf->ecfg_board[slot].eb_cpuarr[pcpuid].cpu_vpid;
	uint error  = errbuf->cpu[vcpuid].cc_ertoip;
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
		for (i = 0; i < IP21_CC_ERROR_MAX; i++) {
			if (error & (1L << i)){
#ifdef	CHIPREV_BUG
				if ((alevel==2)&&(AREV2_ERRBITS & (1 << i)))
				    ev_perr(BIT_LVL,"%s %s\n",ip21_cc_error[i], 
				    		AREV2_MSG(alevel));
				else if((alevel==3)&&(AREV3_ERRBITS & (1<<i)))
				    ev_perr(BIT_LVL,"%s %s\n",ip21_cc_error[i], 
				    		AREV3_MSG(alevel));
				else
#endif
				    ev_perr(BIT_LVL, "%s\n",ip21_cc_error[i]);
			}
		}
	}

#if _KERNEL && !_STANDALONE
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
#endif /* _KERNEL */

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

/*********************************** MC3 *************************************/

static void
mc3_sbe_analyze(int slot, int mbid, int *bank_num, int *simm_num)
{
	mc3error_t  *mc3err = &(errbuf->mc3[mbid]);
	evbrdinfo_t *board  = &(ecbuf->ecfg_board[slot]);
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
}


void
mc3_error_show(int slot)
{
	int	mbid = ecbuf->ecfg_board[slot].eb_mem.eb_mc3num;
	mc3error_t	*mc3err = &(errbuf->mc3[mbid]);
	uint	error, i;
	int	bank_num, simm_num;
	caddr_t vaddr;

	if ((mc3err->ebus_error == 0) && (mc3err->mem_error[0] == 0) &&
	    (mc3err->mem_error[1] ==0))
	    return;

	ev_perr(SLOT_LVL, "%s in slot %d \n", MEMNAME, slot);

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
			"0: PartialWrite Uncorrectable (Multiple Bit) Error\n");
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
		fru_mc3_decode_addr((void (*)())ev_perr, mc3err->erraddrhi[i],
				 mc3err->erraddrlo[i], ecbuf);
	    }

            if (mc3err->syndrome0[i])
                ev_perr(REG_LVL,
		      "MA Leaf %d Memory Syndrome Register 0 (SIMM 0): 0x%x\n",
			i, mc3err->syndrome0[i]);
            if (mc3err->syndrome1[i])
                ev_perr(REG_LVL,
		      "MA Leaf %d Memory Syndrome Register 1 (SIMM 1): 0x%x\n",
			i, mc3err->syndrome1[i]);
            if (mc3err->syndrome2[i])
                ev_perr(REG_LVL,
		      "MA Leaf %d Memory Syndrome Register 2 (SIMM 2): 0x%x\n",
			i, mc3err->syndrome2[i]);
            if (mc3err->syndrome3[i])
                ev_perr(REG_LVL,
		      "MA Leaf %d Memory Syndrome Register 3 (SIMM 3): 0x%x\n",
			i, mc3err->syndrome3[i]);
        }
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

#if STANDALONE
int ok_to_read_syndrome = 1;
static int panic_on_sbe = 1;	/* XXX standalone panics, until MC3 SBE fix */
#else
int ok_to_read_syndrome = 0;
extern int panic_on_sbe;
#endif


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
#define	IO4_EBUSERR_MASK	((1 << IO4_EBUSERROR_MAX) - 1)

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
	int	   window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
	io4error_t *ie = &(errbuf->io4[window]);
	int 	i, error;

	if ((ie->ibus_error== 0) && (ie->ebus_error == 0))
	    return;

	ev_perr(SLOT_LVL, "%s board in slot %d \n", IONAME, slot);
	if (ie->ibus_error) {
	    ev_perr(REG_LVL,"IA IBUS Error Register: 0x%x\n", ie->ibus_error);
		for (i = 0; i < IO4_IBUSERROR_MAX; i++) {
			if (ie->ibus_error & (1L << i))
				ev_perr(BIT_LVL, "%s\n",io4_ibuserror[i]);
		}
	    if (i = ((ie->ibus_error >> IO4_IBUSERROR_MAX) & 0x7)){
		error = ecbuf->ecfg_board[slot].eb_ioarr[i].ioa_type;
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
	int	window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
	ulong	swin   = SWIN_BASE(window, padap);
	int	vadap  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	x, i;
	epcerror_t *epce  = (epcerror_t*)&(errbuf->epc[vadap]);

 	if ((epce->ibus_error == 0) && (epce->ia_np_error2 == 0))
	    return;

	ev_perr(ASIC_LVL,"EPC in %s slot %d adapter %d\n", IONAME, slot, padap);

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

static char *fci_master[]={"VMECC", "FCG", "HIPPI", "NONE" };

#define	FCHIP_IBUS_CMD_ERR	((1 << 3)|(1 << 12)|(1 << 13)|(1 << 14))
#define	FCHIP_FCI_CMD_ERR	(1<<16) 

static void
fchip_error_show(int slot, int padap)
{
	int	window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
	ulong	swin   = SWIN_BASE(window, padap);
	int	vadap  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	type   = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	fchiperror_t	*fe;
	int		i;

	if (type == IO4_ADAP_VMECC) {
		fe = &(errbuf->fvmecc[vadap]);
		i = 0;
	} else if (type == IO4_ADAP_HIPPI) {
		fe = &(errbuf->fio4hip[vadap]);
		i = 2;
	} else if (type == IO4_ADAP_FCG){
		fe = &(errbuf->ffcg[vadap]);
		i = 1;
	}
	else    i = 3;

	if (fe->error == 0)
	    return;
	ev_perr(ASIC_LVL, "Fchip in %s slot %d adapter %d, FCI master: %s\n", 
		IONAME, slot, padap, fci_master[i]);
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

/************************************* FCG ***********************************/

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
#define VMECC_ERROR_MAX         8

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
static char *vme_data_strings[] = {     /* ~DS1  ~DS0  A01  ~LWORD      */
      /* 0 */ "Address Only",           /*   H    H     x      x        */
      /* 1 */ "Address Only",
      /* 2 */ "Address Only",
      /* 3 */ "Address Only",
      /* 4 */ "D24: B1-3",              /*   H    L     L       L       */
      /* 5 */ "D8: B1",                 /*   H    L     L       H       */
      /* 6 */ "Illegal",
      /* 7 */ "D8: B3",                 /*   H    L     H       H       */
      /* 8 */ "D24: B0-2",              /*   L    H     L       L       */
      /* 9 */ "D8: B0",                 /*   L    H     L       H       */
      /* a */ "Illegal",
      /* b */ "D8: B2",                 /*   L    H     H       H       */
      /* c */ "Quadword",               /*   L    L     L       L       */
      /* d */ "D16: B0-1",              /*   L    L     L       H       */
      /* e */ "D16: B1-2",              /*   L    L     H       L       */
      /* f */ "D16: B2-3"               /*   L    L     H       H       */
};

void
io4hip_error_show(int slot, int padap)
{
        int     i;
        int     window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
        ulong   swin   = SWIN_BASE(window, padap);
        int     hadap  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;        io4hiperror_t *he = &(errbuf->io4hip[hadap]);

        he->error &= ~VMECC_ERROR_FCIDB_TO; /* Buggy bit in VMEcc */
        if (he->error == 0)
            return;

        ev_perr(ASIC_LVL, "HIPPI in %s slot %d adapter %d \n",
                IONAME, slot, padap, swin);

        ev_perr(REG_LVL, "Error Cause Register: 0x%x\n", he->error);
        for (i = 0; i < VMECC_ERROR_MAX; i++) {
                if (he->error & (1L << i))
#ifdef  CHIPREV_BUG
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
                ev_perr(REG_LVL,"Extra HIPPI-VMECC Bus signal register: 0x%x\n",                        he->xtravme);
                ev_perr(BIT_LVL, "AM: 0x%x %s %s %s %s %s\n",
                        he->xtravme & VMECC_ERRXTRAVME_AM,
                        he->xtravme & VMECC_ERRXTRAVME_IACK  ? "IACK" : "",
                        he->xtravme & VMECC_ERRXTRAVME_WR    ? "Write" : "Read",                        he->xtravme & VMECC_ERRXTRAVME_AS    ? "AS" : "",
                        he->xtravme & VMECC_ERRXTRAVME_DS0   ? "DS0" : "",
                        he->xtravme & VMECC_ERRXTRAVME_DS1   ? "DS1" : "",
                        he->xtravme & VMECC_ERRXTRAVME_LWORD ? "LWORD" : " ");
                i = VMECC_ERRXTRAVME_GLVL(he->xtravme);
                ev_perr(BIT_LVL, "VMECC-Grant-Level: %s\n",
                        vmecc_errxtra_glvl[i]);
        }
}

#define VMECC_ERROR_MAX               8


void
vmecc_error_show(int slot, int padap)
{
        int     i;
        int     window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
        ulong   swin   = SWIN_BASE(window, padap);
        int     vadap  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;        vmeccerror_t *ve = &(errbuf->vmecc[vadap]);
        ulong address_size;

        ve->error &= ~VMECC_ERROR_FCIDB_TO; /* Buggy bit in VMEcc */
        if (ve->error == 0)
            return;

        ev_perr(ASIC_LVL, "VMECC in %s slot %d adapter %d \n",
                IONAME, slot, padap, swin);

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
                if ((ve->addrvme & 1) &&                /* ~LWORD = 1 */
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
#define A01MASK         2

                /* data_access is AS:DS1:DS0:LWORD. Strip AS, Move DS1:DS0
                 * left by one bit, and copy A01 in bit 1, and put ~LWORD in 0.
                 * This will make data_access as  DS1:DS0:A01:~LWORD
                 */
                data_access = ((data_access & 0x6) << 1) |
                                (ve->addrvme & A01MASK) | (~data_access & 1);

                ev_perr(REG_LVL,"Extra VME Bus signal register: 0x%x\n",
                        ve->xtravme);
                ev_perr(BIT_LVL, "AM: 0x%x %s %s AS:%d DS0:%d DS1:%d LWORD:%d\n",
                        ve->xtravme & VMECC_ERRXTRAVME_AM,
                        ve->xtravme & VMECC_ERRXTRAVME_IACK  ? "IACK" : "",
                        ve->xtravme & VMECC_ERRXTRAVME_WR    ? "Write" : "Read",                        ve->xtravme & VMECC_ERRXTRAVME_AS    ? 1 : 0,
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




/* All three dang error registers are currently 5 bits */
#define DANG_ERRBIT_MIN	0
#define DANG_ERRBIT_MAX 5

void
dang_error_show(int slot, int padap)
{
    int     window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
    ulong   swin   = SWIN_BASE(window, padap);
    int     vadap  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
    dangerror_t *de = (dangerror_t*)&(errbuf->dang[vadap]);
    int	    inuse = 0;
    long long *adptr = (long long *)swin;
    int     i;

    inuse = de->p_error | de->md_error | de->sd_error | de->md_cmplt;
    if (inuse)
	    return;

    ev_perr(ASIC_LVL,"DANG in %s slot %d adapter %d\n", IONAME, slot, padap);

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

#define	S1_CSR_ERR_MASK	(S1_SC_ERR_MASK(0)|S1_SC_ERR_MASK(1)|S1_SC_ERR_MASK(2))
#define S1_IBUSERR_MASK (S1_IERR_ERR_ALL(0) | S1_IERR_ERR_ALL(1) | S1_IERR_ERR_ALL(2))


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
	int	window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
	ulong	swin   = SWIN_BASE(window, padap);
	int	vadap  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	s1error_t *s1  = (s1error_t*)&(errbuf->s1[vadap]);
	int     type   = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_type;

	if ((s1->csr & (S1_CSR_ERR_MASK | S1_SC_IBUS_ERR)) == 0)
	    return;

	ev_perr(ASIC_LVL, "%s in %s slot %d adapter %d\n", 
		(type == IO4_ADAP_SCIP ? "SCIP": "S1"), IONAME, slot, padap);

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

/*****************************************************************************/


/*
	int	window = ecbuf->ecfg_board[slot].eb_io.eb_winnum;
	ulong	swin   = SWIN_BASE(window, padap);
	int	vadap  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_virtid;
	int	type  = ecbuf->ecfg_board[slot].eb_ioarr[padap].ioa_type;
*/


/* Array used to find the actual size of a particular
 * memory bank.  The size is in blocks of 256 bytes, since
 * the memory banks base address drops the least significant
 * 8 bits to fit the base addr in one word.
 */
unsigned int MemSizes[] = {
    0x10000,	/* 0 = 16 megs   */
    0x40000,	/* 1 = 64 megs   */
    0x80000,	/* 2 = 128 megs  */
    0x100000,	/* 3 = 256 megs  */
    0x100000,	/* 4 = 256 megs  */
    0x400000,	/* 5 = 1024 megs */
    0x400000,	/* 6 = 1024 megs */
    0x0		/* 7 = no SIMM   */
};


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


void
fru_mc3_decode_addr(void (*print)(), unsigned int paddrhi, unsigned int paddrlo,
		evcfginfo_t *ec)
{
	uint slot, leaf, bank, simm;
	uint bloc_num;

	bloc_num = (paddrlo >> 8) | ((paddrhi & 0xff) << 24);

	if (addr2sbs(bloc_num, paddrlo & 0xff, &slot, &bank, &simm, ec)) {
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

