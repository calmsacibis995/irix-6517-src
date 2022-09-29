/*
 *=============================================================================
 *              File:           IP32intr.c
 *=============================================================================
 */

#include "sys/types.h"
#include "ksys/xthread.h"
#include "sgidefs.h"
#include "sys/pda.h"
#include "sys/sysinfo.h"
#include "sys/ksa.h"
#include "sys/reg.h"
#include "sys/sbd.h"
#include "sys/crime.h"
#include "sys/cmn_err.h"
#include "sys/kmem.h"
#include "sys/systm.h"
#include "sys/debug.h"
#include "sys/rtmon.h"

#define READ_CRM_STAT()    (READ_REG64(PHYS_TO_K1(CRM_INTSTAT), _crmreg_t))
#define READ_CRM_MASK()    (READ_REG64(PHYS_TO_K1(CRM_INTMASK), _crmreg_t))
#define WRITE_CRM_MASK(rg) (WRITE_REG64(rg, PHYS_TO_K1(CRM_INTMASK), _crmreg_t))

#define CRIME_INTR_DISABLE(ibit, spl)             	\
        {                                               \
           int          indx;                           \
           for (indx = SPLMIN; indx < spl>>3; indx++)   \
                private.p_splmasks[indx] &= ~(ibit);    \
        }
        
#define CRIME_INTR_ENABLE(ibit, spl)              	\
        {                                               \
           int indx;                                    \
           for (indx = SPLMIN; indx < spl>>3; indx++)   \
                private.p_splmasks[indx] |= (ibit);     \
        }


#ifndef NULL
#define NULL	0
#endif

extern	void timein(eframe_t *);
extern	void r4kcount_intr(eframe_t *);
extern 	void clock(eframe_t *, kthread_t *);
extern	void ackkgclock(void);
extern  void buserr_intr(eframe_t *);
extern  void crmerr_intr(eframe_t *, __psint_t);

static 	void strayc0(eframe_t *);
static	void crime_intr(eframe_t *);
static	void crimestray(eframe_t *, __psint_t);

extern	uint get_r4k_counter(void);

extern 	volatile unsigned long cause_ip5_count;
extern 	volatile unsigned long cause_ip6_count;

#ifdef R10000
	static volatile unsigned long cause_hwperf_count = 0;
#endif /* R10000 */

int	ithreads_enabled = 0;

struct intvec_s {
    void	(*isr)(eframe_t *); /* interrupt service routine   */
    int		level;		    /* spl level for isr           */
    uint	bit;		    /* corresponding bit in sr     */
    int		fil;		    /* make sizeof this struct 2^n */
};

static struct intvec_s c0vec_tbl[] = {
    0,		0,	0,		0,  /* (1 based) */
    timein,	1<<3,	SR_IBIT1 >> 8,	0,  /* softint 1 */
    strayc0,	1<<3,	SR_IBIT2 >> 8,	0,  /* softint 2 */
    crime_intr,	3<<3,	SR_IBIT3 >> 8,	0,  /* hardint 4 */
    strayc0,	1<<3,	SR_IBIT4 >> 8,	0,  /* hardint 3 */
    strayc0,	1<<3,	SR_IBIT5 >> 8,	0,  /* hardint 5 */
    strayc0,	1<<3,	SR_IBIT6 >> 8,	0,  /* hardint 6 */ 
    strayc0,	1<<3,	SR_IBIT7 >> 8,	0,  /* hardint 7 */
    r4kcount_intr,	3<<3,	SR_IBIT8 >> 8,	0,  /* hardint 8 */
};

typedef struct crimevec_s {
     intvec_func_t       cv_isr;         /* Interrupt handler              */
     __psint_t           cv_arg;         /* Argument passed to handler     */
     int                 cv_spl;         /* Spl level handler is called at */
     short               cv_priority;    /* Index into priority array      */
                                         /* Used to determine relative     */
                                         /* Priority of CRIME interrupts   */
     short               cv_flags;       /* Flags: CRM_EXCL, CRM_CHKSTAT   */
     _crmreg_t           cv_ibit;        /* bit in CRM_INTMASK             */
     thd_int_t           cv_tinfo;       /* ithread information structure  */
} crimevec_t;

