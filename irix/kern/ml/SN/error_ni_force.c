/**************************************************************************
 *									  *
 *		 Copyright (C) 1992-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#if defined (SN)  && defined (FORCE_ERRORS)
/*
 * File: error_force.c
 */
#include <sys/types.h>
#include <sys/systm.h>
#include "sn_private.h"
#include "error_private.h"
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/SN/module.h>
#include <sys/SN/SN0/slotnum.h>
#include <sys/SN/addrs.h>
#include <sys/debug.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN1/snacioregs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/xbow.h>

#define EF_LINK_RESET_OFFSET 0x30

/*
 * Function   : error_force_rtr_link_down
 *
 * Purpose    : Disable a router link
 * Returns    : 0 - succes, -1 - failure
 */ 

int
error_force_rtr_link_down(cnodeid_t cnode,net_vec_t vec,int port)
{
	router_reg_t	rr_diag_parms;
	int		rc = 0;
	
	EF_PRINT(("Forcing Router link %d down at 0x%x from cnode %d\n",
		     port,vec,cnode));
	
	/* Make sure that we are running on a cpu on the current node */
	if (cnodeid() != cnode) {
		return(-1);
	}

	/* Disable the router port by reading the router
	 * diag parameters and setting the disable bit
	 */
	rc = vector_read(vec,0,RR_DIAG_PARMS,&rr_diag_parms);
	if (rc < 0) {
		return(rc);
	}
	rr_diag_parms |= RDPARM_DISABLE(port);
	return vector_write(vec,0,RR_DIAG_PARMS,rr_diag_parms);
}

/*
 * Function   : error_force_link_reset
 *
 * Purpose    : To force a link reset. Ona O2K We reset the link. On a
 * 		Speedo we reset the entire hub.
 * Returns    : 0 - success, -1 - failure
 */ 

int
error_force_link_reset(ni_error_params_t *params, rval_t *rvp) 
{
  xb_linkregs_t *link_regs = NULL;
  __int64_t ii_ilcsr = 0;
  bridge_t *bridge;
  EF_PRINT(("Made it to ef_link_reset: Node %lld, Port 0x%llx\n",
	       params->node, params->port));

  if (!XBOW_WIDGET_IS_VALID(params->port)) {
    printf("Invalid port passed to ef_link_reset: 0x%llx\n",params->port);
    rvp->r_val1 = -1;
    return -1;	
  }	
  
  if (!error_force_nodeValid(params->node)) {
    printf("Invalid node passed to ef_link_reset: 0x%lld\n",params->node);
    rvp->r_val1 = -1;
    return -1;	
  }

  
  /* So by now we know we have a legit node and port, it is time to
     reset! */
  if (SN00) {
    ii_ilcsr = REMOTE_HUB_L(params->node,II_ILCSR);
    EF_PRINT(("Read ILCSR, Addr:0x%llx, Val:0x%llx\n",II_ILCSR,ii_ilcsr));
    /* Flip the reset bit */
    ii_ilcsr |= (1 << 8);
    /* Write it back */
    EF_PRINT(("Writing back Val:0x%llx\n",ii_ilcsr));
    REMOTE_HUB_S(params->node, II_ILCSR, ii_ilcsr);
  }  else {
    /* First we get the link base */
    link_regs = XBOW_LINKREGS_PTR(params->node,params->port);
    EF_PRINT(("Link Base 0x%llx, writing to 0x%x\n",link_regs,
	       &link_regs->link_reset));

    /* Now we write any value to the link_reset register since that will
       issue the reset */
    link_regs->link_reset = 0xb00;
  }
  EF_PRINT(("Link has been Reset\n"));

  bridge =(bridge_t*)(NODE_SWIN_BASE(params->node, params->port));

  if (bridge->b_wid_control & 0xf) {
    EF_PRINT(("Reset is confirmed\n"));
  } else {
    EF_PRINT(("Reset did not occur\n"));
  }
  
  return 0;
}

#endif /* SN0 && FORCE_ERRORS */

