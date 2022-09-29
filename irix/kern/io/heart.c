/**************************************************************************
 *									  *
 *	    Copyright (C) 1990-1996, 1998, Silicon Graphics, Inc      	  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident	"io/heart.c: $Revision: 1.116 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/invent.h>
#include <sys/debug.h>
#include <sys/sbd.h>
#include <sys/kmem.h>
#include <sys/edt.h>
#include <sys/dmamap.h>
#include <sys/systm.h>
#include <ksys/hwg.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/map.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/atomic_ops.h>
#include <sys/sysinfo.h>
#include <sys/ddi.h>
#include <sys/runq.h>
#include <ksys/xthread.h>
#include <sys/rtmon.h>

#include <sys/proc.h>

#include <sys/xtalk/xtalk.h>
#include <sys/xtalk/xswitch.h>
#include <sys/xtalk/xwidget.h>

#include <sys/xtalk/xtalk_private.h>

#include <sys/RACER/heart.h>
#include <sys/RACER/heartio.h>
#include <sys/RACER/heart_vec.h>

#define NEWf(ptr,f)	(ptr = kmem_zalloc(sizeof (*(ptr)), (f&XTALK_NOSLEEP)?KM_NOSLEEP:KM_SLEEP))
#define NEW(ptr)	(ptr = kmem_zalloc(sizeof (*(ptr)), KM_SLEEP))
#define DEL(ptr)	(kmem_free(ptr, sizeof (*(ptr))))

int			heart_devflag = D_MP;

#if ECC_DEBUG
extern void		heart_ecc_test(void);
#endif

/* =====================================================================
 *    Function Table of Contents
 */

static void		heart_intr_bits_free(__uint64_t);
static void		heart_intr_bits_take(__uint64_t);
static int		heart_intr_bit_alloc(__uint64_t);
static int		heart_intr_level_alloc(int);

void			heart_stray_intr(intr_arg_t);
static void		heart_cpu_intr(intr_arg_t);
static void		heart_debug_intr(intr_arg_t);

void			heart_mlreset(void);
void			heart_mp_intr_init(cpuid_t);
void			heart_init(void);

void			heart_startup(vertex_hdl_t);
int			heart_attach(vertex_hdl_t);

static xwidgetnum_t	heart_widget_num(vertex_hdl_t);

iopaddr_t		heart_addr_phys_to_xio(iopaddr_t, size_t);
iopaddr_t		heart_addr_xio_to_phys(xwidgetnum_t, iopaddr_t, size_t);
caddr_t			heart_addr_xio_to_kva(xwidgetnum_t, iopaddr_t, size_t);

heart_piomap_t		heart_piomap_alloc(vertex_hdl_t, device_desc_t, iopaddr_t, size_t, size_t, unsigned);
void			heart_piomap_free(heart_piomap_t);
caddr_t			heart_piomap_addr(heart_piomap_t, iopaddr_t, size_t);
void			heart_piomap_done(heart_piomap_t);
caddr_t			heart_piotrans_addr(vertex_hdl_t, device_desc_t, iopaddr_t, size_t, unsigned);

heart_dmamap_t		heart_dmamap_alloc(vertex_hdl_t, device_desc_t, size_t, unsigned);
void			heart_dmamap_free(heart_dmamap_t);
iopaddr_t		heart_dmamap_addr(heart_dmamap_t, paddr_t, size_t);
alenlist_t		heart_dmamap_list(heart_dmamap_t, alenlist_t, unsigned);
void			heart_dmamap_done(heart_dmamap_t);
iopaddr_t		heart_dmatrans_addr(vertex_hdl_t, device_desc_t, paddr_t, size_t, unsigned);
alenlist_t		heart_dmatrans_list(vertex_hdl_t, device_desc_t, alenlist_t, unsigned);

void			heart_dmamap_drain(heart_dmamap_t);
void			heart_dmaaddr_drain(vertex_hdl_t, paddr_t, size_t);
void			heart_dmalist_drain(vertex_hdl_t, alenlist_t);

void			heart_intr_preconn(void *, xtalk_intr_vector_t, intr_func_t, intr_arg_t);
void			heart_intr_prealloc(void *, xtalk_intr_vector_t, xwidget_intr_preset_f *, void *, int);
heart_intr_t		heart_intr_alloc(vertex_hdl_t, device_desc_t, vertex_hdl_t);
void			heart_intr_free(heart_intr_t);
int			heart_intr_connect(heart_intr_t, intr_func_t, intr_arg_t, xtalk_intr_setfunc_t, void *, void *);
void			heart_intr_disconnect(heart_intr_t);
vertex_hdl_t		heart_intr_cpu_get(heart_intr_t);

void			heart_provider_startup(vertex_hdl_t);
void			heart_provider_shutdown(vertex_hdl_t);

xwidgetnum_t		heart_paddr_to_port(iopaddr_t, iopaddr_t *, iopaddr_t *);

int			heart_error_refine(char *, int, ioerror_mode_t, ioerror_t *);
static int		heart_error_handler(error_handler_arg_t, int, ioerror_mode_t, ioerror_t *);

void			heart_intr(struct eframe_s *, heartreg_t);

int			heart_error_devenable(vertex_hdl_t, int, int);

void			heart_intrd(heart_ivec_t *);
void			heart_intrd_start(heart_ivec_t *);
int 			heart_intr_swlevel_to_hwlevel(int);
void			heart_ithread_setup(cpuid_t, int, ilvl_t);
void			enable_ithreads(void);

int			heart_rev(void);

/* =====================================================================
 *    STATIC "PER-HEART" VARIABLES
 *
 *	Lots of stuff breaks if we want to work across
 *	multiple hearts, since the CPUs on one heart
 *	will not be cache coherent with the CPUs on
 *	another heart. Beside this, the problems with
 *	finding the "correct" interrupt handler list are
 *	easy. One approach is to use the heart's
 *	crosstalk slot number from the status
 *	register. But we defer solving this problem
 *	until it looks like we might have such a configuraion.
 */

/* XXX MP/prempt -- we need a lock around heart rmw and data structures */

vertex_hdl_t		the_heart_vhdl = GRAPH_VERTEX_NONE;
vertex_hdl_t		the_error_vhdl = GRAPH_VERTEX_NONE;

int			the_heart_rev = -1;

__uint64_t		heart_ivec_free = 0x07f803fffffffff8;

heart_ivec_t		heart_ivec[HEART_INT_VECTORS] =
{
    {heart_stray_intr, INTR_ARG(0),},	/* Interrupt Request */
    {heart_stray_intr, INTR_ARG(1),},	/* Flow Control Timeout 0 */
    {heart_stray_intr, INTR_ARG(2),},	/* Flow Control Timeout 1 */
    {heart_stray_intr, INTR_ARG(3),},	/* dynamic low priority int */

    {heart_stray_intr, INTR_ARG(4),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(5),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(6),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(7),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(8),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(9),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(10),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(11),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(12),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(13),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(14),},	/* dynamic low priority int */
    {heart_stray_intr, INTR_ARG(15),},	/* dynamic low priority int */

    {heart_stray_intr, INTR_ARG(16),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(17),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(18),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(19),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(20),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(21),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(22),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(23),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(24),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(25),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(26),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(27),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(28),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(29),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(30),},	/* dynamic med priority int */
    {heart_stray_intr, INTR_ARG(31),},	/* dynamic med priority int */

    {heart_stray_intr, INTR_ARG(32),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(33),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(34),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(35),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(36),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(37),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(38),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(39),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(40),},	/* dynamic high priority int */
    {heart_stray_intr, INTR_ARG(41),},	/* dynamic high priority int */
    {heart_debug_intr, INTR_ARG(42),},	/* cpu0 debugger int */
    {heart_debug_intr, INTR_ARG(43),},	/* cpu1 debugger int */
    {heart_debug_intr, INTR_ARG(44),},	/* cpu2 debugger int */
    {heart_debug_intr, INTR_ARG(45),},	/* cpu3 debugger int */

    {heart_cpu_intr, INTR_ARG(46),},	/* cpu0 interprocessor int */
    {heart_cpu_intr, INTR_ARG(47),},	/* cpu1 interprocessor int */
    {heart_cpu_intr, INTR_ARG(48),},	/* cpu2 interprocessor int */
    {heart_cpu_intr, INTR_ARG(49),},	/* cpu3 interprocessor int */

    {heart_stray_intr, INTR_ARG(50),},	/* heart counter/timer int */

    {heart_stray_intr, INTR_ARG(51),},	/* xtalk widget error */
    {heart_stray_intr, INTR_ARG(52),},	/* xtalk widget error */
    {heart_stray_intr, INTR_ARG(53),},	/* xtalk widget error */
    {heart_stray_intr, INTR_ARG(54),},	/* xtalk widget error */
    {heart_stray_intr, INTR_ARG(55),},	/* xtalk widget error */
    {heart_stray_intr, INTR_ARG(56),},	/* xtalk widget error */
    {heart_stray_intr, INTR_ARG(57),},	/* xtalk widget error */
    {heart_stray_intr, INTR_ARG(58),},	/* xtalk widget error */
	/* Vectors 59 thru 63 are siphoned off
	 * and sent over to buserror_intr with
	 * the (eframe_t *) which may not fit
	 * in an intr_arg_t anyway.
	 */
    {heart_stray_intr, INTR_ARG(59),},	/* cpu0 bus error */
    {heart_stray_intr, INTR_ARG(60),},	/* cpu1 bus error */
    {heart_stray_intr, INTR_ARG(61),},	/* cpu2 bus error */
    {heart_stray_intr, INTR_ARG(62),},	/* cpu3 bus error */
    {heart_stray_intr, INTR_ARG(63),},	/* heart exception */
};

