/*
 * xbow_test.c : 
 *	Xtalk stress tests via LINC
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
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include <standcfg.h>

#include "pci_config.h"
#include "d_x_test.h"

/* local function protos */
static int init_lincs(struct proc_args *parg_p, struct linc_card *linc);
static void init_linc_bridge(bridge_t *bridge, int port);

/* extern function protos */
int xtalk_probe(xbow_t *xbow, int port);
void init_xbow_credit(xbow_t *xbow, int port, widget_cfg_t *widget, int credits);

int init_linc(int port,bridge_t *bridge, struct linc_card *linc);

void init_bridge_pci(bridge_t *bridge);
void size_bridge_ssram(bridge_t *);
void init_bridge_rrb(bridge_t *);

#define INTERUPTS 1 /* just for now - steve may want this code later */

bool_t
xbow_test(void) 
{

  struct proc_args parg_p;
  struct linc_card linc;

  msg_printf(JUST_DOIT,"xbow_test()\n");

  if (init_lincs(&parg_p,&linc)) {
    msg_printf(JUST_DOIT,"xbow_test: error while initing lincs ...\n");
    return(1);
  }

  msg_printf(JUST_DOIT,"xbow_test() done\n");

  return(0);

}

/*
 * Once basic system has comes up the Xbow has already been initialized
 * and the BaseIO bridge has been found & inited, but other bridges are
 * NOT inited so we have to do this here ...
 */
static int
init_lincs(struct proc_args *parg_p, struct linc_card *linc) {

  xbow_t *xbow = XBOW_K1PTR;
  int port;
  widget_cfg_t *widget;
  int part;

  dprintf(("init_linc_bridges()\n"),DBG_FUNC);

  /* probe ports 9,A,B,C,D,E */
  for (port = BRIDGE_ID-1; port > HEART_ID; port--) {
    dprintf(("init_linc_bridges: probing port=0x%X\n",port),DBG_LOTS);

    /* probe each port for a bridge chip */
    if (xtalk_probe(xbow, port)) {
      widget = (widget_cfg_t *)K1_MAIN_WIDGET(port);
      part = (widget->w_id & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;
      if (part == BRIDGE_WIDGET_PART_NUM) {
	dprintf(("init_linc_bridges: found a bridge at port=0x%X\n",port),
		DBG_ALWS);

	/* XXX should check that is a brige w/ a LINC attached ... */

	/* set to 3 credits ... this was previously set to 5 credits
	   in Chucks code */
	init_xbow_credit(xbow, port, widget, BRIDGE_CREDIT);

	/* init bridge for linc */
	init_linc_bridge((bridge_t *)widget,port);

	/* init both lincs per linc board */
	if(init_linc(port,(bridge_t *)widget,linc))
	  return(1);

      }
    }

  }
  
  /* success */
  return(0);

}

/*
 * try to do here what linc_sys.c: bridge_init() does
 *
 */
static void
init_linc_bridge(bridge_t *bridge, int port) {

#define BRIDGE_CONTROL_INIT_VAL 0x7f0033ff
  bridgereg_t val;
  volatile struct pci_config 	*config_base; /* pci config space on linc */

  /* print out value of Bridge Control Register */
  printf("linc_init_bridge: bridge control register = 0x%X\n",
	 bridge->b_wid_control);
  
  /* set widget ID */
  bridge->b_wid_control &= (0xfffffff0 | port);
  bridge->b_wid_control;	/* inval addr bug war */

  /* print out value of Bridge Control Register */
  printf("linc_init_bridge: set wid, bridge control register = 0x%X\n",
	 bridge->b_wid_control);

  /* print out value of device1 register */
  printf("linc_init_bridge: device 1 reg = 0x%X\n",
	 bridge->b_device[1].reg);

  /*
   * Turn on SWAP_DIRECT in  PCI device 1, 2, 3, 4 registers
   * enable coherent direct map mode & pftch.
   *
   * also, place all devices on the real-time ring so they share
   * priority with the PIO's.
   *
   */
  val = bridge->b_device[1].reg | 
    BRIDGE_DEV_RT | BRIDGE_DEV_COH | BRIDGE_DEV_PREF;
  bridge->b_device[1].reg |= val;
  bridge->b_device[2].reg |= val;
  bridge->b_device[3].reg |= val;
  bridge->b_device[4].reg |= val;

  /*
   * write the bridge device read response registers to allocate 4 buffers each
   * to devices 1-4, all using only virtual device "0".
   */   
  bridge->b_even_resp = 0xaaaa9999;
  bridge->b_odd_resp = 0x99998888;

  /*
   * Turn on DIR_ADD512 & set heart widget id in the Direct Mapping Register
   */
  val = HEART_ID << BRIDGE_DIRMAP_W_ID_SHFT;
  bridge->b_dir_map |= BRIDGE_DIRMAP_ADD512;
  printf("linc_init_bridge: dir map reg = 0x%X\n",
	 bridge->b_dir_map);

  /*
   * Do at least 2 reads from anywhere in PCI space to clear out ping pong buffers
   * required for XT_WR_RQWRP to work - Steve Miller hack
   */
  config_base = (volatile struct pci_config *) &bridge->b_type0_cfg_dev[4];
  val = config_base->vendor_dev_id;
  val = config_base->vendor_dev_id;
  val = config_base->vendor_dev_id;

  /* disable all interrupts and clear all pending interrupts */
  bridge->b_int_enable = 0x0;
  bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;

  /* size bridge ssram - causes 2 bad int target warnings in SABLE */
  dprintf(("calling size_bridge_ssram()\n"),DBG_FUNC);
  size_bridge_ssram(bridge);
  dprintf(("size_bridge_ssram() done\n"),DBG_FUNC);

  /* init rrb */
  init_bridge_rrb(bridge);

#if INTERUPTS
  /*
   * set up interrupt destination register, establish PCI interrupt 0.
   */
  bridge->b_wid_int_upper = HEART_ID << 16;
  printf("linc_init_bridge: bridge int dest upper = 0x%X\n",
	 bridge->b_wid_int_upper);
  bridge->b_wid_int_lower = HEART_XTALK_ISR; /* 0x80 */
  printf("linc_init_bridge: bridge int dest lower = 0x%X\n",
	 bridge->b_wid_int_lower);

  /*
   * correlate interrupts with devices for flushing; each linc has 
   * 1 int but 2 devs, so int-order is not complete (but noty used 
   * in general with linc)
   */
  bridge->b_int_device = 0x00000310;
  bridge->b_int_addr[0].addr = 0x0;
  bridge->b_int_addr[1].addr = 0x0;
  bridge->b_int_addr[2].addr = 0x0;
  bridge->b_int_addr[3].addr = 0x0;
  bridge->b_int_addr[4].addr = 0x0;

  /* enable interrupt-clear packets for PCI INT1-2 */
  bridge->b_int_mode = 0x0006;
  
  /* enable interrupt 0-4 ? , 1-2 */
  bridge->b_int_enable = 0x0000006;
#endif  

  /* interupts for error conditions are OK in prom land */

  /* clear anything hanging about */
  bridge->b_int_rst_stat = BRIDGE_IRR_ALL_CLR;
  
  /* enable bridge error reporting */
  bridge->b_int_enable |=
    (BRIDGE_IRR_CRP_GRP |
     BRIDGE_IRR_RESP_BUF_GRP |
     BRIDGE_IRR_REQ_DSP_GRP |
     BRIDGE_IRR_LLP_GRP |
     BRIDGE_IRR_SSRAM_GRP |
     BRIDGE_IRR_PCI_GRP);
}