#define cv_tflags       cv_tinfo.thd_flags
#define cv_isync        cv_tinfo.thd_isync
#define cv_thread       cv_tinfo.thd_ithread
#define cv_latstats	cv_tinfo.thd_latstats

#define	acv_pri		cv_tinfo.thd_pri

static struct crimevec_s crimevec_tbl[] = {
    {crimestray,	0,	0,	0},
    {crimestray,	1,	0,	1},
    {crimestray,	2,	0,	2},
    {crimestray,	3,	0,	3},
    {crimestray,	4,	0,	4},
    {crimestray,	5,	0,	5},
    {crimestray,	6,	0,	6},
    {crimestray,	7,	0,	7},
    {crimestray,	8,	0,	8},
    {crimestray,	9,	0,	9},
    {crimestray,	10,	0,	10},
    {crimestray,	11,	0,	11},
    {crimestray,	12,	0,	12},
    {crimestray,	13,	0,	13},
    {crimestray,	14,	0,	14},
    {crimestray,	15,	0,	15},
    {crimestray,	16,	0,	16},
    {crimestray,	17,	0,	17},
    {crimestray,	18,	0,	18},
    {crimestray,	19,	0,	19},
    {crimestray,	20,	0,	20},
    {crimestray,	21,	0,	21},
    {crimestray,	22,	0,	22},
    {crimestray,	23,	0,	23},
    {crimestray,	24,	0,	24},
    {crimestray,	25,	0,	25},
    {crimestray,	26,	0,	26},
    {crimestray,	27,	0,	27},
    {crimestray,	28,	0,	28},
    {crimestray,	29,	0,	29},
    {crimestray,	30,	0,	30},
    {crimestray,	31,	0,	31},
};

struct crimevec_s *crimegang_tbl[NCRMINTS];

static int clock_nested = 0;
static int kgclock_nested = 0;

/* Prototypes */

void 	crimevec_thread_setup(int intr, int level, crimevec_t *cv);
void 	crimevec_intrd_init(crimevec_t *cv);
void 	crimevec_intrd(crimevec_t *cv);

void 	enable_ithreads(void);

/*ARGSUSED*/
static void
strayc0(eframe_t *ep)
{
    static strayc0ints = 0;

    cmn_err(CE_WARN,"stray c0 cause interrupt\n");
    strayc0ints++;	/* for crash dumps, etc. */
}

/*ARGSUSED*/
static void
crimestray(eframe_t *ep, __psint_t bit)
{
    static crimestrayints = 0;

    cmn_err(CE_WARN,"stray crime interrupt 0x%x", bit);
    crimestrayints++;
}

/*ARGSUSED*/
void
crmerr_intr(eframe_t *ep, __psint_t arg)
{
    cmn_err(CE_PANIC, "CRIME/VICE error");
}

int	late_clock_count = 0;

#ifdef R10000
#include <sys/hwperfmacros.h>

/*ARGSUSED1*/
void
set_hwperf_intr(struct eframe_s *ep)
{
	/* stop counters */

	r1nkperf_control_register_set(0,0);
	r1nkperf_control_register_set(1,0);

	if (IS_R12000()) {
		r1nkperf_control_register_set(2,0);
		r1nkperf_control_register_set(3,0);
	}

	cause_hwperf_count = 1;
}
#endif /* R10000 */

/*ARGSUSED*/
void
synthclock(eframe_t *ep, __psint_t arg)
{
    _crmreg_t temp; 
    int	s;


    /*
     * Clear the interrupt
     */
    temp = READ_REG64(PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);
    temp &= ~CRM_INT_SOFT0;
    WRITE_REG64(temp, PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);

    /*
     * since clock lowers the spl to 0 once per second, we
     * need to check and see if we are already in this handler.
     * If we are, just return.
     */
    if (clock_nested)
	return;

    clock_nested++;

#ifdef R10000
    if (cause_hwperf_count) {
	    cause_hwperf_count = 0;
	    hwperf_intr(ep);
    }
#endif /* R10000 */

    if (cause_ip5_count) {
        while (1) {
	    clock(ep, curthreadp);
	    temp = READ_REG64(PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);
	    temp &= ~CRM_INT_SOFT0;
	    WRITE_REG64(temp, PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);
	    s = spl7();
	    ASSERT(cause_ip5_count > 0);
	    if (cause_ip5_count > 1)
		    late_clock_count++;
	    cause_ip5_count--;
	    temp = (cause_ip5_count == 0);
	    splx(s);
	    if (temp)
		break;
        }
    }

    clock_nested = 0;
}

