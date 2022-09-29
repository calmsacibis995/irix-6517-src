/*
 * heart_beat.c
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include <sys/types.h>
#include <ksys/cell.h>
#include <sys/immu.h>
#include <sys/page.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/errno.h>
#include <sys/idbgentry.h>
#include <sys/sema.h>
#include <ksys/partition.h>
#include <ksys/cell/service.h>
#include <ksys/cell/cell_set.h>
#include <ksys/cell/cell_hb.h>
#include <ksys/cell/membership.h>
#include "invk_cell_hb_stubs.h"
#include "I_cell_hb_stubs.h"
#include <ksys/cell/subsysid.h>

#ifdef	NUMA_BASE
#include "sys/SN/SN0/bte.h"
#endif

/*
 * Heart beat support:
 * The kernel heat beat support is part of the cellular irix infrastructure
 * to provide failure recovery. The heart beat support is used to detect
 * failures of cells in a cluster. This support will also be used by 
 * failsafe products. The heart beat support is primarily used by the
 * membership services (eg., Array membership services (AMD)) either at
 * the kernel or user level. User level membership service will invoke
 * the heart beat service via the heart_beat driver.
 * The heart beat services are implemented as a set of kernel interfaces.
 * The heart beat subsystem uses a memory location that it polls periodically.
 * It does not know anything about partitions. It knows only about cells.
 * The membership service can use heart beat interfaces to add/delete cells
 * to be polled and register a callback to be notified in case the heart beat
 * detects a failure. It can also change the minimum polling period.
 * A particular cell's polling period can be set so that the heart beat
 * subsystem ignores the cell without declaring it dead for a long time.
 * This might for example be useful if the cell is in the debugger.
 * The interfaces are described in the file sys/heart_beat.h
 *
 * Implementation details:
 * On LEGO the heart beat is done by each cell updating a counter in an
 * uncached/poisoned location. Other cells will get the location via an RPC
 * and poll that location to see if the counter gets updated. If the counter
 * is not updated in a poll period the heart beat system thinks that cell
 * is dead and calls the registered notifier callback function.
 */
 

/*
 * Heart beat info structure. There is an array of these structures for each
 * cell. 
 */
typedef struct hb_info_s {
	cell_t		hb_cell;		/* Cellid  of cell that */
						/* is monitored*/
	hb_state_t	hb_state;		/* State of cell */
	int		hb_prev_counter_val;	/* Previous counter value */	
	int		*hb_counter;		/* Address of hb counter */
	int		*hb_in_debugger;	/* System in debugger pointer */
	int		hb_poll_period;		/* Poll period in ticks */
	int		hb_pending;		/* Number of pending ticks */
						/* before */
						/* checking hb counter */
	int		hb_flags;		/* Flag to indicate a process */
						/* is  waiting */
	sv_t		hb_wait;		/* Signal variable */	
} hb_info_t;


hb_info_t heart_beat_info[MAX_CELLS];

hb_info_t *local_hb_info;	/* Heart beat info for the local cell */
int	hb_min_poll_period;
void (*hb_event_callback)();
lock_t	heart_beat_lock;

/*
 * The variable hb_system_in_debugger_addr is looked when we enter the
 * debugger. So initialize it with a dummy value until its correctly
 * initialized in hb_init. This helps us enter the debugger before
 * heart beat is initialized if we have to.
 */
int	hb_dummy_val;
caddr_t	hb_system_in_debugger_addr = (caddr_t)&hb_dummy_val;

/*
 * Variable is true if system sanity checks are passed.
 */
int		one_sec_sanity_check_passed;
static hb_info_t 	*hb_get_cellinfo(cell_t);
static int 		*hb_alloc_heart_beat_counter(void);
static void 		hb_monitor(void);
static void		__hb_inform_dead_cell(hb_info_t *);
static void 		idbg_dump_hbinfo(void);
static void		dump_cell_hbinfo(hb_info_t *);
static	int		hb_get_counter_val(hb_info_t *);


int	hb_in_debugger_val;

#ifdef	NUMA_BASE
extern int _badaddr_val(volatile void *, int, volatile int *);
#endif

#define	UPDATE_HEART_BEAT_COUNTER(hb_info)	((*(hb_info->hb_counter))++)
#define	CLEAR_IN_DEBUGGER_FLAG(hb_info)	     ((*(hb_info->hb_in_debugger)) = 0)
extern int _badaddr_val(volatile void *, int , volatile int *);
#define SYSTEM_IN_DEBUGGER(hb_info) \
	(_badaddr_val(hb_info->hb_in_debugger, sizeof(int), \
			&hb_in_debugger_val) ? 0  : (hb_in_debugger_val))
