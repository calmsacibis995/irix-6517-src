#ident	"$Revision: 1.28 $"

/*
 * Driver for IOC3 external interrupts
 */
#if IP27 || IP30

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/iobus.h>
#include <sys/errno.h>
#include <sys/atomic_ops.h>
#include <sys/immu.h>
#include <sys/ei.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <ksys/ddmap.h>
#include <sys/mman.h>
#include <sys/kabi.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/frs.h>
#include <sys/ksignal.h>
#ifdef ULI
#include <sys/uli.h>
#include <ksys/uli.h>
#endif

#include <sys/hwgraph.h>
#include <sys/iograph.h>

#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/ioc3.h>
#include <sys/invent.h>
#include <sys/hwgraph.h>

int eidevflag = D_MP;

#define EILOCK(ei_info) mutex_spinlock(&(ei_info)->lock)
#define EIUNLOCK(ei_info, s) mutex_spinunlock(&(ei_info)->lock, s)

/* per process state maintained by the driver.
 */
struct pstate {
	pid_t pstate_pid;		/* pid owning this struct */
	struct pstate *next;
	int privcounter;		/* interrupt queue for this process */
	int signal;			/* signal to send proc on interrupt */
	lock_t lock;
	sv_t wait;
};

/* private per-IOC3 storage */
typedef struct ei_info {
    vertex_hdl_t	conn;
    vertex_hdl_t	vhdl;

    ioc3_mem_t	       *mem;
    /* shadow copy of enabled interrupts */
    ioc3reg_t           imask;

    lock_t		lock;

    struct pstate      *pstate_list;
    int		       *usercounter;
    int			flags;
#if ULI
    int			ulinum;
#endif

    void	       *ioc3_soft;
} ei_info_t;

/* for flags */
#define EI_HWG_OK	1	/* hwgraph alias created for this device */

#define	ei_info_set(v,i)	(hwgraph_fastinfo_set((v),(arbitrary_info_t)(i)))
#define	ei_info_get(v)		((ei_info_t *)hwgraph_fastinfo_get((v)))

/* page from addr */
#define PAGE(x) (caddr_t)((__psunsigned_t)(x) & (~(NBPP-1)))

/* debug printing */
#ifdef DPRINTF
#define dprintf(x) printf x
#else
#define dprintf(x)
#endif

#if _MIPS_SIM == _ABI64
int irix5_to_eiargs_xlate(enum xlate_mode, void *, int,	xlate_info_t *);
int eiargs_to_irix5_xlate(void *, int, xlate_info_t *);
#endif

#ifdef ULI
#define ULI_ISFREE(ulinum) (ulinum == -1)
#define FREE_ULI(ulinum) (ulinum = -1)
#define ULI_ISACTIVE(ulinum) (ulinum >= 0)
#define RESERVE_ULI(ulinum) (ulinum = -2)
#endif

#ifdef ULI_TSTAMP
static int trigger_ts;
#endif

