#ident	"lib/libsk/lib/fault.c:  $Revision: 1.107 $"

/*
 * fault.c -- saio fault handling routines
 */

#include <sys/sbd.h>
#include <genpda.h>
#include <stringlist.h>
#include <libsc.h>
#include <libsk.h>
#include <alocks.h>

#ifdef EVEREST
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/addrs.h>
#endif

#if IP28 || IP30
#include <sys/cpu.h>
#endif

#if R4000 || R10000
static void desc_err(unsigned);
#endif 

extern int ignore_xoff;		/* set during autoboots */
#ifndef SN
extern k_machreg_t excep_regs[MAXCPU][NREGS];
#endif

/*
 * register descriptions
 */
extern struct reg_desc sr_desc[], cause_desc[];

static struct reg_values except_values[] = {
	{ EXCEPT_NORM,	"Normal" },
	{ EXCEPT_UTLB,	"UTLB Miss" },
	{ EXCEPT_BRKPT,	"Breakpoint" },
	{ EXCEPT_XUT,	"XUT" },
	{ EXCEPT_ECC,	"ECC" },
	{ EXCEPT_NMI,	"NMI" },
	{ 0,		NULL }
};

static struct reg_desc except_desc[] = {
	/* mask	     shift      name   format  values */
	{ (__scunsigned_t)-1,0,	"vector",NULL,	except_values },
	{ 0,		0,	NULL,	NULL,	NULL }
};

#ifndef IP24
/* F A U L T  T A B L E */
typedef int (*PFI)(void);

/* 
 * This fault table is used to service the exceptions in the Cause
 * Register 'exception code' field. For now lets treat the 'int' interrupt
 * the same. We could at some later date create a table for the 8 software
 * interrupts.
 */
PFI RFaultTbl[] = {
	(PFI)_exception_handler,	/* 0  ext interrupt - use IntTbl[] */
	(PFI)_exception_handler,	/* 1  TLB modification */
	(PFI)_exception_handler,	/* 2  TLB miss on load */
	(PFI)_exception_handler,	/* 3  TLB miss on store */
	(PFI)_exception_handler,	/* 4  address error on load */
	(PFI)_exception_handler,	/* 5  address error on store */
	(PFI)_exception_handler,	/* 6  bus error on I-fetch */
	(PFI)_exception_handler,	/* 7  data bus error */
	(PFI)_exception_handler,	/* 8  system call */
	(PFI)_exception_handler,	/* 9  breakpoint */
	(PFI)_exception_handler,	/* 10 reserved instruction */
	(PFI)_exception_handler,	/* 11 coprocessor unusable */
	(PFI)_exception_handler,	/* 12 arithmetic overflow */
	(PFI)_exception_handler,	/* 13 trap (mips2) */
#ifdef R10000
	(PFI)_exception_handler,	/* 14 Reserved for future use */
#else
	(PFI)_exception_handler,	/* 14 I-cache Virtual Coherency Exception */
#endif	/* R10000 */
	(PFI)_exception_handler,	/* 15 Floating Point Exception */
	(PFI)_exception_handler,	/* 16 Reserved for future use */
	(PFI)_exception_handler,	/* 17 Reserved for future use */
	(PFI)_exception_handler,	/* 18 Reserved for future use */
	(PFI)_exception_handler,	/* 19 Reserved for future use */
	(PFI)_exception_handler,	/* 20 Reserved for future use */
	(PFI)_exception_handler,	/* 21 Reserved for future use */
	(PFI)_exception_handler,	/* 22 Reserved for future use */
	(PFI)_exception_handler,	/* 23 WatchHi/WatchLo address referenced */
	(PFI)_exception_handler,	/* 24 Reserved for future use */
	(PFI)_exception_handler,	/* 25 Reserved for future use */
	(PFI)_exception_handler,	/* 26 Reserved for future use */
	(PFI)_exception_handler,	/* 27 Reserved for future use */
	(PFI)_exception_handler,	/* 28 Reserved for future use */
	(PFI)_exception_handler,	/* 29 Reserved for future use */
	(PFI)_exception_handler,	/* 30 Reserved for future use */
#ifdef R10000
	(PFI)_exception_handler,	/* 31 Reserved for future use */
#else
	(PFI)_exception_handler,	/* 31 D-cache Virtual Coherency Exception */
#endif	/* R10000 */
	};


/*
 * This table is used for servicing the h/w interrupts field in the Cause
 * Register. These bits are [18..8 TFP] 15..8 of the registers for most 
 * products. Note, this table uses the common name in the table. Although, these
 * routines are product dependent and should be kept in the product
 * tree directory.
 */
PFI IntTbl[] = {
	(PFI)_exception_handler,	/* Software trap 0 */
	(PFI)_exception_handler,	/* Software trap 1 */
	(PFI)_exception_handler,	/* Level 0 Intr (R6000: sbc intr) */
	(PFI)_exception_handler,	/* Level 1 Intr (R6000: fpc intr) */
	(PFI)_exception_handler,	/* Level 2 Interrupt */
	(PFI)_exception_handler,	/* Level 3 Interrupt */
	(PFI)_exception_handler,	/* Level 4 Interrupt */
	(PFI)_exception_handler,	/* Level 5 Interrupt */
#if TFP
	(PFI)_exception_handler,	/* Level 6 Interrupt */
	(PFI)_exception_handler,	/* Level 7 Interrupt */
	(PFI)_exception_handler,	/* Level 8 Interrupt */
#endif
};