static intvec_func_t kgclockfunc;

int	late_kgclock_count = 0;

void
synthackkgclock(eframe_t *ep, __psint_t arg)
{
    _crmreg_t temp;
    int s;

    ASSERT(kgclockfunc != NULL);

    /*
     * Clear the interrupt.
     */
    temp = READ_REG64(PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);
    temp &= ~CRM_INT_SOFT1;
    WRITE_REG64(temp, PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);

    /*
     * Check to see if we are already in this handler.
     * If we are, just return.
     */
    if (kgclock_nested)
	return;

    kgclock_nested++;

    if (cause_ip6_count) {
	while (1) {
	    (*kgclockfunc)(ep, arg);
	    temp = READ_REG64(PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);
	    temp &= ~CRM_INT_SOFT1;
	    WRITE_REG64(temp, PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);
	    s = spl7();
	    ASSERT(cause_ip6_count > 0);
	    if (cause_ip6_count > 1)
		    late_kgclock_count++;
	    cause_ip6_count--;
	    temp = (cause_ip6_count == 0);
	    splx(s);
	    if (temp)
		break;
        }
    }

    kgclock_nested = 0;
}

/*
 *----------------------------------------------------------------------------
 * is_thd()
 * This routine accepts a crime interrupt number { 0..31 }
 * It then returns (0) if the interrupt isn't ithreaded.
 * It then returns (1) if the interrupt *is*  ithreaded.
 *----------------------------------------------------------------------------
 */
static int
is_thd(int intr)
{
        switch (intr){
	case MACE_PCI_SCSI0:
        case MACE_PCI_SCSI1:
	case MACE_ETHERNET:
	case VICE_CPU_INTR:
	case MACE_PCI_SLOT0:
	case MACE_PCI_SHARED0:
	case MACE_PCI_SHARED1:
	case MACE_PCI_SHARED2:
	case MACE_VID_IN_1:
	case MACE_VID_IN_2:
	case MACE_VID_OUT:
	case RE_INTR(5):		/* idle/level trigger */
	case RE_INTR(3):		/* empty/level trigger */
	case GBE_INTR(0):		/* retrace */
	case GBE_INTR(1):		/* preblank */
                return (1);
        default:
                return (0);
        }
}

/*
 * crimegang()
 * Third level handler for CRIME multiplexed (ganged) interrupts
 * XXX The below routine will have to be changed if any threaded
 * interrupt is registered, sharing one CRIME interrupt line.
 */
void
crimegang(eframe_t *ep, __psint_t intr)
{
    struct crimevec_s *cv = crimegang_tbl[intr];
    int i;

    for (i = 0; i < NCRMGANG; i++) {
	if (!cv->cv_isr)
	    return;
	/* no need to set spl here, it's already done */
	cv->cv_isr(ep, cv->cv_arg);
	cv++;
    }
}

void
crime_intr_enable(int intr)
{
	int s = spl7();
	CRIME_INTR_ENABLE(1 << intr, SPLHINTR << 3)
	splx(s);
}

void
crime_intr_disable(int intr)
{
	int s = spl7();
	CRIME_INTR_DISABLE(1 << intr, SPLHINTR << 3)
	splx(s);
}

#ifdef ITHREAD_LATENCY
#define SET_ISTAMP(latstat)	xthread_set_istamp(latstat)
#else
#define SET_ISTAMP(latstat)
#endif /* ITHREAD_LATENCY */

/*
 *----------------------------------------------------------------------------
 * crime_intr()
 * Second level interrupt handler for CRIME interrupt sources.
 * We look at CRIME's status register, and examine each bit that
 * is set. Each interrupt bit could be:
 * (a) A non ithread'ed interrupt, to be handled on the istack.
 * (b) An ithread'ed interrupt, that needs to be masked at CRIME.
 * (c) An ithread'ed interrupt, that needs to be masked at MACE.
 *----------------------------------------------------------------------------
 */
