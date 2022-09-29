/* Copyright 1986-1999, Silicon Graphics Inc., Mountain View, CA. */
#ident	"$Revision: 3.82 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/sysinfo.h"
#include "sys/reg.h"
#include "sys/cmn_err.h"
#include "sys/edt.h"
#include "sys/vmereg.h"
#include "sys/vmevar.h"
#include "sys/edt.h"
#include "sys/debug.h"
#include "sys/sysmacros.h"
#include "sys/dmamap.h"
#include "sys/invent.h"
#ifdef EVEREST
#include "sys/EVEREST/io4.h"
#include "sys/EVEREST/vmecc.h"
#include "ksys/xthread.h"
#endif
#ifdef SN
#include "sys/PCI/univ_vmestr.h"
#endif        /* SN0 */
#ifdef ULI
#include "sys/errno.h"
#endif


#define VME_MINIPL 1		/* lowest interrupt level */
#define VME_MAXIPL 7		/* highest interrupt level */

#if IP22 || IP26 || IP28 || IP30 || IP32 || IPMHSIM
#define NO_VMEBUS	1
#endif
#if IP20 || EVEREST || SN
#define USE_VMEBUS	1
#endif

#ifdef USE_VMEBUS

#if defined(EVEREST) && defined(ULI)
extern int ithreads_enabled;
#endif

extern int *vmevec_alloc;			/* for allocating vectors */
extern struct vme_ivec *vme_ivec;	 /* interrupt vector table */


/*
 * Machine independent vme interrupt handler.
 */
void
vme_handler(
	eframe_t *ep,
	register unchar vec,
	register int lvl,
#if defined(EVEREST) && defined(ULI)
	register __psunsigned_t swin,
#endif /* defined(EVEREST) && defined(ULI) */
	register unchar adapter)
{
	struct vme_ivec *intr;

	adapter = VMEADAP_XTOI(adapter);

	if (vec < MAX_VME_VECTS) {
		intr = &vme_ivec[VME_VEC(adapter, vec)];
		SYSINFO.vmeintr_svcd++;
#if defined(EVEREST) && defined(ULI)
		if (intr->vm_tinfo->thd_flags & THD_OK) {
                  /* set up information so the vme_intrd() thread
		   * can flush the cache if needed and ack the interrupt.
		   */
		  intr->hw_info.inum = lvl;
		  intr->hw_info.adapter = adapter;
		  vsema(&intr->vm_tinfo->thd_isync);
                } else {
		  /* flush the io4 cache if needed
		   * Code moved from vmecc_intr() for this bug fix. */
		  if (io4ia_war && intr->vm_ivec_ioflush)
		    io4_flush_cache(swin);
#endif  /* defined(EVEREST) && defined(ULI) */
		intr->vm_intr(intr->vm_arg);
#if defined(EVEREST) && defined(ULI)
                  /* ack the interrupt here since we don't do this in
		   * vmecc_intr() with this fix.
		   */
                  EV_SET_REG(swin + VMECC_INT_ENABLESET, 1L << lvl);
                }
#endif /* defined(EVEREST) && defined(ULI) */
	} else {
		cmn_err(CE_WARN,
	    "stray VME interrupt on level %d at PC=0x%llx, VME adapter = %d vector=0x%x\n",
			lvl, ep->ef_epc, adapter, vec);
	}
}

#if defined(EVEREST) && defined(ULI)
/*
 * VME interrupt threads
 *
 * This is a bug fix for bug #690661. The VME interrupt thread is only
 * created on EVEREST architectures as a minimal impact solution for bug
 * #690661.
 *
 * These threads are modeled after evdev_intrd() in
 * irix/kern/ml/EVEREST/evintr.c.
 */

/*
 * static void
 * vme_intrd(struct vme_ivec *intr)
 *
 * vme_intrd is a the xthread/ithread daemon for VME interrupts.
 */
