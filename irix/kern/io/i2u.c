/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1997, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Driver for interrupt to user-level execution latency timing
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/sema.h>
#include <sys/errno.h>
#include <sys/clksupport.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/systm.h>
#include <sys/hwgraph.h>
#include <sys/atomic_ops.h>
#include <sys/sema_private.h>
#include <sys/kthread.h>
#include <ksys/xthread.h>
#include <ksys/ithread.h>
#include <sys/cmn_err.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/runq.h>

/*
 * The following interrupt macros are machine dependent
 */

#if defined(EVEREST)
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/groupintr.h>
#endif

#if defined(SN)
#include <sys/SN/intr.h>
#include "../ml/SN/sn_private.h"

#define	CPU_INTRLAT_B	62                 /* XXX should be in intr.h */
#define	CPU_INTRLAT_A	61                 /* XXX should be in intr.h */
#endif

/*
 * Spl debug monitor, used for tracking latency spikes
 */
#ifdef SPLDEBUG
extern void spldebug_log_event(int);
extern void idbg_splx_log(int);
#define spldebug_init() idbg_addfunc("spl", (void (*)())idbg_splx_log)
#else
#define spldebug_log_event(x)
#define spldebug_init()
#endif

#define I2U_INTERRUPT 1
#define I2U_BLOCK 2
#define I2U_DEBUG 3

#define TARGET_CPU 1

int i2udevflag = D_MP;
int i2u_is_open = 0;
int check_cpu = 1;

static sv_t i2u_blocker;
static __int64_t start_time;

void i2u_interrupt(void);

void
i2uinit(void)
{
	vertex_hdl_t vhdl;
	int rc;

#if defined(SN)
	int intr_bit;
	intr_bit = CPU_INTRLAT_A + cputoslice(TARGET_CPU);
	vhdl = cpuid_to_vertex(TARGET_CPU);

	if (intr_bit != intr_reserve_level(TARGET_CPU,
					   intr_bit,
					   II_THREADED,
					   vhdl,
					   "intr_i2u")) {
		cmn_err(CE_WARN, "i2u - Can't reserve interrupt.");
		return;
	}

	rc = intr_connect_level(TARGET_CPU,
				intr_bit,
				INTPEND0_MAXMASK,
				(intr_func_t) i2u_interrupt,
				NULL,
				NULL);

	if (rc != 0) {
	 	cmn_err(CE_WARN, "i2u - Can't connect interrupt.");
		return;
	}
#endif

#if defined(EVEREST)
	evintr_connect(0,
		       EVINTR_LEVEL_EPC_DUARTS4,
		       SPLHIDEV,
		       TARGET_CPU,
		       (EvIntrFunc) i2u_interrupt,
		       NULL);
#endif

	/*
	 * Create a hwgraph node
	 */
	rc = hwgraph_char_device_add(GRAPH_VERTEX_NONE, "i2u/1", "i2u", &vhdl);
	if (rc != GRAPH_SUCCESS) {
		cmn_err(CE_WARN,
			"i2u - hardware graph registration failure: %d\n",
			rc);
		return;
	}

	hwgraph_chmod(vhdl, HWGRAPH_PERM_EXTERNAL_INT);

	/*
	 * Initialize synchronization variable
	 */
	sv_init(&i2u_blocker, SV_FIFO, "i2u_blocker");

	spldebug_init();

	cmn_err(CE_NOTE,"i2u - interrupt latency test driver is installed\n");
	cmn_err(CE_NOTE,"i2u - CPU %d is restricted for testing\n",TARGET_CPU);
}

/* ARGSUSED */
int
i2uopen(dev_t *devp, int mode)
{
	if (test_and_set_int(&i2u_is_open, 1))
		return EBUSY;

	return 0;
}

/* ARGSUSED */
int
i2uclose(dev_t dev)
{

	test_and_set_int(&i2u_is_open, 0);

	return 0;
}
  
/* ARGSUSED */
int
i2uioctl(dev_t dev, int cmd, __psint_t arg, int mode, struct cred *cr, int *rvp)
{
	int s;

	switch(cmd) {

	case I2U_INTERRUPT:

		/*
		 * If there is no waiter, try again later
		 */
		if (!SV_WAITER(&i2u_blocker))
			return EAGAIN;

		/*
		 * Trigger the interrupt
		 */
		s = spl7();
		start_time = GET_LOCAL_RTC;

#if defined(SN)
		REMOTE_HUB_SEND_INTR(
			COMPACT_TO_NASID_NODEID(cputocnode(TARGET_CPU)),
			CPU_INTRLAT_A + cputoslice(TARGET_CPU));
#endif

#if defined(EVEREST)
		EV_SET_LOCAL(EV_SENDINT,
			     EVINTR_VECTOR(EVINTR_LEVEL_EPC_DUARTS4,
					   EVINTR_CPUDEST(TARGET_CPU)));
#endif

		splx(s);
		break;

	case I2U_BLOCK:

		spldebug_log_event(0xbed);

		if (sv_wait_sig(&i2u_blocker, PZERO, 0, 0))
			return EINTR;
    
		/* return time */
		*rvp = start_time;

		spldebug_log_event(0xfed);

#if defined(SN)
		LOCAL_HUB_CLR_INTR(CPU_INTRLAT_A + cputoslice(cpuid()));
#endif
		break;

	case I2U_DEBUG:
		debug(0);
		break;

	default:
		return EINVAL;
	}

	return 0;
}

void
i2u_interrupt(void)
{
	/*
	 * NOTE: to run this handler as an interrupt thread on
	 *       IP19, ml/EVEREST/evintr.c needs to be edited to
	 *       allow EVINTR_LEVEL_EPC_INTR4 interrupts to be
	 *       handled by threads (see ULI_LVL).
	 *
	 *       IP27 platforms handle this automatically.
	 */

	if (check_cpu && (TARGET_CPU != cpuid())) {
		check_cpu = 0;
		(void) setmustrun(TARGET_CPU);
	}

	if (sv_signal(&i2u_blocker) != 1)
		cmn_err(CE_NOTE, "i2u - bad data point\n");
}    
