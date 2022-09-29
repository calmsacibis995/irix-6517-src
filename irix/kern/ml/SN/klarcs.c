/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.          	  *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * Support routines for the SN0 implementation of the ARCS standard.
 * Unlike the mid-range and low-end systems, SN0 doesn't keep the ARCS
 * code in an executible PROM.  As a result, once Unix has started
 * most of the PROM executable is overwritten.  To avoid problems,
 * this module patches the SPB transfer vector table so that certain
 * important functions pointers point to the appropriate kernel functions
 * defined in this module.  Not all ARCS functions are supported; if the kernel
 * calls an unsupported function the system will panic.
 */

#ident "$Revision: 1.18 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/arcs/spb.h>
#include <sys/arcs/tvectors.h>

#include <sys/SN/gda.h>
#include <ksys/elsc.h>
#include "sn_private.h"

extern int	_prom_cleared;
extern cpuid_t	master_procid;

extern void	set_lladdr(int);
extern void 	ni_reset_port(void);

extern int 	reboot_on_panic;

/*
 * he_arcs_set_vectors()
 *	set the global _prom_cleared so that arcs_* routines panic the kernel
 */

void
he_arcs_set_vectors(void)
{
	_prom_cleared = 1;
}


/* ARGSUSED */
static void
restart(int mode)
{
	int i;
	extern int panic_level, power_off_flag;

	if (!panic_level)
		mode |= PROMOP_SKIP_DIAGS;

	/*
	 * Everest never got restart to work.  Let's hope we can.  Until
	 * then, we use the C0_LLADDR register (unused by T5 and not cleared
	 * on reset) to tell the prom what mode we want to use.
	 */

	for (i = 0; i < maxnodes; i++)
		REMOTE_HUB_S(COMPACT_TO_NASID_NODEID(i), PROMOP_REG, mode);

	us_delay(1000000);	/* wait for console output to drain */

	if (reboot_on_panic == 2) {
		elsc_t	e;

		elsc_init(&e,get_nasid());
		if (elsc_system_reset(&e) < 0) {
	
			/* Command did not complete correctly.. Force 
			 * a warm reset
			 */
			printf("MSC Reset command failed. Doing warm reset\n");
			us_delay(1000000);	/* wait a sec.. */
			LOCAL_HUB_S(NI_PORT_RESET, NPR_PORTRESET | NPR_LOCALRESET);
		}
		
	} else {

		for (i = 0; ; i++) {
			if (cpuid() == master_procid || i > 0) {
				if (power_off_flag)
					sysctlr_power_off(0);
				else
					ni_reset_port();
			}
			/* if the master didn't do it, everyone elsc tries */
			us_delay(3000000);
		}
	}

#if 0
#ifdef __BTOOL__
#pragma btool_prevent
		/* btooling this can cause a hang due to so much
		 * contention for the one cache line containing the
		 * counter
		 */
	while (1) /* Loop forever */ ;
#pragma btool_unprevent
#else /* !__BTOOL__ */
	while (1) /* Loop forever */ ;
#endif
#endif
}

/*
 * he_arcs_restart()
 *	
 */

void
he_arcs_restart(void)
{
	restart(PROMOP_RESTART);
}


/*
 * he_arcs_reboot()
 *	
 */

void
he_arcs_reboot(void)
{
	restart(PROMOP_REBOOT);
}

#if SABLE
int
he_arcs_write(char *buf, ULONG n, ULONG *cnt)
{
	*cnt = n;
	ducons_write(buf, n);
	return 0;
}
#endif
