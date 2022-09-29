/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: klconfdev.c
 * klconfdev.c - Routines to init the KLCONFIG struct for each
 *              type of board.
 *
 *
 * Runs in MP mode, on all processors. These routines write in their
 * local memory areas.
 */

#include <sys/types.h>
#include <sys/mips_addrspace.h>
#include <sys/graph.h>
#include <sys/nic.h>
#include <sys/SN/addrs.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/xbow.h>
#include <sys/SN/nvram.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/kldiag.h>
#include <sys/PCI/bridge.h>
#include <arcs/cfgdata.h>
#include <libsc.h>
#include <libkl.h>
#include <libsk.h>
#include <libsc.h>
#include <report.h>

#ifndef SABLE
#define LDEBUG		0		/* Set for 1 for debug compilation */
#endif

#ifdef SABLE
/* needed for definitions of NI_NODEID_SHFT, NI_NODEID_MASK */
#include <sys/SN/SN0/klhwinit.h>
#endif

#if defined(LD)
#undef LD
#endif

#define	LD(x)		(*(volatile __int64_t  *) (x))
#define	LW(x)		(*(volatile __int32_t  *) (x))

/* These are defined in different places in the IO6prom and IP27prom. */
extern int get_cpu_irr(void);
extern int get_fpc_irr(void);

#define TBD XXX

extern int kl_sable ;
extern off_t kl_config_info_alloc(nasid_t, int, unsigned int);
extern void init_config_malloc(nasid_t);
/*
 * Local function prototype declarations.
 */
int klconfig_port_update(void);
lboard_t *kl_config_alloc_board(nasid_t);
klinfo_t *kl_config_alloc_component(nasid_t, lboard_t *);
lboard_t *kl_config_add_board(nasid_t, lboard_t *);

int setup_hub_port(pcfg_hub_t *, pcfg_router_t *);
int setup_router_port_to_hub(pcfg_router_t *, pcfg_hub_t *, int);
int setup_router_port_to_router(pcfg_router_t *, pcfg_router_t *, int);

extern int get_hub_nic_info(nic_data_t, char *) ;

#define GET_PCI_VID(_id)	(_id & 0xffff)
#define GET_PCI_DID(_id)	((_id >> 16) & 0xffff)

#if defined (SABLE)

/* Number of and routers in this version of sable */
#define SABLE_ELEMENT_COUNT 2

init_ip27()
{
	pcfg_ent_t	*pcfgp ;
	pcfg_port_t	*pp ;
	int	i, j ;
	__psunsigned_t	hub_base ;


	PCFG(0)->magic	 = PCFG_MAGIC ;
	PCFG(0)->alloc	 = 128 ;
	PCFG(0)->count	 = SABLE_ELEMENT_COUNT ;
	PCFG(0)->gmaster = 0;

	/*
 	 * Need to create a 2 router 2 hub structure for testing.
 	 * Hubs 1,2 Routers 1,2
 	 *
 	 * hub1          ---> router1.port2,
 	 * router1.port3 ---> router2.port5
 	 * router2.port4 ---> hub2
 	 */

	for (i=0; i<SABLE_ELEMENT_COUNT; i++) {
		pcfgp = PCFG_PTR(get_nasid(), i) ;
		if (i%2) {
			pcfgp->hub.type = PCFG_TYPE_HUB ;
			pcfgp->hub.pttn = 1 ;
			pcfgp->hub.nasid = i/2 ;  /* Hub0 and Hub1 */

			hub_base = (__psunsigned_t) REMOTE_HUB(i/2, 0) ;
			PUT_FIELD64((hub_base + NI_STATUS_REV_ID),
				NSRI_NODEID_MASK, 0, i/2) ;

			pcfgp->hub.flags = 0 ;
			pcfgp->hub.membank[0] = 0 ;
			pcfgp->hub.memsize = 0 ;
			pcfgp->hub.port.index = /* i-1 */ 0 ; /* router index */
			pcfgp->hub.port.port = 2 ; /* router phys port */
			pcfgp->hub.nic = 0xBC101 ;
		}
		else {
			/* Index is itself router number */
			/* so each router may have different number
			   depending on the scan order. */
			pcfgp->router.type = PCFG_TYPE_ROUTER ;
			pcfgp->router.flags = 0 ;
			pcfgp->router.nic  = 0xDAA101 ;
			for (j=1; j<7; j++) {
				pp = &pcfgp->router.port[j] ;
				pp->index = -1 ;
				pp->port = -1 ;
			}
		}
	}
	/* Interconnections */
	/* router 1 */
	PCFG_PTR(get_nasid(), 0)->router.port[3].index = 2 ; /* router 2 */
	PCFG_PTR(get_nasid(), 0)->router.port[3].port = 5 ;

	PCFG_PTR(get_nasid(), 0)->router.port[2].index = 1 ; /* hub 0 */
	PCFG_PTR(get_nasid(), 0)->router.port[2].port = -1 ;

	/* router 2 */
	/* When hub 2 becomes active make index = 3 */
/*
	PCFG_PTR(get_nasid(), 2)->router.port[4].index = 3 ;
	PCFG_PTR(get_nasid(), 2)->router.port[4].port = -1 ;
*/

	PCFG_PTR(get_nasid(), 2)->router.port[5].index = 0 ; /* router 0 */
	PCFG_PTR(get_nasid(), 2)->router.port[5].port = 3 ;


	PCFG_PTR(get_nasid(), 2)->router.port[4].index = 3 ; /* hub 1 */
	PCFG_PTR(get_nasid(), 2)->router.port[4].port = -1 ;


	return 1 ;

}
#endif /* SABLE */