void
crime_intr(eframe_t *ep)
{
    int         s, intr, index;
    int         priorities[NCRMINTS];
    uint        bit, activeints = 0;
    struct      crimevec_s *cv;
    _crmreg_t   intstat, intmask;
                   
    intstat = READ_CRM_STAT();
    intmask = ep->ef_crmmsk;
                   
    activeints = intstat & intmask;

    do {
                   
        /* We need to examine each bit that has been set in: activeints */
        /* However, we do not service them strictly in that order, but  */
        /* instead, we service them in the order of decreasing spl level*/
        /* For that purpose, we first sort the high bits in: activeints */
                   
        /* Once that is done, we examine the kind of interrupt each one */
        /* is and we do the 'right thing'. The 'right thing' depends on */
        /* whether or not an interrupt is threaded, and if so at which  */
        /* level the interrupt needs to be masked */   

        for (bit = 0x01, index = 0; bit != 0x00; bit <<= 1, index++){
                cv = &crimevec_tbl[index];
                priorities[cv->cv_priority] = (activeints & bit) ? index : -1;
        }

        for (bit = 0x01, index = 0; bit != 0x00; bit <<= 1, index++){
           intr = priorities[index];
           if (intr != -1){
                cv   = &crimevec_tbl[intr];

		/* disable in both istack/ithread cases */ 
		if (cv->cv_flags & CRM_DRVENB)
		   	CRIME_INTR_DISABLE(cv->cv_ibit, cv->cv_spl);

                if (cv->cv_tflags & THD_OK){
                   /*
                    * Looks like this interrupt is threaded
                    * Mask the required interrupt at CRIME
                    * Signal the thread associated with intr
                    */
                   ASSERT(cv->cv_ibit == (1 << intr));
                   s = splint(cv->cv_spl);
		   /* Setup new CRM_INTMASK */
                   intmask &= ~cv->cv_ibit;     

                   /* Disable in private.p_splmasks */
                   /* Signal the required ithread */
                   SET_ISTAMP(cv->cv_latstats);
		   CRIME_INTR_DISABLE(cv->cv_ibit, cv->cv_spl);
                   vsema(&cv->cv_isync);
                   splx(s);
                }
                else {
                        /*
                         * Looks like it isn't threaded
			 * Execute isr while at some spl.
                         */
                        s = splint(cv->cv_spl);
                        (*cv->cv_isr)(ep, cv->cv_arg);
			if (cv->cv_flags & CRM_DRVENB) {
				ep->ef_crmmsk &= ~cv->cv_ibit;
				intmask &= ~cv->cv_ibit;
			} else {
		   		CRIME_INTR_ENABLE(cv->cv_ibit, cv->cv_spl);
			}
                        splx(s);
                }
           }
        }
        /* Re-read the CRIME status register    */
        intstat = READ_CRM_STAT();
    } while (activeints = intstat & intmask);

    /* VEC_int() sets up ep->ef_crmmsk as new CRM_INTMASK */
    ep->ef_crmmsk = intmask;
}

/*
 * find first interrupt
 *
 * This is an inline version of the ffintr routine.  It differs in
 * that it takes a single 8 bit value instead of a shifted value.
 */
static char ffintrtbl[][16] = { { 0,5,6,6,7,7,7,7,8,8,8,8,8,8,8,8 },
			        { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 } };
#define FFINTR(c) (ffintrtbl[0][c>>4]?ffintrtbl[0][c>>4]:ffintrtbl[1][c&0xf])
#define COUNT_INTR

/* XXX temporary swtch to enable kernel preemption */
extern int ip32nokp;

/*
 * intr - handle 1st level interrupts
 */
/*ARGSUSED2*/
intr(eframe_t *ep, uint code, uint sr, uint cause)
{
    int svcd = 0;
    int s;
    int softints = 0;
    int check_kp = 0;
    _crmreg_t intstat, intmask;
    int spllvl = 0;

    LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTRENTRY, NULL, NULL, NULL, NULL);

    intstat = READ_REG64(PHYS_TO_K1(CRM_INTSTAT), _crmreg_t);
    spllvl = ep->ef_crmmsk >> 32;

    intmask = (ep->ef_crmmsk &= (__uint64_t)0xffffffff);

#ifdef USE_McGriff
    WRITE_REG64(0x7fffLL, PHYS_TO_K1(McGriff), _crmreg_t);