static void
vme_intrd(struct vme_ivec *intr)
{
  __psunsigned_t swin = vmecc[intr->hw_info.adapter].ioadap.swin;

  /*
   * XXX
   * ASSERT(cpuid() == ??);
   *
   * evdev_intrd() can be made to run on a particular cpu. At the moment,
   * vme_intrd() does not allow this functionality, but it should if it
   * wants to remain as close the evintr_intrd() functionality.
   */

  /*
   * flush the cache
   * This code was brought over from vmecc_intr().
   */
  if (io4ia_war && 
      intr->vm_ivec_ioflush){
    io4_flush_cache(swin);
  }

  intr->vm_intr(intr->vm_arg);

  /*
   * ack the interrupt
   * This code was brought over from vmecc_intr().
   */
  EV_SET_REG(swin + VMECC_INT_ENABLESET, 1L << intr->hw_info.inum);

  ipsema(&intr->vm_tinfo->thd_isync);
  /* NOTREACHED */
}

/*
 * static void
 * vme_intrd_start(struct vme_ivec *intr)
 *
 * vme_intrd_start() is called as the initial function of the xthread/ithread
 * daemon for VME interrupts. vme_intrd_start() sets the final function,
 * marks the thread as fully initialized, and then goes to sleep on a
 * semaphore.
 */
static void
vme_intrd_start(struct vme_ivec *intr)
{
  xthread_set_func(KT_TO_XT(curthreadp), (xt_func_t *)vme_intrd, intr);
  atomicSetInt(&intr->vm_tinfo->thd_flags, THD_INIT);
  ipsema(&intr->vm_tinfo->thd_isync);
  /* NOTREACHED */
}

/*
 * static void
 * vme_intr_thrd_setup(int level)
 *
 * vme_intr_thrd_setup() creates the xthread/ithread to service a VME
 * interrupt for a particular VME interrupt level.
 */
static void
vme_intr_thrd_setup(int level)
{
	char thread_name[32];

	vme_ivec[level].vm_tinfo->thd_pri = 230; /* XXX - this shouldn't be
						    hardcoded */

	sprintf(thread_name, "vme_intrd%d", level);

	xthread_setup(thread_name,
		      230,  /* XXX - this shouldn't be hardcoded */
		      vme_ivec[level].vm_tinfo,
		      (xt_func_t *)vme_intrd_start, &vme_ivec[level]);
	ASSERT(vme_ivec[level].vm_tinfo->thd_isync.s_un.s_st.count == 0);
}

/*
 * void
 * vme_enable_ithreads(void)
 *
 * vme_enable_ithreads() starts the VME interrupt ithreads/xthreads once
 * the kernel is ready to run ithreads/xthreads.
 */
void
vme_enable_ithreads(void)
{
  int level;

  for (level = 0; level < MAX_VME_VECTS; level++) {
    if ((vme_ivec[level].vm_tinfo->thd_flags & THD_REG) &&
	!(vme_ivec[level].vm_tinfo->thd_flags & THD_INIT)) {
      ASSERT ((vme_ivec[level].vm_tinfo->thd_flags & THD_ISTHREAD));
      ASSERT (!(vme_ivec[level].vm_tinfo->thd_flags & THD_OK));
      initnsema(&(vme_ivec[level].vm_tinfo->thd_isync), 0, "vme_ivec_sema");
      vme_intr_thrd_setup(level);
    }
  }
}
#endif /* defined(EVEREST) && defined(ULI) */

#define NOMAPPINGFOR0	1

