/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/systm.h>
#include <ksys/cell.h>
#include <ksys/sthread.h>
#include <ksys/cell/tkm.h>
#include <ksys/cell/mesg.h>
#include <ksys/cell/cell_hb.h>
#include <ksys/cell/membership.h>
#include <ksys/partition.h>
#include <ksys/cell/recovery.h>
#include <sys/cmn_err.h>

/*
 * Global data for the Cell module itself.
 */
short   kdebug_running = 0;     /* if set then idbg is running */
cell_t golden_cell = 0;		/* 'golden_cell' may not be shutdown */

cell_t my_cellid = 0;		/* Set before boot, returned by cellid() */

/*
 * Cell private variables.
 */
static volatile int cell_initted;

volatile int 		allowcells;

#ifdef	DEBUG
/*
 * Cell test stuff to send a receive a test message using 
 * syssgi.
 */
#include "ksys/cell/subsysid.h"
#include "I_cell_test_stubs.h"
#include "invk_cell_test_stubs.h"

sv_t	cell_test_sv;
lock_t	cell_test_lock;
#endif

/*
 * Called once on each cpu to initialize the cell private data & join
 * a particular cell.
 *
 * It's important that cpus come here before allocating any memory
 * since its here that they get a cell identity and therefore
 * can access cell private memory and allocate cell specific virtual
 * memory.
 *
 * WARNING - in this routine - use 'cellid' NOT cellid().
 * NOTE - we aren't called with any thread identity
 * so the only reason we can access cell private data is that cellda() uses
 * private.p_cellid when there is no thread, and we set that up here FIRST.
 */