void			powerfail_ql_thread(intr_arg_t);
static heart_ivec_t	acfail_ivec = {powerfail_ql_thread, 0};
thd_int_t	       *acfail_tinfo = 0;

int		ithreads_enabled = 0;

/* =====================================================================
 *    MP REGISTER MANAGEMENT
 *
 *	Some Heart registers are updated with
 *	read-modify-write cycles, and just using the
 *	normal C "|=" or "&=~" operations is not safe in
 *	the face of MP and preemptable UP kernels and
 *	modification of the registers at interrupt time.
 *
 *	What we generally want here is something like
 *	an "splock" except that, if we are acting in
 *	response to an exception or to an interrupt that
 *	could happen while the lock is held, and if
 *	the current CPU "owns" the lock, we just do
 *	the RMW (and don't "free" the lock, either).
 */

/* NOTE: depends on cpuid_t never comparing equal to (long)-1.
 *    should be OK, cpuid_t is unsigned short.
 */

#define NOBODY	-1
long			heart_imr_own[4] =
{NOBODY, NOBODY, NOBODY, NOBODY};

/* Places that modify heart_imr directly:
 *    heart_mlreset (all)
 *	heart_mp_intr_init (specified)
 *	VEC_dbe (current)
 *	heart_imr_bits_rmw (specified)
 *
 * by calling heart_imr_bits_rmw,
 *   for current cpu:
 *	heart_stray_intr
 *	heart_intr_preconn
 *	dobuserr_common
 *	stopkgclock
 *
 *   for specified cpu:
 *	heart_intr_connect
 *	heart_intr_disconnect
 *
 *   for master_procid:
 *	startkgclock
 *	slowkgclock
 *	cpu_soft_powerdown
 */

heartreg_t
heart_imr_bits_rmw(cpuid_t which_imr,
		   heartreg_t mask,
		   heartreg_t value)
{
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;
    cpuid_t		    this_cpu;
    volatile heartreg_t	   *h_imr;
    long		   *where;
    unsigned		    s;
    heartreg_t		    rv;

    this_cpu = cpuid();
    which_imr &= 3;		/* sanitized for your protection */
    h_imr = &heart_piu->h_imr[which_imr];
    where = &heart_imr_own[which_imr];

    if (*where == this_cpu) {
	/* Get here if we take an exception while holding the lock on
	 * this IMR. It is OK to go ahead and change the register, since
	 * the net change across really high recoverable things is
	 * always zero, and other CPUs won't mess with this IMR since
	 * they don't own it.
	 */
	rv = *h_imr;
	*h_imr = (rv & ~mask) | value;
	flushbus();
    } else {
	/* We want to cooperate with code running in response to normal
	 * interrupts; since this is like a spin lock, we need to SPL
	 * above the IRL of the interrupts.
	 *
	 * We drop our SPL and hang out for a bit if we can't get the
	 * lock, in case the owner has been pinned by the case described
	 * above.
	 */

	/* This SPL must prevent interrupts that call this function to
	 * make permanent changes, and must prevent our thread from
	 * being preempted.
	 *
	 * NOTE: yes, this needs to be spl7; we can't handle heart
	 * exception interrupts while we own an IMR. Be very, very sure
	 * of the addresses "where" and "h_imr".
	 */
	s = spl7();		/* hold off interrupts before owning h_imr */

	while (!compare_and_swap_long(where, NOBODY, this_cpu)) {
	    splx(s);		/* don't hold off interrupts for long */
	    while (*where != NOBODY)	/* spin on cache until it */
		;		/* looks free (reduce coherency traffic) */
	    s = spl7();		/* hold off interrupts before owning h_imr */
	}

	rv = *h_imr;
	*h_imr = (rv & ~mask) | value;

	flushbus();
	*where = NOBODY;	/* release the lock */
	splx(s);
    }

    return rv;
}

/* =====================================================================
 *    INTERRUPT HANDLING
 *
 *	No provision is made for multiple devices to
 *	share heart interrupt vectors at this
 *	level. Lower level providers that wish to
 *	multiplex devices on a single vector need to
 *	register their interrupt demux routines with
 *	heart and manage distribution themselves.
 */

static void
heart_intr_bits_free(__uint64_t mask)
{
    atomicSetUlong(&heart_ivec_free, mask);
}

static void
heart_intr_bits_take(__uint64_t mask)
{
    atomicClearUlong(&heart_ivec_free, mask);
}

static int
heart_intr_bit_alloc(__uint64_t mask)
{
    __uint64_t		    bits;
    __uint64_t		    temp;
    int			    bit;

    while (1) {
	/* which ones are available? */
	bits = mask & heart_ivec_free;
	if (bits == 0)
	    return -1;		/* none available */

	/* chose one. this gets the lowest one. */
	bits = bits & ~(bits - 1);

	/* try to allocate it. */
	temp = atomicClearUlong(&heart_ivec_free, bits);

	/* if we got it, return the bit index. */
	if (temp & bits) {
	    /* Figure out the vector number. */
	    bit = 0;
	    temp = bits;
	    while (temp >>= 8)
		bit += 8;
	    temp = bits >> bit;
	    while (temp >>= 1)
		bit += 1;
	    return bit;
	}
    }
}

int		swlevel_thread_hi = 250;
int		swlevel_thread_med = 240;
int		swlevel_thread_lo = 5;

int heart_intr_swlevel_to_hwlevel(int pri)
{
    	/* Change a software interrupt to a hardware interrupt */
    
    	if (pri < 0)
	{
		cmn_err(CE_PANIC,
		  "heart: unable to allocate level %d interrupt\n",
		  pri);
		return -1;
	}
	if (pri >= swlevel_thread_hi)
		return 2;
	if (pri >= swlevel_thread_med)
	    	return 1;
	return 0;
}    
	    

static int
heart_intr_level_alloc(int level)
{
    int			    ibit;

    switch (level) {
    case 4:
	if ((ibit = heart_intr_bit_alloc(HEART_INT_LEVEL4)) != -1)
	    return ibit;
    case 3:
	if ((ibit = heart_intr_bit_alloc(HEART_INT_LEVEL3)) != -1)
	    return ibit;
    case 2:
	if ((ibit = heart_intr_bit_alloc(HEART_INT_LEVEL2)) != -1)
	    return ibit;
    case 1:
	if ((ibit = heart_intr_bit_alloc(HEART_INT_LEVEL1)) != -1)
	    return ibit;
    case 0:
	/*
	 * We can't use Heart LEVEL0 (IRL2) interrupts for
	 * generic stuff on Rev.A and Rev.B hearts.
	 */
	if (heart_rev() > HEART_REV_B)
	    if ((ibit = heart_intr_bit_alloc(HEART_INT_LEVEL0)) != -1)
		return ibit;

	/*
	 * In the threaded world, the precise interrupt level
	 * is of lesser importance. If we couldn't get
	 * the interrupt level we wanted, try the other
	 * general levels.
	 */
	if (level < 1)
	    if ((ibit = heart_intr_bit_alloc(HEART_INT_LEVEL1)) != -1)
		return ibit;

	if (level < 2)
	    if ((ibit = heart_intr_bit_alloc(HEART_INT_LEVEL2)) != -1)
		return ibit;

    default:
	cmn_err(CE_PANIC,
	     "heart: unable to allocate level %d interrupt\n",
		level);
    }
    return -1;
}

