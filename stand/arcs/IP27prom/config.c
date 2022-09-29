/***********************************************************************\
 *	File:		config.c					*
 * 									*
 *	This file contains the routines which do the first 		*
 *	initialization of the evcfginfo data structure and		*
 *	which subsequently read in the hardware inventory and 		*
 *	device enable data from the EPROM.				*
 *									*
 \***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/ktime.h>
#include <sys/mips_addrspace.h>
#include <sys/SN/gda.h>
/*#include <sys/SN/intr.h>*/
#include <sys/SN/nvram.h>
#include <sys/SN/SN0/IP27.h>
#include <sys/SN/addrs.h>
#include <libkl.h>
#include "pod.h"
#include "ip27prom.h"
#include "libasm.h"

#define INV_READ(_b, _f) \
get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + (_f))
#define INV_READU(_b, _u, _f) \
get_nvreg(NVOFF_INVENTORY + ((_b) * INV_SIZE) + \
	  INV_UNITOFF + ((_u) * INV_UNITSIZE) + (_f))


extern void dump_errors();
extern void dump_mem_error(int);
extern int  initCPUSpeed(void);


/* any_io_boards()
 *	returns 1 if there's at least one IO6 board, else returns 0
 */
uint
any_io_boards(void)
{
	printf("fixme: any_io_boards\n");
	return 0;
}

/*
 * init_cfginfo()
 *	Initializes the parameters in the klcfginfo to rational
 *	default values.
 */
void
init_cfginfo(void)
{
	printf("fixme: init_cfginfo\n");
}

/*
 * set_timeouts()
 * 	Goes through all of the boards in the system and ensures that
 *	the various timeout registers are set to the correct value.
 */
void
set_timeouts(void)
{
	printf("fixme: set_timeouts\n");
}


/*
 * init_inventory()
 *	Reads the NVRAM and fills in the inventory and virtual
 *	adapter fields for all of the boards in the system. This
 * 	also sets up board enable  value. This needs to be called
 *	before the diagnostic and initialization routines for
 *	specific boards are invoked.
 */

int
init_inventory(uint nv_valid)
{
    uint	slot;		/* Slot containing board being inited */
    uint	unit;		/* Unit number for iterating */
    jmp_buf	fault_buf;	/* Buffer for jumping if a fault occurs */
    void	*old_buf;	/* Previous fault buffer value */
    uint	mult_exc = 0;	/* Flag for multiple exceptions */

    /* Set up an exception handler.  If something goes wrong while
     * we're trying to set up the inventory, we'll jump back to here
     * and clean up after ourselves.  We do this by making the nvram
     * look invalid.
     */
    if (setfault(fault_buf, &old_buf)) {
	if (mult_exc) {
	    printf("*** Multiple exceptions while initializing inventory.\n");
	    restorefault(old_buf);
	    return -1;
	} else
	    mult_exc = 1;

	printf("*** Error occurred while reading NVRAM -- using defaults.\n");
	nv_valid = 0;
    }

    /* Switch off nv_valid if it the valid inventory flag isn't set */
    if (nv_valid)
	if (get_nvreg(NVOFF_INVENT_VALID) != INVENT_VALID)
	    nv_valid = 0;

    printf("fixme: init_inventory\n");

    /* Make sure that the previous fault handler is restored */
    restorefault(old_buf);
    return 0;
}

/*
 * configure_cpus()
 * 	Reads the IP0 register to determine which processors have
 * 	sent us the "alive" interrrupt and prints a string indicating
 *	that the processors are alive.  Processors which don't
 *	respond get their enable entry switched off in the evcfginfo
 *	structure.
 */

void
configure_cpus(void)
{
	printf("fixme: configure_cpus\n");
}

/*
 * init_mpconf()
 * 	Sends out an interrupt instructing the slave processors
 *	to initialize their MPCONF entries.  Initializes the
 *	master CPU's MPCONF.
 */

void
init_mpconf(void)
{
    uint ppid, vpid;		/* Virtual and physical processor ID's */
    uint slot, proc;
    uint scachesz;
#if 0
    mpconf_t *mpconf;		/* work around ragnarok compiler bug */

    /* XXX fixme - We don't have MPCONF at all on SN0. */

    /* Zero the MPCONF region */
    for (vpid = 0; vpid < MAXCPUS; vpid++) {
	mpconf = &MPCONF[vpid];
	mpconf->mpconf_magic = 0;
	mpconf->phys_id = 0;
	mpconf->virt_id = 0;
    }

    printf("fixme: init_mpconf: send interrupt to slaves to init\n");

    mpconf = &MPCONF[vpid];
    mpconf->phys_id = ppid;
    mpconf->virt_id = vpid;
    mpconf->errno = 0;
    mpconf->proc_type = EV_CPU_R10000;
    mpconf->launch = 0;
    mpconf->rendezvous = 0;
    mpconf->bevnormal = 0;
    mpconf->scache_size = scachesz;
    mpconf->stack = 0;
    mpconf->lnch_parm = mpconf->rndv_parm = 0;
    mpconf->mpconf_magic = MPCONF_MAGIC;
    mpconf->pr_id = get_cop0(C0_PRID);

#endif
}