void
vme_init()
{
	register int i;
	register uint_t vmeadapters = readadapters(ADAP_VME);

	if( vmeadapters == 0 )
		return;

	/* add the busses to the inventory */
	for( i = 0 ; i < vmeadapters ; i++ )
		add_to_inventory(INV_BUS, INV_BUS_VME, NOMAPPINGFOR0, 0,
			VMEADAP_ITOX(i));

#ifdef EVEREST
	vmecc_lateinit();
#endif /* EVEREST */

#ifdef	NOT_NEEDED /* SN0 */
	unvme_lateinit();
#endif	/* SN0 */

	/* on MP, assign interrupt levels to different CPU 
	** on simplex, CPU# will always be zero, this is used to
	** only turn on the mask for the VME IPL that is occupied
	** by a real device .
	** VME IPL that has no device configured for it will not be turned 
	** on
	*/
	for (i = 0; i < nedt; i++) {
		if (edt[i].e_bus_type != ADAP_VME) 
			continue;

		/* ignore errors.  bad adapter will be caught by
		 * the driver.  non-loadable vector collisions
		 * are caught by lboot.
		 */
		(void)vme_init_adap(&edt[i]);
	}
}

static int 
vme_adap_setup(struct edt *edtp)
{
	int adap;
	vme_intrs_t *intrs = edtp->e_bus_info;
	int cpu;

	
	/* we don't have to do anything for VME devices which
	 * don't have interrupts.
	 */
	if (intrs == NULL )
		return 1;

	/* v_unit should no longer be specified in the VECTOR line.  
	 * Since it has survived in the vme_intrs_t structure,
	 * set it to a reasonable value.
	 */
	intrs->v_unit = edtp->e_ctlr;

	adap = VMEADAP_XTOI(edtp->e_adap);

	if ( (adap < 0) || (adap >= readadapters(ADAP_VME))) {
		cmn_err(CE_CONT, "VME: bus %d not present\n", edtp->e_adap);
		return 0;
	}

	cpu = iointr_at_cpu(intrs->v_brl,edtp->v_cpuintr,edtp->e_adap); 

	if( cpu == -1 ) {
		cmn_err(CE_CONT, "VME: unable to turn on IPL %d\n",
			intrs->v_brl);
		return 0;
	}

	if( cpu != edtp->v_cpuintr ) {
#if DEBUG
		/* leave this as DEBUG otherwise single processor systems
		 * will pour out messages with regards to networking drivers
		 * and the IPL 4 directive.
		 */
		cmn_err(CE_CONT, "VME: ipl %d mapped to cpu %d, NOT %d\n", 
			intrs->v_brl,cpu,edtp->v_cpuintr);
#endif /* DEBUG */
		edtp->v_cpuintr = cpu;
	}

	/* if no vector, this one's done. */
	if (intrs->v_vec == 0)
		return 1;
	
	/* An irq was specified, take it over if possible */
	if( vme_ivec_set(edtp->e_adap, intrs->v_vec, intrs->v_vintr, 
			intrs->v_unit) < 0 )
		return 0;

	return 1;
}

/*
 * vme_init_adap - initialize adapters and set iointr level
 *
 * NOTE - this code is also called for loadable VME drivers.
 * also, return value is reverse of normal, 0 means failure.
 */
int 
vme_init_adap (struct edt *edtp)
{
	int i, adaps;

	if( edtp->e_adap != -1 )
		return vme_adap_setup(edtp);

	adaps = readadapters(ADAP_VME);
	for( i = 0 ; i < adaps ; i++ ) {

		edtp->e_adap = VMEADAP_ITOX(i);

		if( !vme_adap_setup(edtp) ) {
			edtp->e_adap = -1;
			return 0;
		}
	}

	edtp->e_adap = -1;
	return 1;
}

/*
 * static int
 * vme_strayint(int)
 * 
 * Stray VME interrupt catcher
 *
 * No return value. Reports the occurence of an unexpected,
 * unallocated VME interrupt.
 */

static int
vme_strayint(int arg)
{
	/* Try and avoid very early stray interrupt. 
	 * Ideally, it should check if edt_init is done, and then allow
	 * stray interrupt message to be printed. Instead, just check the
	 * value of lbolt and print message only if system has been running 
	 * for a couple of seconds 
	 */
	if(lbolt)
		cmn_err(CE_WARN, "stray VME interrupt, vector=0x%x\n", arg);
	return 0;
}

