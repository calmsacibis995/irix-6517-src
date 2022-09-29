/**************************************************************************
 *									  *
 *		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.66 $"

/*
 * heart_err.c - IP30 error handling
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/ecc.h>
#include <sys/kabi.h>
#include <sys/pda.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/atomic_ops.h>

#include <sys/RACER/IP30.h>

#include "RACERkern.h"

#define F_KERN		0
#define F_KERN_NF	1
#define F_USER		2

#undef	XWIDGET_NONE
#define XWIDGET_NONE	0xFF
#define XWIDGET_MEMORY	0xFE

#define	HEART_MEM_PHYSADDR_DW_MSK	(HME_PHYS_ADDR & ~0x7)

#define SYNC	(void)heart_piu->h_sync

#if ECC_DEBUG
extern volatile ulong_t ecc_test_addr;
extern volatile ulong_t ecc_test_sb_count;
extern volatile ulong_t ecc_test_db_count;
#endif

extern xwidgetnum_t	heart_paddr_to_port(iopaddr_t, iopaddr_t *, iopaddr_t *);
extern int		heart_error_refine(char *, int, ioerror_mode_t, ioerror_t *);

/*
 * function table of contents
 */

int			earlynofault(struct eframe_s *, uint);
void			idle_err(inst_t *, uint, void *, void *);
int			dobuserre(eframe_t *, inst_t *, uint);
int			dobuserr(eframe_t *, inst_t *, uint);
int			dobuserr_common(eframe_t *, inst_t *, uint, int);

static paddr_t		tlb_to_phys(caddr_t);

/* Handle very early global faults.  Returns:
 *    1 if should do nofault
 *	0 if not
 */
int
earlynofault(struct eframe_s *ep, uint code)
{
    if (code != EXC_DBE)
	return (0);

    dobuserre(ep, (inst_t *) ep->ef_epc, F_KERN_NF);

    return (1);
}

/* Should never receive an exception (other than interrupt) while
 * running on the idle stack -- Check for memory errors.
 */
void
idle_err(inst_t *epc, uint cause, void *k1, void *sp)
{
    if ((cause & CAUSE_EXCMASK) == EXC_IBE ||
	(cause & CAUSE_EXCMASK) == EXC_DBE)
	dobuserre(NULL, epc, F_KERN);
    else if (cause & CAUSE_BERR_INTR)
	dobuserr(NULL, epc, F_KERN);

    cmn_err(CE_PANIC,
	    "exception on IDLE stack "
	    "k1:0x%x epc:0x%x cause:0x%w32x sp:0x%x badvaddr:0x%x",
	    k1, epc, cause, sp, getbadvaddr());
    /* NOTREACHED */
}

/*
 * dobuserre - handle bus error exception
 */
int
dobuserre(eframe_t *ep, inst_t *epc, uint flag)
{
    return dobuserr_common(ep, epc, flag, 1);
}

/*
 * dobuserr - handle bus error interrupt
 */
int
dobuserr(eframe_t *ep, inst_t *epc, uint flag)
{
    return dobuserr_common(ep, epc, flag, 0);
}

volatile int		LLPRxCnt255_count;
volatile int		LLPTxCnt255_count;

#define HEM_ADD_STR(s)		cmn_err(CE_CONT, "%s", s)

#define HEM_ADD_NVAR(n,v)	cmn_err(CE_CONT,			\
					"\t%20s: 0x%X\n",		\
					n, v)

#define HEM_ADD_NSVAR(n,v)	cmn_err(CE_CONT,			\
					((v) == NULL)			\
					? "\t%20s: NULL\n"		\
					: "\t%20s: '%s'\n",		\
					n, v)

#define HEM_ADD_XIO(x,v)	cmn_err(CE_CONT,			\
					"\tXIO port %X addr: 0x%X\n",	\
					x, v)

#define HEM_ADD_NREG(n,v)	cmn_err(CE_CONT,			\
					"\t%20s: %R\n",			\
					n, v, v ## _desc)

#define HEM_ADD_VAR(v)		HEM_ADD_NVAR(#v, v)
#define HEM_ADD_SVAR(v) 	HEM_ADD_NSVAR(#v, v)
#define HEM_ADD_REG(v)		HEM_ADD_NREG(#v, v)

static void
hem_add_dimm(char *n, iopaddr_t v)
{
    extern char		   *maddr_to_dimm(paddr_t);
    char		   *dimm = maddr_to_dimm(v);

    HEM_ADD_NVAR(n, v);
    cmn_err(CE_CONT,
	    (dimm && *dimm)
	    ? "\t(This address is within DIMM %s's assigned range)\n"
	    : "\t(This address is not within any DIMM's assigned range)\n",
	    dimm);
}

#define HEM_ADD_NDIMM(n,v)	hem_add_dimm(n, v)
#define HEM_ADD_DIMM(v)		HEM_ADD_NDIMM(#v,v)

static void
hem_add_sync(char *sync_type,
	     caddr_t sync_vaddr,
	     paddr_t sync_paddr,
	     int sync_xport,
	     iopaddr_t sync_xaddr,
	     iopaddr_t sync_maddr)
{
    HEM_ADD_SVAR(sync_type);
    HEM_ADD_VAR(sync_vaddr);
    HEM_ADD_VAR(sync_paddr);
    if (sync_xport == XWIDGET_MEMORY)
	HEM_ADD_DIMM(sync_maddr);
    else if (sync_xport != XWIDGET_NONE)
	HEM_ADD_XIO(sync_xport, sync_xaddr);
}

#define HEM_ADD_SYNC()	if (sync_valid)					    \
			    hem_add_sync(sync_type, sync_vaddr, sync_paddr, \
					 sync_xport, sync_xaddr, sync_maddr)

#define HEM_ADD_IOEF(n) if (IOERROR_FIELDVALID(ioe,n))			    \
			    HEM_ADD_NVAR("ioe." #n,			    \
					 IOERROR_GETVALUE(ioe,n))

static void
hem_add_ioe(ioerror_t *ioe)
{
#if !DEBUG
    if (kdebug) {
#endif
	HEM_ADD_STR("DEBUG DATA -- intermediate ioerror struct:\n");
	HEM_ADD_IOEF(errortype);
	HEM_ADD_IOEF(widgetnum);
	HEM_ADD_IOEF(widgetdev);
	HEM_ADD_IOEF(srccpu);
	HEM_ADD_IOEF(srcnode);
	HEM_ADD_IOEF(errnode);
	HEM_ADD_IOEF(sysioaddr);
	HEM_ADD_IOEF(xtalkaddr);
	HEM_ADD_IOEF(busspace);
	HEM_ADD_IOEF(busaddr);
	HEM_ADD_IOEF(vaddr);
	if (IOERROR_FIELDVALID(ioe, memaddr))
	    HEM_ADD_NDIMM("ioe.memaddr", IOERROR_GETVALUE(ioe, memaddr));
	HEM_ADD_IOEF(epc);
	HEM_ADD_IOEF(ef);
#if !DEBUG
    }
#endif
}

#define HEM_ADD_IOE()	(hem_add_ioe(ioe))

/* periodic: report once per minute, max. */
#define HEM_ADD_PERIODIC_WARN(errstr)					\
		do {							\
		    static unsigned	times_since_boot = 0;		\
		    static time_t	next_print = 0;			\
		    times_since_boot ++;				\
		    if (next_print <= time) {				\
		    HEM_ADD_STR(errstr);				\
			HEM_ADD_VAR(times_since_boot);			\
			next_print = time + 60;				\
		    }							\
		} while (0)

/* Track most recent snapshot of data from
 * the Heart for each CPU in the system.
 *
 * XXX- reentrancy may cause problems, since
 * the "inner" will still see all the error
 * bits being handled by the "outer" instance.
 * We may have to divide the handler up based
 * on whether this is a HEART_EXC or CPU_BERR.
 */
heartreg_t		_heart_mode[MAXCPU];
heartreg_t		_heart_cause[MAXCPU];
heartreg_t		_heart_piur_acc_err[MAXCPU];
heartreg_t		_heart_berr_misc[MAXCPU];
heartreg_t		_heart_berr_addr[MAXCPU];
heartreg_t		_heart_wid_err_type[MAXCPU];
heartreg_t		_heart_w_err_cmd_word[MAXCPU];
heartreg_t		_heart_w_err_upper_addr[MAXCPU];
heartreg_t		_heart_w_err_lower_addr[MAXCPU];
heartreg_t		_heart_wid_pio_rdto_addr[MAXCPU];
heartreg_t		_heart_wid_pio_err_upper[MAXCPU];
heartreg_t		_heart_wid_pio_err_lower[MAXCPU];
heartreg_t		_heart_memerr_addr[MAXCPU];
heartreg_t		_heart_memerr_data[MAXCPU];

#define heart_mode		_heart_mode[this_cpu]
#define heart_cause		_heart_cause[this_cpu]
#define heart_piur_acc_err	_heart_piur_acc_err[this_cpu]
#define heart_berr_misc		_heart_berr_misc[this_cpu]
#define heart_berr_addr		_heart_berr_addr[this_cpu]
#define heart_wid_err_type	_heart_wid_err_type[this_cpu]
#define heart_w_err_cmd_word	_heart_w_err_cmd_word[this_cpu]
#define heart_w_err_upper_addr	_heart_w_err_upper_addr[this_cpu]
#define heart_w_err_lower_addr	_heart_w_err_lower_addr[this_cpu]
#define heart_wid_pio_rdto_addr _heart_wid_pio_rdto_addr[this_cpu]
#define heart_wid_pio_err_upper _heart_wid_pio_err_upper[this_cpu]
#define heart_wid_pio_err_lower _heart_wid_pio_err_lower[this_cpu]
#define heart_memerr_addr	_heart_memerr_addr[this_cpu]
#define heart_memerr_data	_heart_memerr_data[this_cpu]

#define EOT	{0}

#define F(s,n)		{ 1l<<(s),-(s), n }
#define V(w,s,n,f)	{ ((1l<<w)-1)<<(s),-(s),n,(f) }
#define T(w,s,n,t)	{ ((1l<<w)-1)<<(s),-(s),n,0,(t) }
#define PFB(s,n,x)	F(s+x,n ## #x)
#define PF(s,n)		PFB(s,n,3),PFB(s,n,2),PFB(s,n,1),PFB(s,n,0)
#define D(w,s,n)	V(w,s,n,"%d")
#define X(w,s,n)	V(w,s,n,"%x")