/*
 * The IntTbl_Default[] table is used when removing user installed interrupt 
 * handlers from the IntTbl[] table.
 */
static PFI IntTbl_Default[] = {
	(PFI)_exception_handler,	/* Software trap 0 */
	(PFI)_exception_handler,	/* Software trap 1 */
	(PFI)_exception_handler,	/* Level 0 Intr  */
	(PFI)_exception_handler,	/* Level 1 Intr  */
	(PFI)_exception_handler,	/* Level 2 Interrupt */
	(PFI)_exception_handler,	/* Level 3 Interrupt */
	(PFI)_exception_handler,	/* Level 4 Interrupt */
	(PFI)_exception_handler,	/* Level 5 Interrupt */
#if TFP
	(PFI)_exception_handler,	/* Level 6 Interrupt */
	(PFI)_exception_handler,	/* Level 7 Interrupt */
	(PFI)_exception_handler,	/* Level 8 Interrupt */
#endif
};
/*
 * CacheErrTbl is the R4000/R10000 Cache Error Exception table. It is used for
 * servicing parity error exception detected on the SysAD bus (usually for
 * external memory), primary cache, and secondary cache.
 */
PFI CacheErrTbl[] = {
	(PFI)_exception_handler,	/* 0 SysAD bus parity error */
	(PFI)_exception_handler,	/* 1 Primary Cache parity error */
	(PFI)_exception_handler,	/* 2 Secondary Cache ECC error */
};

/*
 * Default table for R4000/R10000 cache errors when the user service routine is
 * being removed.
 */
static PFI CacheErrTbl_Default[] = {
	(PFI)_exception_handler,	/* 0 SysAD bus parity error */
	(PFI)_exception_handler,	/* 1 Primary Cache parity error */
	(PFI)_exception_handler,	/* 2 Secondary Cache ECC error */
};

void FillAll(register PFI addr);

#endif /* not IP24 */

/*#define PDADEBUG*/

#if defined(PDADEBUG)
int isaneverest = YES_EVEREST;
#endif

int verbose_faults = 1;

/*
 * clear_nofault -- clear any existing fault handling
 */
void
clear_nofault(void)
{
    register int vidme = cpuid();
    register struct generic_pda_s *gp;

    gp = (struct generic_pda_s *)&GEN_PDA_TAB(vidme);
    ignore_xoff = 0;
    _init_alarm();		/* cancel any timers */
    gp->pda_nofault = 0;	/* cancel nofault handling */
    gp->notfirst = 0;
}

/*
 * set_nofault -- point processor's nofault field (in pda) at the
 * jmpbuf area into which the calling process (hopefully) saved its 
 * state via a setjmp() call.  The fault code will then longjmp there
 * if an exception occurs.
 */
void
set_nofault(jmp_buf nf)
{
    register int vidme = cpuid();
    register struct generic_pda_s *gp;

    gp = (struct generic_pda_s *)&GEN_PDA_TAB(vidme);
    gp->pda_nofault = (jmpbufptr)nf;
}
/*************************/

#ifndef IP24
/*
 * PROCEDURE RFaultHandler: Process all exceptions. This little routine
 * was added to give a program resume execution after an interrupt. This
 * can be done by placing your interrupt handler into the FaultTBL using
 * the InstallHandler function.
 *
 * CALLING SEQUENCE
 *
 *	Called only by exceptutlb LEAF routine during exception processing.
 *
 * RETURN  0 - Retry offending instruction
 *	   1 - To skip the offending instruction
 *
 */