lboard_t *
kl_config_alloc_board(nasid_t nasid)
{
	off_t brd_offset;
	lboard_t *brd_ptr;
	int	i;

	if ((brd_offset = kl_config_info_alloc(nasid, BOARD_STRUCT,
					      sizeof(lboard_t))) == -1)
	    return NULL;

	brd_ptr = (lboard_t *)NODE_OFFSET_TO_K1(nasid, brd_offset);

	brd_ptr->struct_type = LOCAL_BOARD;
	brd_ptr->brd_sversion = LBOARD_STRUCT_VERSION ;
	brd_ptr->brd_brevision = 0 ; /* XXX */
	brd_ptr->brd_promver = 1 ;
	brd_ptr->brd_flags = ENABLE_BOARD ;
	brd_ptr->brd_numcompts = 0 ;
	brd_ptr->brd_next = 0;
	brd_ptr->brd_type = KLTYPE_UNKNOWN;
	brd_ptr->brd_module = -2 ;	/* These will be numbered later */
	brd_ptr->brd_nasid = nasid ;
	brd_ptr->brd_partition = 1 ;	/*** FIXME ****/
	brd_ptr->brd_nic = (nic_t) -1 ;	/*** FIXME ****/

	/* XXX what should these values be at init */

	brd_ptr->brd_diagval = KLDIAG_PASSED;
	brd_ptr->brd_diagparm = 0;
	brd_ptr->brd_inventory = 0;

	brd_ptr->brd_parent = NULL ;
	brd_ptr->brd_graph_link = GRAPH_VERTEX_NONE ;

	for (i=0; i < MAX_COMPTS_PER_BRD; i++)
	    brd_ptr->brd_compts[i] = 0;

	return (brd_ptr);
}

klinfo_t *
kl_config_alloc_component(nasid_t nasid, lboard_t *brd_ptr)
{
	off_t comp_offset;
	klinfo_t *comp_ptr;

	if ((comp_offset = kl_config_info_alloc(nasid, COMPONENT_STRUCT,
					      sizeof(klcomp_t))) == -1)
	    return NULL;

	brd_ptr->brd_compts[brd_ptr->brd_numcompts] = comp_offset;
	brd_ptr->brd_numcompts += 1 ; /* one more component found */

	comp_ptr = (klinfo_t *)NODE_OFFSET_TO_K1(nasid, comp_offset);

	comp_ptr->struct_type              = KLTYPE_UNKNOWN;
	comp_ptr->struct_version           = 1;
	comp_ptr->flags                    = KLINFO_ENABLE; /* XXX */
	comp_ptr->revision                 = ~0;
	comp_ptr->diagval 		   = KLDIAG_PASSED;
	comp_ptr->diagparm 		   = 0;
	comp_ptr->inventory 		   = 0;
	comp_ptr->nic                      = (nic_t) -1 ;
	comp_ptr->physid 		   = ~0;
	comp_ptr->virtid 		   = ~0;
#if 0
	comp_ptr->widid 		   = 0 ;           /* XXX */
	comp_ptr->nasid 		   = ~0 ;          /* XXX */
	comp_ptr->arcs_compt 		   = NULL ;        /* XXX */
	comp_ptr->errinfo    		   = NULL ;        /* XXX */
#endif

	return (comp_ptr);
}


/*
 * Add the board given by lbptr to the end of the list
 * on that node.
 */

lboard_t *
kl_config_add_board(nasid_t nasid, lboard_t *lbptr)
{
	off_t	brd_offset;
	lboard_t *temp1 ;

	brd_offset = K0_TO_NODE_OFFSET(lbptr);
	if (KL_CONFIG_INFO_OFFSET(nasid) == 0) {
	    /* First board on this node... add it to header */
		KL_CONFIG_INFO_SET_OFFSET(nasid, brd_offset);
		return lbptr;
	}

	temp1 = (lboard_t *)KL_CONFIG_INFO(nasid);
	while (KLCF_NEXT(temp1))
		temp1 = KLCF_NEXT(temp1);

	KL_CONFIG_BOARD_SET_NEXT(temp1, brd_offset);

	return lbptr;
}