#endif
    while (1) {
	register struct intvec_s *iv;

	cause = (cause & sr) & SR_IMASK;
	if (! cause)
	    break;

	/*
	 * check to see if we really should have gotten an
	 * interrupt from CRIME.  If not, clear the CRIME
	 * interrupt bit in cause and try again.
	 *
	 * If we did get a real CRIME interrupt, we need to
	 * save the current mask in the exception frame so
	 * that the CRIME interrupt handler can refer to it.
	 */
	if ((cause & SR_CRIME_INT_ON) && !(intstat & intmask)) {
	    cause &= SR_CRIME_INT_OFF;
	    continue;
	} 

	iv = c0vec_tbl+FFINTR((cause >> CAUSE_IPSHIFT));
	COUNT_INTR;
	s = splint(iv->level);
        /*
         * set check_kp if this kthread was running at a
         * preemtable spl (< splhi()).
	 *
	 * XXX also check switch used to *disable* kp
         */
        check_kp = (spllvl < (1<<3)) && !ip32nokp;
	(*iv->isr)(ep);
	splx(s);

	svcd  += 1;
	cause &= ~(iv->bit << CAUSE_IPSHIFT);

	cause |= getcause();
    }

    /*
     * we need to preserve any softints which are pending.
     */
    softints = READ_REG64(PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);

    /* 
     * If we are already in the process of handling clock
     * ticks, don't set the interrupt bit again as this will
     * cause the clock handler to be reentered infinitely.
     */
    if ((cause_ip5_count 
#ifdef R10000
	 ||
	 cause_hwperf_count
#endif /* R10000 */
	 )&& !clock_nested) softints |= CRM_INT_SOFT0;
    if (cause_ip6_count && !kgclock_nested) softints |= CRM_INT_SOFT1;

    if (softints & (CRM_INT_SOFT0|CRM_INT_SOFT1)) {
	WRITE_REG64(softints, PHYS_TO_K1(CRM_SOFTINT), _crmreg_t);
    }
    SYSINFO.intr_svcd += svcd;

    LOG_TSTAMP_EVENT(RTMON_INTR, TSTAMP_EV_INTREXIT, TSTAMP_EV_INTRENTRY, NULL, NULL, NULL);

    return (check_kp);
}

/*
 * setkgvector - set interrupt handler for the profiling clock
 */
void
setkgvector(intvec_func_t func)
{
    kgclockfunc = func;
}

/*
 * getcvpriority -- returns the relative priority of an interrupt
 * being installed in the CRIME interrupt vector table.
 *
 * The relative priority is used as an index into a local array
 * declared in the 2nd level interrupt handler.  If a particular
 * interrupt is active it's slot in the relative priority array
 * is marked with the bit index of the interrupt, which is then used
 * as an index into crimevec_tbl to find the appropriate handler.
 * The relative priority  array is traversed from the smallest index
 * to the largest, so smaller values of cv_priority indicate higher
 * priority interrupts.
 *
 * What we are doing here is basically presorting the interrupts
 * so that a single pass will suffice to prioritize interrupts at
 * interrupt level.  This eliminates extra context switches taken
 * when both a high and low priority interrupt are present at the
 * same time.
 */
short
getcvpriority(int intr)
{
    int i,j;
    int sorted;
    struct crimevec_s *cv1, *cv2, *cva[NCRMINTS];

    /*
     * First copy the addresses of the vector table
     * into a temporary array used for sorting.
     * We copy them in (mostly) spl sorted order so
     * that the actual sort will finish in one pass.
     */
    for (i = 0; i < NCRMINTS; i++) {
	cva[(NCRMINTS-1) - crimevec_tbl[i].cv_priority] = &crimevec_tbl[i];
    }

    /*
     * Sort interrupt handlers by spl level
     */
    sorted = 0;
    for (i = NCRMINTS - 1; i >= 0 && !sorted; i--) {
	sorted = 1;
	for (j = 1; j <= i; j++) {
	    cv1 = cva[j-1];
	    cv2 = cva[j];
	    if (cv1->cv_spl > cv2->cv_spl) {
		sorted = 0;
		cva[j-1] = cv2;
		cva[j] = cv1;
	    }
	}
    }

    /*
     * reassign priorities.
     */
    for (i = NCRMINTS - 1, j = 0; i >= 0; i--,j++) 
	cva[i]->cv_priority = j;

    return crimevec_tbl[intr].cv_priority;
}