int
RFaultHandler(void)
{
	register volatile struct generic_pda_s *gp;
	register k_machreg_t cause;
	register int ret_code = 0;
	register int i;
	k_machreg_t ex_code;
	k_machreg_t stat;

	/* CPU in this routine */
	int vid_me = _cpuid();		


	/* Get pointer to this CPU's saved registers */
	gp = (volatile struct generic_pda_s *)&GEN_PDA_TAB(vid_me);

	/* Get the cause register to see the type of error */
	cause = gp->cause_save;

	ex_code = (cause & CAUSE_EXCMASK) >> CAUSE_EXCSHIFT;

#if TFP
	/* on TFP, the FPI comes in on a different line though if it happens,
	 * change the ex_code to 15 (FPE on R4k) */
	if (cause & CAUSE_FPI)
		ex_code = 15;

	/* also, the Coproc Virtual Coherency interrupt comes in differently than
     * the R4K so adjust accordingly */
	else if (cause & CAUSE_VCI)
		ex_code = 14;
#endif

	/* See if an external interrupt or cache error  */
	if (ex_code) {
		/* If calling the default stuff, the code will never return 
	 	 * back to this program because the default stuff will panic.
	 	 */
		/* Not external Interrupt */
		ret_code |= (*RFaultTbl[ex_code])();
	}
	else {
		/* Since cache errors don't set the cause register
		 * we need to look at the exception valued stored
		 * by the low level vector code
		 */
#ifndef TFP
		if( gp->exc_save == EXCEPT_ECC) {
#ifndef R10000
			if(gp->cache_err_save & CACHERR_EE) 
				ret_code |= (*CacheErrTbl[SYSAD_PARERR])();
			else if (gp->cache_err_save & CACHERR_EC) 
				ret_code |= (*CacheErrTbl[SCACHE_PARERR])();
			else 
				ret_code |= (*CacheErrTbl[PCACHE_PARERR])();
			
#else
#if IP30
			extern int _prom;

			if (_prom == 1)
				uncache_K0();
#endif	/* IP30 */
			switch (gp->cache_err_save & CACHERR_SRC_MSK) {
			case CACHERR_SRC_PI:
			case CACHERR_SRC_PD:
				ret_code |= (*CacheErrTbl[PCACHE_PARERR])();
				break;

			case CACHERR_SRC_SD:
				ret_code |= (*CacheErrTbl[SCACHE_PARERR])();
				break;

			case CACHERR_SRC_SYSAD:
				ret_code |= (*CacheErrTbl[SYSAD_PARERR])();
				break;

			}
#if IP30
			if (_prom == 1)
				cache_K0();
#endif	/* IP30 */
#endif
		} else
#endif	/* ! TFP */
		{
			/* external interrupt, get level */
			ex_code = (cause & CAUSE_IPMASK) >> CAUSE_IPSHIFT;

			/* Get the status register to see what's enabled */
			stat = gp->sr_save;
			stat = (stat & SR_IMASK) >> SR_IMASKSHIFT;

			/* Number of interrupts in status register */
			for (i = N_INT -1; i >= 0; i--) {

				/*
			 	* Check cause register for exception. Make
				* sure the cooresponding enable is set in
				* the status register.
			 	*/
				if( (ex_code & (1 << i)) && (stat & (1 << i)) ) {
					ret_code |= (*IntTbl[i])();
				}
			}
		}
	}


	/* If we get to here the return will take us back to exceptutlb,
	 * which resumes execution in the routine _resume. This routine
	 * restores the state of the register upon interrupt then returns
	 * back to the instruction which received the interrupt.
	 */
	/*printf("rRFaultHandler routine \n");*/
	return ret_code;
}

/*
 * PROCEDURE InstallHandler: Install exception handler into the fault table.
 *
 * CALLING SEQUENCE
 *
 *	status = InstallHandler(index, addr);
 *
 * PROCESSING
 *
 *	Returns OK if index was valid.  Otherwise, returns BAD.  
 *	Does not check addr.
 */
InstallHandler(u_short index, PFI addr)
{
	register u_short tblindex;

	/* Display routine name in Trace mode */
/*
	printf("InstallHandler(index %x, addr= %x) Routine\n",
		index, addr);
*/

	/*
	 * Do a quick check on the address.
	 */
	if ((__psint_t)addr % 4) {
		printf("Bad install handler address. Not word aligned\n");
		printf("Address %x\n", addr);
		return(1);
	}

	tblindex = FLT_TBLINDEX(index);
	switch (FLT_TBLPTR(index)) {
	case FLT_TBL:
		if ((index == FLT_BAD) || (tblindex >= N_FLT)) {
			printf("Bad install index into FaultTbl: %x\n", index);
			return(1);
		}
		else {
			/*sr = sploff();*/
			RFaultTbl[tblindex] = addr;
			/*splx(sr);*/
		}
		break;

	case INT_TBL:
/*
		printf("int table\n");
*/
		if (tblindex >= N_INT) {
			printf("Bad install index into IntTbl: %x\n", index);
			return(1);
		}
		else {
			/*sr = sploff();*/
			IntTbl[tblindex] = addr;
/*
			printf("table index %x\n", tblindex);
			for(i=0;i < 8 ;i ++) {
				printf(" IntTbl[%x]= %x\n", i, IntTbl[i]);
			}
*/
			/*splx(sr);*/
		}
		break;

	case ALLFLT_TBL:
		FillAll(addr);
		break;

	case CACHEERR_TBL:
		if (tblindex >= N_CACHEERR) {
			printf("Bad install index into CacheErrTbl: %x\n", index);
			return(1);
		}
		else {
			/*sr = sploff();*/
			CacheErrTbl[tblindex] = addr;
			/*splx(sr);*/
		}
		break;

	default:
		printf("Bad install index into fault tables: %x\n", index);
		return(1);
	}

	return(0);
}


/*
 * PROCEDURE RemoveHandler: Remove exception handler from the fault table.
 *
 * CALLING SEQUENCE
 *
 *	status = RemoveHandler(index);
 *
 * PROCESSING
 *
 *	Returns OK if index was valid.  Otherwise, returns BAD.  
 *	Places UnexpectHandler() as the default handler.
 */
int
RemoveHandler(int index)
{
	register u_int tblindex;


	tblindex = FLT_TBLINDEX(index);
	switch (FLT_TBLPTR(index)) {
	case FLT_TBL:
		if ((index == FLT_BAD) || (tblindex >= N_FLT)) {
			printf("Bad remove index into FaultTbl: %x\n", index);
			return(1);
		}
		else {
			/*sr = sploff();*/
			RFaultTbl[tblindex] = (PFI)_exception_handler;
			/*splx(sr);*/
		}
		break;

	case INT_TBL:
		if (tblindex >= N_INT) {
			printf("Bad remove index into IntTbl: %x\n", index);
			return(1);
		}
		else {
			/*sr = sploff();*/
			IntTbl[tblindex] = IntTbl_Default[tblindex];
			/*splx(sr);*/
		}
		break;

	case ALLFLT_TBL:
		FillAll((PFI)_exception_handler);
		break;

	case CACHEERR_TBL:
		if (tblindex >= N_CACHEERR) {
			printf("Bad remove index into CacheErrTbl: %x\n", index);
			return(1);
		}
		else {
			/*sr = sploff();*/
			CacheErrTbl[tblindex] = CacheErrTbl_Default[tblindex];
			/*splx(sr);*/
		}
		break;

	default:
		printf("Bad remove index into fault tables: %x\n", index);
		return(1);
	}

	return(0);
}