/*
 * static int
 * vme_nullint(int)
 * 
 * Stray VME interrupt catcher for allocated vectors
 *
 * No return value. Reports the occurence of an VME interrupt
 * which had been allocated, but not initialized with it's
 * owner's handler routine.
 */

static int
vme_nullint(int arg)
{
	cmn_err(CE_WARN,
	    "No handler for intr, vector=0x%x\n", arg);
	return 0;
}

/*
 * int
 * vme_ivec_set_ext(uint_t adapter,
 *                  int vec,
 *                  int (*intr)(int),
 *                  int arg,
 *                  int exec_mode)
 *
 * Register this vector into vme_ive[] and, unlike vme_ivec_set() allow
 * the caller to set the execution mode of the adpater's interrupt service
 * routine. The exec_mode argument is used as a boolean variable to
 * determine execution mode.
 *
 * Returns -1 if adapter or vec is invalid or vector is inuse.
 *
 */
int
#if defined(EVEREST) && defined(ULI)
/*
 * Fix for bug #690661.
 */
vme_ivec_set_ext(int adapter,
		 int vec,
		 int (*intr)(int),
		 int arg,
		 int exec_mode)
#else
vme_ivec_set_ext(int adapter,
		 int vec,
		 int (*intr)(int),
		 int arg)
#endif
{
	register uint_t vmeadapters = readadapters(ADAP_VME);


	adapter = VMEADAP_XTOI(adapter);

	if ((adapter < 0) || (adapter >= vmeadapters) || 
	    (vec >= MAX_VME_VECTS) || (intr == 0) ||
	    ((vme_ivec[VME_VEC(adapter, vec)].vm_intr != vme_nullint) && 
	     (vme_ivec[VME_VEC(adapter, vec)].vm_intr != vme_strayint) &&
	     (intr != vme_strayint)))
		return(-1);
	vme_ivec[VME_VEC(adapter, vec)].vm_intr = intr;
	vme_ivec[VME_VEC(adapter, vec)].vm_arg = arg;
#ifdef	EVEREST
	/*
	 * IO4 IA WAR in progress.
	 *
	 * If attaching a stray interrupt, disable flushing io4 caches
	 * otherwise, enable it. Drivers will use vme_ivec_ioflush_disable
	 * to disable it.
	 */
	if (intr == vme_strayint){
		vme_ivec[VME_VEC(adapter, vec)].vm_ivec_ioflush = 0;
	} else {
		vme_ivec[VME_VEC(adapter, vec)].vm_ivec_ioflush = 1;
	}

#if defined(ULI)
	/*
	 * Execution mode
	 *
	 * This is a fix for a problem first noted in bug #690661.
	 */
	switch (exec_mode) {
	  case EXEC_MODE_ISTK:
	    /*
	     * Destroy the xthread associated with this vector element since
	     * uli's do not use them
	     */
	    vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_flags = 0;
	    if (vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_xthread) {
	      xthread_set_func(vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_xthread,
			       (xt_func_t *)xthread_exit, 0);
	      vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_xthread = NULL;
	      vsema(&(vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_isync));
	    }
	    break;
	  case EXEC_MODE_THRD:
	    initnsema(&(vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_isync),
		      0, "vme_ivec_sema");
	    atomicSetInt(&vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_flags,
			 THD_ISTHREAD | THD_REG);
	    if (ithreads_enabled)
	      vme_intr_thrd_setup(VME_VEC(adapter, vec));
	    break;
	  default:
	    cmn_err(CE_WARN,
		    "Unknown VME device interrupt handler execution mode\n");
	    break;
	}
#endif  /* defined(ULI) */
#endif	/* EVEREST */

	return(0);
}

/*
 * int
 * vme_ivec_set(uint_t adapter, int vec, int (*intr)(int), int arg)
 *
 * Register this vector into vme_ive[].
 *
 * Returns -1 if adapter or vec is invalid or vector is inuse.
 *
 */