/*
 * create_crimegang -- create a new ganged interrupt
 */
int
create_crimegang(int intr, int spl, intvec_func_t func, __psint_t arg,
		 short flags)
{
    struct crimevec_s *cv = &crimevec_tbl[intr];

    cv->cv_isr = crimegang;
    cv->cv_spl = spl;
    cv->cv_arg = intr; 
    /* 
     * we leave the cv_priority alone, since we are not changing the 
     * the top level assignment of priorities, we will come in at the
     * same IPL we came in before
     */

    if (crimegang_tbl[intr] == NULL) {
	crimegang_tbl[intr] = 
	    (struct crimevec_s *)
	    kmem_zalloc(sizeof(struct crimevec_s) * NCRMGANG, 
			KM_SLEEP);
    }

    if (crimegang_tbl[intr] == NULL)
	return 2;

    cv = crimegang_tbl[intr];
    cv->cv_isr = func;
    cv->cv_arg = arg;
    cv->cv_spl = spl;
    cv->cv_flags = flags;

    return 0;
}

int
join_crimegang(int intr, int spl, intvec_func_t func, __psint_t arg, 
	       short flags)
{
    int ospl = crimevec_tbl[intr].cv_spl;
    struct crimevec_s *cv;
    int i;

    /*
     * all ganged interrupts must be at the same
     * spl level.
     */
    if (ospl != spl<<3)
	return 1;

    /*
     * find an empty slot in this gang
     */
    cv = crimegang_tbl[intr];
    ASSERT(cv != NULL);
    if (!cv) 
	return 2;

    for (i = 0; i < NCRMGANG; i++) {
	if (!cv->cv_isr)
	    break;
	cv++;			
    }

    /*
     * we didn't find an empty slot
     */
    if (cv->cv_isr)
	return 2;

    cv->cv_isr = func;
    cv->cv_spl = spl<<3;
    cv->cv_arg = arg;
    cv->cv_priority = crimevec_tbl[intr].cv_priority;
    cv->cv_flags = flags;

    return 0;
}

/*
 * modify the IPL masks used to mask CRIME interrupts.
 */
void
setsplmasks(int intr, int spl)
{
    int i;

	
    /*
     * enable interrupt at all levels below spl
     */
    for (i = SPLMIN; i < spl; i++)
	private.p_splmasks[i] |= (1 << intr);

    /*
     * disable interrupt at all levels above spl
     */
    for (i = spl; i < SPLMAX; i++)
	private.p_splmasks[i] &= ~(1 << intr);
}

/*
 *----------------------------------------------------------------------------
 * setcrimevector
 * This routine sets the interrupt handler for a given crime interrupt
 * Returns 0 if successful
 *         1 if spl level is incorrect
 *         2 if unable to allocate an interrupt vector
 *----------------------------------------------------------------------------
 */
int
setcrimevector(int intr, int spl, intvec_func_t func,
               __psint_t arg, short flags)
{
    int s, rv = 0;
    struct crimevec_s *cv = &crimevec_tbl[intr];
    extern void crimestray();
    extern int graphics_intr_pri;
    extern int video_intr_pri;
    extern int default_intr_pri;

    /*
     * some sanity checks
     */
    if (!func)
        return 2;

    if (spl < SPLMIN || spl > SPLMAX)
        return 2;

    if (intr < 0 || intr > NCRMINTS)
        return 2;

    s = spl7();
    if (cv->cv_isr == crimegang) {
        rv = join_crimegang(intr, spl, func, arg, flags);
        goto out;
    }

    if ((cv->cv_flags & CRM_EXCL) && (cv->cv_isr != crimestray)) {
        rv = 2;
        goto out;
    }

    if ((flags & CRM_EXCL) && (cv->cv_isr != crimestray)) {
        rv = 2;
        goto out;
    }

    if (cv->cv_isr != crimestray) {
        /*
         * all interrupts on the same vector
         * must have the same spl level.
         */
        if (spl<<3 != cv->cv_spl) {
            rv = 1;
            goto out;
        }
        if (create_crimegang(intr, cv->cv_spl, cv->cv_isr,
                             cv->cv_arg, cv->cv_flags)) {
            rv = 2;
            goto out;
        }
        rv = join_crimegang(intr, spl, func, arg, flags);
        goto out;
    }

    cv->cv_isr    = func;
    cv->cv_spl    = spl<<3;
    cv->cv_arg    = arg;
    cv->cv_priority = getcvpriority(intr);
    cv->cv_flags  = flags;
    cv->cv_ibit   = (1 << intr);

    /*
     * XXX Make sure retrace and preblank interrupts have highest priority.
     * XXX Should fix setcrimevector to allow priority to be adjusted.
     */
    if ((intr == GBE_INTR(0)) || (intr == GBE_INTR(1))) 
	cv->acv_pri = graphics_intr_pri;
    else if( ( intr == MACE_VID_OUT )
	  || ( intr == MACE_VID_IN_1 )
	  || ( intr == MACE_VID_IN_2 ) ) {
	cv->acv_pri = video_intr_pri;
    }
    else
	cv->acv_pri = default_intr_pri;;

    if (!(flags & CRM_DRVENB))
	setsplmasks(intr, spl);

out:
    /*
     * Scan the table of interrupts that are threaded
     * If this particular interrupt is an ithread, register it
     * and also call the routine: crimevec_thread_setup()
     */
    if (is_thd(intr) && !(cv->cv_tflags & THD_EXIST)) {
        atomicSetInt(&cv->cv_tflags, THD_ISTHREAD|THD_REG);
        if (ithreads_enabled){
                crimevec_thread_setup(intr, cv->acv_pri, cv);
        }
    }
    splx(s);
    return rv;
}