struct reg_values	h_mode_trig_tbl[] =
{
    {0, "LinkStatus0"},
    {1, "TrigBit0"},
    {2, "HEART_Exc"},
    {3, "Walked_iorwrb_vld"},
    {4, "Cohdone_mux"},
    {5, "Rb2i_exec_compl_out"},
    {6, "reserved"},
    {7, "reserved"},
    {0}
};

struct reg_desc		h_mode_desc[] =
{
    PF(28, "ProcDis"),
    D(3, 57, "MaxPSR"),
    D(3, 54, "MaxIOSR"),
    D(3, 51, "MaxPendIOSR"),
    T(3, 48, "TrigSrcSel", h_mode_trig_tbl),
    X(4, 40, "PIU_TestMode"),
    X(4, 36, "GP_Flag"),
    D(4, 32, "MaxProcHyst"),
    F(29, "ClrXtkUsedCrdCnt"),
    F(28, "LLPWarmRstAfterRst"),
    F(27, "LLPLinkRst"),
    F(26, "LLPWarmRst"),
    F(25, "CorECC_Lock"),
    F(24, "ReducedPower"),
    F(23, "ColdRst"),
    F(22, "SoftwareReset"),
    F(21, "MemForceWr"),
    F(20, "DB_ErrGen"),
    F(19, "SB_ErrGen"),
    F(18, "CachedPIO_En"),
    F(17, "CachedPROM_En"),
    F(16, "PE_SysCorErrERE"),
    F(15, "GlobalECC_En"),
    F(14, "IO_CohEn"),
    F(13, "IntEn"),
    F(12, "DataChkEn"),
    F(11, "RefEn"),
    F(10, "BadSysWrERE"),
    F(9, "BadSysRdERE"),
    F(8, "SysStateERE"),
    F(7, "SysCmdERE"),
    F(6, "NCorSysERE"),
    F(5, "CorSysERE"),
    F(4, "DataElmntERE"),
    F(3, "MemAddrProcERE"),
    F(2, "MemAddrIO_ERE"),
    F(1, "NCorMemERE"),
    F(0, "CorMemERE"),
    {0}
};

struct reg_desc		h_cause_desc[] =
{
    PF(60, "PE_SysCorErr"),
    F(44, "PIOWDB_OV"),
    F(43, "PIORWRB_OV"),
    F(42, "PIUR_AccessErr"),
    F(41, "BadSysWrErr"),
    F(40, "BadSysRdErr"),
    PF(36, "SysStateErrProc"),
    PF(32, "SysCmdErrProc"),
    PF(28, "NCorSysErrProc"),
    PF(24, "CorSysErrProc"),
    PF(20, "DataElmntErrProc"),
    F(16, "WidgetError"),
    PF(4, "MemAddrErrProc"),
    F(2, "MemAddrErrIO"),
    F(1, "NCorMemErr"),
    F(0, "CorMemErr"),
    {0}
};

struct reg_values	h_piur_acc_err_type_v[] =
{
    {0, "Rd"},
    {1, "Wr"},
    {0}
};

struct reg_desc		h_piur_acc_err_desc[] =
{
    T(1, 22, "PIU_AccType", h_piur_acc_err_type_v),
    D(2, 20, "PIU_AccProcNum"),
    {0xFFFF8, 0, "PIU_AccErrAddr", "%x"},
    {0}
};

struct reg_desc		h_berr_misc_desc[] =
{
    F(23, "BadRdWrtbk"),
    D(2, 21, "PBErrProcNum"),
    X(8, 13, "PBErrSyndrome"),
    F(12, "PBErrSysCmdPar"),
    X(12, 0, "PBErrSysCmd"),
    {0}
};

struct reg_desc		h_w_err_type_desc[] =
{
    F(29, "XbarCreditOver"),
    F(28, "XbarCreditUnder"),
    F(27, "IO_NonHead"),
    F(26, "IO_BadFormat"),
    F(25, "IOResp_UnexpResp"),
    F(24, "IOR_BufOverErr"),
    F(23, "IOR_CmdErr"),
    F(22, "IOR_CmdWarn"),
    F(21, "IOR_IntVectErr"),
    F(20, "IOR_IntVectWarn"),
    F(18, "LLP_RcvWarmReset"),
    F(17, "LLP_RcvLinkReset"),
    F(16, "LLP_RcvSNError"),
    F(15, "LLP_RcvCBError"),
    F(14, "LLP_RcvSquashData"),
    F(13, "LLP_TxRetryTO"),
    F(12, "LLP_TxUpktRetry"),
    F(11, "LLP_REC_CNT_255"),
    F(10, "LLP_REC_CNT_255"),
    F(3, "PIO_RdTO"),
    F(2, "PIO_WrTO"),
    F(1, "PIO_XT_AccessErr"),
    F(0, "PIO_WCR_AccessErr"),
    {0}
};

struct reg_values	h_w_err_cmd_word_pactyp_v[] =
{
    {0x0, "rd_req"},
    {0x1, "rd_resp"},
    {0x2, "wr_req_with_resp"},
    {0x3, "wr_resp"},
    {0x4, "wr_req_no_resp"},
    {0x6, "fetch_and_op"},
    {0x8, "store_and_op"},
    {0xE, "special_req"},
    {0xF, "special_resp"},
    {0}
};

struct reg_values	h_w_err_cmd_word_ds_v[] =
{
    {0, "DW"},
    {1, "QCL"},
    {2, "CL"},
    {0}};
struct reg_desc		h_w_err_cmd_word_desc[] =
{
    X(4, 28, "DIDN"),
    X(4, 24, "SIDN"),
    T(4, 20, "PacTyp", h_w_err_cmd_word_pactyp_v),
    X(5, 15, "Tnum"),
    F(14, "Coherent"),
    T(2, 12, "Size", h_w_err_cmd_word_ds_v),
    F(11, "GBR"),
    F(10, "VBPM"),
    F(9, "ERROR"),
    F(8, "Barrier"),
    X(8, 0, "Other"),
    {0}
};

struct reg_desc		h_w_err_upper_addr_desc[] =
{
    X(8, 16, "DE"),
    X(16, 0, "UPP_ADDR"),
    {0}
};

struct reg_desc		h_w_err_lower_addr_desc[] =
{
    X(32, 0, "LOW_ADDR"),
    {0}
};

struct reg_values	h_w_pio_rdto_addr_space_v[] =
{
    {0, "small"},
    {1, "medium"},
    {2, "large"},
    {0}
};

struct reg_desc		h_w_pio_rdto_addr_desc[] =
{
    D(2, 18, "ProcNum"),
    T(2, 16, "IO_Space", h_w_pio_rdto_addr_space_v),
    X(4, 12, "DIDN"),
    {0xFFF, 12, "PhyAddr"},
    {0}
};

struct reg_desc		h_w_pio_err_upper_desc[] =
{
    D(2, 22, "ProcNum"),
    X(2, 20, "UncAttr"),
    X(11, 8, "SysCmd"),
    X(8, 0, "PhyAddrHi"),
    {0}
};

struct reg_desc		h_w_pio_err_lower_desc[] =
{
    X(32, 0, "PhyAddrLo"),
    {0}
};

struct reg_values	h_memerr_addr_src_v[] =
{
    {0, "cpu0"},
    {1, "cpu1"},
    {2, "cpu2"},
    {3, "cpu3"},
    {4, "I/O"},
    {0}
};

struct reg_values	h_memerr_addr_type_v[] =
{
    {0, "data"},
    {1, "address"},
    {0}
};

struct reg_desc		h_memerr_addr_desc[] =
{
    T(3, 43, "ReqSrc", h_memerr_addr_src_v),
    T(1, 42, "Type", h_memerr_addr_type_v),
    X(8, 34, "Syndrome"),
    X(34, 0, "PhyAddr"),
    {0}
};

int			heart_SysCorErr_count[MAXCPU];
time_t			heart_SysCorErr_ptime[MAXCPU];

#define HEART_WHO_INITIAL	(0x000)
#define HEART_WHO_OWN(n)	(0x001<<(n))
#define HEART_WHO_OWNS		(0x00F)
#define HEART_WHO_WANT(n)	(0x010<<(n))
#define HEART_WHO_WANTS		(0x0F0)
#define HEART_WHO_NEST(n)	(0x100<<(n))
#define HEART_WHO_NESTS		(0xF00)

#define	data_eccsyns		real_data_eccsyns

volatile unsigned	heart_error_cpumap = HEART_WHO_INITIAL;

/* dobuserr_common - diagnose errors, common
 * to both synchronous and asynchronous paths.
 */