#define	HEART_BEAT_LOCK()	(mp_mutex_spinlock(&heart_beat_lock))
#define	HEART_BEAT_UNLOCK(s)	(mp_mutex_spinunlock(&heart_beat_lock, s))


#define	DEFAULT_POLL_PERIOD	1

/* Flag for hb_flags field */
#define	HB_WAIT			1 	/* Process is waiting on hb_wait */

/*
 * hb_get_cellinfo:
 * Get a pointer to the  heart beat info. for a given cell.
 * Heart beat lock should be held by the caller.
 */
static hb_info_t *
hb_get_cellinfo(cell_t cell)
{
	hb_info_t *hb_info = heart_beat_info;
	int	i;

	for (i = 0; i < MAX_CELLS; i++, hb_info++)
		if (hb_info->hb_cell == cell) {
			return hb_info;
		}
	return NULL;
}

/*
 * hb_init:
 * Initialize the heart beat info. structures. Also allocate a heart beat
 * address for the local cell.  At init time the heart beat code knows
 * only about the local cell. New cells will be added later.
 */
void
hb_init()
{
	hb_info_t *hb_info;
	int	i;

	spinlock_init(&heart_beat_lock, "Heart beat lock");
	hb_info = heart_beat_info;
	hb_info->hb_cell = cellid();
	hb_info->hb_counter = hb_alloc_heart_beat_counter();
	hb_system_in_debugger_addr = (caddr_t)(hb_info->hb_counter + 1);
	hb_info->hb_in_debugger = (int *)hb_system_in_debugger_addr;
	hb_info->hb_prev_counter_val = 0;
	hb_info->hb_poll_period = DEFAULT_POLL_PERIOD;
	hb_info->hb_pending = 0;
	hb_info->hb_state = UP;
	local_hb_info = hb_info;	/* Let clock get it */
	hb_min_poll_period	= DEFAULT_MIN_POLL_PERIOD;
	one_sec_sanity_check_passed = TRUE;

	hb_info = &heart_beat_info[1];
    	for (i = 1; i < MAX_CELLS; i++, hb_info++) {
		hb_info->hb_counter = 0;
		hb_info->hb_state = DOWN;
		hb_info->hb_cell = CELL_NONE;

		/*
		 * For starters the counter is initialized to zero and
 		 * the previous counter value is -1. This ensures that
		 * the counter is > prev counter val and the cell is
		 * considered up when it is first added.
 		 */

		hb_info->hb_prev_counter_val = -1;
		hb_info->hb_poll_period = DEFAULT_POLL_PERIOD;
		hb_info->hb_pending = 0;
	}
	timeout(hb_monitor, 0, hb_min_poll_period);
	idbg_addfunc("hb_dump", (void (*)())idbg_dump_hbinfo);

	mesg_handler_register(cell_hb_msg_dispatcher, DHEART_BEAT_SUBSYSID);
}

/*
 * hb_alloc_heart_beat_counter:
 * Allocate a heart beat counter page for the local cell. This page is
 * in mapped uncached so that other cells can access the counter values
 * and get bus errors immediately if the cell where the page is located
 * goes down. Also the page is poisoned to avoid R10K speculation.
 */
static int * 
hb_alloc_heart_beat_counter()
{
#ifdef NUMA_BASE
	caddr_t counter = kvpalloc(1, VM_DIRECT|VM_UNCACHED_PIO, 0);
	caddr_t	tmp_page = kvpalloc(1, VM_DIRECT, 0);
	pgno_t	pfn;
	/* REFERENCED */
	pfd_t *pfd;
	bte_handle_t *bte_handle;

	if (counter) {
		pfn = kvtophyspnum(counter);
		pfd = pfntopfdat(pfn);

		/*
		 * Poison the page so that it is never accessed cached.
		 * also avoids speculative accesses.
		 */
		bte_handle = bte_acquire(btoct(NBPP));

		bte_poison_copy(bte_handle, K1_TO_PHYS(counter),
				K0_TO_PHYS(counter), NBPP, BTE_POISON);
		bte_release(bte_handle);
		/* page_poison(pfd); */
	}
	kvpfree(tmp_page, btoct(NBPP));
#else
	caddr_t counter = kvpalloc(1, VM_DIRECT, 0);
#endif
	return ((int *)counter);
}

/*
 * hb_get_counter_val:
 * Gets the hb counter value. It reads the uncached location and
 * if it cannot read the location (the cell might have been powered
 * down) it returns -1 else returns the old counter value.
 */

