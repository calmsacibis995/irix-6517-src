/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(SABLE) && defined(FAKE_PROM)

/*
 * This file is only used for sable simulation.  When the IO6 prom is run
 * from the IP27 prom (or from another IO6 prom, none of this code is
 * necessary.
 */

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h>
#include <libsk.h>
#include <sys/SN/launch.h>
#include <sys/SN/promcfg.h>
#include "io6prom_private.h"
#include <libkl.h>
volatile int sable_mastercounter = 0;
volatile int sable_launchslaves = 0;

extern int ip27prom_simulate(void);
extern int ip27prom_epilogue(void);
extern void init_other_local_dir();
extern int init_ip27(void);


void
ip27_slaveloop()
{
	while (!sable_launchslaves)
		;

	/* Go into the IP27 prom's slave loop. */
	LAUNCH_LOOP();
}

void
more_ip27prom_init()
{
    int i;

    /* If the IP27prom launches the IO6prom on all CPUs and we dont have
     * a real IP27prom doing this, then this piece of code simulates
     * everything that a IP27prom would have done and launches the
     * IO6prom on all CPUs. We dont need it now since IO6prom is
     * launched UP by IP27prom and the slave launch loop is in the
     * PROM itself. 
     */
    /* All CPUs in the system are running this code. */

    ip27prom_simulate() ;
    /* 
     * Call init_klcfg() by each Local Master. 
     * Inits KLPROMDIR data structures on each node.
     */
    ip27prom_epilogue() ;

    putchar('G') ;

    /* Set up a 2 Hub 2 Router config as the IP27prom would have done. */
    /* Note that in the actual PROM the same table is available on all nodes
     * but we actually use only the one on the global master in the IO6prom.
     */

    init_ip27() ;
    init_other_local_dir();

    for (i = 0; i < PCFG(get_nasid())->count; i++) {
        pcfg_hub_t *hub_cf = &PCFG_PTR(get_nasid(), i)->hub;
	switch (hub_cf->type) {
	case PCFG_TYPE_HUB:
	    init_klcfg(hub_cf->nasid);
	    break;
        default:
	    break;
        }
    }
	
    for (i = 0; i < PCFG(get_nasid())->count; i++) {
	int j;
        pcfg_hub_t *hub_cf = &PCFG_PTR(get_nasid(), i)->hub;
	pcfg_router_t *rou_cf = &PCFG_PTR(get_nasid(), i)->router;

	switch (hub_cf->type) {
	case PCFG_TYPE_HUB:
	    init_klcfg_hw(hub_cf->nasid);
	    break;

        case PCFG_TYPE_ROUTER:
	    for (j = 0; j < MAX_ROUTER_PORTS; j++) {
	        if (rou_cf->port[j].index != PCFG_INDEX_INVALID) {
		    hub_cf = &PCFG_PTR(get_nasid(),
				       rou_cf->port[j].index)->hub;
		    if (hub_cf->type != PCFG_TYPE_HUB) continue;
		    /* check for error return */
		    init_klcfg_routerb(hub_cf->nasid, rou_cf, 
				       0, KLTYPE_ROUTER2);
		}	
	    }
	    break;

        default:
	    break;
	}
    }
    
    klconfig_discovery_update();

    /*
     * Now that the directories are set up, put the slave processor(s)
     * back in the IP27prom slave loop.
     */
    sable_launchslaves = 1;

}

#endif /* SABLE && FAKE_PROM */

