#ifdef ULI

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* pseudo device for VME user level interrupts. This provides a
 * non-volatile interrupt handler (as opposed to a process-based
 * handler which goes away when the process does). We want a
 * persistent interrupt handler since there are no interrupt teardown
 * functions built into the kernel to disconnect things when a ULI
 * process exits.
 */
#include <sys/types.h>
#include <sys/sema.h>
#include <ksys/uli.h>
#include <sys/edt.h>
#include <sys/cmn_err.h>
#include <sys/vmereg.h>

int vmeulidevflag;

extern lock_t ivec_mutex;
extern int vmeuli_inited;
extern int (*vmeuliintrp)(int);

int
vmeuliintr(int ulinum)
{
    extern void frs_handle_uli(void);

    /* call up to the specified ULI, if any */
    if (ulinum >= 0 && ulinum < MAX_ULIS) {
	uli_callup(ulinum);

	if (private.p_frs_flags)
		frs_handle_uli();
    } else
	cmn_err(CE_WARN,"vmeuli stray intr\n");

    return(0);
}

void
vmeuliedtinit(struct edt *e)
{
    vme_intrs_t *intrs = e->e_bus_info;
    
    spinlock_init(&ivec_mutex, "vmeuli");

    /* since this module isn't necessarily linked into the kernel,
     * we can't have other areas of the kernel directly accessing
     * functions local to this file. One solution to this problem
     * would be to have stubs linked in when this module isn't
     * present, but I prefer to have a function pointer stored
     * elsewhere which is only set if this module exists, and is 0
     * otherwise. This way with a single pointer we can tell if the
     * module is present and if so where the interrupt function is.
     */
    vmeuliintrp = vmeuliintr;

    /* running at boot time: don't need lock */
    if (intrs->v_vec)
	vmeuli_ivec_reset(e->e_adap, intrs->v_vec, -1);
    vmeuli_inited = 1;
}

#endif