void
cell_cpu_init(void)
{
	cell_t cellid;
	int rv;

	/*
	 * init our cellid ....
	 * XXX currently a simple split of cpus -> cells
	 * Once we assign p_cellid we can access cell private data.
	 */
#if defined(EVEREST) && defined(MULTIKERNEL)
	cellid = evmk_cellid;
#else /* !EVEREST || !MULTIKERNEL */
	cellid = 0;	/* CELLBAD - need to get cellid. */
#endif /* !EVEREST || !MULTIKERNEL */

	/* Assign this cpu's pda to cell's pdaindr */

	pdaindr[cpuid()].pda = pdaindr[cpuid()].pda;
	pdaindr[cpuid()].CpuId = cpuid();

	/*
	 * All cells come up with all cpus running so we have to
	 * synchronize.
	 */
	rv = test_and_set_int((int *)&cell_initted, 2); /* returns old value */
	if (rv == 2) {
		/* someone else initializing - wait */
		while (cell_initted == 2)
			;
		return;
	} else if (rv == 1) {
		cell_initted = 1;
		return;
	}

	/*
	 * Only one cpu per cell gets to this point.
	 */

	/*
	 * Special hack for cpu 0 / cell 0 - per-cell initialization
	 * called as part of mainline initialization, so it doesn't
	 * have to be done here -> i.e., just return.
	 */

#ifdef MULTIKERNEL
	if (cpuid() == 0) {
#else
	if (cpuid() == master_procid) {
#endif
		ASSERT(cellid == 0);
		allowcells = 1;
		cell_initted = 1; /* well, not really */
		return;
	}

	/*
	 * Perform per-cell initialization, but must wait for cpu 0 to
	 * do the mainline initialization. 
	 */
	while (!allowcells)
#if defined(EVEREST) && defined(MULTIKERNEL)
		/*  need to import this here since an evmk_replicate() from
		 * master cell does not work (allowcells is in BSS and gets
		 * explicitly zeroed by non-golden cell master cpu).
		 * So import it from golden cell.
		 */
		if (evmk_cellid != evmk_golden_cellid) {
			evmk_import( evmk_golden_cellid, (void*)&allowcells,
				sizeof(allowcells));
		}
#endif
		;

#if defined(EVEREST) && defined(MULTIKERNEL)
	if ((evmk_cellid == cpuid()) && (evmk_cellid != evmk.golden_cellid))
		cell_initted = 1;

#endif /* EVEREST && MULTIKERNEL */
	/*
	 * Wait for cell_init_one() to be executed for this cpu's cell.
	 */
	while (cell_initted != 1) {
#if DEBUG
		if (kdebug) du_conpoll();
#endif
	}

	/*
	 * Let the game begin!
	 */
}

/*
 * cell_up_init:
 * Enable new cell notifications. This is a separate 
 * init routine. Its called after the subsystems which are interested
 * in knowing when cells come up have been initted. For now these are
 * heart beat and messaging subsystems.
 */
void
cell_up_init()
{
	part_register_handlers(cell_up, cell_down);
#ifdef	DEBUG
        mesg_handler_register(cell_test_msg_dispatcher, CELL_TEST_SUBSYSID);
	sv_init(&cell_test_sv, SV_DEFAULT, "Cell test sv");
	init_spinlock(&cell_test_lock, "Cell test lock", 0);
#endif
}

/*
 * Generic cell backoff.  Used when a client is asked to wait for a
 * previous server response.  It backs off relative to the number 	
 * iterations it has been waiting.
 */
void
cell_backoff(int iter)
{
	timespec_t lookup_delay;
	if (iter > 10) {
		lookup_delay.tv_sec = 0;
		/* 10s of uS */
		lookup_delay.tv_nsec = (iter * 10) * 1000;
		nano_delay(&lookup_delay);
	}
}

void
cell_down(partid_t part)
{
	cell_t  cell;
	
	cell = PARTID_TO_CELLID(part);
	cms_notify_cell(cell, B_FALSE);
#ifdef	DEBUG
	cmn_err(CE_NOTE,
		"WARNING: cell %d has gone down\n", PARTID_TO_CELLID(part));
#endif
	/*
	 * Disconnect any channels.
	 */
	if (cell != cellid()) {
		mesg_disconnect(cell, MESG_CHANNEL_MAIN);
		mesg_disconnect(cell, MESG_CHANNEL_MEMBERSHIP);
	}
}

void
cell_up(partid_t part)
{
	cell_t  cell;
	
	cell = PARTID_TO_CELLID(part);
	if (cell != cellid())  {
		if (!mesg_connect(cell, MESG_CHANNEL_MEMBERSHIP))
			panic("cell_up: No membership message channel");
		mesg_channel_set_state(cell, MESG_CHANNEL_MEMBERSHIP,
						MESG_CHAN_READY);	
		cms_notify_cell(cell, B_TRUE);
	}
}

/*
 * Called by the membership services to connect cells together
 * Returns B_FALSE on failure  and B_TRUE on success.
 */ 
boolean_t
cell_connect(cell_t cell)
{

	if (cell != cellid())  {
		if (!mesg_connect(cell, MESG_CHANNEL_MAIN))
			return B_FALSE;
		if (!hb_add_cell(cell))
			return B_TRUE;
	}
	return B_TRUE;
}

void
cell_disconnect(cell_t cell)
{
	if (cell != cellid())  {
		mesg_channel_set_state(cell, MESG_CHANNEL_MAIN, 
							MESG_CHAN_CLOSED);
		mesg_disconnect(cell, MESG_CHANNEL_MAIN);
		hb_remove_cell(cell);
	}
}


#ifdef	DEBUG

void
cell_test_mesg(cell_t cell)
{
	service_t       svc;
	int		error;

	SERVICE_MAKE(svc, cell, SVC_CELL_TEST);
	error = invk_cell_test_mesg_send(svc, cell);
	printf("Invk returns error %d\n", error);
}


int	cell_test_first_time = 1;


/* ARGSUSED */
int
I_cell_test_mesg_send(cell_t cell)
{
	int	spl;

	spl = mutex_spinlock(&cell_test_lock);
	if (sv_wait_sig(&cell_test_sv, PZERO|PLTWAIT, &cell_test_lock, spl)) {
		cmn_err(CE_CONT,"Sv wait sig. Interrupted thread for "
					"cell %d\n", cell);
		return 1;
	}
	return 0;
}

#include  "sys/pda.h"
void	cell_cpu_spin(void);

void
cell_cpu_spin()
{
	splhi();
	for(;;) ;
}	

void
cell_suicide()
{
	cpuid_t	cpu;

	for (cpu = 0; cpu < maxcpus; cpu++) {
		if (!cpu_enabled(cpu))
			continue;
		if (cpu == cpuid())
			continue;
		cpuaction(cpu, (cpuacfunc_t)cell_cpu_spin, A_NOW);
	}
	cell_cpu_spin();
}

#endif
