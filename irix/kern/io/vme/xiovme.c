/*
 * Copyright 1996-1997, Silicon Graphics, Inc.
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

/* Driver to initialize the XIO-VME board */

#ident  "$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/hwgraph.h>
#include <sys/iograph.h>
#include <sys/param.h>
#include <sys/nic.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/pcibr.h>
#include <sys/PCI/bridge.h>

int xiovme_devflag = D_MP;

static nic_vmc_func     	xiovme_vmc;
static pcibr_intr_bits_f	xiovme_intr_bits;

void
xiovme_init(void)
{

#if DEBUG_XIOVME
        cmn_err(CE_CONT, "xiovme_init()\n");
#endif
        nic_vmc_add("030-1213-", xiovme_vmc);
        nic_vmc_add("030-1221-", xiovme_vmc);
}

static void
xiovme_vmc(vertex_hdl_t vhdl)
{
        bridge_t *      bridge;
	unsigned        rev;

#if DEBUG_XIOVME
        cmn_err(CE_CONT, "%v is XIO-VME board\n", vhdl);
#endif

	bridge = (bridge_t *)
		xtalk_piotrans_addr(vhdl, 
				    NULL,
				    0,
				    sizeof(bridge_t), 
				    0);

	/* VME has to live on PCI Bridge RevD or up */
	rev = XWIDGET_REV_NUM(bridge->b_wid_id);
	if (rev < BRIDGE_REV_D) {
		panic("PCI Bridge on XIO-VME board is downrev");
	}

	bridge->b_bus_timeout = 0x103ff;
 
	pcibr_hints_intr_bits(vhdl, xiovme_intr_bits);
        pcibr_hints_fix_rrbs(vhdl);
	pcibr_alloc_all_rrbs(vhdl, 1, 0,0, 0,0, 0,0, 8,0);

				    
}

/*ARGSUSED*/
static unsigned
xiovme_intr_bits(pciio_info_t info,
		 pciio_intr_line_t lines)
{
    /* lines: bitmap of PCI interrupts
     * returns: bitmap of Bridge interrupts
     * xiovme pulls all eight Bridge interrupts
     * out to the universe chip.
     */
    return lines;
}