/*
 * PROCEDURE FillAll: Installs exception handler into all table entries.
 *
 * CALLING SEQUENCE
 *
 *	status = FillAll(addr);
 *
 * PROCESSING
 */
void
FillAll(register PFI addr)
{
	register u_int i;

	/* Display routine name in Trace mode */
	printf("FillAll(addr= %x) Routine\n",  addr);

	/*sr = sploff();*/
	for (i = 1; i < N_FLT; i++) {
		RFaultTbl[i] = addr;
	}

	for (i = 0; i < N_INT; i++) {
		IntTbl[i] = addr;
	}
        for (i = 0; i < N_CACHEERR; i++) {
                CacheErrTbl[i] = addr;
        }

	/*splx(sr);*/
}
/*******************************/
#endif /* not IP24 */
/*
 * _exception_handler -- libsk library fault handler
 */
void
_exception_handler(void)
{
    register volatile struct generic_pda_s *gp;
    register k_machreg_t *rp;
    int vid_me = _cpuid();
    jmpbufptr jbp;
#if defined(PDADEBUG)
    char nbuf[16];
#endif

#if MULTIPROCESSOR
    char nbuf[16];

    if (vid_me >= MAXCPU || vid_me < 0) {
	_errputs("\n!--> invalid _cpuid() ");
	sprintn(nbuf, vid_me, 10);
	_errputs(nbuf);
	_errputs("\n");
	panic("\n_exception_handler: invalid vid_me !");
    }
#endif

#if defined(PDADEBUG)
    {
	int faultsp = get_sp();

	sprintn(nbuf, faultsp, 16);
	_errputs("\n  currsp: ");
	_errputs(nbuf);
	_errputs("  ");
    }
#endif

    gp = (volatile struct generic_pda_s *)&GEN_PDA_TAB(vid_me);
    rp = &(gp->regs[0]);
    if (!rp) {
	/*  if pointer not initialized yet, use cpu 0's area */
#ifdef SN
	gp->regs = rp = (k_machreg_t *)gp->excep_regs;
#else
	gp->regs = rp = excep_regs[0];
#endif
    }

    if (gp->pda_nofault) {
#if defined(PDADEBUG)
	_errputs("\n  --> _exception_handler, nofault: vid ");
	sprintn(nbuf, vid_me, 10);
	_errputs(nbuf);
#endif
#ifdef IP30
	cpu_clearnofault();
#endif
	jbp = gp->pda_nofault;
	gp->pda_nofault = 0;
	gp->stack_mode = MODE_NORMAL;  /* reset stack mode! */
	longjmp(*(jmp_buf *)jbp, 1);
	/*NOTREACHED*/
    }

#if MULTIPROCESSOR && !IP30
    if (verbose_faults)
	printf(
	  "\n  ARCS MP fault handler(vid %d/pidx %d):  Unexpected exception\n", 
	  gp->my_virtid, gp->my_physid);
#endif

#if IP30
    /* check for soft power interrupt */
    if (ip30_checkintrs(gp->epc_save, rp[REG(R_RA)]))
	return;
#endif

    mp_show_fault(vid_me);
    clear_nofault();

    panic("Unexpected exception");

    /* don't want to exit, or message won't be seen */
    /*NOTREACHED*/
}

#if IP19
#define ERTOIP_ERRNUM	16
char *ertoip_names[ERTOIP_ERRNUM] = {
  "R4k Data Double ECC",		/* 0 */
  "R4k Data Single ECC",		/* 1 */
  "Tag RAM Parity",			/* 2 */
  "A Chip Addr Parity",			/* 3 */
  "D Chip Data Parity",			/* 4 */
  "Ebus MyReq timeout",			/* 5 */
  "A Chip MyResp drsc tout",		/* 6 */
  "A Chip MyIntrvResp drsc tout",	/* 7 */
  "EBus MyReq Addr Err",		/* 8 */
  "EBus MyData Data Err",		/* 9 */
  "Intern Bus vs. A_SYNC",		/* 10 */
  "Not defined",			/* 11 */
  "Not defined",			/* 12 */
  "Not defined",			/* 13 */
  "Not defined",			/* 14 */
  "Not defined"				/* 15 */
};
#endif