/*ARGSUSED */
void
heart_stray_intr(intr_arg_t arg)
{
    int			    vect = (int) (__psunsigned_t) arg;

    heart_imr_bits_rmw(cpuid(), 1ull << vect, 0);

    cmn_err(CE_CONT, "heart: stray interrupt at vector %d.\n", vect);
}

/*ARGSUSED */
static void
heart_cpu_intr(intr_arg_t arg)
{
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;

    heart_piu->h_clr_isr = HEART_ISR_IPI;
    doacvec();
    doactions();
}

/*ARGSUSED */
static void
heart_debug_intr(intr_arg_t arg)
{
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;

    heart_piu->h_clr_isr = HEART_ISR_DEBUG;
    debug("zing");
}

/*
 * Poll the interrupt register to see if another cpu has asked us
 * to drop into the debugger (without lowering spl).
 */
void
chkdebug(void)
{
	heart_piu_t	*heart_piu = HEART_PIU_K1PTR;

	if (heart_piu->h_isr & HEART_ISR_DEBUG) {
		heart_piu->h_clr_isr = HEART_ISR_DEBUG;
		debug("zing");
	}
}

/* clear inter-processor interrupt */
void
clear_cpu_intr(cpuid_t destid)
{
	HEART_PIU_K1PTR->h_clr_isr = HEART_IMR_IPI_(destid);
}

#if HEART_BONKIFY
void		       *heart_bonkify_here = 0;

static void
heart_bonkify(void *here)
{
    void *there = heart_bonkify_here;
    heart_bonkify_here = 0;
    if (here)
	*(volatile int *) here;
    if (there) {
	cmn_err(CE_WARN, "heart_bonkify: will hit 0x%x ...\n", there);
	timeout(heart_bonkify, there, 10*HZ);
    } else
	timeout(heart_bonkify, 0, HZ);
}
#endif

/* =====================================================================
 *	Heart "Device Driver"
 */

/*
 *    heart_mlreset: called at mlreset time to take
 *	care of anything we need to do that early, when
 *	platform specific code determines that we have
 *	a Heart that needs initializing.
 */

void
heart_mlreset(void)
{
    heart_cfg_t		   *heart_cfg = HEART_CFG_K1PTR;
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;
    cpuid_t		    cpu = getcpuid();

    /* clear pending error interrupts, shouldn't have any */
    /* ASSERT(heart_piu->h_cause == 0x0); */
    heart_cfg->h_wid_err_mask = 0;
    heart_cfg->h_wid_err_type = heart_cfg->h_wid_err_type;
    heart_cfg->h_widget.w_err_cmd_word = 0x0;
    heart_piu->h_cause = heart_piu->h_cause;

    /* enable all error interrupts except for
     * a few things we ignore.
     * NOTE: ERRTYPE_PIO_RD_TIMEOUT will also
     * show up as a sync trap; if we allow
     * heart to send an interrupt, we might
     * take the interrupt rather than the sync
     * trap, which makes it harder to diagnose.
     */
    heart_cfg->h_wid_err_mask = ERRTYPE_ALL
	&~ ERRTYPE_LLP_RCV_SN_ERR
	&~ ERRTYPE_LLP_RCV_CB_ERR
	&~ ERRTYPE_LLP_RCV_SQUASH_DATA
	&~ ERRTYPE_LLP_TX_RETRY
	&~ ERRTYPE_PIO_RD_TIMEOUT
	;

    /* Clear interrupt masks on all CPUs */
    heart_piu->h_imr[0] = 0;	/* MP/MT OK */
    heart_piu->h_imr[1] = 0;	/* MP/MT OK */
    heart_piu->h_imr[2] = 0;	/* MP/MT OK */
    heart_piu->h_imr[3] = 0;	/* MP/MT OK */

    /* Enable heart exception, CPU bus error,
     * heart timer interrupt and
     * interprocessor interrupt for
     * the current [master] processor.
     */
    heart_piu->h_imr[cpu] =	/* MP/MT OK */
	((HEART_INT_EXC << HEART_INT_L4SHIFT) |
	 HEART_IMR_BERR_(cpu) |
	 HEART_IMR_IPI_(cpu) |
	 HEART_IMR_DEBUG_(cpu));
}

/* Called to intialize interrupts on non-boot CPU.
 */
void
heart_mp_intr_init(cpuid_t cpu)
{
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;

    /* Enable heart exception, CPU bus error
     * and interprocessor interrupt for the
     * specified processor.
     */
    heart_piu->h_imr[cpu] =	/* MP/MT OK */
	((HEART_INT_EXC << HEART_INT_L4SHIFT) |
	 HEART_IMR_BERR_(cpu) |
	 HEART_IMR_IPI_(cpu) |
	 HEART_IMR_DEBUG_(cpu));
}

/*
 *    heart_init: called once during system startup or
 *	when a loadable driver is loaded. Call bus
 *	provider register routine from here.
 *
 *    NOTE: a bus provider register routine should always be
 *	called from _reg, rather than from _init. In the case
 *	of a loadable module, the devsw is not hooked up
 *	when the _init routines are called.
 *
 */
void
heart_init(void)
{
    xwidget_driver_register(HEART_WIDGET_PART_NUM,
			    HEART_WIDGET_MFGR,
			    "heart_",
			    0);
}

/*
 *    heart_startup: called after all device XXX_init
 *	routines have been called; given the vertex
 *	representing the heart, and the pointer to the
 *	PIU registers, responsible for generating all
 *	the nodes downstream of heart.
 */