/*
 *----------------------------------------------------------------------------
 * enable_ithreads()
 * This routine scans the crime vector table. It then sets up any interrupt
 * threads that might be called out of main.  It also sets 'ithreads_enabled'.
 *----------------------------------------------------------------------------
 */
void
enable_ithreads(void)
{
    	int		s;
    	int		intr;
	crimevec_t	*cv;

	s = spl7();
        ithreads_enabled = 1;
        for (intr = 0; intr < NCRMINTS; intr++){
                cv = &crimevec_tbl[intr];
                if (cv->cv_tflags & THD_REG){
                        /*
                         * Assert: cv *is* an ithread
                         * Assert: cv *is not* ready to run
                         * Then call: crimevec_thread_setup()
                         */
                        ASSERT ( (cv->cv_tflags & THD_ISTHREAD));
                        ASSERT (!(cv->cv_tflags & THD_OK));
                        crimevec_thread_setup(intr, cv->acv_pri, cv);
                }
        }
	splx(s);
        return;
}

/*
 *----------------------------------------------------------------------------
 * crimevec_thread_setup()
 * This routine handles thread setup, given a crime vector
 *----------------------------------------------------------------------------
 */
void
crimevec_thread_setup(int intr, int pri, crimevec_t *cv)
{
        char    thread_name[32];

        /* XXX What about thread_name ?    */

        sprintf(thread_name, "crime_intr(%d)", intr);
        xthread_setup(thread_name, pri, &cv->cv_tinfo,
                      (xt_func_t *) crimevec_intrd_init, (void *) cv);
        return;
}

/*
 *----------------------------------------------------------------------------
 * crimevec_intrd_init()
 *----------------------------------------------------------------------------
 */
void
crimevec_intrd_init(crimevec_t *cv)
{
     xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *) crimevec_intrd, cv);
     atomicSetInt(&cv->cv_tflags, THD_INIT);

     /* Now, this ithread will wait at the beginning   */
     /* of the routine: crimevec_intrd(). Everytime    */
     /* someone vsema()'s this thread, execution will  */
     /* commence at the beginning of: crimevec_intrd() */

     ipsema(&cv->cv_isync);
     /* This point is never reached */
}

/*
 *----------------------------------------------------------------------------
 * crimevec_intrd()
 * This routine is a wrapper around the specific isr that runs as an ithread
 * The ithread remains waiting at the very beginning. Everytime someone sig-
 * nals this ithread by means of a vsema(), the ithread will re-execute from
 * the very beginning, and invoke the required isr.
 *----------------------------------------------------------------------------
 */