#if IP21
#define ERTOIP_ERRNUM	16
char *ertoip_names[ERTOIP_ERRNUM] = {
  "DB chip Parity error DB0",			/* 0 */
  "DB chip Parity error DB1",			/* 1 */
  "A Chip Addr Parity",				/* 2 */
  "Time Out on MyReq on EBus Channel 0",	/* 3 */
  "Time Out on MyReq on EBus Wback Channel ",	/* 4 */
  "Addr Error on MyReq on EBus Channel 0",	/* 5 */
  "Addr Error on MyReq on EBus Wback Channel ",	/* 6 */
  "Data Sent Error Channel 0",			/* 7 */
  "Data Sent Error Wback Channel ",		/* 8 */
  "Data Receive Error",				/* 9 */
  "Intern Bus vs. A_SYNC",			/* 10 */
  "A Chip MyResponse Data Resources Time Out",  /* 11 */
  "A Chip MyIntrvention Data Resources Time Out",/* 12 */
  "Parity Error on DB - CC shift lines",	/* 13 */
  "Not defined",				/* 14 */
  "Not defined"					/* 15 */
};
#endif

#if IP25
#define ERTOIP_ERRNUM	32
char *ertoip_names[ERTOIP_ERRNUM] = {
  "Not Defined",				/* 0 */
  "Double bit ECC error write/intervention CPU",/* 1 */
  "Single bit ECC error write/intervention CPU",/* 2 */
  "Parity error on Tag RAM data",		/* 3 */
  "A Chip Addr Parity",				/* 4 */
  "MyRequest timeout on EBus",			/* 5 */
  "A Chip MyResponse D-Resource time out",	/* 6 */
  "A Chip MyIntrvention D-Resource time out", 	/* 7 */
  "Addr Error on MyRequest on Ebus",		/* 8 */
  "My Ebus Data Error",				/* 9 */
  "Intern Bus vs. A_SYNC",			/* 10 */
  "CPU bad data indication",			/* 11 */
  "Parity error on data from D-chip [15:0]",	/* 12 */
  "Parity error on data from D-chip [31:16]",	/* 13 */
  "Parity error on data from D-chip [47:32]",	/* 13 */
  "Parity error on data from D-chip [48:63]",	/* 14 */
  "Double bit ECC error CPU SYSAD/Command",	/* 15 */
  "Single bit ECC error CPU SYSAD/Command",	/* 16 */
  "SysState parity error",			/* 17 */
  "SysCmd parity error",			/* 18 */
  "Multiple Errors detected",			/* 19 */
  "Not defined",				/* 20 */
  "Not defined"					/* 21 */
  "Not defined"					/* 22 */
  "Not defined"					/* 23 */
  "Not defined"					/* 24 */
  "Not defined",				/* 25 */
  "Not defined"					/* 26 */
  "Not defined"					/* 27 */
  "Not defined"					/* 28 */
  "Not defined"					/* 29 */
  "Not defined"					/* 30 */
  "Not defined"					/* 31 */
};
#endif

#if EVEREST
static void
gcache_parity(void)
{
#if IP21
	__scunsigned_t	cause;

	cause = getcause();
	if (cause & (CAUSE_IP9 | CAUSE_IP10)) {
		set_cause(cause & ~(CAUSE_IP9 | CAUSE_IP10));
		printf("*** G-cache parity error: CAUSE %x\n", cause);
	}
#endif
}

static void
xlate_ertoip(__uint64_t value)
{
	int i;

	if (value)
		printf("\n*** Error/TimeOut Interrupt(s) Pending: 0x%x ==\n",
									value);
	for (i = 0; i < ERTOIP_ERRNUM; i++)
		if (value & (1 << i))
			printf("\t %s\n", ertoip_names[i]);
}
#endif

/*
 * mp_show_fault -- display info pertinent to fault on specified cpu
 *
 * Use _errputs on nested exceptions so that if printf fails for some
 * reason, we still know that something happened.  We have seen at
 * least one case where an exception printed nothing at all, but kept
 * looping through show_fault(), which made it look like a complete
 * system hang.  Since printf and co. now use malloc, any malloc
 * corruption can cause this to happen.
 */