static void
ei_make_hwgraph_nodes(ei_info_t *ei)
{
    /*REFERENCED*/
    int rc, num;
    char name[4];
    vertex_hdl_t eidir_vhdl;

    /* only do this once per device */
    if (atomicSetInt(&ei->flags, EI_HWG_OK) & EI_HWG_OK)
	return;

    /* get the assigned number for this ei device */
    num = device_controller_num_get(ei->vhdl);
    if (num < 1 || num > 999)
	return;
    
    /* make sure ei dir exists */
    rc = hwgraph_path_add(hwgraph_root, EI_HWG_DIR, &eidir_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    /* create a duplicate edge to the real device */
    sprintf(name, "%d", num);
    hwgraph_edge_add(eidir_vhdl, ei->vhdl, name);
}

/* called when a process which had previously opened the EI device exits.
 * We free up the process's pstate struct here
 */
static void
ei_exit(void *arg)
{
    int s;
    struct pstate **ptmpp, *ptmp;
    ei_info_t *ei_info = (ei_info_t *)arg;

    dprintf(("ei_exit pid %d\n", current_pid()));
    s = EILOCK(ei_info);

    /* clear the pstate for this proc */
    for (ptmpp = &ei_info->pstate_list; *ptmpp; ptmpp = &((*ptmpp)->next)) {
	ptmp = *ptmpp;

	if (ptmp->pstate_pid == current_pid()) {
	    *ptmpp = ptmp->next;
	    spinlock_destroy(&ptmp->lock);
	    sv_destroy(&ptmp->wait);
	    kmem_free(ptmp, sizeof(struct pstate));
	    break;
	}
    }
    EIUNLOCK(ei_info, s);
}

/* find the pstate for the current proc, or allocate one if there is none.
 */
static struct pstate *
find_pstate(ei_info_t *ei_info)
{
    struct pstate *ptmp;
    int s;
    /* REFERENCED */
    int err;

    s = EILOCK(ei_info);
    for(ptmp = ei_info->pstate_list; ptmp; ptmp = ptmp->next)
	if (ptmp->pstate_pid == current_pid()) {
	    EIUNLOCK(ei_info, s);
	    return(ptmp);
	}
    EIUNLOCK(ei_info, s);

    /* This is the first time this proc has accessed the device. Set
     * up an exit callback to ensure that the pstate is torn down
     * when the proc exits
     */
    err = add_exit_callback(current_pid(), 0,
				(void (*)(void *))ei_exit, (void*)ei_info);
    ASSERT(err == 0);
    dprintf(("first EI access by proc: add exit func\n"));

    if ((ptmp = (struct pstate*)kmem_zalloc(sizeof(struct pstate), 0)) == 0)
	return(0);
    ptmp->pstate_pid = current_pid();
    spinlock_init(&ptmp->lock, "eilock");
    sv_init(&ptmp->wait, SV_DEFAULT, "eiwait");

    s = EILOCK(ei_info);
    ptmp->next = ei_info->pstate_list;
    ei_info->pstate_list = ptmp;
    EIUNLOCK(ei_info, s);

    dprintf(("new pstate pid %d\n", current_pid()));
    return(ptmp);
}

void
ioc3_ei_intr(intr_arg_t arg)
{
    struct pstate *ptmp;
    int s;
    ei_info_t *ei_info = (ei_info_t *)arg;
    
    extern void frs_handle_eintr(void);

    /* ACK the interrupt */
    PCI_OUTW(&ei_info->mem->sio_ir, SIO_IR_RT);

    /* Frame Scheduler */
    if (private.p_frs_flags) {
	frs_handle_eintr();
	IOC3_WRITE_IES(ei_info->ioc3_soft, ei_info->imask);
	return;
    }

#ifdef ULI
    if (ULI_ISACTIVE(ei_info->ulinum)) {
	uli_callup(ei_info->ulinum);
	IOC3_WRITE_IES(ei_info->ioc3_soft, ei_info->imask);
	return;
    }
#endif
    
    s = EILOCK(ei_info);
    
    /* increment user counter */
    (*ei_info->usercounter)++;

    /* increment privcounter, send signal or do wakeup if necessary
     * on each process
     */
    for (ptmp = ei_info->pstate_list; ptmp; ptmp = ptmp->next) {
	atomicAddInt(&ptmp->privcounter, 1);
	if (ptmp->signal) {
	    dprintf(("sending signal %d to pid %d\n",
		     ptmp->signal, ptmp->pstate_pid));

	    /* potential race if the process is exiting. Since we are
	     * holding the EILOCK here, the process will stall in
	     * ei_exit() if it tries to exit while we are signalling it
	     */
	    sigtopid(ptmp->pstate_pid, ptmp->signal, SIG_ISKERN|SIG_NOSLEEP,
			0, 0, 0);
	}
        nested_spinlock(&ptmp->lock);
	sv_signal(&ptmp->wait);
        nested_spinunlock(&ptmp->lock);
    }
    
    EIUNLOCK(ei_info, s);

    IOC3_WRITE_IES(ei_info->ioc3_soft, ei_info->imask);
}

/*
 *    eiinit: called once during system startup or
 *      when a loadable driver is loaded.
 */
void
eiinit(void)
{
}

/*
 *    eireg: called once during system startup or
 *      when a loadable driver is loaded. Call bus
 *	provider register routine from here.
 *
 *    NOTE: a bus provider register routine should always be 
 *	called from _reg, rather than from _init. In the case
 *	of a loadable module, the devsw is not hooked up
 *	when the _init routines are called.
 *
 */
void
eireg(void)
{
	pciio_driver_register(IOC3_VENDOR_ID_NUM,
			      IOC3_DEVICE_ID_NUM,
			      "ei",
			      0);
}

/*
 *    eiunreg: called when a loadable driver is unloaded.
 */
void
eiunreg(void)
{
	pciio_driver_unregister("ei");
}

static int
use_ithread(intr_arg_t intrarg)
{
	/* don't use an ithread for ULI or if the frame scheduler is
	 * active.
	 */
	
	return(
#if ULI
	       ULI_ISFREE(((ei_info_t*)intrarg)->ulinum) &&
#endif
	       ((private.p_frs_flags & FRS_INTRSOURCE_EXTINTR) == 0));
}

/* ei initialization routine, called at system startup */
int
eiattach(vertex_hdl_t conn)
{
    /*REFERENCED*/
    graph_error_t	rc;
    ioc3_mem_t	       *mem;
    vertex_hdl_t	vhdl, ioc3_vhdl;
    ei_info_t	       *ei_info;
    void	       *ioc3_soft;

    if (!ioc3_subdev_enabled(conn, ioc3_subdev_rt))
	return -1;	/* not brought out to connectors */

    /*
     * the ioc3 may be dualslot; if so, we want to
     * attach to its alternate connect point.
     */
    (void)hwgraph_traverse(conn, EDGE_LBL_GUEST, &conn);

    /* get back pointer to the ioc3 soft area */
    rc = hwgraph_traverse(conn, EDGE_LBL_IOC3, &ioc3_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    ioc3_soft = (void*)hwgraph_fastinfo_get(ioc3_vhdl);

    /*
     * get a PIO mapping to the IOC3 chip
     */
    mem = ioc3_mem_ptr(ioc3_soft);

    /*
     * allocate the softinfo, and fill it in.
     */
    ei_info = (ei_info_t *)kmem_zalloc(sizeof(ei_info_t), KM_NOSLEEP);
    ei_info->conn = conn;
    ei_info->mem = mem;
    ei_info->ioc3_soft = ioc3_soft;

    spinlock_init(&ei_info->lock, "ei_info");

    /* allocate a private page to hold the user counter. */
    if ((ei_info->usercounter = (int*)kvpalloc(1, VM_VM, 0)) == 0)
	return -1;
	
    /* set up IOC3 hardware */
    
    /* configure pin 1 as an input */
    PCI_OUTW(&mem->gpcr_c, GPCR_DIR_PIN(1));

    /* XXX IP30 uses this pin on base IOC3s for the LED! so this is bad */
    /* make pin 1 edge triggered input */
    PCI_OUTW(&mem->gpcr_s, GPCR_EDGE_PIN(1));

    /* turn off the interrupt generator */
    PCI_OUTW(&mem->int_out, INT_OUT_MODE_0);

#ifdef ULI
    FREE_ULI(ei_info->ulinum);
#endif

    /* create a device node in the hwgraph */
    rc = hwgraph_char_device_add(conn, "ei", "ei", &vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    hwgraph_chmod(vhdl, HWGRAPH_PERM_EXTERNAL_INT);
    ei_info->vhdl = vhdl;
    ei_info_set(vhdl, ei_info);

    /* create inventory entry */
    device_inventory_add(vhdl,
			 INV_MISC,
			 INV_MISC_IOC3_EINT,
			 0,0,0);
    
    /* for baseio, create the hwgraph alias */
    if (ioc3_is_console(conn)) {

	/* set up hwgraph nodes for console port */
	device_controller_num_set(vhdl, 1);
	ei_make_hwgraph_nodes(ei_info);
    }
    else
	device_controller_num_set(vhdl, -1);

    /* connect interrupt handler */
    
    ioc3_intr_connect(conn, SIO_IR_RT,
		      (ioc3_intr_func_t)ioc3_ei_intr,
		      (intr_arg_t)ei_info,
		      vhdl,
		      vhdl,
		      use_ithread);

    /* no intrs enabled */
    ei_info->imask = 0;

    return 0;
}

/* ARGSUSED */
int
eiopen(dev_t *devp, int mode)
{
    vertex_hdl_t vhdl;
    struct pstate *ptmp;
    ei_info_t *ei_info;

    vhdl = dev_to_vhdl(*devp);
    if (vhdl == GRAPH_VERTEX_NONE)
	return(ENODEV);

    ei_info = ei_info_get(vhdl);

    /* allocate a pstate if there isn't already one for this proc */
    ptmp = find_pstate(ei_info);
    if (ptmp == 0)
	return(ENOMEM);

    /* enable the output */
    PCI_OUTW(&ei_info->mem->gpcr_s, GPCR_INT_OUT_EN);

    return(0);
}

/* on last close of the device, we know all processes have gone away,
 * so nuke the entire pstate_list. Also, reset everything to default 
 * values
 */
/* ARGSUSED */
int
eiclose(dev_t dev)
{
    vertex_hdl_t vhdl;
    ei_info_t *ei_info;
    ioc3_mem_t *mem;

    vhdl = dev_to_vhdl(dev);
    if (vhdl == GRAPH_VERTEX_NONE)
	return(ENODEV);

    ei_info = ei_info_get(vhdl);
    mem = ei_info->mem;

    /* disable interrupts, deassert outgoing lines */
    IOC3_WRITE_IEC(ei_info->ioc3_soft, SIO_IR_RT);
    /* update shadow interrupt mask */
    ei_info->imask &= ~SIO_IR_RT;

    PCI_OUTW(&mem->int_out, INT_OUT_MODE_0);
    
#ifdef ULI
    /* if we called eiclose with a ULI registered, we have to do
     * the ULI unregistration here, otherwise setintrcpu()
     * below fails. Anyway, it's meaningless to have a ULI registered
     * to a device whose interrupts have been disabled.
     */
    FREE_ULI(ei_info->ulinum);
#endif

    return(0);
}

#ifdef ULI

#ifdef EPRINTF
#define eprintf(x) printf x
#else
#define eprintf(x)
#endif

/* teardown func called when ULI proc exits */
/* ARGSUSED */
static void
ei_uli_teardown(struct uli *uli)
{
    ei_info_t *ei_info = (ei_info_t *)uli->teardownarg1;
    FREE_ULI(ei_info->ulinum);
    dprintf(("tore down ULI for ei\n"));
}

/* register a ULI to handle the ei */
static int
register_uli(ei_info_t *ei_info, __psint_t arg)
{
    struct uliargs args;
    struct uli *uli;
    int s, err, runtime_intrcpu;
    extern cpuid_t ioc3_intr_cpu_get(void*);

    if (COPYIN_XLATE((void*)arg, &args, sizeof(args),
		     irix5_to_uliargs, get_current_abi(), 1)) {
	eprintf(("eiioctl: bad copyin\n"));
	return(EFAULT);
    }

    dprintf(("eiioctl args pc 0x%x sp 0x%x funcarg 0x%x\n",
	     args.pc, args.sp, args.funcarg));
    
    /* reserve the ulinum.
     */
    s = EILOCK(ei_info);
    if ( ! ULI_ISFREE(ei_info->ulinum)) {
	EIUNLOCK(ei_info, s);
	return(EBUSY);
    }
    RESERVE_ULI(ei_info->ulinum);
    EIUNLOCK(ei_info, s);

    runtime_intrcpu = ioc3_intr_cpu_get(ei_info->ioc3_soft);
    if (!cpu_isvalid(runtime_intrcpu)) {
	FREE_ULI(ei_info->ulinum);
	return(EINVAL);
    }

    if (err = new_uli(&args, &uli, runtime_intrcpu)) {
	eprintf(("eiioctl: new_uli failed\n"));
	FREE_ULI(ei_info->ulinum);
	return(err);
    }
    
    uli->teardown = ei_uli_teardown;
    uli->teardownarg1 = (__psint_t)ei_info;
    
    args.id = uli->index;
    if (XLATE_COPYOUT(&args, (void*)arg, sizeof(args),
		      uliargs_to_irix5, get_current_abi(), 1)) {
	eprintf(("eiioctl: bad copyout\n"));
	free_uli(uli);
	FREE_ULI(ei_info->ulinum);
	return(EFAULT);
    }

    /* Once the ulinum is set, the ULI may be invoked at any time,
     * so only do this when everything is set to go. No need to lock
     * here since ulinum is reserved so nobody else can touch it.
     */
    ei_info->ulinum = uli->index;

    dprintf(("eiioctl: return normal\n"));
    return(0);
}
#endif

/* set the output mode without changing the count value */
static void
set_output_mode(ei_info_t *ei_info, ioc3reg_t mode)
{
    ioc3reg_t int_out;
    int s;

    s = EILOCK(ei_info);
    int_out = PCI_INW(&ei_info->mem->int_out);
    int_out &= ~INT_OUT_MODE;
    int_out |= mode;
    PCI_OUTW(&ei_info->mem->int_out, int_out);
    EIUNLOCK(ei_info, s);
}

/* ARGSUSED */
int
eiioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *cr, int rvp)
{
    vertex_hdl_t vhdl;
    struct pstate *ptmp;
    ei_info_t *ei_info;

    dprintf(("called eiioctl on cpu %d minor %d\n", cpuid(), minor(dev)));

    vhdl = dev_to_vhdl(dev);
    if (vhdl == GRAPH_VERTEX_NONE)
	return(ENODEV);

    ei_info = ei_info_get(vhdl);
    ptmp = find_pstate(ei_info);

    switch(cmd) {
	
	/* create hwgraph nodes */
      case EIMKHWG:
	ei_make_hwgraph_nodes(ei_info);
	return(0);

	/* generate outgoing pulse. */
      case EIIOCSTROBE:
	set_output_mode(ei_info, INT_OUT_MODE_1PULSE);
	return(0);

      case EIIOCSETHI:
	set_output_mode(ei_info, INT_OUT_MODE_1);
	return(0);

      case EIIOCSETLO:
	set_output_mode(ei_info, INT_OUT_MODE_0);
	return(0);

      case EIIOCPULSE:
	set_output_mode(ei_info, INT_OUT_MODE_PULSES);
	return(0);

      case EIIOCSQUARE:
	set_output_mode(ei_info, INT_OUT_MODE_SQW);
	return(0);

      case EIIOCENABLE:
	/* enable incoming interrupts */
	ei_info->imask |= SIO_IR_GEN_INT1;
	
	IOC3_WRITE_IES(ei_info->ioc3_soft, SIO_IR_GEN_INT1);
	return(0);

      case EIIOCDISABLE:
	/* disable incoming interrupts */
	ei_info->imask &= ~SIO_IR_GEN_INT1;
	
	IOC3_WRITE_IEC(ei_info->ioc3_soft, SIO_IR_GEN_INT1);
	return(0);

      case EIIOCENABLELB:
	/* enable loopback interrupt */
	ei_info->imask |= SIO_IR_RT_INT;
	
	IOC3_WRITE_IES(ei_info->ioc3_soft, SIO_IR_RT_INT);
	return(0);

      case EIIOCDISABLELB:
	/* disable loopback interrupt */
	ei_info->imask &= ~SIO_IR_RT_INT;
	
	IOC3_WRITE_IEC(ei_info->ioc3_soft, SIO_IR_RT_INT);
	return(0);

      case EIIOCRECV: {
	  struct eiargs eiargs;
	  int rv, haveto, s;
	  timespec_t ts, rts;

	  if (ptmp == 0)
	      return(ENOMEM);

	  if (COPYIN_XLATE((char *)arg, (char*)&eiargs, sizeof(eiargs),
	  			irix5_to_eiargs_xlate, get_current_abi(),
				1))
	      return(EFAULT);

	  /* if caller passed in intval true, flush the intr queue. This
	   * doesn't need a lock because increments and decrements of
	   * this counter are done with ll-sc
	   */
	  if (eiargs.intval)
	      ptmp->privcounter = 0;

	  /* go to sleep if the queue is empty and the user requested it */
	  if (ptmp->privcounter == 0 &&
	      (eiargs.timeval.tv_sec || eiargs.timeval.tv_usec)) {

	      /* if wakeup is not infinite, set a timer */
	      haveto = 0;
	      if (eiargs.timeval.tv_sec != -1) {
		  if (hzto(&eiargs.timeval) == 0) {
			/* provide minimum timeout */
			tick_to_timespec(1, &ts, NSEC_PER_USEC);
		  } else {
			ts.tv_sec = eiargs.timeval.tv_sec;
			ts.tv_nsec = eiargs.timeval.tv_usec * NSEC_PER_USEC;
		  }
		  haveto = 1;
	      }

	      /* sleep until either the counter is incremented (by the
	       * interrupt handler) or our timeout, if it exists,
	       * expires, or a signal comes in
	       */
	      s = mutex_spinlock(&ptmp->lock);
	      while (ptmp->privcounter == 0) {

		  if (haveto) {
			rv = sv_timedwait_sig(&ptmp->wait, 0,
			    &ptmp->lock, s, 1, &ts, &rts);
		  } else {
			rv = sv_wait_sig(&ptmp->wait, 0, &ptmp->lock, s);
		  }
		  if (rv == -2) {
		      /*
		       * got signal but it was cancelled - keep going
		       */
		      ts = rts;
		  } else {
		      /* got signal or timeout - time to quit */
		      break;
		  }
	          s = mutex_spinlock(&ptmp->lock);
	      }
	  }

	  /* if we got an intr, dequeue it and notify the caller */
	  if (ptmp->privcounter > 0) {
	      atomicAddInt(&ptmp->privcounter, -1);
	      dprintf(("cnt now down to %d\n", ptmp->privcounter));
	      eiargs.intval = 1;
	  }
	  else
	      eiargs.intval = 0;

	  XLATE_COPYOUT((char*)&eiargs, (char *)arg, sizeof(eiargs),
	  		eiargs_to_irix5_xlate, get_current_abi(), 1);
	  return(0);
      }

      case EIIOCSETSIG: {
	  if (ptmp == 0)
	      return(ENOMEM);

	  ptmp->signal = arg;
	  return(0);
      }

	/* backward compatibility: we should return no error on these
	 * calls even though they do nothing so the ABI isn't broken
	 */
      case EIIOCSETOPW:
      case EIIOCSETIPW:
      case EIIOCSETSPW:
	return(0);

      case EIIOCGETOPW:
      case EIIOCGETIPW: {
	  /* return microseconds per pulse */
	  int width = INT_OUT_NS_PER_TICK * INT_OUT_TICKS_PER_PULSE / 1000;
	  if (copyout((char*)&width, (char*)arg, sizeof(width)))
	    return(EFAULT);
	  return(0);
      }
	
      case EIIOCGETSPW:
	if (uzero((char*)arg, sizeof(int)))
	    return(EFAULT);
	return(0);

      case EIIOCSETPERIOD: {
	  ioc3reg_t int_out;
	  int count;
	  int s;

	  count = INT_OUT_US_TO_COUNT((int)(__psint_t)arg);

	  if (count < INT_OUT_MIN_TICKS || count > INT_OUT_MAX_TICKS)
	      return(EINVAL);

	  s = EILOCK(ei_info);
	  int_out = PCI_INW(&ei_info->mem->int_out);
	  int_out &= ~INT_OUT_COUNT;
	  int_out |= count;
	  PCI_OUTW(&ei_info->mem->int_out, int_out);
	  EIUNLOCK(ei_info, s);
	  return(0);
      }

      case EIIOCGETPERIOD: {
	  int period;
	  ioc3reg_t int_out;

	  int_out = PCI_INW(&ei_info->mem->int_out);
	  period = INT_OUT_COUNT_TO_US(int_out & INT_OUT_COUNT);

	  if (copyout((char*)&period, (char*)arg, sizeof(period)))
	    return(EFAULT);
	  return(0);
      }

#ifdef ULI
#if ULI_TSTAMP
      case 1002:
#endif
      case EIIOCREGISTERULI:
	return(register_uli(ei_info, arg));
#endif

#ifdef ULI_TSTAMP
	/* a bunch of internal use only ioctls to get ULI and
	 * interrupt latencies.
	 */
      case 1000: {
	  int s;

	  /* trigger the interrupt */
	  s = spl7();
	  set_output_mode(ei_info, INT_OUT_MODE_1PULSE);
	  /* keep a separate copy of this timestamp rather than writing
	   * directly to the timestamp array, which would invalidate that
	   * cache line on the interrupt cpu
	   */
	  trigger_ts = get_timestamp();
	  splx(s);
	  return(0);
      }

      case 1001:
	/* get the tstamps */
	if (copyout((char*)&trigger_ts, (char*)arg, sizeof(int))
#ifdef ULI_TSTAMP1
	    || copyout((char*)&uli_saved_tstamps[1], (char*)(((int*)arg)+1),
		       sizeof(int) * (TS_MAX - 1))
#endif
	    )
	    return(EFAULT);
	return(0);

#ifdef ULI_TSTAMP1
      case 1003:
	return(0);
#endif
	
#endif /* ULI_TSTAMP */

      default:
	dprintf(("unknown cmd %d\n", cmd));
	return(EINVAL);
    }
}

/* map the counterpage into the caller's address space. The counter
 * page is used to hold a global counter that is incremented each
 * time an interrupt is received. This counter is never reset. The
 * eic library routines implement a busywait feature by examining
 * this counter and waiting for it to be incremented. A per-process
 * queue is implemented in usermode by keeping a separate counter
 * and comparing it to the global counter.
 */
/* ARGSUSED */
int
eimap(dev_t dev, vhandl_t *vt, off_t off, size_t len, uint_t prot)
{
    caddr_t mapaddr;
    vertex_hdl_t vhdl;
    ei_info_t *ei_info;

    vhdl = dev_to_vhdl(dev);
    if (vhdl == GRAPH_VERTEX_NONE)
	return(ENODEV);

    ei_info = ei_info_get(vhdl);

    dprintf(("map dev %x off %x len %d\n", dev, off, len));
    
    /* len must be 1 page */
    if (len != NBPP)
	return(EINVAL);

    /* if off is 0, request is to map the counter page for the
     * calling process
     */
    if (off == 0) {
	if (prot & PROT_WRITE)
	    return(EACCES);
	mapaddr = (caddr_t)ei_info->usercounter;
    }

    else
	return(EINVAL);
	
    return(v_mapphys(vt, mapaddr, len));
}

/* ARGSUSED */
int
eiunmap(dev_t dev, vhandl_t *vt)
{
    return(0);
}

#if _MIPS_SIM == _ABI64

struct irix5_eiargs {
	int			intval;
	struct irix5_timeval	timeval;
};

/*ARGSUSED*/
int
irix5_to_eiargs_xlate(enum xlate_mode mode, void *to, int count,
					register xlate_info_t *info)
{
	struct eiargs *ei_to = (struct eiargs *)to;
	struct irix5_eiargs *ei_from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_eiargs) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf =
			    kern_malloc(sizeof(struct irix5_eiargs));
		info->copysize = sizeof(struct irix5_eiargs);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_eiargs));
	ASSERT(info->copybuf != NULL);

	ei_from = (struct irix5_eiargs *)info->copybuf;
	ei_to->intval = ei_from->intval;
	irix5_to_timeval(&ei_to->timeval, &ei_from->timeval);
	return 0;
}

/*ARGSUSED*/
int
eiargs_to_irix5_xlate(void *to, int count, register xlate_info_t *info)
{
	struct eiargs *ei_from = (struct eiargs *)to;
	struct irix5_eiargs *ei_to;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_eiargs) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_eiargs));
	info->copysize = sizeof(struct irix5_eiargs);

	ei_to = (struct irix5_eiargs *)info->copybuf;
	ei_to->intval = ei_from->intval;
	timeval_to_irix5(&ei_from->timeval, &ei_to->timeval);

	return 0;
}
#endif /* _MIPS_SIM == _ABI64 */
#endif /* IP27 || IP30 */