void
heart_startup(vertex_hdl_t conn)
{
    /* REFERENCED */
    graph_error_t	    rc;
    vertex_hdl_t	    heart, xswitch, widget;
    widget_cfg_t	   *wptr;
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;
    heart_cfg_t		   *heart_cfg = HEART_CFG_K1PTR;
    struct xwidget_hwid_s   hwid;
    heartreg_t		    status;
    int			    heartid;
    int			    port;
    widgetreg_t		    id;
    /*REFERENCED*/
    int			    rev;
    xswitch_info_t	    xswitch_info;
#if HEART_COHERENCY_WAR
    extern int		    fibbed_heart;
#endif	/* HEART_COHERENCY_WAR */

#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "heart_startup: %v\n", conn);
#endif

    status = heart_piu->h_status;
    heartid = status & HEART_STAT_WIDGET_ID;

    /* Provide a short PIO Read Response timeout
     * for debug kernels, and a longer one for
     * production kernels.
     */
#if !DEBUG
    if (!kdebug)
	heart_cfg->h_wid_req_timeout = 40000;		/* ~50ms */
    else
#endif
 	heart_cfg->h_wid_req_timeout = 128;		/* ~163us (GIO) */

    /*
     * Create a vertex to represent the heart.
     */
    rc = hwgraph_path_add(conn, EDGE_LBL_NODE, &heart);
    ASSERT(rc == GRAPH_SUCCESS);

#if DEBUG || SHOW_REVS
    rev = heart_rev();

#if !DEBUG
    if (kdebug)
#endif
	cmn_err(CE_CONT,
		"Heart ASIC: %sfibbed rev %c (code=%d)\n",
#ifdef HEART_COHERENCY_WAR
		fibbed_heart ? "" : "non-",
#else /* !HEART_COHERENCY_WAR */
		"",
#endif /* HEART_COHERENCY_WAR */
		'A' + rev - HEART_REV_A,
		rev);
#endif

#if DEBUG
    ASSERT(the_heart_vhdl == GRAPH_VERTEX_NONE);
    the_heart_vhdl = heart;
#endif

    xtalk_provider_register(heart, &heart_provider);
    xtalk_provider_startup(heart);

    /*
     * Create the vertex or verticies corresponding to the
     * thing attached to the heart, and get it initialized;
     * if it is a switch, it will generate additional
     * verticies representing attached widgets, and mark
     * itself as an xswitch.
     *
     * To keep paths relatively constant, we always
     * have an edge "xtalk" pointing at a vertex
     * reserved for a crosstalk switch, off of which
     * hangs a connection point.
     *
     * the edge from the
     * switch vertex to the connection point is
     * labelled "0". Switch drivers will then back
     * up from the connect point to find the switch
     * point, and leave some switch-related
     * information attached (in addition to creating
     * their own private vertex, if appropriate).
     */

    rc = hwgraph_path_add(heart, EDGE_LBL_XTALK, &xswitch);
    ASSERT(rc == GRAPH_SUCCESS);
    xswitch_id_set(xswitch,0);
    rc = hwgraph_path_add(xswitch, "0", &widget);
    ASSERT(rc == GRAPH_SUCCESS);

    the_error_vhdl = widget;

    /* add an "io" edge parallel to the "xtalk" edge */
    rc = hwgraph_edge_add(heart, xswitch, EDGE_LBL_IO);
    ASSERT(rc == GRAPH_SUCCESS);

    (void) device_master_set(widget, heart);

    /* Set up the widget that is connected to us */
    wptr = (widget_cfg_t *) K1_MAIN_WIDGET(XBOW_ID);
    id = wptr->w_id;

    hwid.part_num = XWIDGET_PART_NUM(id);
    hwid.mfg_num = XWIDGET_MFG_NUM(id);

    (void) xwidget_init(&hwid, widget,XBOW_ID, heart, heartid, NULL);

    /*
     * If the newly attached and initialized device is in fact a
     * crosstalk switch, it will build an xswitch_info node
     * containing the census of widgets on the bus.
     *
     * XXX- This does not allow us to use a loadable device driver
     * for a switch attached to heart. Tsk, tsk. To fix this
     * problem, we would have to move the following loop into a
     * callback that the xswitch's attach routine could call in the
     * proper xtalk master providers.
     */

    xswitch_info = xswitch_info_get(xswitch);

    if (xswitch_info != NULL) {

	port = HEART_MAX_PORT + 1;

	while (port-- > HEART_MIN_PORT)
	    if (xswitch_info_link_ok(xswitch_info, port)) {
		char			name[4];

		/*
		 * Create the vertex for the widget,
		 * using the decimal port number as the
		 * name of the primary edge.
		 */
		sprintf(name, "%d", port);
		rc = hwgraph_path_add(xswitch, name, &widget);
		ASSERT(rc == GRAPH_SUCCESS);

		/*
		 * crosstalk switch code tracks which
		 * widget is attached to each link; the
		 * switch does not count on the naming
		 * of the edges (this is faster, too).
		 */
		xswitch_info_vhdl_set(xswitch_info,
				      port, widget);

		/*
		 * Peek at the widget to get its
		 * crosstalk part and mfgr numbers,
		 * then present it to the generic xtalk
		 * bus provider to have its driver
		 * attach routine called (or not).
		 */
		wptr = (widget_cfg_t *) K1_MAIN_WIDGET(port);
		id = wptr->w_id;

		hwid.part_num = (id & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;
		hwid.mfg_num = (id & WIDGET_MFG_NUM) >> WIDGET_MFG_NUM_SHFT;

		(void) xwidget_init(&hwid, widget, port,
				    heart, heartid,
				    NULL);
	    }
    }
#if ECC_DEBUG
    heart_ecc_test();
#endif
#if HEART_BONKIFY
    heart_bonkify(0);
#endif
}

/*
 *    heart_attach: called every time the crosstalk
 *	infrastructure is asked to initialize a widget
 *	that matches the part number we handed to the
 *	registration routine above.
 *
 *	This establishes the backlink from the heart's
 *	xtalk connection point to the heart.
 *
 *	NOTE: There have been proposals for placing
 *	additional HEART chips on XTalk cards. This
 *	is where code to support such cards should be
 *	added; care must, of course, be taken not to
 *	treat the "primary" bridge as such an expansion
 *	card.
 *
 *	Currently, this routine does NOTHING IMPORTANT,
 *	other than verify that we can get a mapping from
 *	xtalk_piotrans_addr to heart's widget
 *	configuration registers.
 */
/*ARGSUSED */
int
heart_attach(vertex_hdl_t xconn)
{
    xwidget_info_t	    widget_info;
    vertex_hdl_t	    heart;

#if DEBUG && ATTACH_DEBUG
    cmn_err(CE_CONT, "%v: heart_attach\n", xconn);
#endif

    widget_info = xwidget_info_get(xconn);
    ASSERT(widget_info != NULL);
    heart = xwidget_info_master_get(widget_info);
    ASSERT(heart != GRAPH_VERTEX_NONE);
#if DEBUG
    ASSERT(heart == the_heart_vhdl);
#endif
    hwgraph_edge_add(xconn, heart, EDGE_LBL_HEART);
#ifdef	EDGE_LBL_CONN
    hwgraph_edge_add(heart, xconn, EDGE_LBL_CONN);
#endif
    xwidget_error_register(xconn, heart_error_handler, widget_info);

    return 0;
}

/* =====================================================================
 *	Heart "Bus Provider"
 */

/*
 *    heart_dev_widget: translate a device vertex
 *	handle into a crosstalk port number.
 */
static xwidgetnum_t
heart_widget_num(vertex_hdl_t xconn)
{
    xwidget_info_t	    widget_info = xwidget_info_get(xconn);
    xwidgetnum_t	    widget = xwidget_info_id_get(widget_info);

    return widget;
}

/* =====================================================================
 *    PIU MANAGEMENT
 *
 *	Heart has no modifiable state related to PIO mappings;
 *	all PIO is done via fixed windows.
 */

struct heart_piomap_s {
    struct xtalk_piomap_s   xtalk;	/* xconn, target, addr, size, kvaddr */
    unsigned		    flags;
};

/*ARGSUSED*/
iopaddr_t
heart_addr_phys_to_xio(iopaddr_t paddr,
		       size_t byte_count)
{
    xwidgetnum_t	widget;
    iopaddr_t		offset;

#if PARANOID
    if (byte_count > HUGE_IO_SIZE)
	return XIO_NOWHERE;
#endif

    if (paddr < SYSTEM_MEMORY_ALIAS_SIZE)
#if PARANOID
	if ((paddr + byte_count) <= SYSTEM_MEMORY_ALIAS_SIZE)
#endif
	    paddr += ABS_SYSTEM_MEMORY_BASE;

    if (paddr >= ABS_SYSTEM_MEMORY_BASE)
#if PARANOID
	if ((paddr+byte_count) <= ABS_SYSTEM_MEMORY_TOP)
#endif
	    return paddr;

    if (IS_MAIN_IO_SPACE(paddr)) {
	widget = MAIN_IO_TO_WIDGET_ID(paddr);
	offset = paddr % MAIN_IO_SIZE;
#if PARANOID
	if ((byte_count + offset) <= MAIN_IO_SIZE)
#endif
	    if (widget == HEART_ID)
		return offset;
	    else
		return XIO_PACK(widget, offset);
    }

    if (IS_LARGE_IO_SPACE(paddr)) {
	widget = LARGE_IO_TO_WIDGET_ID(paddr);
	offset = paddr % LARGE_IO_SIZE;
#if PARANOID
	if ((byte_count + offset) <= LARGE_IO_SIZE)
#endif
	    if (widget == HEART_ID)
		return offset;
	    else
		return XIO_PACK(widget, offset);
    }

    if (IS_HUGE_IO_SPACE(paddr)) {
	widget = HUGE_IO_TO_WIDGET_ID(paddr);
	offset = paddr % HUGE_IO_SIZE;
#if PARANOID
	if ((byte_count + offset) <= HUGE_IO_SIZE)
#endif
	    if (widget == HEART_ID)
		return offset;
	    else
		return XIO_PACK(widget, offset);
    }

    return XIO_NOWHERE;
}

iopaddr_t
heart_addr_xio_to_phys(xwidgetnum_t widget,
		       iopaddr_t offset,
		       size_t length)
{
#if PARANOID
    if ((length < 1) ||
	(length > HUGE_IO_SIZE) ||
	(offset >= HUGE_IO_SIZE) ||
	(widget > 15))
	return NULL;
#endif

    if (XIO_PACKED(offset)) {
	widget = XIO_PORT(offset);
	offset = XIO_ADDR(offset);
    }

    if ((offset + length) <= MAIN_IO_SIZE)
	return MAIN_WIDGET(widget) + offset;

    if ((offset + length) <= LARGE_IO_SIZE)
	return LARGE_WIDGET(widget) + offset;

#if PARANOID
    if (widget == 0)
	return NULL;
#endif

    if ((offset + length) <= HUGE_IO_SIZE)
	return HUGE_WIDGET(widget) + offset;

    return NULL;
}

/*
 *    heart_addr_xio_to_kva: This routine encapsulates
 *	knowledge of how to get to specific address
 *	ranges on a given crosstalk widget.
 *
 *	   If the request can be satisfied within one of
 *	the MAIN widget mapping windows, then we return
 *	the starting address within that window.
 *
 *	   If the request can be satisfied within one of
 *	the LARGE widget mapping windows, then we return
 *	the starting address within that window.
 *
 *	   If the request can be satisfied within one of
 *	the HUGE widget mapping windows, then we return
 *	the starting address within that window.
 *
 *	   Otherwise, the mapping is one that we are
 *	unable to map, and we should return NULL; this
 *	error case is not expected to occur, since it
 *	implies access requests to space more than 2G
 *	above the base of I/O Widget 0, or more than 64G
 *	above any other widget. Only PARANOID compiles
 *	include tests for this case.
 */
caddr_t
heart_addr_xio_to_kva(xwidgetnum_t widget,
		      iopaddr_t xtalk_addr,
		      size_t byte_count)
{
    iopaddr_t	paddr;

    paddr = heart_addr_xio_to_phys(widget, xtalk_addr, byte_count);

    return paddr ? (caddr_t)PHYS_TO_K1(paddr) : NULL;
}

/*
 *    heart_piomap_alloc: allocate resources to allow
 *	PIO mappings to be constructed to a range of
 *	addresses on the specified crosstalk widget.
 */

/*ARGSUSED */
heart_piomap_t
heart_piomap_alloc(vertex_hdl_t xconn,
		   device_desc_t dev_desc,
		   iopaddr_t xtalk_addr,
		   size_t byte_count,
		   size_t byte_count_max,
		   unsigned flags)
{
    xwidgetnum_t	    widget;
    caddr_t		    kvaddr;
    heart_piomap_t	    piomap;

#if PARANOID
    if (byte_count_max > byte_count)
	return NULL;
#endif

    widget = heart_widget_num(xconn);

    kvaddr = heart_addr_xio_to_kva(widget, xtalk_addr, byte_count);

    if (!kvaddr || !NEWf(piomap, flags))
	return NULL;

    piomap->xtalk.xp_dev = xconn;
    piomap->xtalk.xp_target = widget;
    piomap->xtalk.xp_xtalk_addr = xtalk_addr;
    piomap->xtalk.xp_mapsz = byte_count;
    piomap->xtalk.xp_kvaddr = kvaddr;
    piomap->flags = flags;

    return piomap;
}

/*
 *    heart_piomap_free: release resources reserved above.
 */

/*ARGSUSED */
void
heart_piomap_free(heart_piomap_t heart_piomap)
{
    DEL(heart_piomap);
}

/*
 *    heart_piomap_addr: Using mapping resources
 *	previously allocated, construct a PIO
 *	translation to the specified address range. The
 *	destination device and selected mapping window
 *	is carried in the piomap data.
 *
 *	PARANOID kernels check that the requested
 *	address range is enclosed in the range that was
 *	specified in the piomap call.
 */

/*ARGSUSED */
caddr_t
heart_piomap_addr(heart_piomap_t piomap,
		  iopaddr_t xtalk_addr,
		  size_t byte_count)
{
    caddr_t		    win_kvaddr;
    iopaddr_t		    win_xaddr;

#if PARANOID
    ulong_t		    win_bytes;
    iopaddr_t		    win_limit;
#endif

    win_kvaddr = piomap->xtalk.xp_kvaddr;
    win_xaddr = piomap->xtalk.xp_xtalk_addr;

#if PARANOID
    win_bytes = piomap->xtalk.xp_mapsz;
    win_limit = win_xaddr + win_bytes;
    if ((byte_count < 1) || (byte_count > win_bytes) ||
	(xtalk_addr < win_xaddr) ||
	(xtalk_addr + byte_count > win_limit))
	return 0;
#endif

    return win_kvaddr + xtalk_addr - win_xaddr;
}

/*
 *    heart_piomap_done: We are done with the
 *	translation set up in the specified resources;
 *	do any associated flushing or anything else we
 *	might need to do before using these resources to
 *	map a different range.
 */

/*ARGSUSED */
void
heart_piomap_done(heart_piomap_t heart_piomap)
{
}

/*
 *    heart_piotrans_addr: Attempt to provide an
 *	address that can be used to do PIO to the
 *	specified range, which does not require any
 *	resources to be allocated and which will not
 *	require any terminating actions.
 */

/*ARGSUSED */
caddr_t
heart_piotrans_addr(vertex_hdl_t xconn,
		    device_desc_t dev_desc,
		    iopaddr_t xtalk_addr,
		    size_t byte_count,
		    unsigned flags)
{
    return heart_addr_xio_to_kva(heart_widget_num(xconn),
				 xtalk_addr, byte_count);
}

/* =====================================================================
 *    DMA MANAGEMENT
 */

struct heart_dmamap_s {
    struct xtalk_dmamap_s   xtalk;
    unsigned		    flags;
};

/*ARGSUSED */
heart_dmamap_t
heart_dmamap_alloc(vertex_hdl_t xconn,
		   device_desc_t dev_desc,
		   size_t byte_count_max,
		   unsigned flags)
{
    heart_dmamap_t	    dmamap;

    if (NEWf(dmamap, flags)) {
	dmamap->xtalk.xd_dev = xconn;
	dmamap->xtalk.xd_target = heart_widget_num(xconn);;
	dmamap->flags = flags;
    }
    return dmamap;
}

/*ARGSUSED */
void
heart_dmamap_free(heart_dmamap_t dmamap)
{
    DEL(dmamap);
}

/*ARGSUSED */
iopaddr_t
heart_dmamap_addr(heart_dmamap_t dmamap,
		  paddr_t paddr,
		  size_t byte_count)
{
    iopaddr_t	xaddr;
    xaddr = heart_addr_phys_to_xio(paddr, byte_count);

    /*
     * In "DEBUG" kernels,
     * attempting to dmamap an address
     * that does not exist triggers
     * a panic.
     */

#if DEBUG
    if (!block_is_in_main_memory(xaddr, byte_count))
	cmn_err(CE_PANIC, "%v: attempt to set up dma outside memory\n"
		"\tattempted address: 0x%x..0x%x",
		dmamap->xtalk.xd_dev,
		xaddr, xaddr + byte_count - 1);
#endif
    return xaddr;
}

/*ARGSUSED */
alenlist_t
heart_dmamap_list(heart_dmamap_t heart_dmamap,
		  alenlist_t phys_alenlist,
		  unsigned flags)
{
    alenlist_t		    xtalk_alenlist = 0;
    alenaddr_t		    paddr;
    alenaddr_t		    xaddr;
    size_t		    bytes;

    unsigned                al_flags = (flags & XTALK_NOSLEEP) ? AL_NOSLEEP : 0;
    int                     inplace = flags & XTALK_INPLACE;

    alenlist_cursor_init(phys_alenlist, 0, NULL);

    if (inplace) {
	xtalk_alenlist = phys_alenlist;
    } else {
	xtalk_alenlist = alenlist_create(al_flags);
	if (!xtalk_alenlist)
	    goto fail;
    }

    while (ALENLIST_SUCCESS ==
	   alenlist_get(phys_alenlist, NULL, 0, &paddr, &bytes, 0)) {

	xaddr = heart_addr_phys_to_xio(paddr, bytes);

	/*
	 * In "DEBUG" kernels,
	 * attempting to dmamap an address
	 * that does not exist triggers
	 * a panic.
	 */

#if DEBUG
	if (!block_is_in_main_memory(xaddr, bytes))
	    cmn_err(CE_PANIC, "%v: attempt to set up dma outside memory\n"
		    "\tattempted address: 0x%x..0x%x",
		    heart_dmamap->xtalk.xd_dev,
		    xaddr, xaddr + bytes - 1);
#endif

	/* write the PCI DMA address
	 * out to the scatter-gather list.
	 */
	if (inplace) {
	    if (xaddr != paddr)
		if (ALENLIST_SUCCESS !=
		    alenlist_replace(xtalk_alenlist, NULL,
				     &xaddr, &bytes, al_flags))
		    goto fail;
	} else {
	    if (ALENLIST_SUCCESS !=
		alenlist_append(xtalk_alenlist,
				xaddr, bytes, al_flags))
		goto fail;
	}
    }

    if (!inplace)
	alenlist_done(phys_alenlist);
    return xtalk_alenlist;

fail:
    if (xtalk_alenlist && !inplace)
	alenlist_destroy(xtalk_alenlist);
    return 0;
}

/*ARGSUSED */
void
heart_dmamap_done(heart_dmamap_t heart_dmamap)
{
}

/*ARGSUSED */
iopaddr_t
heart_dmatrans_addr(vertex_hdl_t xconn,
		    device_desc_t dev_desc,
		    paddr_t paddr,
		    size_t byte_count,
		    unsigned flags)
{
    iopaddr_t	xaddr;
    xaddr = heart_addr_phys_to_xio(paddr, byte_count);

    /*
     * In "DEBUG" kernels,
     * attempting to dmamap an address
     * that does not exist triggers
     * a panic.
     */

#if DEBUG
    if (!block_is_in_main_memory(xaddr, byte_count))
	cmn_err(CE_PANIC, "%v: attempt to set up dma outside memory\n"
		"\tattempted address: 0x%x..0x%x",
		xconn,
		xaddr, xaddr + byte_count - 1);
#endif
    return xaddr;
}

/*ARGSUSED */
alenlist_t
heart_dmatrans_list(vertex_hdl_t xconn,
		    device_desc_t dev_desc,
		    alenlist_t phys_alenlist,
		    unsigned flags)
{

    alenlist_t		    xtalk_alenlist = 0;
    alenaddr_t		    paddr;
    alenaddr_t		    xaddr;
    size_t		    bytes;

    unsigned                al_flags = (flags & XTALK_NOSLEEP) ? AL_NOSLEEP : 0;
    int                     inplace = flags & XTALK_INPLACE;

    alenlist_cursor_init(phys_alenlist, 0, NULL);

    if (inplace) {
	xtalk_alenlist = phys_alenlist;
    } else {
	xtalk_alenlist = alenlist_create(al_flags);
	if (!xtalk_alenlist)
	    goto fail;
    }

    while (ALENLIST_SUCCESS ==
	   alenlist_get(phys_alenlist, NULL, 0, &paddr, &bytes, 0)) {

	xaddr = heart_addr_phys_to_xio(paddr, bytes);


	/*
	 * In "DEBUG" kernels,
	 * attempting to dmamap an address
	 * that does not exist triggers
	 * a panic.
	 */

#if DEBUG
	if (!block_is_in_main_memory(xaddr, bytes))
	    cmn_err(CE_PANIC, "%v: attempt to set up dma outside memory\n"
		    "\tattempted address: 0x%x..0x%x",
		    xconn,
		    xaddr, xaddr + bytes - 1);
#endif

	/* write the PCI DMA address
	 * out to the scatter-gather list.
	 */
	if (inplace) {
	    if (xaddr != paddr)
		if (ALENLIST_SUCCESS !=
		    alenlist_replace(xtalk_alenlist, NULL,
				     &xaddr, &bytes, al_flags))
		goto fail;
	} else {
	    if (ALENLIST_SUCCESS !=
		alenlist_append(xtalk_alenlist,
				xaddr, bytes, al_flags))
		goto fail;
	}
    }

    if (!inplace)
	alenlist_done(phys_alenlist);
    return xtalk_alenlist;

fail:
    if (xtalk_alenlist && !inplace)
	alenlist_destroy(xtalk_alenlist);
    return 0;
}

/*ARGSUSED*/
void
heart_dmamap_drain(	heart_dmamap_t map)
{
    /* XXX- flush caches, if cache coherency WAR is needed */
}

/*ARGSUSED*/
void
heart_dmaaddr_drain(	vertex_hdl_t vhdl,
			iopaddr_t addr,
			size_t bytes)
{
    /* XXX- flush caches, if cache coherency WAR is needed */
}

/*ARGSUSED*/
void
heart_dmalist_drain(	vertex_hdl_t vhdl,
			alenlist_t list)
{
    /* XXX- flush caches, if cache coherency WAR is needed */
}

/* =====================================================================
 *    INTERRUPT MANAGEMENT
 */

struct heart_intr_s {
    struct xtalk_intr_s	    xtalk;	/* standard crosstalk intr info */
    ilvl_t		    swlevel;	/* software level for blocking intr */
    cpuid_t		    cpuid;	/* target CPU */
    int			    flags;	/* defined in heartio.h */
};

/*
 *    heart_intr_preconn: called during very early
 *	startup to associate a particular interrupt
 *	handling function with a particular vector.
 */

/*ARGSUSED1 */
void
heart_intr_preconn(void *which_xtalk,
		   xtalk_intr_vector_t xtalk_vector,
		   intr_func_t intr_func,
		   intr_arg_t intr_arg)
{
    heartreg_t		    bit = 1ull << xtalk_vector;

    /* prevent dynamic reallocation of this vector. */
    heart_intr_bits_take(bit);

    heart_ivec[xtalk_vector].hv_func = intr_func;
    heart_ivec[xtalk_vector].hv_arg = intr_arg;

    /* now that the handler is set up,
     * we can enable interrupts on this vector.
     */
    heart_imr_bits_rmw(getcpuid(), 0, bit);
}

/*
 * Machine specific setup code needs to be
 * able to hook up specific vectors to
 * widget interrupt handlers.
 */
/*ARGSUSED1 */
void
heart_intr_prealloc(void *which_xtalk,
		    xtalk_intr_vector_t xtalk_vector,
		    xwidget_intr_preset_f *preset_func,
		    void *which_widget,
		    int which_widget_intr)
{
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;
    heartreg_t		    bit = 1ull << xtalk_vector;

    /* prevent dynamic reallocation of this vector. */
    heart_intr_bits_take(bit);

    (*preset_func)
	(which_widget,
	 which_widget_intr,
	 HEART_STAT_WIDGET_ID & heart_piu->h_status,
	 HEART_XTALK_ISR,
	 xtalk_vector);
}

/*
 *    heart_intr_init: this vertex is a new heart
 *	provider. set up the software state so we can
 *	manage interrupts through it.
 */

/*ARGSUSED */
heart_intr_t
heart_intr_alloc(vertex_hdl_t xconn,
		 device_desc_t dev_desc,
		 vertex_hdl_t owner_dev)
{
    heart_intr_t	    intr;
    extern int default_intr_pri;
    extern int graphics_intr_pri;

    if (NEW(intr)) {
	int			ibit;
	int			ilvl;
	unsigned		dev_desc_flags;

	if (!dev_desc)
	    dev_desc = device_desc_default_get(xconn);
	if (dev_desc) {
	    dev_desc_flags = device_desc_flags_get(dev_desc);
	    ilvl = device_desc_intr_swlevel_get(dev_desc);
	    if (ilvl < 0)
		ilvl = default_intr_pri;
	} else {
	    dev_desc_flags = 0;
	    ilvl = default_intr_pri;
	}

	if (dev_desc_flags & D_INTR_ISERR) {
	    /* use the proper bit for this
	     * widget's error interrupt:
	     * #0=>58, #15->57 ... #9->51
	     */
	    xwidget_info_t	    widget_info;
	    xwidgetnum_t	    xid;

	    widget_info = xwidget_info_get(xconn);
	    xid = xwidget_info_id_get(widget_info);
	    ibit = 51 + (7 & (7 + xid));
	    heart_intr_bits_take(1ull << ibit);
	} else {
	    ibit = heart_intr_level_alloc(heart_intr_swlevel_to_hwlevel(ilvl));

	    if (ilvl == graphics_intr_pri)
		atomicSetInt(&heart_ivec[ibit].hv_flags,
			     HEART_RETRACE);

	    if (dev_desc_flags & D_INTR_NOTHREAD)
		atomicClearInt(&heart_ivec[ibit].hv_flags,
			       THD_ISTHREAD);
	    else
		atomicSetInt(&heart_ivec[ibit].hv_flags,
			     THD_ISTHREAD);
	}
	intr->xtalk.xi_dev = xconn;
	intr->xtalk.xi_target = HEART_ID;
	intr->xtalk.xi_vector = ibit;
	intr->xtalk.xi_addr = HEART_XTALK_ISR;
	intr->xtalk.xi_sfarg = 0;
	intr->xtalk.xi_setfunc = 0;
	intr->flags = 0;
	intr->swlevel = ilvl;
	intr->cpuid = intr_spray_heuristic(dev_desc);

#if DEBUG && INTR_DEBUG
	printf("%v allocated heart vector %d on CPU %d\n", xconn, ibit,
	       intr->cpuid);
#endif
    }
    return intr;
}

/*ARGSUSED */
void
heart_intr_free(heart_intr_t intr)
{
    heart_intr_bits_free(1ull <<intr->xtalk.xi_vector);
    DEL(intr);
}

/*ARGSUSED */
int
heart_intr_connect(heart_intr_t intr,
		   intr_func_t func,
		   intr_arg_t arg,
		   xtalk_intr_setfunc_t setfunc,
		   void *setfunc_arg,
		   void *thread)
{
    int			    vec = intr->xtalk.xi_vector;
    heartreg_t		    bit = 1ull << vec;
    cpuid_t		    cpu = intr->cpuid;

    heart_ivec[vec].hv_func = func;
    heart_ivec[vec].hv_arg = arg;
    intr->xtalk.xi_sfarg = setfunc_arg;
    intr->xtalk.xi_setfunc = setfunc;

    if (heart_ivec[vec].hv_flags & THD_ISTHREAD) {
	heart_ivec[vec].hv_dest = cpu;
	atomicSetInt(&heart_ivec[vec].hv_flags, THD_REG);
	if (ithreads_enabled)
	    heart_ithread_setup(cpu, vec, intr->swlevel);
    }
    if (setfunc)
	(*setfunc) ((xtalk_intr_t) intr);

    heart_imr_bits_rmw(cpu, 0, bit);
#if DEBUG && INTR_DEBUG
    printf("%v heart vector %d is 0x%X(0x%X)\n",
	   intr->xtalk.xi_dev,
	   vec, func, arg);
#endif
    return 0;
}

/*ARGSUSED */
void
heart_intr_disconnect(heart_intr_t intr)
{
    int			    vec = intr->xtalk.xi_vector;
    cpuid_t		    cpu = intr->cpuid;
    heartreg_t		    bit = 1ull << vec;
    xtalk_intr_setfunc_t    setfunc = intr->xtalk.xi_setfunc;

    heart_imr_bits_rmw(cpu, bit, 0);
    heart_ivec[vec].hv_func = heart_stray_intr;
    heart_ivec[vec].hv_arg = INTR_ARG(vec);
    if (setfunc)
	(*setfunc) ((xtalk_intr_t) intr);
}

/*ARGSUSED */
vertex_hdl_t
heart_intr_cpu_get(heart_intr_t intr_hdl)
{
    cpuid_t		    cpuid = intr_hdl->cpuid;

    ASSERT(cpuid != CPU_NONE);
    return cpuid_to_vertex(cpuid);
}

/* =====================================================================
 *    CONFIGURATION MANAGEMENT
 */

/*
 *    heart_provider_startup: the provided vertex has
 *	just been declared to be the provider vertex for
 *	a heart; set up the software state.
 */

/*ARGSUSED */
void
heart_provider_startup(vertex_hdl_t heartio)
{
}

/*ARGSUSED */
void
heart_provider_shutdown(vertex_hdl_t heart)
{
    xtalk_provider_unregister(heart);
}

xtalk_provider_t	heart_provider =
{
    (xtalk_piomap_alloc_f *) heart_piomap_alloc,
    (xtalk_piomap_free_f *) heart_piomap_free,
    (xtalk_piomap_addr_f *) heart_piomap_addr,
    (xtalk_piomap_done_f *) heart_piomap_done,
    (xtalk_piotrans_addr_f *) heart_piotrans_addr,

    (xtalk_dmamap_alloc_f *) heart_dmamap_alloc,
    (xtalk_dmamap_free_f *) heart_dmamap_free,
    (xtalk_dmamap_addr_f *) heart_dmamap_addr,
    (xtalk_dmamap_list_f *) heart_dmamap_list,
    (xtalk_dmamap_done_f *) heart_dmamap_done,
    (xtalk_dmatrans_addr_f *) heart_dmatrans_addr,
    (xtalk_dmatrans_list_f *) heart_dmatrans_list,
    (xtalk_dmamap_drain_f *) heart_dmamap_drain,
    (xtalk_dmaaddr_drain_f *) heart_dmaaddr_drain,
    (xtalk_dmalist_drain_f *) heart_dmalist_drain,

    (xtalk_intr_alloc_f *) heart_intr_alloc,
    (xtalk_intr_free_f *) heart_intr_free,
    (xtalk_intr_connect_f *) heart_intr_connect,
    (xtalk_intr_disconnect_f *) heart_intr_disconnect,
    (xtalk_intr_cpu_get_f *) heart_intr_cpu_get,

    (xtalk_provider_startup_f *) heart_provider_startup,
    (xtalk_provider_shutdown_f *) heart_provider_shutdown,
};

#undef	XWIDGET_NONE
#define XWIDGET_NONE	0xFF
#define XWIDGET_MEMORY	0xFE

xwidgetnum_t
heart_paddr_to_port(iopaddr_t paddr,
		    iopaddr_t *maddr,
		    iopaddr_t *xaddr)
{
    if (paddr < SYSTEM_MEMORY_ALIAS_SIZE ||
        (paddr >= ABS_SYSTEM_MEMORY_BASE + SYSTEM_MEMORY_ALIAS_SIZE &&
	 paddr < ABS_SYSTEM_MEMORY_TOP)) {
	if (maddr)
	    *maddr = paddr;
	return XWIDGET_MEMORY;
    }
    if (IS_MAIN_IO_SPACE(paddr)) {
	if (xaddr)
	    *xaddr = paddr % MAIN_IO_SIZE;
	return (paddr - MAIN_IO_SPACE) / MAIN_IO_SIZE;
    }
    if (IS_LARGE_IO_SPACE(paddr)) {
	if (xaddr)
	    *xaddr = paddr % LARGE_IO_SIZE;
	return (paddr - LARGE_IO_SPACE) / LARGE_IO_SIZE;
    }
    if (IS_HUGE_IO_SPACE(paddr)) {
	if (xaddr)
	    *xaddr = paddr % HUGE_IO_SIZE;
	return paddr / HUGE_IO_SIZE;
    }
    return XWIDGET_NONE;
}

/*ARGSUSED */
int
heart_error_refine(char *message,
		   int error_code,
		   ioerror_mode_t error_mode,
		   ioerror_t *ioe)
{
    iopaddr_t		    paddr;
    iopaddr_t		    xaddr;
    xwidgetnum_t	    xport;
    iopaddr_t		    maddr;

    if (IOERROR_FIELDVALID(ioe, sysioaddr) &&
	(!IOERROR_FIELDVALID(ioe, widgetnum) ||
	 !IOERROR_FIELDVALID(ioe, xtalkaddr))) {
	paddr = IOERROR_GETVALUE(ioe, sysioaddr);
	xport = heart_paddr_to_port(paddr, &maddr, &xaddr);
	if (xport <= 15) {
	    IOERROR_SETVALUE(ioe, widgetnum, xport);
	    IOERROR_SETVALUE(ioe, xtalkaddr, xaddr);
	} else if (xport == XWIDGET_MEMORY)
	    IOERROR_SETVALUE(ioe, memaddr, maddr);
    }
    if (IOERROR_FIELDVALID(ioe, widgetnum) &&
	((xport = IOERROR_GETVALUE(ioe, widgetnum)) <= 15))
	return xtalk_error_handler(the_error_vhdl, error_code, error_mode, ioe);
    return IOERROR_UNHANDLED;
}

/* heart_error_handler:
 * called from xtalk when an error occurred accessing
 * the crosstalk port heart is using.
 *
 * We *REALLY* never expect to get here.
 */

/*ARGSUSED*/
static int
heart_error_handler(
		       error_handler_arg_t einfo,
		       int error_code,
		       ioerror_mode_t mode,
		       ioerror_t *ioerror)
{
    if (mode == MODE_DEVPROBE)
	return IOERROR_HANDLED;

    ioerror_dump("Heart", error_code, mode, ioerror);

    return IOERROR_PANIC;
}

/* =====================================================================
 *    INTERRUPT DELIVERY
 */

/*
 *    heart_intr(): generic heart interrupt
 *	processing; knows where to go to get heart
 *	registers, triggers calls to service routines
 *	that have been presented with heart_connect
 *	whose interrupts are active. Only interrupts
 *	designated in the mask are serviced.
 *
 * NOTE: Code to set interrupt mask has moved to IP30asm.s.
 */
void
heart_intr(struct eframe_s *ep, heartreg_t mask)
{
    heart_piu_t		   *heart_piu = HEART_PIU_K1PTR;
    heartreg_t		    imsr;
    int			    svcd = 0;
    extern int		    ffintr64(__uint64_t);

    /*
     * Continue servicing until no more pending ints.
     */
    while (imsr = mask & heart_piu->h_imsr) {
	int vec = ffintr64(imsr);

	if (vec > 58) {
	    /*
	     * CPU Bus Errors and Heart Exceptions:
	     * always handled by buserror_intr, and
	     * it needs the eframe pointer.
	     */
	    buserror_intr(ep);
	} else {
	    heart_ivec_t	   *vec_p = &heart_ivec[vec];

	    /*
	     * Special case graphics hiwater and lowater interrupts.
	     * 
	     * Bit 0 is read-only.
	     * Bits 1 and 2 are flow control timeouts, which should
	     * not be cleared immediately as the credit level may
	     * have not increased above the bias level.
	     *
	     * Before modifying this code, including removing the
	     * below code, the impact on serial, keyboard, and
	     * mouse drivers and real-time performance should be
	     * considered.
	     *
	     * This code was removed to improve real-time performance
	     * (bug #623708) and clearing of the bit in the heart
	     * ISR added to the thread and no-thread cases below.
	     *
	     * Everything was wonderful until midi loopback tests,
	     * were performed. (The loopback tests simulate an
	     * external midi devices sending and receiving data.)
	     * When the loopback tests were run, the keyboard and
	     * mouse would become unresponsive after brief usage.
	     *
	     * Returning graphics special casing of the form found
	     * in heart.c until rev 1.98 removes the keyboard and
	     * mouse unresponsiveness problem while maintaining
	     * real-time performance.
	     */
	    if (vec > 2) {
	        heart_piu->h_clr_isr = 0x1ul << vec;
	    }

	    if (vec_p->hv_flags & THD_OK) {
#ifdef ITHREAD_LATENCY
		xthread_set_istamp(vec_p->hv_lat);
#endif /* ITHREAD_LATENCY */
		/* This can happen in the case of interrupt
		 * handlers which loop to check if there
		 * is an interrupt pending while servicing
		 * an interrupt. This can potentially
		 * cause vsema's with no corresponding
		 * ipsema's.
		 */
		if (valusema(&vec_p->hv_isync) > 0x7000) {
		  cvsema(&vec_p->hv_isync);
		} else
		  /* Cleared intr above, so no need to mask it. */
		  vsema(&vec_p->hv_isync);
	    } else {
		(vec_p->hv_func)(vec_p->hv_arg);
	    }
	    
	    /*
	     * Special case graphics hiwater and lowater interrupts.
	     *
	     * Clear the interrupt here since the MGRAS hi- and lo-water
	     * handlers (mgras_hq4_hiwater_intr() and
	     * mgras_hq4_lowater_intr()) do not clear the heart ISR.
	     */
	    if (vec <= 2) {
	        heart_piu->h_clr_isr = 0x1ul << vec;
	    }
	}
	svcd++;
    }

    atomicAddInt((int *)&SYSINFO.intr_svcd, svcd-1);	/* intr() adds 1 also */
}

/*
 * Re-enable all needed stuff as part of handling error.
 */
/*ARGSUSED3 */
int
heart_error_devenable(vertex_hdl_t xconn_vhdl, int devnum, int error_code)
{
    return 0;
}

void
heart_intrd(heart_ivec_t * ivp)
{
    ASSERT(ivp->hv_dest == cpuid());
#ifdef ITHREAD_LATENCY
    xthread_update_latstats(ivp->hv_lat);
#endif
    ivp->hv_func(ivp->hv_arg);
    /* No nead to re-enable interrupt, since we never disabled it.
     */
    ipsema(&ivp->hv_isync);
    /* NOTREACHED */
}

void
heart_intrd_start(heart_ivec_t * ivp)
{
    ASSERT(ivp->hv_dest >= 0);
    setmustrun(ivp->hv_dest);

    xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *) heart_intrd, ivp);

    atomicSetInt(&ivp->hv_flags, THD_INIT);
    ipsema(&ivp->hv_isync);
    /* NOTREACHED */
}