void
mp_show_fault(int vid)
{
        register volatile struct generic_pda_s *gp;
	register varcs_reg_t *rp;
	k_machreg_t ccode;
#if defined(PDADEBUG)
	uint nf_addr;
#endif
#ifdef EVEREST
	register int ertoip;
#endif

	gp = (volatile struct generic_pda_s *)&GEN_PDA_TAB(vid);
	rp = (varcs_reg_t *)&(gp->regs[0]);

	ignore_xoff = 0;

#if defined(PDADEBUG)
	nf_addr = (uint)&(GEN_PDA_TAB(vid).gdata.notfirst);
	printf( "    !first %d, &!first 0x%x, EPC 0x%x ErrorEPC 0x%x\n", 
		gp->notfirst, (uint)&(gp->notfirst), gp->epc_save,
		gp->error_epc_save);
#endif
	if (gp->notfirst++) {
		char nbuf[10];

		_errputs("\r\nNESTED EXCEPTION #");
		sprintn(nbuf, gp->notfirst-1, 10);
		_errputs(nbuf);
#ifdef MULTIPROCESSOR
		_errputs(" (vid ");
		sprintn(nbuf, vid, 10);
		_errputs(nbuf);
		_errputs(")");
#endif
#if R4000 || R10000
		if (gp->exc_save == EXCEPT_ECC) {
			_errputs(" at ErrorEPC: ");
			sprintn(nbuf, gp->error_epc_save, 16);
		} else
#endif	/* R4000 || R10000 */
		{
			_errputs(" at EPC: ");
			sprintn(nbuf, gp->epc_save, 16);
		}
		_errputs(nbuf);
		_errputs("; first exception at PC: ");
		sprintn(nbuf, gp->first_epc, 16);
		_errputs(nbuf);
		_errputs("\r\n");

		/* allow what message there was to be seen */
		nested_exception_spin();
	}

	printf( "\nException: %r\n", gp->exc_save, except_desc);
	printf( "Status register: %R\n", gp->sr_save, sr_desc);
#if R4000 || R10000
	if (gp->exc_save == EXCEPT_ECC) {
		gp->first_epc = gp->error_epc_save;
		printf( "Error EPC: %#x, Exception RA: %#x\n",
			gp->error_epc_save, rp[REG(R_RA)]);
		desc_err((unsigned)gp->cache_err_save);
		ecc_error_decode((unsigned)gp->cache_err_save);
	} else
#if IP28 || IP30
	if (gp->exc_save == EXCEPT_NMI) {
		/* all info on NMIs */
		gp->first_epc = gp->error_epc_save;
		printf( "Error EPC: %#x\n", gp->error_epc_save);
		printf( "Cause register: %R\n",
			(__psunsigned_t)gp->cause_save, cause_desc);
		printf( "Exception PC: 0x%x, Exception RA: 0x%x\n",
			gp->epc_save,rp[REG(R_RA)]);
#if IP28
		ip28_ecc_error();
#endif	/* IP28 */
	}
	else
#endif /* IP28 || IP30 */
#endif	/* R4000 || R10000 */
	{
		gp->first_epc = gp->epc_save;
		printf( "Cause register: %R\n", (__psunsigned_t)gp->cause_save,
			cause_desc);
		printf( "Exception PC: 0x%x, Exception RA: 0x%x\n",
			gp->epc_save,rp[REG(R_RA)]);
	}

	if (gp->exc_save != EXCEPT_ECC && gp->exc_save != EXCEPT_NMI) {
		int print_badaddr = 0;

		ccode = gp->cause_save & CAUSE_EXCMASK;

		switch (ccode) {
		case EXC_MOD:
			printf("TLB Modified");
			print_badaddr = 1;
			break;
		case EXC_RMISS:
			printf("Read TLB miss");
			print_badaddr = 1;
			break;
		case EXC_WMISS:
			printf("Write TLB miss");
			print_badaddr = 1;
			break;
		case EXC_RADE:
			printf("Read address error");
			print_badaddr = 1;
			break;
		case EXC_WADE:
			printf("Write address error");
			print_badaddr = 1;
			break;
#ifdef R4000
		case EXC_VCEI:
			printf("Instruction-Fetch Virtual Coherency");
			print_badaddr = 1;
			break;
		case EXC_VCED:
			printf("Data-Read Virtual Coherency");
			print_badaddr = 1;
			break;
#endif
		case EXC_IBE:	/* Instruction Bus Error */
		case EXC_DBE:	/* Data Bus Error */
			printf("%s Bus error\n", 
				(ccode == EXC_DBE ? "Data" : "Instruction"));
			break;
		case EXC_OV:		/* OVerflow */
			printf("Integer overflow\n");
			break;
		case EXC_SYSCALL:	/* SYSCALL */
			printf("System call\n");
			break;
		case EXC_CPU:		/* CoProcessor Unusable */
			printf("Coprocessor Unusable\n");
			break;
#if !TFP
		case EXC_FPE:		/* Floating Point Exception */
			printf("Floating-Point: 0x%x\n",GetFPSR());
			break;
#endif

		case EXC_II:
			printf("Reserved Instruction exception, contents of PC = 0x%x\n",
				*(volatile unsigned int *)gp->epc_save);
			break;

		case EXC_INT:		/* interrupt */
			printf( "Interrupt exception\n");
			break;

		case EXC_BREAK:		/* BREAKpoint */
			printf( "Breakpoint exception at address 0x%x\n", 
				gp->badvaddr_save);
			break;
		case EXC_TRAP:		/* Trap exception */
			printf( "Processor Trap exception at address 0x%x\n", 
				gp->badvaddr_save);
			break;

#if R4000 || R10000
		case EXC_WATCH:		/* Watchpoint reference */
			printf( "Watchpoint-Reference exception at address 0x%x\n",
				gp->badvaddr_save);
			break;
#endif /* R4000 || R10000 */

#if R10000
		case EXC_CODE(25):
		case EXC_CODE(26):
			/* IP28/R10000 seems to do this on various ECC errors.
			 * We have already printed an appropriate message.
			 */
			break;
#endif

		default:
			printf("  ??, showfault, bad `cause' case:  code %d (0x%x)\n",
				((gp->cause_save & CAUSE_EXCMASK) >> 2),
				gp->cause_save);
			break;
		}

		if (print_badaddr)
			printf( " exception, bad address: 0x%x\n",
				gp->badvaddr_save);
	}

#if EVEREST
	gcache_parity();
	if (ertoip = load_double_lo((long long *) EV_ERTOIP))
		xlate_ertoip(ertoip);
#endif
#if defined(MULTIPROCESSOR) && !defined(IP30)
	dump_gpda(vid);
#endif
	cpu_show_fault(gp->cause_save);

	if (verbose_faults)
		dump_saved_uregs(vid);
	gp->notfirst = 0;
	return;

} /* mp_show_fault */