/*ARGSUSED */
int
dobuserr_common(eframe_t *ef, inst_t *epc, uint flag, int sync)
{
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;
    heart_cfg_t		   *heart_cfg = HEART_CFG_K1PTR;
    ioerror_mode_t	    error_mode;
    ioerror_t		    ioe[1];

    int			    this_cpu = cpuid();

    int			    fatal = 0;
    int			    handled = 0;

    int			    sync_valid;
    int			    sync_pend;
    char		   *sync_type;
    caddr_t		    sync_vaddr;
    paddr_t		    sync_paddr;
    int			    sync_xport;
    iopaddr_t		    sync_xaddr;
    iopaddr_t		    sync_maddr;

    heartreg_t		    piur_valid; /* h_piur_acc_err */
    heartreg_t		    berr_valid; /* h_berr_{misc,addr} */
    heartreg_t		    werr_valid; /* w_err_{cmd_word,{upper,lower}_addr} */
    heartreg_t		    rdto_valid; /* h_w_pio_rdto_addr */
    heartreg_t		    wpio_valid; /* h_w_pio_error_{upper,lower} */

    /*REFERENCED*/
    heartreg_t		    merr_valid; /* h_memerr_addr */

    heartreg_t		    h_mode;
    heartreg_t		    h_cause;
    heartreg_t		    h_piur_acc_err;
    heartreg_t		    h_berr_misc;
    heartreg_t		    h_berr_addr;
    heartreg_t		    h_w_err_type;
    heartreg_t		    h_w_err_cmd_word;
    heartreg_t		    h_w_err_upper_addr;
    heartreg_t		    h_w_err_lower_addr;
    heartreg_t		    h_w_pio_rdto_addr;
    heartreg_t		    h_w_pio_err_upper;
    heartreg_t		    h_w_pio_err_lower;
    heartreg_t		    h_memerr_addr;
    heartreg_t		    h_memerr_data;

    heartreg_t		    h_cause_pend;
    heartreg_t		    h_w_err_type_pend;

    heartreg_t		    some_bits;

#if ECC_DEBUG
    int			    is_ecc_test;
#endif

    unsigned		    myobit = HEART_WHO_OWN(this_cpu);
    unsigned		    mywbit = HEART_WHO_WANT(this_cpu);
    unsigned		    mynbit = HEART_WHO_NEST(this_cpu);
    unsigned		    cpumap = heart_error_cpumap;
    unsigned		    cpumap_clr;

    if (cpumap & myobit) {
	/* already own it -- must be a "nest" call. */
	cpumap = atomicSetUint(&heart_error_cpumap, mynbit);
	cpumap_clr = (mynbit & ~cpumap);
    } else {
	heartreg_t	    ipi_bit = HEART_IMR_IPI_(this_cpu);
	extern void         heart_intr(struct eframe_s *, heartreg_t);

	cpumap = atomicSetUint(&heart_error_cpumap, mywbit);
	cpumap_clr = (mywbit & ~cpumap) | myobit;
	while (((cpumap = heart_error_cpumap) & HEART_WHO_OWNS) ||
	       (!compare_and_swap_int((int *) &heart_error_cpumap,
				      cpumap, cpumap | myobit))) {

	    /* spin here until we own the heart;
	     * if we panic, check for (and execute)
	     * any incoming IPIs.
	     */
	    if (panicstr)
		if (heart_piu->h_isr & ipi_bit)
		    heart_intr(ef, ipi_bit);
		else
		    DELAY(5);	/* don't beat on h_isr */
	}
    }

    /* Always read everything,
     * even if we thing the data
     * is not stale, so we have
     * a shot at debugging any
     * strange problems.
     */
    heart_mode = h_mode = heart_piu->h_mode;
    heart_cause = h_cause = heart_piu->h_cause;
    heart_piur_acc_err = h_piur_acc_err = heart_piu->h_piur_acc_err;
    heart_berr_misc = h_berr_misc = heart_piu->h_berr_misc;
    heart_berr_addr = h_berr_addr = heart_piu->h_berr_addr;
    heart_wid_err_type = h_w_err_type = heart_cfg->h_wid_err_type;
    heart_w_err_cmd_word = h_w_err_cmd_word = heart_cfg->h_wid_err_cmd_word;
    heart_w_err_upper_addr = h_w_err_upper_addr = heart_cfg->h_wid_err_upper_addr;
    heart_w_err_lower_addr = h_w_err_lower_addr = heart_cfg->h_wid_err_lower_addr;
    heart_wid_pio_rdto_addr = h_w_pio_rdto_addr = heart_cfg->h_wid_pio_rdto_addr;
    heart_wid_pio_err_upper = h_w_pio_err_upper = heart_cfg->h_wid_pio_err_upper;
    heart_wid_pio_err_lower = h_w_pio_err_lower = heart_cfg->h_wid_pio_err_lower;
    heart_memerr_addr = h_memerr_addr = heart_piu->h_memerr_addr;
    heart_memerr_data = h_memerr_data = heart_piu->h_memerr_data;

    /* decide which registers we want to diagnose */
    piur_valid = h_cause
	& (HC_PIUR_ACC_ERR);
    berr_valid = h_cause
	& (HC_BAD_SYSWR_ERR |
	   HC_BAD_SYSRD_ERR |
	   HC_NCOR_SYSAD_ERR_MSK |
	   HC_COR_SYSAD_ERR_MSK);
    merr_valid = h_cause
	& (HC_MEM_ADDR_ERR_PROC(this_cpu) |
	   HC_MEM_ADDR_ERR_IO |
	   HC_NCOR_MEM_ERR |
	   HC_COR_MEM_ERR);

    werr_valid = h_w_err_type
	& (ERRTYPE_IO_BAD_FORMAT |
	   ERRTYPE_IOR_CMD_ERR |
	   ERRTYPE_IOR_CMD_WARN |
	   ERRTYPE_IOR_INT_VEC_ERR |
	   ERRTYPE_IOR_INT_VEC_WARN);

    rdto_valid = h_w_err_type
	& (ERRTYPE_PIO_RD_TIMEOUT);

    wpio_valid = h_w_err_type
	& (ERRTYPE_PIO_WR_TIMEOUT |
	   ERRTYPE_PIO_XTLK_ACC_ERR |
	   ERRTYPE_PIO_WCR_ACC_ERR);
#if ECC_DEBUG
    /* figure out if this is an ECC TEST stimulated error */
    is_ecc_test =
	(merr_valid && ecc_test_addr &&
	 ((HEART_MEM_PHYSADDR_DW_MSK & h_memerr_addr) ==
	  (HEART_MEM_PHYSADDR_DW_MSK & ecc_test_addr)));
#endif

    switch (flag) {
    case F_KERN:
	error_mode = MODE_DEVERROR;
	break;
    case F_KERN_NF:
	error_mode = MODE_DEVPROBE;
	break;
    case F_USER:
	error_mode = MODE_DEVUSERERROR;
	break;
    }

    /*
     * Figure out much address info based on eframe.
     * We match the sync error data with errors in
     * heart by comparing logged address information.
     */
    if (sync && ef) {

	/*
	 * Get the virtual address at fault by interpreting the
	 * instruction stream and saved CPU registers. This
	 * provides us with more information than just looking
	 * at what is logged in Heart (which may apply to
	 * some other processor).
	 */
	if ((ef->ef_cause & CAUSE_EXCMASK) == EXC_IBE) {
	    sync_type = "IBE";
	    sync_vaddr = (caddr_t) ef->ef_epc;
	    if (ef->ef_cause & CAUSE_BD)
		sync_vaddr += sizeof(inst_t);
	} else if ((ef->ef_cause & CAUSE_EXCMASK) == EXC_DBE) {
	    sync_type = "DBE";
	    sync_vaddr = (caddr_t) ldst_addr(ef);
	} else {
	    sync_type = "unknown";
	    ASSERT(0);
	}

	/* translate the virtual address into physical address */
	if (IS_XKPHYS(sync_vaddr))	/* 64-bit direct mapped address */
	    sync_paddr = XKPHYS_TO_PHYS(sync_vaddr);
	else if (IS_COMPAT_PHYS(sync_vaddr))	/* 32-bit direct mapped addr */
	    sync_paddr = COMPAT_TO_PHYS(sync_vaddr);
	else				/* mapped address */
	    sync_paddr = tlb_to_phys(sync_vaddr);

	sync_xport = heart_paddr_to_port(sync_paddr, &sync_maddr, &sync_xaddr);
	sync_valid = 1;
    } else
	sync_valid = 0;
    h_cause_pend = h_cause;
    h_w_err_type_pend = h_w_err_type;
    sync_pend = sync_valid;

    /* Bug #455466: if we get a sync DBE for a PIO_RdTO,
     * the h_cause(HC_WIDGET_ERR) bit is not turned on
     * since the interrupt enable bit for this error
     * is off. Allow us to diagnose this error by forcing
     * the HC_WIDGET_ERR bit on.
     */
    if (sync_pend && rdto_valid)
	h_cause_pend |= HC_WIDGET_ERR;

#if DEBUG && ERROR_DEBUG
#if ECC_DEBUG
    if (!is_ecc_test)
#endif
    {
	HEM_ADD_STR("DEBUG DATA: dobuserr_common");
	if (flag == F_KERN)
	    HEM_ADD_STR(" KERNEL");
	if (flag == F_KERN_NF)
	    HEM_ADD_STR(" PROBE");
	if (flag == F_USER)
	    HEM_ADD_STR(" USER");
	if (sync)
	    HEM_ADD_STR(" SYNC");
	else
	    HEM_ADD_STR(" ASYNC");

#if ECC_DEBUG
	if (is_ecc_test)
	    HEM_ADD_STR(" ECCTEST");
#endif

	HEM_ADD_STR("\n");

#if ECC_DEBUG
	if (ecc_test_addr)
	    HEM_ADD_VAR(ecc_test_addr);
#endif
	if (ef)
	    HEM_ADD_VAR(ef);
	if (epc)
	    HEM_ADD_VAR(epc);
	if (sync_valid)
	    HEM_ADD_SYNC();
	HEM_ADD_REG(h_mode);
	HEM_ADD_REG(h_cause);
	if (h_cause != 0) {
	    if (piur_valid)
		HEM_ADD_REG(h_piur_acc_err);

	    if (berr_valid) {
		HEM_ADD_REG(h_berr_misc);
		HEM_ADD_VAR(h_berr_addr);
	    }
	    if (h_cause & HC_WIDGET_ERR) {
		HEM_ADD_REG(h_w_err_type);
		if (werr_valid) {
		    HEM_ADD_REG(h_w_err_cmd_word);
		    HEM_ADD_REG(h_w_err_upper_addr);
		    HEM_ADD_REG(h_w_err_lower_addr);
		}
		if (rdto_valid)
		    HEM_ADD_REG(h_w_pio_rdto_addr);
		if (wpio_valid) {
		    HEM_ADD_REG(h_w_pio_err_upper);
		    HEM_ADD_REG(h_w_pio_err_lower);
		}
	    }
	    if (merr_valid) {
		HEM_ADD_REG(h_memerr_addr);
		HEM_ADD_VAR(h_memerr_data);
	    }
	}
    }
#endif

    /*
     * I classify heart cause bits somewhat
     * differently from the Spec. Rather than
     * diagnose them based on functional units,
     * I divide them by triage.
     *
     * First, check for the errors that are
     * presumably rare and immediately fatal;
     * these get quickly and efficiently pulled
     * out of the mix.
     *
     * Second, check for errors that we may be
     * able to hand off to error handlers
     * registered for various crosstalk widgets;
     * it is possible that we have a normally
     * ugly situation which the widget knows
     * about and cleans up after.
     *
     * Third, we check for correctable ECC errors
     * and apply corrective measures to memory as
     * appropriate.
     *
     * Fourth, we report any unhandled heart
     * errors (including sync_vaddr/sync_paddr set
     * above, if no heart cause accounted for
     * and cleaned up after them).
     */

    if (!h_cause_pend)
	goto no_cause;

    /*
     * Error Triage Case 1: immediately fatal stuff.
     *
     * PIOWDB_OV: PIOWDB overflow
     *	    This bit is set when the PIOWDB overflows. A failure
     *	    reported by this bit indicates that the high water mark
     *	    of the buffer is not set properly, and should only occur
     *	    during design verification, not during normal
     *	    operation.
     *
     * PIORWRB_OV: PIORWRB overflow
     *	    This bit is set when the PIORWRB overflows. A failure
     *	    reported by this bit indicates that the high water mark
     *	    of the buffer is not set properly, and should only occur
     *	    during design verification, not during normal operation.
     *
     * SysStateErrProc(3:0): SysState bus parity error
     *	    This bit indicates that a parity error was observed on
     *	    the SysState bus for the specified CPU, which means that
     *	    there is a signal integrity problem on SysState, that
     *	    the system is marginal and requires maintenance work.
     *
     *	    This should be very rare, and if it ever occurs, the
     *	    system should be shut down immediately and the system
     *	    board should be repaired.
     *
     * SysCmdErrProc(3:0): SysCmd bus parity error
     *	    This bit indicates that the PIU has detected a parity
     *	    error on the SysCmd bus for the specified CPU.
     *
     * NCorSysErrProc(3:0): Non-correctable SysAD bus error
     *	    This bit indicates that an ECC (or parity) error has
     *	    occurred on the SysAD bus that can not be corrected;
     *	    additional data has been logged to h_berr_addr and
     *	    h_berr_misc to identify the error.
     *
     *	    If both NCorSysErrProc and SysCmdErr are set, then they
     *	    were set during the same cycle.
     *
     * DataElmntErrProc: Data Element Error.
     *	    This bit indicates that the CPU owning the bus on a
     *	    given data cycle when the Data Element Error bit is set
     *	    in the SysCmd. No further information is logged in Heart.
     */
    some_bits = h_cause_pend
	& (HC_PIOWDB_OFLOW
	   | HC_PIORWRB_OFLOW
	   | HC_SYSSTATE_ERR(this_cpu)
	   | HC_SYSCMD_ERR(this_cpu)
	   | HC_NCOR_SYSAD_ERR(this_cpu)
	   | HC_DATA_ELMNT_ERR(this_cpu));
    if (some_bits) {
	HEM_ADD_STR("Fatal Heart Error (\"should never happen\")\n");
	if (some_bits & HC_PIOWDB_OFLOW)
	    HEM_ADD_STR("\tPIOWDB Overflow\n");
	if (some_bits & HC_PIORWRB_OFLOW)
	    HEM_ADD_STR("\tPIORWRB Overflow\n");
	if (some_bits & HC_SYSSTATE_ERR(this_cpu))
	    HEM_ADD_STR("\tSysState bus parity error\n");
	if (some_bits & HC_SYSCMD_ERR(this_cpu))
	    HEM_ADD_STR("\tSysCmd bus parity error\n");
	if (some_bits & HC_NCOR_SYSAD_ERR(this_cpu))
	    HEM_ADD_STR("\tNon-correctable SysAD bus error\n");
	if (some_bits & HC_DATA_ELMNT_ERR(this_cpu))
	    HEM_ADD_STR("\tData Element Error\n");
	HEM_ADD_REG(h_mode);
	HEM_ADD_REG(h_cause);
	fatal++;
	h_cause_pend &= ~some_bits;
	if (!h_cause_pend)
	    goto no_cause;
    }
    /*
     * XBarCreditOver: Crossbar Credit Overflow
     *	    This bit is set when the crossbar credit for heart
     *	    overflows; no further information is logged. This error
     *	    may only be due to design flaws in the HEART chip or in
     *	    its XTalk neighbor.
     *
     * XBarCreditUnder: Crossbar Credit Underflow
     *	    This bit is set when the crossbar credit for heart
     *	    underflows; no further information is logged. This error
     *	    may only be due to design flaws in the HEART chip or in
     *	    its XTalk neighbor.
     *
     * IOResp_UnexpResp: Unexpected response. This bit is set when a
     *	    head micropacket is received for an IO Response, and the
     *	    response is not expected.
     *
     *	    In more precise terms, the head micropacket is considered
     *	    as a "Bad-Head" due to either "Bad Read Response" or "Bad
     *	    Write Response", but without BadTail. (The definition and
     *	    details of the quoted terms can be found in the section
     *	    6.7.2. XtErrBadHead.)
     *
     *	    There is no error information logged for this error
     *	    flag. This is because the error cannot be linked to any
     *	    specific request and the erred Response packet does not
     *	    contain any useful information for debugging.
     *
     * IOR_BufOverErr: IO Buffer Overflow Error
     *	    This bit indicates that a request packet was received
     *	    but the I/O buffers are full, which can only happen when
     *	    credit control between Heart and its partner is broken;
     *	    this should not occur in normal operation, and indicates
     *	    that the LLP_XBAR_CRD field is set to low (or possibly
     *	    a design flaw in Heart or its partner).
     *
     * LLP_RcvWarmReset: LLP Receive Link Warm Reset
     * LLP_RcvLinkReset: LLP Receive Link Reset
     *
     * LLP_TxRetryTO: LLP Transmit Retry Timeout
     *	    This bit indicates that there is something really,
     *	    really wrong with the LLP; further data sent to the port
     *	    will be dropped on the floor. Typical handling of this
     *	    timeout is a full system reset and reboot. Masking off
     *	    this error condition is *NOT*RECOMMENDED*.
     *
     */
    if (h_cause & HC_WIDGET_ERR) {
	some_bits = h_w_err_type_pend
	    & (ERRTYPE_XBAR_CREDIT_OVER
	       | ERRTYPE_XBAR_CREDIT_UNDER
	       | ERRTYPE_IO_UNEXPECTED_RESP_ERR
	       | ERRTYPE_IORWRB_OFLOW_ERR
	       | ERRTYPE_LLP_RCV_WARM_RST
	       | ERRTYPE_LLP_RCV_LNK_RST
	       | ERRTYPE_LLP_TX_RETRY_TIMEOUT);
	if (some_bits) {
	    HEM_ADD_STR("Fatal Heart Widget Error (\"should never happen\")\n");
	    if (some_bits & ERRTYPE_XBAR_CREDIT_OVER)
		HEM_ADD_STR("\tCrossbar Credit Overflow\n");
	    if (some_bits & ERRTYPE_XBAR_CREDIT_UNDER)
		HEM_ADD_STR("\tCrossbar Credit Underflow\n");
	    if (some_bits & ERRTYPE_IO_UNEXPECTED_RESP_ERR)
		HEM_ADD_STR("\tUnexpected I/O Response\n");
	    if (some_bits & ERRTYPE_IORWRB_OFLOW_ERR)
		HEM_ADD_STR("\tIO Buffer Overflow Error\n");
	    if (some_bits & ERRTYPE_LLP_RCV_WARM_RST)
		HEM_ADD_STR("\tLLP Receive Link Warm Reset\n");
	    if (some_bits & ERRTYPE_LLP_RCV_LNK_RST)
		HEM_ADD_STR("\tLLP Receive Link Reset\n");
	    if (some_bits & ERRTYPE_LLP_TX_RETRY_TIMEOUT)
		HEM_ADD_STR("\tLLP Transmit Retry Timeout\n");
	    HEM_ADD_REG(h_mode);
	    HEM_ADD_REG(h_cause);
	    HEM_ADD_REG(h_w_err_type);

	    fatal++;
	    h_w_err_type_pend &= ~some_bits;
	    if (!h_w_err_type_pend) {
		h_cause_pend &= ~HC_WIDGET_ERR;
		if (!h_cause_pend)
		    goto no_cause;
	    }
	}
    }
    /* Error Triage Case 2: someone else's problems
     */

    /*
     * Group 1: stuff that logs to h_berr_misc/h_berr_addr
     *
     * BadSysWrErr: Processor Write Request Address Error
     *
     *	    This error indicates that the processor attempted to
     *	    write to the Heart PIU Register Space, but the write was
     *	    neither a Word or DoubleWord. Additional data is logged
     *	    to h_berr_misc and h_berr_addr.
     *
     * BadSysRdErr: Processor Read Request Address Error
     *
     *	    This error indicates that the CPU made a read request
     *	    with the wrong size (or type). Additional data is logged
     *	    in h_berr_misc and h_berr_addr.
     *
     *	    This error includes badly sized reads of PIU registers,
     *	    processor upgrade requests to PIU registers, accesses to
     *	    IO space (outside Prom space) for a block read when
     *	    cached_pio_enable is not set, accesses to Prom space for
     *	    a block read when cached_prom_enable is not set, or any
     *	    coherent read or upgrade transaction to IO space.
     *
     * CorSysErrProc(3:0): Correctable SysAD bus error.
     *	    This bit indicates than an ECC error has occurred on the
     *	    SysAD bus which is correctable. Additional data is
     *	    locked up in h_berr_addr and h_berr_misc to identify the
     *	    error appropriately.
     */
    some_bits = berr_valid & h_cause_pend;
    if (some_bits) {
	int			berr_cpu;
	iopaddr_t		berr_addr;

	berr_cpu = (h_berr_misc & HBM_PROC_ID) >> HBM_PROC_ID_SHFT;
	if (berr_cpu == this_cpu) {
	    IOERROR_INIT(ioe);
	    if (epc)
		IOERROR_SETVALUE(ioe, epc, (caddr_t) epc);
	    if (ef)
		IOERROR_SETVALUE(ioe, ef, (caddr_t) ef);
	    IOERROR_SETVALUE(ioe, srccpu, berr_cpu);
	    berr_addr = h_berr_addr & HBA_ADDR;
	    if (sync_pend && (berr_addr == sync_paddr)) {
		IOERROR_SETVALUE(ioe, vaddr, sync_vaddr);
		IOERROR_SETVALUE(ioe, sysioaddr, sync_paddr);
		sync_pend = 0;
	    } else {
		IOERROR_SETVALUE(ioe, sysioaddr, berr_addr);
	    }
	    switch (heart_error_refine
		    ("heart bus error",
		     ((some_bits & HC_BAD_SYSWR_ERR) ? IOECODE_WRITE : 0) |
		     ((some_bits & HC_BAD_SYSRD_ERR) ? IOECODE_READ : 0) |
		     IOECODE_PIO,
		     error_mode, ioe)) {

	    case IOERROR_HANDLED:
		handled++;
		break;

	    case IOERROR_UNHANDLED:
		HEM_ADD_STR("Heart Bus Error\n");
		if (some_bits & HC_BAD_SYSWR_ERR)
		    HEM_ADD_STR("\tProcessor Write Request Address Error\n");
		if (some_bits & HC_BAD_SYSRD_ERR)
		    HEM_ADD_STR("\tProcessor Read Request Address Error\n");
		if (some_bits & HC_NCOR_SYSAD_ERR(this_cpu))
		    HEM_ADD_STR("\tUnorrectable SysAD bus error\n");
		if (some_bits & HC_COR_SYSAD_ERR(this_cpu))
		    HEM_ADD_STR("\tCorrectable SysAD bus error\n");
		HEM_ADD_VAR(berr_cpu);
		HEM_ADD_VAR(berr_addr);

	    default:

		HEM_ADD_IOE();
		fatal++;
	    }
	    h_cause_pend &= ~some_bits;
	    if (!h_cause_pend)
		goto no_cause;
	}
    }
    /*
     * Group 2: stuff that logs to w_err_{cmd_word,{upper,lower}_addr}
     *
     * IO_BadFormat: XTalk packet contains wrong number of micropackets
     *	    This error reflects a severe protocol violation, and
     *	    almost always warrants a link reset or system reset by
     *	    system software.
     *
     * IOR_CmdErr: I/O Request Command Error
     *	    See the spec for details on this complex
     *	    error.
     *
     * IOR_CmdWarn: I/O Request Command Warning
     *	    Similar to IOR_CmdErr (see the spec).
     *
     * IOR_IntVectErr: I/O Request Interrupt Vector Error
     *	    This bit indicates that some device has made a DSPW or
     *	    QCL write request to the Widget Interrupt Status
     *	    register with a bad vector number, and has not requested
     *	    a response.
     *
     * IOR_IntVectWarn: I/O Request Interrupt Vector Warning
     *	    This bit indicates that some device has made a DSPW or
     *	    QCL write request to the Widget Interrupt Status
     *	    register with a bad vector number, and has requested
     *	    a response.
     */
    some_bits = werr_valid & h_w_err_type_pend;
    if (some_bits) {
	w_err_cmd_word_u	u;
	iopaddr_t		werr_addr;
	int			werr_port;

	u.r = h_w_err_cmd_word;
	werr_addr =
	    ((h_w_err_upper_addr
	      & WIDGET_ERR_UPPER_ADDR_ONLY) << 32)
	    | h_w_err_lower_addr;
	werr_port = u.f.sidn;
	IOERROR_INIT(ioe);
	if (epc)
	    IOERROR_SETVALUE(ioe, epc, (caddr_t) epc);
	if (ef)
	    IOERROR_SETVALUE(ioe, ef, (caddr_t) ef);
	IOERROR_SETVALUE(ioe, widgetnum, werr_port);
	IOERROR_SETVALUE(ioe, xtalkaddr, werr_addr);

	switch (heart_error_refine
		("heart widget error",
		 ((u.f.other & HBM_SYSCMD_WR_CMD)
		  ? IOECODE_DMA_WRITE : IOECODE_DMA_READ),
		 error_mode, ioe)) {

	case IOERROR_HANDLED:
	    handled++;
	    break;

	case IOERROR_UNHANDLED:
	    HEM_ADD_STR("Heart Widget Error\n");
	    if (some_bits & ERRTYPE_IO_BAD_FORMAT)
		HEM_ADD_STR("\tBad Format (XIO protocol violation)\n");
	    if (some_bits & ERRTYPE_IOR_CMD_ERR)
		HEM_ADD_STR("\tI/O Request Command Error\n");
	    if (some_bits & ERRTYPE_IOR_CMD_WARN)
		HEM_ADD_STR("\tI/O Request Command Warning\n");
	    if (some_bits & ERRTYPE_IOR_INT_VEC_ERR)
		HEM_ADD_STR("\tBad Interrupt Vector (error)\n");
	    if (some_bits & ERRTYPE_IOR_INT_VEC_WARN)
		HEM_ADD_STR("\tBad Interrupt Vector (warning)\n");
	    cmn_err(CE_CONT,
		    "\tBad trans was from port %X to port %X offset 0x%X\n",
		    u.f.sidn, u.f.didn, werr_addr);
	    cmn_err(CE_CONT,
		    "\tAttributes: type=0x%x size=%d%s%s%s%s%s other=0x%x\n",
		    u.f.pactyp,
		    u.f.ds,
		    u.f.ct ? " COHERENT" : "",
		    u.f.gbr ? " PRIORITY" : "",
		    u.f.vbpm ? " VIRTUAL" : "",
		    u.f.bo ? " BARRIER" : "",
		    u.f.error ? " ERROR" : "",
		    u.f.other);

	default:
	    HEM_ADD_IOE();
	    fatal++;
	}
	/* discard IO_NonHead if it appears here. */
	some_bits |= ERRTYPE_IO_NONHEAD;
	h_w_err_type_pend &= ~some_bits;
	if (!h_w_err_type_pend) {
	    h_cause_pend &= ~HC_WIDGET_ERR;
	    if (!h_cause_pend)
		goto no_cause;
	}
    }
    /*
     * Group 3: stuff that logs to h_w_pio_rdto_addr
     *
     * PIO_RdTO: Response Time-out Error
     *	    Additional information is logged in h_w_pio_rdto_addr,
     *	    since the full address may not be available in the R4K
     *	    series processors.
     */
    if (rdto_valid) {
	h_wid_pio_rdto_addr_u	u;
	int			pio_cpu;
	int			pio_port;
	iopaddr_t		pio_addr;

	u.r = h_w_pio_rdto_addr;
	pio_cpu = u.f.cpu;
	if (pio_cpu == this_cpu) {
	    pio_addr = u.f.addr << 12;
	    pio_port = u.f.didn;
	    if (sync_valid) {
		IOERROR_INIT(ioe);
		if (epc)
		    IOERROR_SETVALUE(ioe, epc, (caddr_t) epc);
		if (ef)
		    IOERROR_SETVALUE(ioe, ef, (caddr_t) ef);
		IOERROR_SETVALUE(ioe, srccpu, pio_cpu);

		IOERROR_SETVALUE(ioe, vaddr, sync_vaddr);
		IOERROR_SETVALUE(ioe, sysioaddr, sync_paddr);
		IOERROR_SETVALUE(ioe, widgetnum, sync_xport);
		IOERROR_SETVALUE(ioe, xtalkaddr, sync_xaddr);

		switch (heart_error_refine
			("heart pio RTO error", IOECODE_PIO_READ, error_mode, ioe)) {
		case IOERROR_HANDLED:
		    handled++;
		    break;

		case IOERROR_UNHANDLED:
		    HEM_ADD_STR("Heart PIO Read Timeout Error\n");
		    HEM_ADD_VAR(pio_cpu);
		    HEM_ADD_VAR(pio_port);
		    HEM_ADD_VAR(pio_addr);
		    HEM_ADD_STR("\tAccess is via ");
		    HEM_ADD_STR((u.f.ios == 0) ? "main" :
				(u.f.ios == 1) ? "medium" :
				(u.f.ios == 2) ? "large" :
				"unknown");
		    HEM_ADD_STR(" heart window\n");

		default:
		    if (pio_port != sync_xport)
			cmn_err(CE_WARN, "sync_xport(%X) != Heart RDTO pio_port(%X)\n",
				sync_xport, pio_port);
		    if (pio_addr != (sync_xaddr & 0x00FFF000))
			cmn_err(CE_WARN, "sync_xaddr(0x%x) & 0xFFF000 != Heart RDTO pio_addr(0x%x)\n",
				sync_xaddr, pio_addr);
		    HEM_ADD_IOE();
		    fatal++;
		}
		sync_pend = 0;

		h_w_err_type_pend &= ~rdto_valid;
		if (!h_w_err_type_pend) {
		    h_cause_pend &= ~HC_WIDGET_ERR;
		    if (!h_cause_pend)
			goto no_cause;
		}
	    } else {
		/* XXX- async handler, ignore PIO_RdTO */
	    }
	} else {
	    /* XXX- read timeout, other CPU */
	}
    }
    /*
     * Group 4: stuff that logs to h_w_pio_err_{upper,lower}
     */
    if (wpio_valid) {
	int			pio_cpu;

	pio_cpu =
	    (h_w_pio_err_upper
	     & HW_PIO_ERR_PROC_ID)
	    >> HW_PIO_ERR_PROC_ID_SHFT;
	if (pio_cpu == this_cpu) {
	    iopaddr_t		    pio_paddr;
	    int			    pio_syscmd;

	    pio_paddr = (((h_w_pio_err_upper & HW_PIO_ERR_ADDR) << 32) |
			 (h_w_pio_err_lower & HW_PIO_ERR_LOWER_ADDR));
	    pio_syscmd = (h_w_pio_err_upper & HW_PIO_ERR_SYSCMD) >> 8;

	    IOERROR_INIT(ioe);
	    if (epc)
		IOERROR_SETVALUE(ioe, epc, (caddr_t) epc);
	    if (ef)
		IOERROR_SETVALUE(ioe, ef, (caddr_t) ef);
	    IOERROR_SETVALUE(ioe, srccpu, pio_cpu);
	    if (sync_pend &&
		(pio_paddr == sync_paddr)) {
		IOERROR_SETVALUE(ioe, vaddr, sync_vaddr);
		IOERROR_SETVALUE(ioe, sysioaddr, sync_paddr);
		IOERROR_SETVALUE(ioe, widgetnum, sync_xport);
		IOERROR_SETVALUE(ioe, xtalkaddr, sync_xaddr);
		sync_pend = 0;
	    } else {
		IOERROR_SETVALUE(ioe, sysioaddr, pio_paddr);
	    }

	    switch (heart_error_refine
		    ("heart widget pio error",
		     ((pio_syscmd & HBM_SYSCMD_WR_CMD)
		      ? IOECODE_PIO_WRITE
		      : IOECODE_PIO_READ),
		     error_mode, ioe)) {

	    case IOERROR_HANDLED:
		handled++;
		break;

	    case IOERROR_UNHANDLED:
		HEM_ADD_STR("Heart Widget PIO Error\n");

	    default:
		HEM_ADD_VAR(pio_cpu);
		HEM_ADD_VAR(pio_paddr);
		HEM_ADD_VAR(pio_syscmd);
		HEM_ADD_IOE();
		fatal++;
	    }
	    h_w_err_type_pend &= ~wpio_valid;
	    if (!h_w_err_type_pend) {
		h_cause_pend &= ~HC_WIDGET_ERR;
		if (!h_cause_pend)
		    goto no_cause;
	    }
	}
    }
    /*
     * Error Triage Case 3: ECC
     */
    if (h_cause_pend & (HC_COR_MEM_ERR | HC_NCOR_MEM_ERR)) {
	int			ecc_cpu;

	/*
	 * CorMemErr: Correctable Memory Error
	 *  This bit is set when a correctable ECC error is
	 *  detected on a memory read. Additional data is locked up
	 *  in h_memerr_addr and h_memerr_data.
	 *
	 * NCorMemErr: Non-Correctable Memory Error
	 *  This bit is set when a non-correctable ECC error is
	 *  detected on a memory read. Additional data is locked up
	 *  in h_memerr_addr and h_memerr_data.
	 */

	ecc_cpu = (h_memerr_addr & HME_REQ_SRC_MSK) >> HME_REQ_SRC_SHFT;

	if ((h_memerr_addr & HME_REQ_SRC_IO) || (ecc_cpu == this_cpu)) {
	    int			    ecc_fixed = 0;
	    int			    ecc_sync;
	    int			    ecc_syn;
	    iopaddr_t		    ecc_addr;

	    ecc_addr = h_memerr_addr & HME_PHYS_ADDR;
	    ecc_syn = (h_memerr_addr & HME_SYNDROME) >> HME_SYNDROME_SHFT;

	    if ((ecc_cpu == this_cpu) &&
		(h_cause_pend & HC_PE_SYS_COR_ERR(this_cpu)) &&
		(h_cause_pend & HC_COR_MEM_ERR)) {
		/*
		 * NOTE: When a CPU gets a COR_MEM_ERR,
		 * it also gets a SYS_COR_ERR. Discard
		 * the second.
		 * dwong: since a DSPW write will do a read-modify-write
		 * and correct any single bit error encountered, SYS_COR_ERR
		 * may not be set
		 */
		h_cause_pend &= ~HC_PE_SYS_COR_ERR(this_cpu);
	    }
	    if (sync_pend &&
		(sync_xport == XWIDGET_MEMORY) &&
		((HEART_MEM_PHYSADDR_DW_MSK & sync_maddr) == ecc_addr)) {
		ecc_sync = 1;
		sync_pend = 0;
	    } else
		ecc_sync = 0;

	    if (h_memerr_addr & HME_ERR_TYPE_ADDR) {
		fatal++;
	    } else {
		char			bad_bit = -1;
		int			hard_error = 1;

		if (data_eccsyns[ecc_syn].type == DB)
		    bad_bit = data_eccsyns[ecc_syn].value;
		if (data_eccsyns[ecc_syn].type == CB)
		    bad_bit = data_eccsyns[ecc_syn].value + 64;

		/*
		 * if both HC_COR_MEM_ERR and HC_NCOR_MEM_ERR are set,
		 * highly unlikely, treat it as HC_NCOR_MEM_ERR.  should use
		 * the saved syndrome to determine which one gets logged
		 * in logging registers
		 */
		if ((h_cause_pend & (HC_COR_MEM_ERR | HC_NCOR_MEM_ERR)) ==
		    HC_COR_MEM_ERR) {
		    /*
		     * Repair the data in memory.
		     *
		     * We force a cache line fill from the line
		     * containing the bad ECC, then make the
		     * line dirty. Using an atomic op for this
		     * guarantees that, should the data change
		     * on us, we will not drop stale data on
		     * top of the modified data.
		     *
		     * After modification, we flush the data out
		     * of our cache, forcing it to be written to
		     * memory, making the ECC bits correct.
		     *
		     * During this operation, we must hold off
		     * all DMA to memory, lest a noncoherent DMA
		     * sneak in between our cache line fill and
		     * the subsequent writeback.
		     *
		     * XXXMP- need to verify whether all CPUs take
		     * this trap; if not, need to "pause" all
		     * other CPUs. If so, need to synchronize
		     * so the CPU(s) not doing the fixup hang out
		     * until fixup is complete. IT IS IMPORTANT
		     * that we prevent other CPUs from doing
		     * noncacheable stores inside this cache line
		     * while we are doing the fixup.
		     */
		    volatile ulong_t	   *k0addr;
		    extern void		    __dcache_wb_inval(void *, int);
		    heartreg_t		    h_mem_req_arb;

		    heart_piu->h_cause = HC_COR_MEM_ERR;
		    SYNC;
		    *(volatile __uint64_t *) PHYS_TO_K1(ecc_addr);
		    hard_error = heart_piu->h_cause & HC_COR_MEM_ERR;

		    k0addr = (void *) PHYS_TO_K0(ecc_addr);

		    SYNC;
		    h_mem_req_arb = heart_piu->h_mem_req_arb;
		    heart_piu->h_mem_req_arb =	/* XXXMP */
			h_mem_req_arb | HEART_MEMARB_IODIS;
		    SYNC;
		    atomicAddLong((long *) k0addr, 0);
		    SYNC;
		    __dcache_wb_inval((void *) k0addr, 8);
		    SYNC;
		    heart_piu->h_mem_req_arb = h_mem_req_arb;
		    SYNC;

		    handled++;
		    ecc_fixed++;

		    ASSERT(data_eccsyns[ecc_syn].type == DB ||
			   data_eccsyns[ecc_syn].type == CB);

#if ECC_DEBUG
		    if (is_ecc_test)
			ecc_test_sb_count++;
		} else if ((h_cause_pend & HC_NCOR_MEM_ERR) &&
			   is_ecc_test) {

		    /*
		     * The data is gone, but we want to continue
		     * for the sake of testing.
		     * Since Heart uses R-M-W for all double-word
		     * stores, we must use HM_MEM_FORCE_WR to force
		     * it to write back the incorrect data with
		     * ECC claiming it is correct.
		     * We can't read from k1addr without getting
		     * another sync bus error, so we take the
		     * unfortunately bad data from h_memerr_data.
		     */
		    volatile ulong_t	   *k1addr;

		    k1addr = (void *) PHYS_TO_K1(ecc_addr);
		    SYNC;
		    heart_piu->h_mode = h_mode | HM_MEM_FORCE_WR;
		    SYNC;
		    *k1addr = h_memerr_data;
		    SYNC;
		    heart_piu->h_mode = h_mode;
		    SYNC;
		    handled++;
		    ecc_fixed++;
		    ecc_test_db_count++;
#endif
		} else
		    fatal++;

		/* log all data errors */
#if ECC_DEBUG
		/* unless induced by testing */
		if (!is_ecc_test)
#endif
		    log_dimmerr(ecc_addr, h_cause & HC_COR_MEM_ERR,
				bad_bit, h_memerr_data, hard_error);
	    }

	    /* if we didn't fix it, complain. Loudly. */
	    if (!ecc_fixed) {
		cmn_err(CE_CONT, "%s%s%s Memory %s ECC error\n",
			(h_cause_pend & HC_NCOR_MEM_ERR) ? "Uncorrectable " : "",
			(h_cause_pend & HC_COR_MEM_ERR) ? "Correctable " : "",
			etstrings[data_eccsyns[ecc_syn].type],
			(h_memerr_addr & HME_ERR_TYPE_ADDR) ? "Address" : "Data");
		HEM_ADD_REG(h_memerr_addr);
		HEM_ADD_VAR(h_memerr_data);
		if (h_memerr_addr & HME_REQ_SRC_IO)
		    HEM_ADD_NSVAR("ecc_cpu", "I/O");
		else
		    HEM_ADD_VAR(ecc_cpu);
		HEM_ADD_VAR(ecc_syn);
		HEM_ADD_VAR(ecc_addr);
		if (ecc_sync)
		    HEM_ADD_SYNC();
		fatal++;
	    }
	    h_cause_pend &= ~(HC_COR_MEM_ERR | HC_NCOR_MEM_ERR);
	    if (!h_cause_pend)
		goto no_cause;
	}
    }
    /* Error Triage Case 4: remaining problems
     */

    if (h_cause_pend & HC_PE_SYS_COR_ERR(this_cpu)) {
	/*
	 * PE_SysCorErr(3:0): Processor System Correctable Error.
	 *  This bit is set when the corresponding processor reports
	 *  that it has detected and corrected a correctable error
	 *  on the SysAD bus. An error reported on these bits
	 *  indicates that there may be a signal integrity problem
	 *  or failing secondary cache on a T5 based system.
	 */
	heart_SysCorErr_count[this_cpu]++;
	handled++;
	if (heart_SysCorErr_ptime[this_cpu] >= time) {
	    HEM_ADD_STR("Heart: Processor System Correctable Error\n");
	    HEM_ADD_REG(h_cause);
	    HEM_ADD_NVAR("SysCorErr_count", heart_SysCorErr_count[this_cpu]);
	    /* XXX- isn't there *anything* else of interest to print? */

	    /*
	     * Limit this message to once per minute.
	     * XXX- is this too frequent?
	     */
	    heart_SysCorErr_ptime[this_cpu] = time + 60;
	}
	h_cause_pend &= ~HC_PE_SYS_COR_ERR(this_cpu);
	if (!h_cause_pend)
	    goto no_cause;
    }
    if (piur_valid) {
	int			piur_cpu;
	iopaddr_t		piur_addr;
	int			piur_sync;

	/*
	 * PIUR_AccessErr
	 *  This bit is set when an access, targetted at the HEART
	 *  PIU Register address space, references an undefined
	 *  register or specifies an illegal access type; pertinant
	 *  information is logged in h_piur_acc_err.
	 */
	piur_cpu = (h_piur_acc_err
		    & HPE_ACC_PROC_ID)
	    >> HPE_ACC_PROC_ID_SHFT;
	if (piur_cpu == this_cpu) {
	    piur_addr = h_piur_acc_err & HPE_ACC_ERR_ADDR;
	    if (sync_pend && (sync_paddr == (piur_addr | HEART_PIU_BASE))) {
		piur_sync = 1;
		sync_pend = 0;
	    } else
		piur_sync = 0;

	    fatal++;

	    HEM_ADD_STR("Heart PIU Register Access Error\n");
	    HEM_ADD_REG(h_cause);
	    HEM_ADD_REG(h_piur_acc_err);
	    HEM_ADD_VAR(piur_cpu);
	    HEM_ADD_VAR(piur_addr);
	    HEM_ADD_NSVAR("access type",
			  (h_piur_acc_err
			   & HPE_ACC_TYPE_WR)
			  ? "Write"
			  : "Read");
	    if (piur_sync)
		HEM_ADD_SYNC();
	    h_cause_pend &= ~piur_valid;
	    if (!h_cause_pend)
		goto no_cause;
	}
    }
    if (h_cause_pend & HC_WIDGET_ERR) {
	/*
	 * WidgetError
	 *  This bit indicates that there is a widget error of some
	 *  sort, with further information available in the Widget
	 *  Error Type register.
	 */
	if (h_w_err_type_pend) {
	    /*
	     * IO_BadFormat: XTalk packet contains wrong number of micropackets
	     *	    This error reflects a severe protocol violation, and
	     *	    almost always warrants a link reset or system reset by
	     *	    system software. Additional data has been logged in
	     *	    w_err_cmd_word, w_err_upper_addr and w_err_lower_addr.
	     *
	     * IOR_CmdErr: I/O Request Command Error
	     *	    See the spec for details on this complex
	     *	    error. Additional information is logged in
	     *	    w_err_cmd_word, w_err_upper_addr and w_err_lower_addr.
	     *
	     * IOR_CmdWarn: I/O Request Command Warning
	     *	    Similar to IOR_CmdErr (see the spec). Additional
	     *	    information is logged in w_err_cmd_word,
	     *	    w_err_upper_addr and w_err_lower_addr.
	     *
	     * IOR_IntVectErr: I/O Request Interrupt Vector Error
	     *	    This bit indicates that some device has made a DSPW or
	     *	    QCL write request to the Widget Interrupt Status
	     *	    register with a bad vector number, and has not requested
	     *	    a response.	 Additional information is logged in
	     *	    w_err_cmd_word, w_err_upper_addr and w_err_lower_addr.
	     *
	     * IOR_IntVectWarn: I/O Request Interrupt Vector Warning
	     *	    This bit indicates that some device has made a DSPW or
	     *	    QCL write request to the Widget Interrupt Status
	     *	    register with a bad vector number, and has requested
	     *	    a response.	 Additional information is logged in
	     *	    w_err_cmd_word, w_err_upper_addr and w_err_lower_addr.
	     */
	    some_bits = werr_valid & h_w_err_type_pend;
	    if (some_bits) {
		w_err_cmd_word_u	u;
		iopaddr_t		wid_addr;

		u.r = h_w_err_cmd_word;
		wid_addr = (((h_w_err_upper_addr
			      & WIDGET_ERR_UPPER_ADDR_ONLY)
			     << 32)
			    | h_w_err_lower_addr);

#if 0	/* XXX use this somewhere */
		wid_src_port = u.f.sidn;
#endif

		/* XXX- should "refine" this and get the
		 * widget to print it's state.
		 */

		HEM_ADD_STR("Heart Widget Error\n");
		if (some_bits & ERRTYPE_IO_BAD_FORMAT)
		    HEM_ADD_STR("    I/O Bad Format\n");
		if (some_bits & ERRTYPE_IOR_CMD_ERR)
		    HEM_ADD_STR("    I/O Request Command Error\n");
		if (some_bits & ERRTYPE_IOR_CMD_WARN)
		    HEM_ADD_STR("    I/O Request Command Warning");
		if (some_bits & ERRTYPE_IOR_INT_VEC_ERR)
		    HEM_ADD_STR("    I/O Request Interrupt Vector Error\n");
		if (some_bits & ERRTYPE_IOR_INT_VEC_WARN)
		    HEM_ADD_STR("    I/O Request Interrupt Vector Warning\n");
		cmn_err(CE_CONT,
			"\tBad trans was from port %X to port %X offset 0x%X\n",
			u.f.sidn, u.f.didn, wid_addr);
		cmn_err(CE_CONT,
			"\tAttributes: type=0x%x size=%d%s%s%s%s%s other=0x%x\n",
			u.f.pactyp,
			u.f.ds,
			u.f.ct ? " COHERENT" : "",
			u.f.gbr ? " PRIORITY" : "",
			u.f.vbpm ? " VIRTUAL" : "",
			u.f.bo ? " BARRIER" : "",
			u.f.error ? " ERROR" : "",
			u.f.other);
		fatal++;
		h_w_err_type_pend &= ~some_bits;
	    }
	    /*
	     * Some of the LLP error bits are informative
	     * but not fatal.
	     */
	    some_bits = h_w_err_type_pend
		& (ERRTYPE_LLP_RCV_CB_ERR
		   | ERRTYPE_LLP_RCV_SQUASH_DATA
		   | ERRTYPE_LLP_TX_RETRY
		   | ERRTYPE_LLP_RCV_SN_ERR);
	    if (some_bits) {

		/*
		 * LLP_RcvCBError: LLP Receive Check Bit Error
		 * LLP_RcvSquashData: LLP Receive Squash Data
		 * LLP_RcvSNError: LLP Receive Sequence Number Error
		 * LLP_TxUpktRetry: LLP Transmitter Retry
		 *
		 * These errors should be reported "periodically"
		 */

		if (some_bits & ERRTYPE_LLP_RCV_CB_ERR)
		    HEM_ADD_PERIODIC_WARN("Heart: LLP Receive Check Bit Error\n");
		if (some_bits & ERRTYPE_LLP_RCV_SQUASH_DATA)
		    HEM_ADD_PERIODIC_WARN("Heart: LLP Receive Squash Data\n");
		if (some_bits & ERRTYPE_LLP_TX_RETRY)
		    HEM_ADD_PERIODIC_WARN("Heart: LLP Transmitter Retry\n");
		if (some_bits & ERRTYPE_LLP_RCV_SN_ERR)
		    HEM_ADD_PERIODIC_WARN("Heart: LLP Receive Sequence Number Error\n");
		handled++;
		h_w_err_type_pend &= ~some_bits;
	    }
	    if (h_w_err_type_pend & ERRTYPE_IO_NONHEAD) {
		/*
		 * IO_NonHead: dropped a non-head packet when looking for a head
		 *  This bit is commonly seen along with other errors, where
		 *  the head micropacket of a multi-micropacket packet has
		 *  an error causing the rest of the micropackets to be
		 *  discarded via this NonHead mechanism. If we get to this spot
		 *  in the code, we are seeing an IO_NONHEAD that corresponds
		 *  to an error we must have handled on a previous run through
		 *  the code (normally, we will get this *with* the other error
		 *  and we won't get here).
		 */
		HEM_ADD_PERIODIC_WARN("Heart dropped nonhead micropacket(s)\n");
		handled++;
		h_w_err_type_pend &= ~ERRTYPE_IO_NONHEAD;
	    }
	    if (h_w_err_type_pend & ERRTYPE_LLP_RCV_CNT_255) {
		/*
		 * LLP_REC_CNT_255: LLP Receive Counter reached 255
		 *  This bit is used to facilitate accruing large LLP
		 *  receive counts.
		 *
		 * XXX- need a way to deliver this to whatever
		 *  monitoring software is tracking the counters.
		 */
		LLPRxCnt255_count++;

		handled++;
		h_w_err_type_pend &= ~ERRTYPE_LLP_RCV_CNT_255;
	    }
	    if (h_w_err_type_pend & ERRTYPE_LLP_TX_CNT_255) {
		/*
		 * LLP_TX_CNT_255: LLP Transmit Counter reached 255
		 *  This bit is used to facilitate accruing large LLP
		 *  transmit counts.
		 *
		 * XXX- need a way to deliver this to whatever
		 *  monitoring software is tracking the counters.
		 */
		LLPTxCnt255_count++;

		handled++;
		h_w_err_type_pend &= ~ERRTYPE_LLP_TX_CNT_255;
	    }
	    some_bits = wpio_valid & h_w_err_type_pend;
	    if (some_bits) {
		int			wpio_sync;
		int			wpio_cpu;
		int			wpio_syscmd;
		iopaddr_t		wpio_addr;

		/*
		 * PIO_WrTO: Write Request Time-out
		 *	This error indicates that a PIO write request has been
		 *	pending with no outstanding read requests when the write
		 *	request time-out counter times out; this is typically
		 *	due to crosstalk credit starvation. Additional data is
		 *	available in h_w_pio_err_upper and
		 *	h_w_pio_err_lower.
		 *
		 * PIO_XT_AccessErr: PIO Crosstalk Access Error
		 *	For all details, see chapter 6 of the Heart
		 *	spec. Acdditional information is available in
		 *	h_w_pio_err_upper and h_w_pio_err_lower.
		 *
		 * PIO_WCR_AccessErr: PIO Widget Config. Reg. Access Error
		 *	This error indicates that a processor PIO request to
		 *	access the widget configuration registers had an error,
		 *	such as writing to a read-only register, data size, bad
		 *	alignment, reserved space, etc. See chapter 6 in the
		 *	Heart Spec. Additional information is available in
		 *	h_w_pio_err_upper and h_w_pio_err_lower.
		 */

		wpio_cpu = ((h_w_pio_err_upper
			     & HW_PIO_ERR_PROC_ID)
			    >> HW_PIO_ERR_PROC_ID_SHFT);
		wpio_syscmd = ((h_w_pio_err_upper
				& HW_PIO_ERR_SYSCMD)
			       >> 8);
		wpio_addr = (((h_w_pio_err_upper
			       & HW_PIO_ERR_ADDR) << 32)
			     | (h_w_pio_err_lower
				& HW_PIO_ERR_LOWER_ADDR));

		if (sync_pend && (wpio_addr == sync_paddr)) {
		    wpio_sync = 1;
		    sync_pend = 0;
		} else {
		    wpio_sync = 0;
		}

		fatal++;
		h_w_err_type_pend &= ~some_bits;

		HEM_ADD_STR("	 Heart PIO Error\n");

		if (some_bits & ERRTYPE_PIO_WR_TIMEOUT)
		    HEM_ADD_STR("    Write Request Time-out\n");
		if (some_bits & ERRTYPE_PIO_XTLK_ACC_ERR)
		    HEM_ADD_STR("    PIO XIO Access Error\n");
		if (some_bits & ERRTYPE_PIO_WCR_ACC_ERR)
		    HEM_ADD_STR("    PIO Widget Config. Reg. Access Error\n");

		HEM_ADD_REG(h_cause);
		HEM_ADD_REG(h_w_err_type);
		HEM_ADD_REG(h_w_pio_err_upper);
		HEM_ADD_REG(h_w_pio_err_lower);
		HEM_ADD_VAR(wpio_cpu);
		HEM_ADD_VAR(wpio_addr);
		HEM_ADD_VAR(wpio_syscmd);
		if (wpio_sync)
		    HEM_ADD_SYNC();
	    }
	    if (!h_w_err_type_pend) {
		h_cause_pend &= ~HC_WIDGET_ERR;
		if (!h_cause_pend)
		    goto no_cause;
	    }
	}
    }
    some_bits = h_cause_pend & HC_MEM_ADDR_ERR_PROC(this_cpu);
    if (some_bits) {
	int			memerr_cpu;
	iopaddr_t		memerr_addr;
	int			memerr_sync;

	/*
	 * MemAddrErrProc(3:0): Memory Address Error for Processor N
	 *  This bit is set if a processor-to-memory access
	 *  addresses a location that does not exist or exists in
	 *  more than one DIMM bank.
	 */
	memerr_cpu = ((h_memerr_addr
		       & HME_REQ_SRC_MSK)
		      >> HME_REQ_SRC_SHFT);
	memerr_addr = h_memerr_addr & HME_PHYS_ADDR;

	/* if the pending sync data is a memory
	 * address, and either the addresses match
	 * or the logged data is for the wrong CPU,
	 * consume the sync error info.
	 */

	if (sync_pend &&
	    (sync_xport == XWIDGET_MEMORY) &&
	    ((memerr_cpu != this_cpu) ||
	     (sync_maddr == memerr_addr))) {
	    memerr_sync = 1;
	    sync_pend = 0;
	} else
	    memerr_sync = 0;

	fatal++;

	HEM_ADD_STR("Heart Memory Address Error\n");
	HEM_ADD_REG(h_cause);
	HEM_ADD_REG(h_memerr_addr);
	HEM_ADD_VAR(memerr_cpu);
	HEM_ADD_VAR(memerr_addr);
	if (memerr_sync)
	    HEM_ADD_SYNC();

	h_cause_pend &= ~some_bits;
	if (!h_cause_pend)
	    goto no_cause;
    }
    some_bits = h_cause_pend & HC_MEM_ADDR_ERR_IO;
    if (some_bits) {
	iopaddr_t		memerr_addr;

	/*
	 * MemAddrErrIO: Memory Address Error for I/O
	 *  This bit is set if an I/O-to-memory access addresses a
	 *  location that does not exist or exists in more than one
	 *  DIMM bank.
	 */
	memerr_addr = h_memerr_addr & HME_PHYS_ADDR;

	fatal++;
	HEM_ADD_STR("Heart: DMA Memory Address Error\n");
	HEM_ADD_REG(h_cause);
	HEM_ADD_REG(h_memerr_addr);
	HEM_ADD_VAR(memerr_addr);
	h_cause_pend &= ~some_bits;
	if (!h_cause_pend)
	    goto no_cause;
    }
    /*
     * branch to here if h_cause_pend is (or goes to) zero.
     */
  no_cause:

    if ((sync_pend) &&
	(sync_xport >= 0) &&
	(sync_xport <= 15)) {
	IOERROR_INIT(ioe);
	if (epc)
	    IOERROR_SETVALUE(ioe, epc, (caddr_t) epc);
	if (ef)
	    IOERROR_SETVALUE(ioe, ef, (caddr_t) ef);
	IOERROR_SETVALUE(ioe, srccpu, this_cpu);
	IOERROR_SETVALUE(ioe, vaddr, sync_vaddr);
	IOERROR_SETVALUE(ioe, sysioaddr, sync_paddr);
	IOERROR_SETVALUE(ioe, widgetnum, sync_xport);
	IOERROR_SETVALUE(ioe, xtalkaddr, sync_xaddr);
	sync_pend = 0;
	switch (heart_error_refine
		("Heart: Bus Error", IOECODE_PIO, error_mode, ioe)) {
	case IOERROR_HANDLED:
	    handled++;
	    break;

	case IOERROR_UNHANDLED:
	    HEM_ADD_STR("Heart: Bus Error\n");

	default:
	    HEM_ADD_IOE();
	    fatal++;
	}
    }
    /* check for sync DBE on invalid memory address */
    if (sync_pend &&
	(sync_xport == XWIDGET_MEMORY) &&
	!is_in_main_memory(sync_maddr)) {
	sync_pend = 0;
	fatal++;
	HEM_ADD_STR("Heart: access to nonexistant memory\n");
	HEM_ADD_REG(h_cause);
	HEM_ADD_SYNC();
    }
    if (sync_pend) {
	sync_pend = 0;
	fatal++;
	HEM_ADD_STR("Heart: Bus Error\n");
	HEM_ADD_REG(h_cause);
	HEM_ADD_SYNC();
    }
    if ((flag != F_KERN_NF) &&
#if !DEBUG
	kdebug &&
#endif
	fatal) {

	if (sync_valid) {
	    HEM_ADD_STR("DEBUG DATA -- Data derived from sync trap info:\n");
	    if (ef)
		HEM_ADD_VAR(ef);
	    if (epc)
		HEM_ADD_VAR(epc);
	    HEM_ADD_SYNC();
	}
	HEM_ADD_STR("DEBUG DATA -- selected raw Heart ASIC registers:\n");
	HEM_ADD_REG(h_mode);
	HEM_ADD_REG(h_cause);
	HEM_ADD_REG(h_piur_acc_err);
	HEM_ADD_REG(h_berr_misc);
	HEM_ADD_VAR(h_berr_addr);
	HEM_ADD_REG(h_w_err_type);
	HEM_ADD_REG(h_w_err_cmd_word);
	HEM_ADD_REG(h_w_err_upper_addr);
	HEM_ADD_REG(h_w_err_lower_addr);
	HEM_ADD_REG(h_w_pio_rdto_addr);
	HEM_ADD_REG(h_w_pio_err_upper);
	HEM_ADD_REG(h_w_pio_err_lower);
	HEM_ADD_REG(h_memerr_addr);
	HEM_ADD_VAR(h_memerr_data);
    } else if (fatal && sync_valid) {
	HEM_ADD_VAR(sync_vaddr);
	HEM_ADD_VAR(sync_paddr);
    }
    if (flag == F_USER) {
	;
    } else if (flag == F_KERN_NF) {
	;
    } else if (fatal) {
	if (sync) {
	    atomicClearUint((int *) &heart_error_cpumap, cpumap_clr);
	    return 0;			/* triggers normal bus error */
	}
	cmn_err_tag(68,CE_PANIC, "Bus Error");
    }
    /* unlock error logging registers
     */
    if (werr_valid & ~h_w_err_type_pend)
	heart_cfg->h_wid_err_cmd_word = 0;
    h_w_err_type_pend ^= h_w_err_type;
    if (h_w_err_type_pend)
	heart_cfg->h_wid_err_type = h_w_err_type_pend;
    h_cause_pend ^= h_cause;
    if (h_cause_pend)
	heart_piu->h_cause = h_cause_pend;
    /*
     * VEC_dbe and VEC_ibe turn off
     * HEART_INT_EXC and HEART_CPU_BERR(this_cpu);
     * turn them back on now so we can take
     * any new errors that come up.
     */
    if (sync)
	heart_imr_bits_rmw(this_cpu, 0,
			   (HEART_ISR_BERR |
			    (HEART_INT_EXC <<
			     HEART_INT_L4SHIFT)));

    /*
     * If the cause register is still nonzero,
     * stimulate a BERR interrupt to get us to
     * pass through here again (in async mode).
     */
    if (heart_piu->h_cause)
	heart_piu->h_set_isr = HEART_ISR_BERR;

    atomicClearUint((int *) &heart_error_cpumap, cpumap_clr);

    if (flag == F_KERN_NF)
	return 0;
    if (fatal)
	return 0;			/* covers user-fatal case */
    return 1;
}

/* probe tlb for virtual address vaddr, returning its physical address */
static paddr_t
tlb_to_phys(caddr_t vaddr)
{
    uint		    page_mask;
    uint		    pfn;
    unsigned char	    pid;
    k_machreg_t		    tlblo;

    if (IS_KSEG2(vaddr))		/* kernel mapped */
	pid = 0;
    else				/* user mapped */
	pid = tlbgetpid();

    if ((tlblo = tlb_probe(pid, vaddr, &page_mask)) == -1)
	return (paddr_t) -1;

    pfn = (tlblo & TLBLO_PFNMASK) >> TLBLO_PFNSHIFT;
    pfn >>= PGSHFTFCTR;

    return (paddr_t) (ctob(pfn) + poff(vaddr));
}
