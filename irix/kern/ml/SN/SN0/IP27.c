/**************************************************************************
 *                                                                        *
 *  Copyright (C) 1996 Silicon Graphics, Inc.                             *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * SN0.c
 *	Support for  SN0 machines
 */
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dmamap.h>
#include <sys/hwgraph.h>
#include <sys/iobus.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/pio.h>
#include <sys/sema.h>
#include <sys/systm.h>
#include <sys/xtalk/xwidget.h>
#include "sn0_private.h"
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/SN/klconfig.h>


xwidgetnum_t
hub_widget_id(nasid_t nasid)
{
	hubii_wcr_t	ii_wcr;	/* the control status register */
		
	ii_wcr.wcr_reg_value = REMOTE_HUB_L(nasid,IIO_WCR);
		
	return ii_wcr.wcr_fields_s.wcr_widget_id;
}

/*
 * get_nasid() returns the physical node id number of the caller.
 */
nasid_t
get_nasid(void)
{
	return (nasid_t)((LOCAL_HUB_L(NI_STATUS_REV_ID) & NSRI_NODEID_MASK)
	       >> NSRI_NODEID_SHFT);
}

int
get_slice(void)
{
	return LOCAL_HUB_L(PI_CPU_NUM);
}

int
is_fine_dirmode(void)
{
	return (((LOCAL_HUB_L(NI_STATUS_REV_ID) & NSRI_REGIONSIZE_MASK)
		>> NSRI_REGIONSIZE_SHFT) & REGIONSIZE_FINE);
}



hubreg_t
get_hub_chiprev(nasid_t nasid)
{
	return ((REMOTE_HUB_L(nasid, NI_STATUS_REV_ID) & NSRI_REV_MASK)
		                                         >> NSRI_REV_SHFT);
}




int
verify_snchip_rev(void)
{
	int hub_chip_rev;
	int i;
	static int min_hub_rev = 0;
	nasid_t nasid;
	static int first_time = 1;

        
	if (first_time) {
	    for (i = 0; i < maxnodes; i++) {	
		nasid = COMPACT_TO_NASID_NODEID(i);
		hub_chip_rev = get_hub_chiprev(nasid);

		if ((hub_chip_rev < min_hub_rev) || (i == 0))
		    min_hub_rev = hub_chip_rev;
	    }

	
	    first_time = 0;
	    if ((min_hub_rev < HUB_REV_2_1)) {
#ifdef DEBUG
		cmn_err(CE_CONT, "NOTICE: Running with some <= Hub 2.0 chips.\n");
#endif
	    } else {
#ifdef DEBUG
		cmn_err(CE_CONT, "NOTICE: Running with all >= Hub 2.1 chips.\n");
#endif
	    }
	}

	return min_hub_rev;
	
}

#ifdef SN0_USE_POISON_BITS
int
hub_bte_poison_ok(void)
{
	int min_hub_rev, hub_chip_rev;
	int i;
	nasid_t nasid;

        
	for (i = 0; i < maxnodes; i++) {	
		nasid = COMPACT_TO_NASID_NODEID(i);
		hub_chip_rev = get_hub_chiprev(nasid);

		if ((hub_chip_rev < min_hub_rev) || (i == 0)) {
                        min_hub_rev = hub_chip_rev;
                }
	}
        
        /*
         * We can use the bte and posionous bits only
         * if *all* hubs are rev >= HUB_REV_2_3
         */


        if (min_hub_rev >= HUB_REV_2_3) {
                return (1);
        } else {
                return (0);
        }
}
#endif /* SN0_USE_POISON_BITS */
                

void
ni_reset_port(void)
{
	LOCAL_HUB_S(NI_PORT_RESET, NPR_PORTRESET | NPR_LOCALRESET);
}


#define	SGI2100_MAXNODES	4			
#define	SGI2100_PIMM_ID		"Name:IP31PIMMSG2100"

int
is_sgi2100_PIMM(cnodeid_t cnode)
{
	vertex_hdl_t v;
	char         *p;
        extern char *nic_hub_vertex_info(vertex_hdl_t);


	if ((v = cnodeid_to_vertex(cnode)) == GRAPH_VERTEX_NONE)
		return 0;

	p = nic_hub_vertex_info(v);

	return strstr(p,SGI2100_PIMM_ID) != NULL ? 1 : 0;
}

void
sgi2100_alert(cnodeid_t cnode)
{
	vertex_hdl_t v;
	char         name[MAXDEVNAME];

	if ((v = cnodeid_to_vertex(cnode)) == GRAPH_VERTEX_NONE)
		return;

	vertex_to_name(v,name,MAXDEVNAME);

	cmn_err(CE_ALERT,"Unsupported sgi2100 node board in %s\n",name);
}


void
cpu_confchk()
{

	cnodeid_t cnode;
	int	  sgi2100_node_boards = 0;


	for (cnode = 0 ; cnode < maxnodes ; cnode++) 
		if (is_sgi2100_PIMM(cnode))
			sgi2100_node_boards++;

	if (sgi2100_node_boards) {

		if (maxnodes > SGI2100_MAXNODES) { 

			for (cnode = 0 ; cnode < maxnodes ; cnode++) 
				if (is_sgi2100_PIMM(cnode)) 
					sgi2100_alert(cnode);

			cmn_err(CE_PANIC,
			"UNSUPPORTED CONFIGURATION - Contact your Service Provider\n");
		}
	}
}