/*ARGSUSED */
void
heart_ithread_setup(cpuid_t cpu, int vec, ilvl_t pri)
{
    char		    tname[32];
    extern int              graphics_intr_pri;

    if (heart_ivec[vec].hv_flags & HEART_RETRACE)
	pri = graphics_intr_pri;
    sprintf(tname, "heart_ivec(%d)", vec);
    xthread_setup(tname, pri, &heart_ivec[vec].hv_tinfo,
		  (xt_func_t *) heart_intrd_start,
		  (void *) &heart_ivec[vec]);
}

/*
 * This routine is called once during startup. Some (most!) interrupts
 * get registered before threads can be setup on their behalf.	We don't
 * worry about locking, since this is done while still single-threaded,
 * and if an interrupt comes in while we're traversing the table, it will
 * either run the old way, or, if it sees the THD_OK bit, then we've
 * already set up the thread, so it can just signal the thread's sema.
 */
void
enable_ithreads(void)
{
    int vec;
    extern int default_intr_pri;

    for (vec = 0; vec < HEART_INT_VECTORS; vec++) {
	heart_ivec_t	       *ivp = &heart_ivec[vec];

	if (ivp->hv_flags & THD_REG) {
	    ASSERT(ivp->hv_flags & THD_ISTHREAD);
	    ASSERT(!(ivp->hv_flags & THD_OK));
	    heart_ithread_setup(ivp->hv_dest, vec, default_intr_pri);
	}
    }

    acfail_ivec.hv_dest = cpuid();	/* run on bootmaster */
    atomicSetInt(&acfail_ivec.hv_flags, THD_ISTHREAD | THD_REG);
    xthread_setup("acfail", 255, &acfail_ivec.hv_tinfo,
		  (xt_func_t *) heart_intrd_start,
		  &acfail_ivec);
    acfail_tinfo = &acfail_ivec.hv_tinfo;
    ithreads_enabled = 1;
}