void
crimevec_intrd(crimevec_t *cv)
{
        int s;

#ifdef ITHREAD_LATENCY
	xthread_update_latstats(cv->cv_latstats);
#endif

        /*
         * crime_intr has cleared the CRM_INTMASK bit,
         * reenable the bit, when the isr returns.
         */
        (*cv->cv_isr)(0, cv->cv_arg);

        s = disableintr();
 
        /* re-enable bit in private.p_splmasks */
	if (!(cv->cv_flags & CRM_DRVENB))
		CRIME_INTR_ENABLE(cv->cv_ibit, cv->cv_spl);
        /*
         * no need to update CRM_INTMASK with the cv_ibit,
         * this will be done for us via enableintr/splx. 
         */
        enableintr(s);
     
        /* Wait for the next interrupt */
        ipsema(&cv->cv_isync);
} 

static void 
bust_crimegang(struct crimevec_s *cv, int intr)
{
    struct crimevec_s *ncv = &crimevec_tbl[intr];

    ncv->cv_isr = cv->cv_isr;
    ncv->cv_arg = cv->cv_arg;
    ncv->cv_spl = cv->cv_spl;
    ncv->cv_flags = cv->cv_flags;
    ncv->cv_priority = cv->cv_priority;

    cv->cv_isr = NULL;
    cv->cv_spl = 0;
    cv->cv_arg = 0;
    cv->cv_flags = 0;
    cv->cv_priority = 0;
}

static int 
leave_crimegang(intvec_func_t func, int intr)
{
    int i;
    struct crimevec_s *cv = crimegang_tbl[intr];
    struct crimevec_s *ncv;
    int s = spl7();
    int rv = 0;
    int cnt;

    /*
     * nothin' there, somethin' wrong!
     */
    if (!cv) {
	goto out;
    }

    /*
     * find our interrupt.
     */
    for (i = 0; i < NCRMGANG; i++, cv++) {
	if (!cv->cv_isr) {
	    goto out;
	} else if (cv->cv_isr == func) {
	    break;
	} else {
	    cnt++;
	}
    } 

    /* 
     * didn't find our function 
     */ 
    if (i == NCRMGANG) {
	goto out;
    }

    /*
     * zero this baby out -- it's gone!
     */
    cv->cv_isr = NULL;
    cv->cv_arg = 0;
    cv->cv_spl = 0;
    cv->cv_flags = 0;
    cv->cv_priority = 0;

    /*
     * now we walk through the table looking for another
     * moving each isr into the position of the previous
     * one.
     */
    ncv = cv + 1;

    for (++i; i < NCRMGANG; i++) {
	if (ncv->cv_isr)
	    cnt++;
	cv->cv_isr = ncv->cv_isr;
	cv->cv_arg = ncv->cv_arg;
	cv->cv_spl = ncv->cv_spl;
	cv->cv_flags = ncv->cv_flags;
	cv->cv_priority = ncv->cv_priority;
	cv++; ncv++;
    }

	
    /* and finally, zero out the last entry in the table */
    cv->cv_isr = NULL;
    cv->cv_arg = 0;
    cv->cv_spl = 0;
    cv->cv_flags = 0;
    cv->cv_priority = 0;

    if (cnt <= 1)
	bust_crimegang(crimegang_tbl[intr], intr);

    rv = 1;
	
out:
    splx(s);
    return(rv);
}
	

/*
 * remove a previously installed interrupt handler from the crime
 * interrupt subsystem.  returns 0 if requested func was not removed, 
 * 1 if it was.
 */
int
unsetcrimevector(int intr, intvec_func_t func)
{
    struct crimevec_s *cv = &crimevec_tbl[intr];
    int s;

    if (cv->cv_isr == crimestray)
	return 0;

    if (cv->cv_isr == crimegang)
	return leave_crimegang(func, intr);

    s = spl7();	
    cv->cv_isr = crimestray;
    cv->cv_spl = 0;
    cv->cv_arg = cv->cv_priority = getcvpriority(intr);
    cv->cv_flags = 0;
    setsplmasks(intr, 0);
    splx(s);

    return 1;
}

#if 0
/*ARGSUSED*/
static void
powerfail(__psint_t arg, eframe_t *ep)
{
    cmn_err(CE_WARN,"Power failure detected\n");
}
#endif


/*
 * sendintr - send an interrupt to another CPU; fatal error on IP32.
 */
/*ARGSUSED*/
int
sendintr(cpuid_t destid, unchar status)
{
    cmn_err(CE_PANIC, "sendintr");
    /* NOTREACHED */
}
