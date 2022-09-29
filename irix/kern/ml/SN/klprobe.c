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

#ident	"$Revision: 1.5 $"

#include <sys/types.h>
#include <sys/reg.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/iotlb.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/runq.h>		/* setmustrun	*/
#include <sys/var.h>
#include <sys/atomic_ops.h>


/*
 * Badaddr probing can be initiated from any cpu. A probe info structure is
 * added to the list and  the process is migrated to the bootmaster.
 * A mutex probe_sema is used to protect the shared data structure.
 *
 * Error interrupts will be masked and those error interrupts caused by probe
 * will be cleared. Other error interrupts can only be seen after the probe.
 */

typedef enum {
	PROBE_BADADDR, PROBE_WBADADDR, PROBE_BADADDR_VAL, PROBE_WBADADDR_VAL
}probe_type_t;

typedef struct {
	probe_type_t	type;
	int		len;
	__psunsigned_t	value;
	volatile void	*addr;
	volatile void	*ptr;
} probe_info_t;

static int probe_common(probe_info_t *);
mutex_t		probe_sema;


/*
 * Called from buserror_intr() in trap.c, or ecc_cleanup() in ml/error.c
 * Unlike previous platforms, we will not clear the buserror since we get here
 * for hardware failures and always panic.
 * As a matter of fact, badaddr() will be done with interrupt masked and
 * T5 would not even see this interrupt and would not call this function.
 * The error register in each ASIC and the hub errintpend register
 * be checked/cleared by is_buserr() if probing.
 */
/* ARGSUSED */
int
dobuserr(eframe_t *ep, inst_t *epc, uint flag)
{
	caddr_t fault_addr;

	if (flag == 1)          /* 0: kernel  1: nofault  2: user */
		return 0;

	fault_addr = (caddr_t)ldst_addr(ep);

	cmn_err(CE_CONT|CE_CPUID, "Bus Error at %s address %x\n",
		(USERMODE(ep->ef_sr) ? "User" : "Sys"), fault_addr);

	cmn_err(CE_PANIC, "Bus Error\n");
	/* NOTREACHED */
}


int
badaddr(volatile void* addr, int len)
{
	probe_info_t	pi;

	pi.type  = PROBE_BADADDR;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = &pi.value;

	return probe_common(&pi);

}

int
badaddr_val(volatile void *addr, int len, volatile void *ptr)
{
	probe_info_t	pi;

	pi.type  = PROBE_BADADDR_VAL;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = (void *)ptr;
	return probe_common(&pi);
}

int
wbadaddr(volatile void *addr, int len)
{
	probe_info_t	pi;

	pi.type  = PROBE_WBADADDR;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = &pi.value;
	pi.value = 0;
	return probe_common(&pi);
}

int
wbadaddr_val(volatile void *addr, int len, volatile void *value)
{
	probe_info_t	pi;

	pi.type  = PROBE_WBADADDR;
	pi.addr  = addr;
	pi.len   = len;
	pi.ptr   = (void *)value;
	return probe_common(&pi);
}

/*
 * Check if probing caused any undesired error.
 */
/* XXX - Probably need to check hub registers. */
/*ARGSUSED*/
int
is_buserror(volatile void *addr)
{
	return 0;
}


/*
 * This function actually does the probe operation and checks for buserror.
 * It is equivalent to badaddr et. al. for previous platforms.
 */
extern int	_badaddr_val(volatile void *, int, volatile int *);
extern int	_wbadaddr_val(volatile void *, int, volatile int *);

int
probe_common(probe_info_t *pi)
{
	ulong	sr;
	int	result;

	/*
	 * Check if address is in the range that could take a probing.
	 */

	mutex_lock(&probe_sema,PRIBIO);

	sr = getsr();
	setsr(SR_KADDR); /* Desired sr status for kernel */

	ASSERT((pi->len == 1) || (pi->len == 2) || (pi->len == 4) ||
				(pi->len == 8));

	if (pi->type == PROBE_WBADADDR || pi->type == PROBE_WBADADDR_VAL) {
		result = _wbadaddr_val(pi->addr, pi->len, pi->ptr);
	} else {
		result = _badaddr_val(pi->addr, pi->len, pi->ptr);
	}

	/* error state will be cleared if caused by probe */
	result |= is_buserror(pi->addr);
	setsr(sr);

	mutex_unlock(&probe_sema);
	return result;
}