int
heart_rev(void)
{
    if (the_heart_rev < 0)
	the_heart_rev = XWIDGET_REV_NUM(HEART_CFG_K1PTR->h_widget.w_id);
    return the_heart_rev;
}

/* Heart gfx flow control hooks. */
void
heart_flow_control_connect(int fc_num,
		    intr_func_t gfx_flow_delayed_hiwater_intr_prehandler)
{
    heartreg_t		    mask = 1 << (fc_num + 1);
    long		    arg = (long)fc_num;

    heart_intr_preconn(NULL, fc_num + 1,
	   (intr_func_t) gfx_flow_delayed_hiwater_intr_prehandler, (intr_arg_t)arg);

    /* clear enable bit on all processors */
    heart_imr_bits_rmw(0, mask, 0);
    heart_imr_bits_rmw(1, mask, 0);
    heart_imr_bits_rmw(2, mask, 0);
    heart_imr_bits_rmw(3, mask, 0);
}

void
heart_flow_control_disconnect(int fc_num)
{
    heartreg_t		    mask = 1 << (fc_num + 1);

    /* XXX replace flow-control intr handler with spurious intr handler */

    /* clear enable bit on all processors */
    heart_imr_bits_rmw(0, mask, 0);
    heart_imr_bits_rmw(1, mask, 0);
    heart_imr_bits_rmw(2, mask, 0);
    heart_imr_bits_rmw(3, mask, 0);
}

heartreg_t
heart_flow_control_enable(int fc_num)
{
    heartreg_t		    mask = 1 << (fc_num + 1);
    int			    cpuid = cpuid();

    ASSERT((HEART_PIU_K1PTR->h_imr[cpuid ^ 1] & mask) == 0);
    ASSERT((HEART_PIU_K1PTR->h_imr[cpuid ^ 2] & mask) == 0);
    ASSERT((HEART_PIU_K1PTR->h_imr[cpuid ^ 3] & mask) == 0);

    /* set interrupt mask to enable interrupt again */
    return mask & heart_imr_bits_rmw(cpuid, mask, mask);
}

heartreg_t
heart_flow_control_disable(int fc_num)
{
    heartreg_t		    mask = 1 << (fc_num + 1);
    int			    cpuid = cpuid();

    /*
     * We need to block the flow control timeout interrupt until we
     * get a low water status interrupt from HQ4.  Otherwise we will
     * get a lot of timeout interrupt in case the pipe takes a while
     * to drain.  And clearing the isr bit in heart will cause the
     * timer to reload.
     */
    return mask & heart_imr_bits_rmw(cpuid, mask, 0);
}