int
vme_ivec_set(int adapter, int vec, int (*intr)(int), int arg)
{
#if defined(EVEREST) && defined(ULI)
  /* Prior to this fix for bug #690661, VME interrupts were, by default,
   * handled threaded by evdev_intrd(). So, any interrupt handler that
   * calls vme_ivec_set() will be threaded to preserve the previous
   * behavior. This is especially important for networking adapters'
   * interrupt handlers.
   */
  return vme_ivec_set_ext(adapter, vec, intr, arg, EXEC_MODE_THRD);
#else
  return vme_ivec_set(adapter, vec, intr, arg);
#endif
}

#ifdef ULI
#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

/* dprintf: general info */

#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

int (*vmeuliintrp)(int);

/* redefine the arg for a uli vme interrupt */
int
vmeuli_ivec_reset(int adapter, int vec, int arg)
{
    register uint_t vmeadapters = readadapters(ADAP_VME);

    dprintf(("ivec reset adap %d vec %d intr 0x%x arg %d\n",
	     adapter, vec, intr, arg));
    adapter = VMEADAP_XTOI(adapter);
    
    if ((adapter < 0) || (adapter >= vmeadapters) || 
	(vec >= MAX_VME_VECTS) || (intr == 0)) {
	eprintf(("vmeuli_ivec_reset bad something\n"));
	return(EINVAL);
    }
    if (vme_ivec[VME_VEC(adapter, vec)].vm_intr != vmeuliintrp) {
	eprintf(("bad ivec, vm_intr 0x%x, intr 0x%x\n",
		 vme_ivec[VME_VEC(adapter, vec)].vm_intr, vmeuliintrp));
	return(EACCES);
    }
    if (vme_ivec[VME_VEC(adapter, vec)].vm_arg != -1 &&
	arg != -1) {
	eprintf(("ivec busy %d %d\n", 
		 vme_ivec[VME_VEC(adapter, vec)].vm_arg,
		 arg));
	return(EBUSY);
    }
    vme_ivec[VME_VEC(adapter, vec)].vm_arg = arg;
#if defined(EVEREST)
    /*
     * Fix for bug #690661
     */
    vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_flags = 0;

    /*
     * Destroy the xthread associated with this vector element since uli's
     * do not use them
     */
    if (vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_xthread) {
      xthread_set_func(vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_xthread,
		       (xt_func_t *)xthread_exit, 0);
      vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_xthread = NULL;
      vsema(&(vme_ivec[VME_VEC(adapter, vec)].vm_tinfo->thd_isync));
    }
#endif
    dprintf(("ivec_reset ok\n"));
    return(0);
}
#endif

void
vme_ivec_init(void)
{
	int i, j, xadap;
	register uint_t vmeadapters = readadapters(ADAP_VME);
	
	/*
 	 * Init all entries in vme_ivec[] to point at vme_strayint().
	 */

	for (i = 0; i < vmeadapters; i++){
		xadap = VMEADAP_ITOX(i);
		for (j = 0; j < MAX_VME_VECTS; j++)
#if defined(EVEREST) && defined(ULI)
		        /*
			 * With the fix for bug #690661, just calling
			 * vme_ivec_set() would create MAX_VME_VECTS
			 * xthreads, which is excessive. So setup
			 * vme_strayint's as executed on the interrupt
			 * stack.
			 */
		        vme_ivec_set_ext(xadap, j, vme_strayint, j,
					 EXEC_MODE_ISTK);
#else
			vme_ivec_set(xadap, j, vme_strayint, j);
#endif /* defined(EVEREST) && defined(ULI) */
	}
}

/*
 * Allocate the next available vector from a given adapter.
 *
 * Return -1 if no vectors remain for allocation.
 */