int
hb_get_counter_val(hb_info_t       *hb_info)
{

#ifdef	NUMA_BASE
	int	counter_val;

	if (_badaddr_val(hb_info->hb_counter, sizeof(int), &counter_val))
		return -1;
	else	
		return counter_val;
#else
	return (*(hb_info->hb_counter));
#endif
}

/*
 * update_heart_beat:
 * Update the heart beat counter for the local cell. This is called
 * from clock every clock tick. This has to be a low overhead operation.
 */
cell_t debug_bad_cell = CELL_NONE;

void
hb_update_local_heart_beat()
{
	if (local_hb_info) {
		CLEAR_IN_DEBUGGER_FLAG(local_hb_info);
		if (one_sec_sanity_check_passed)
			UPDATE_HEART_BEAT_COUNTER(local_hb_info);
	}
}


/*
 * I_cell_hb_get_counter:
 * RPC service routine to return the address of the counter.
 */ 
/* ARGSUSED */
void 
I_cell_hb_get_counter_address(int **heart_beat_address)
{
	*heart_beat_address = local_hb_info->hb_counter;
}

/*
 * hb_sanity_checks:
 * Routine to do some heuristic checks that determine if a system is really
 * alive. For now it is a dummy routine which is always successfull.
 */  
#define	hb_sanity_checks()	1

/*
 * hb_monitor:
 * Monitor the heart beat of other cells in our set. This is called at 
 * a minimum time interval of hb_min_poll_period ticks. For each cell 
 * the polling period can be a multiple of hb_min_poll_period ticks. It checks 
 * to see if the new counter value is greater than the previous counter value 
 */
static void
hb_monitor()
{
	hb_info_t	*hb_info = heart_beat_info;
	int		current_hb_counter;
	int		i, s;


	s = HEART_BEAT_LOCK();
	for (i = 0; i < MAX_CELLS; i++, hb_info++) {
		if ((hb_info->hb_cell != CELL_NONE) && 
						(hb_info->hb_state == UP)) {
			if (hb_info->hb_pending){
				hb_info->hb_pending--;
				continue;
			}
			hb_info->hb_pending = hb_info->hb_poll_period;
			current_hb_counter = hb_get_counter_val(hb_info);
			if (current_hb_counter <= hb_info->hb_prev_counter_val){
				if (!SYSTEM_IN_DEBUGGER(hb_info))
					__hb_inform_dead_cell(hb_info);
			} else  {
				hb_info->hb_prev_counter_val 
					= current_hb_counter;
			}

			if (hb_info->hb_flags & HB_WAIT) {
				hb_info->hb_flags &= ~HB_WAIT;
				sv_signal(&hb_info->hb_wait);
			}
		}
	}
	timeout(hb_monitor, 0, hb_min_poll_period);
	HEART_BEAT_UNLOCK(s);
}

/*
 * hb_inform_dead_cell:
 * Inform the member ship service that cell has been detected as dead.
 * Heart beat lock is held.
 */
static void
__hb_inform_dead_cell(hb_info_t *hb_info)
{
	hb_info->hb_state = DOWN;
	cms_detected_cell_failure(hb_info->hb_cell);
}

/* EXPORTED INTERFACES */

/*
 * hb_detected_cell_failure:
 * Called by the messaging subsystem if it detects a cell failure.
 */
void
hb_detected_cell_failure(cell_t cell)
{
	hb_info_t *hb_info;
	int	s;

	s = HEART_BEAT_LOCK();

	hb_info = hb_get_cellinfo(cell);

	if (hb_info == NULL)
		return;

	/*
	 * If the heart beat has already detected the failure
 	 * do nothing.
	 */
	if (hb_info->hb_state == DOWN)
		return;

	__hb_inform_dead_cell(hb_info);

	HEART_BEAT_UNLOCK(s);
}
	

/*
 * hb_add_cell:
 * Add a new cell to the set of cells to be polled for heart beats. 
 * It does an RPC to get the cell's  heart beat counter address.
 */