/*
 * wrapper to preserve compatibility with the zillions of existing calls
 * which don't specify the faulting cpu to display.
 */
void
show_fault(void)
{
	register int vid_me = cpuid();

	mp_show_fault(vid_me);
}

void
dump_saved_uregs(int vid)
{
        register volatile struct generic_pda_s *gp;
        k_machreg_t *rp;

	gp = (volatile struct generic_pda_s *)&GEN_PDA_TAB(vid);
	rp = &(gp->regs[0]);
	if (!rp) {
	    /*  if pointer not initialized yet, use cpu 0's area */
#ifdef SN
	    gp->regs = rp = (k_machreg_t *)gp->excep_regs;
#else
	    gp->regs = rp = excep_regs[0];
#endif
	}

#if MULTIPROCESSOR
	printf("  VID %d's saved user regs in hex (gpda=0x%x):\n",
		vid, (__scunsigned_t)gp);
#else
	printf("  Saved user regs in hex (gpda=0x%x):\n",
		(__scunsigned_t)gp);
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	printf("  arg: %x %x %x %x\n", 
		rp[REG(R_A0)], rp[REG(R_A1)], rp[REG(R_A2)], rp[REG(R_A3)]);

	printf("  tmp: %x %x %x %x %x %x %x %x\n", 
		rp[REG(R_T0)], rp[REG(R_T1)], rp[REG(R_T2)], rp[REG(R_T3)], 
		rp[REG(R_T4)], rp[REG(R_T5)], rp[REG(R_T6)], rp[REG(R_T7)]); 
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	printf("  arg: %x %x %x %x\n       %x %x %x %x\n", 
		rp[REG(R_A0)], rp[REG(R_A1)], rp[REG(R_A2)], rp[REG(R_A3)],
		rp[REG(R_A4)], rp[REG(R_A5)], rp[REG(R_A6)], rp[REG(R_A7)]); 

	printf("  tmp: %x %x %x %x\n", 
		rp[REG(R_T0)], rp[REG(R_T1)], rp[REG(R_T2)], rp[REG(R_T3)]);
#endif

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	printf("  sve: %x %x %x %x\n       %x %x %x %x\n", 
#else
	printf("  sve: %x %x %x %x %x %x %x %x\n", 
#endif
		rp[REG(R_S0)], rp[REG(R_S1)], rp[REG(R_S2)], rp[REG(R_S3)], 
		rp[REG(R_S4)], rp[REG(R_S5)], rp[REG(R_S6)], rp[REG(R_S7)]); 

#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	printf("  t8 %x t9 %x at %x\n  v0 %x v1 %x k1 %x\n", 
#else
	printf("  t8 %x t9 %x at %x v0 %x v1 %x k1 %x\n", 
#endif
		rp[REG(R_T8)], rp[REG(R_T9)], rp[REG(R_AT)],
		rp[REG(R_V0)], rp[REG(R_V1)], rp[REG(R_K1)]);

	printf("  gp %x fp %x sp %x ra %x\n", rp[REG(R_GP)], rp[REG(R_FP)],
		rp[REG(R_SP)], rp[REG(R_RA)]);

#ifdef DUMP_STACK	/* sometimes handy for debugging */
#define SDDEPTH	8	/* number of lines */
#define SDOFF	0	/* offset from current stack (useful sometimes) */
	{
		__psunsigned_t *p = (__psunsigned_t *)(rp[REG(R_SP)]-SDOFF);
		int i;

		if (p) {
			printf("\nStack dump from 0x%x:\n",p);
			for (i=0; i < 8; i++) {
				printf( "  %016x %016x %016x %016x\n",
					p[0],p[1],p[2],p[3]);
				p += 4;
			}
		}
	}
#endif

	return;
}


#if R4000
#define NO_REF		0
#define D_REF		1
#define T_REF		2
#define DNT_REF		3
/*
 * interpret the terse info-bits of the CacheErr reg, describing where
 * the ECC error occurred
 */
static void
desc_err(unsigned cache_err)
{
	int dandt = NO_REF;
	extern struct reg_desc cache_err_desc[];

	printf( "CacheErr %R\n", cache_err, cache_err_desc);

	if ((cache_err & CACHERR_ED) && (cache_err & CACHERR_ET))
		dandt = DNT_REF;
	else if (cache_err & CACHERR_ET)
		dandt = T_REF;
	else if (cache_err & CACHERR_ED)
		dandt = D_REF;
	/* else a wierd setting! */

	printf( "--> ECC/Parity ERROR ");
	if (cache_err & CACHERR_ES)
		printf("RESULTING FROM EXTERNAL RQST!\n");
	else if (cache_err & CACHERR_EE)
		printf( "on the SysAD bus\n");
	else {
		printf( " during %s reference, %s cache\n",
			((cache_err & CACHERR_ER)?"data":"instr"),
			((cache_err & CACHERR_EC)?"secondary":"primary"));
		if (dandt) {
			if (dandt == DNT_REF)
				printf("        data- AND tag-field error\n");
			else
				printf( "        %s-field error\n",
					(dandt == D_REF?"data":"tag"));
		}
	}
}

#elif R10000

#ifdef DEBUG
#define DEBUG_CACHE	1
#define DEBUG_SDATA	1
#define DEBUG_PDATA	1
#define DEBUG_SYSAD	1
#endif

#if IP28 && !DEBUG		/* we need data about the primary caches */
#define DEBUG_CACHE	1
#define DEBUG_PDATA	1
#endif

#if DEBUG_SDATA || DEBUG_PDATA
static void
report_cache_data_err(int cache, __psunsigned_t c_idx)
{
	int count;		/* # of cacheops to get all relevant data */
	__uint32_t ecc;
	int incr;		/* index increment per cacheop */
	__uint32_t tag[2];

	if (cache == CACH_PI) {
		count = 16;	/* PIdx field has block resolution */
		incr = 0x4;
	}
	else if (cache == CACH_PD) {
		count = 2;	/* PIdx field has doubleword resolution */
		incr = 0x4;
	}
	else {
		count = 16;	/* SIdx field has block resolution */
		incr = 0x8;
	}

	printf("Data at %#06x:\n", c_idx);
	while (count--) {
		ecc = _read_cache_data(cache, PHYS_TO_K0(c_idx), tag);
		if (cache == CACH_PD)
			printf("\t%#08x\t   Parity:%#x\n",
				(__uint64_t)tag[0], ecc);
		else if (cache == CACH_PI)
			printf("\t%#x%08x\t   Parity:%#x\n",
				(__uint64_t)tag[1], (__uint64_t)tag[0], ecc);
		else
			printf("\t%#08x%08x\t   Parity+ECC:%#x\n",
				(__uint64_t)tag[1], (__uint64_t)tag[0], ecc);

		c_idx += incr;
	}
}
#endif /* DEBUG_SDATA || DEBUG_PDATA */

#ifdef DEBUG_CACHE
static void
report_cache_tag_err(int cache, __psunsigned_t c_idx)
{
	extern struct reg_desc p_taglo_desc[];
	extern struct reg_desc s_taglo_desc[];
	__uint32_t tag[2];

	(void)_read_tag(cache, PHYS_TO_K0(c_idx), tag);
	printf("\tTag @ %#06x: %#016R\n", c_idx, (__uint64_t)tag[1]<<32|tag[0],
	       (cache == CACH_SD) ? s_taglo_desc : p_taglo_desc);
}
#endif	/* DEBUG_CACHE */

static void
desc_err(unsigned cache_err)
{
	extern struct reg_desc pcache_err_desc[];
	extern struct reg_desc scache_err_desc[];
	extern struct reg_desc sysad_err_desc[];
	struct reg_desc *p;
#ifdef DEBUG_CACHE
	__psunsigned_t c_idx;
#endif	/* DEBUG_CACHE */

	switch (cache_err & CACHERR_SRC_MSK) {
	case CACHERR_SRC_PI:
		printf("Parity Error on the primary instructions cache\n");
		p = pcache_err_desc;
#ifdef DEBUG_CACHE
		c_idx = cache_err & CACHERR_PIDX_MASK;
#ifdef DEBUG_PDATA
		if (cache_err & CE_D_WAY0)
			report_cache_data_err(CACH_PI, c_idx);
		if (cache_err & CE_D_WAY1)
			report_cache_data_err(CACH_PI, c_idx | 0x1);
#endif
		report_cache_tag_err(CACH_PI, c_idx);
		report_cache_tag_err(CACH_PI, c_idx | 0x1);
#endif	/* DEBUG_CACHE */
		break;

	case CACHERR_SRC_PD:
		printf("Parity Error on the primary data cache\n");
		p = pcache_err_desc;
#ifdef DEBUG_CACHE
		c_idx = cache_err & CACHERR_PIDX_MASK;
#ifdef DEBUG_PDATA
		if (cache_err & CE_D_WAY0)
			report_cache_data_err(CACH_PD, c_idx);
		if (cache_err & CE_D_WAY1)
			report_cache_data_err(CACH_PD, c_idx | 0x1);
#endif
		report_cache_tag_err(CACH_PD, c_idx);
		report_cache_tag_err(CACH_PD, c_idx | 0x1);
#endif	/* DEBUG_CACHE */
		break;

	case CACHERR_SRC_SD:
		printf("ECC Error on the secondary cache\n");
		p = scache_err_desc;
#ifdef DEBUG_CACHE
		c_idx = cache_err & CACHERR_SIDX_MASK;
#ifdef DEBUG_SDATA
		if (cache_err & CE_D_WAY0)
			report_cache_data_err(CACH_SD, c_idx);
		if (cache_err & CE_D_WAY1)
			report_cache_data_err(CACH_SD, c_idx | 0x1);
#endif
		report_cache_tag_err(CACH_SD, c_idx);
		report_cache_tag_err(CACH_SD, c_idx | 0x1);
#endif	/* DEBUG_CACHE */
		break;

	case CACHERR_SRC_SYSAD:
		printf("ECC Error on the SYSAD bus\n");
		p = sysad_err_desc;
#ifdef DEBUG_SYSAD
		c_idx = cache_err & CACHERR_SIDX_MASK;
		if (cache_err & CE_D_WAY0)
			report_cache_data_err(CACH_SD, c_idx);
		if (cache_err & CE_D_WAY1)
			report_cache_data_err(CACH_SD, c_idx | 0x1);
#endif	/* DEBUG_SYSAD */
		break;
	}

	printf("\tCacheErr %R\n", (__psunsigned_t)cache_err, p);
}
#endif /* R4000/R10000 */