int
vme_ivec_alloc(uint_t adapter)
{
	uint_t vec;
	int count;
	register uint_t vmeadapters = readadapters(ADAP_VME);


	adapter = VMEADAP_XTOI(adapter);

	if (adapter >= vmeadapters)
		return(-1);
	vec = vmevec_alloc[adapter];
	for (count = 0; count < MAX_VME_VECTS; count++) {
		if ((vec != 0) && (vec != 255) &&
		(vme_ivec[VME_VEC(adapter, vec)].vm_intr == vme_strayint)) {
	
			/* Return success */

			/* 
			 * set vmevec_alloc[adapter] to the next 
			 * vector location.
			 */

			vmevec_alloc[adapter] = (vec + 1) % MAX_VME_VECTS;

			/* 
			 * set  the allocated vector's handler to vme_nullint
			 * so we don't try to allocate it again until it
			 * is explicitly released.
			 */

#if defined(EVEREST) && defined(ULI)
			/*
			 * With the bug fix for 690661, set up the vector
			 * element's interrupt handler to execute on stack
			 * until the callee later calls vme_ivec_set() or
			 * vme_ivec_set_ext()
			 */
			vme_ivec_set_ext(adapter, vec, vme_nullint, (int)vec,
					 EXEC_MODE_ISTK);
#else
			vme_ivec_set(adapter, vec, vme_nullint, (int)vec);
#endif
			return(vec);
		}
		
		/* try next possible vector */
		
		vec = (vec + 1) % MAX_VME_VECTS;
	}

	return(-1);
}

/*
 * void
 * vme_ivec_free(int adapter, int vec)
 *
 * Return the specified interrupt vector to the free list.
 */
void
vme_ivec_free(int adapter, int vec)
{

	adapter = VMEADAP_XTOI(adapter);

	ASSERT(adapter < readadapters(ADAP_VME));
	ASSERT(adapter >= 0);
	ASSERT(vec < MAX_VME_VECTS);

#if defined(EVEREST) && defined(ULI)
	/* fix for bug #690661 */
	vme_ivec_set_ext(adapter, vec, vme_strayint, vec, EXEC_MODE_ISTK);
#else
	vme_ivec_set(adapter, vec, vme_strayint, vec);
#endif /* defined(EVEREST) && defined(ULI) */
}

#ifdef	EVEREST
/*
 * vme_ivec_ioflush_disable
 *      Disable flushing io caches for the specified <Adap,Vec> pair.
 *      Driver is expected to take care of it in its absence.
 */

int
vme_ivec_ioflush_disable(int adapter, int vec)
{
	register uint_t vmeadapters = readadapters(ADAP_VME);

	adapter = VMEADAP_XTOI(adapter);

	if ((adapter < 0) || (adapter >= vmeadapters) ||
		(vec >= MAX_VME_VECTS) ||
		(vme_ivec[VME_VEC(adapter, vec)].vm_intr == vme_nullint) ||
		(vme_ivec[VME_VEC(adapter, vec)].vm_intr == vme_strayint))
			return(-1);

	vme_ivec[VME_VEC(adapter, vec)].vm_ivec_ioflush = 0;
	return(0);

}

#endif	/* EVEREST */

#endif /* USE_VMEBUS */

#ifdef NO_VMEBUS

/*ARGSUSED*/
int vme_init_adap (struct edt *edtp)
{
	return -1;
}

/*ARGSUSED*/
int
vme_ivec_set(int adapter, int vec, int (*intr)(int), int arg)
{
	return -1;
}

/*ARGSUSED*/
int
vme_ivec_alloc(uint_t adapter)
{
	return -1;
}

/*ARGSUSED*/
void
vme_ivec_free(int adapter, int vec)
{
}
#endif /* NO_VMEBUS */

#if !EVEREST && !SN
#ifndef vme_to_phys
/*
 * Address conversion from VME address
 */
vme_to_phys(addr, type)
 unsigned long addr;
 int type;			/* which space */
{
	return(vme_ioaddr[type].v_base + addr);
}
#endif
#endif /* !EVEREST && !SN0 */