int
hb_add_cell(cell_t cell)
{
	int		i, s;
	service_t 	hb_svc;
	hb_info_t	*hb_info;
	int		error = 0;
	int		*hb_counter_address;


	if (cell == cellid()) {
		return 0;
	}
	SERVICE_MAKE(hb_svc, cell, SVC_HEART_BEAT);

	if (error = 
		invk_cell_hb_get_counter_address(hb_svc, &hb_counter_address)) {
		return error;
	}

	s = HEART_BEAT_LOCK();
	/*
	 * If it has already been added we're done.
 	 */
	hb_info = hb_get_cellinfo(cell);

	if (hb_info == NULL) {
		hb_info = heart_beat_info;
		for (i = 0; i < MAX_CELLS; i++, hb_info++)
			if (hb_info->hb_cell == CELL_NONE)
				break;
	} 

	if (hb_info) {
		hb_info->hb_cell = cell;
		hb_info->hb_state = UP;
		hb_info->hb_prev_counter_val = -1;
		hb_info->hb_counter = hb_counter_address;
		hb_info->hb_in_debugger = ++hb_counter_address;
		hb_info->hb_prev_counter_val = hb_get_counter_val(hb_info);

		/*
		 * Mark the prev counter val one less than the actual counter
 		 * to make the state consistent with UP. This will make the 
		 * monitor wait one poll period before deciding the state.
		 */

		hb_info->hb_prev_counter_val--; 
		error = 0;
	} else  {
		cmn_err(CE_NOTE,
				"Cannot find a free cell, bump MAX_CELLS %d\n", 
								MAX_CELLS);
		error = ENOMEM;
	}

	HEART_BEAT_UNLOCK(s);
	return error;
}


/*
 * hb_remove_cell:
 * Remove a cell from the heart beat polling set.
 */
int
hb_remove_cell(cell_t cell)
{
	int	s = HEART_BEAT_LOCK();
	hb_info_t *hb_info; 
	int	error;

	
	if (cell == cellid()) {
		error = EINVAL;
	} else {
		hb_info = hb_get_cellinfo(cell);
		if (hb_info) {
			hb_info->hb_cell = CELL_NONE;
			error = 0;
		} else {
			error = EINVAL;
		}
	}
	HEART_BEAT_UNLOCK(s);
	return error;
}


/*
 * hb_set_cell_poll_period:
 * Set the heart beat monitoring parameters (poll period) for a given cell.
 */
int
hb_set_cell_poll_period(cell_t cell, int poll_period)
{
	int	s = HEART_BEAT_LOCK();
	int	error = 0;
	hb_info_t *hb_info = hb_get_cellinfo(cell);

	
	if (hb_info != NULL) 
		hb_info->hb_poll_period = poll_period/hb_min_poll_period;
	else 
		error = EINVAL;

	HEART_BEAT_UNLOCK(s);
	return error;
}


/*
 * hb_num_cells_monitored:
 * Return number of cells monitored.
 */
int
hb_num_cells_monitored()
{
	int i;
        hb_info_t *hb_info = heart_beat_info;
	int	num_cells_monitored = 0;
	int	s;

	s = HEART_BEAT_LOCK();
        for (i = 0; i < MAX_CELLS; i++, hb_info++) {
                if (hb_info->hb_cell != CELL_NONE) {
			num_cells_monitored++;
                }
        }
	HEART_BEAT_UNLOCK(s);
	return num_cells_monitored;
}

void
idbg_dump_hbinfo()
{
	int i;
	hb_info_t *hb_info = heart_beat_info;

	for (i = 0; i < MAX_CELLS; i++, hb_info++) {
		if (hb_info->hb_cell != CELL_NONE) {
			dump_cell_hbinfo(hb_info);
		}
	}
}

void
dump_cell_hbinfo(hb_info_t *hb_info)
{
	qprintf("Heart beat info for cell %d\n", hb_info->hb_cell);
	qprintf("Counter address 0x%x  current val %d prev val %d\n", 
					hb_info->hb_counter, 
					hb_get_counter_val(hb_info), 
					hb_info->hb_prev_counter_val);
	qprintf("%s\n", SYSTEM_IN_DEBUGGER(hb_info) 
					? "System in debugger" : "");
	qprintf("Heart poll period %d min poll period %d\n",
			hb_info->hb_poll_period, hb_min_poll_period);
	qprintf("Heart beat state %s\n",
				hb_info->hb_state == UP ? "up" : "down");
}

void
hb_get_connectivity_set(cell_set_t *send_set, cell_set_t *recv_set)
{
        int i;
        hb_info_t *hb_info = heart_beat_info;
        int     s;

	set_init(send_set);
	set_init(recv_set);

        s = HEART_BEAT_LOCK();
        for (i = 0; i < MAX_CELLS; i++, hb_info++) {
                if (hb_info->hb_cell != CELL_NONE) {
                        if (hb_info->hb_state == UP) {
				set_add_member(send_set, hb_info->hb_cell);
				set_add_member(recv_set, hb_info->hb_cell);
			}
                }
        }
        HEART_BEAT_UNLOCK(s);
}
