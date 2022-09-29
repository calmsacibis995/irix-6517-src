/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident  "$Revision: 1.150 $"

/*
 * iodiscover.c - Hardware discovery and initing KLCONFIG.
 * For power-on, the discovery is called by the global master
 * cpu for each HUB with the hub_base as parameter.
 */

#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <libsc.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/xbow.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/ioc3.h>
#include <sys/tpureg.h>
#include <sys/gsnreg.h>
#include <promgraph.h>
#include <prom_msgs.h>
#include <sys/SN/kldiag.h>
#include <ksys/elsc.h>
#include <sys/SN/nvram.h>

#include "iodiscover.h"

/*
 * XXX need to add stuff to read NICs.
 */

extern int kl_sable;
extern nasid_t master_nasid;
extern void hubii_wcr_credits(nasid_t, int);

static lboard_t * xbow_discover(volatile __psunsigned_t, int, lboard_t *) ;
lboard_t * bridge_discover(volatile __psunsigned_t, int) ;
void pci_discover(volatile __psunsigned_t, lboard_t *, int, klpci_device_t *) ;
lboard_t * gfx_widget_discover(volatile __psunsigned_t, int, int);
lboard_t * xg_widget_discover(volatile __psunsigned_t, int, int);
void init_tpu_widget_discover(__psunsigned_t) ;
lboard_t * tpu_widget_discover(volatile __psunsigned_t, int);
void init_gsn_widget_discover(__psunsigned_t) ;
lboard_t * gsn_widget_discover(volatile __psunsigned_t, int, int);
void update_gsn_nic_info(volatile __psunsigned_t, lboard_t *, int, int);


static void early_ioc3_nvram_init(__psunsigned_t, __psunsigned_t) ;
static void
find_inactive_links(__psunsigned_t xbow_base);

#ifdef SABLE
#define XBOW_WIDGET_PART_NUM 0xc111
#endif

int
io_discover(lboard_t *ip27_brd_ptr, int diag_mode)
{
	volatile __psunsigned_t wid_base ;
	volatile reg32_t	wid_id ;
	reg32_t			xwidget_part_num;
	lboard_t 		*widget_brd_ptr = NULL ;
	jmp_buf			new_buf;
	void			*old_buf;
	char			slot[4];

        printf("Discovering local IO ......................\t\t") ;
	DM_PRINTF(("\n")) ;

	wid_base = SN0_WIDGET_BASE(ip27_brd_ptr->brd_nasid, 0);

        if (setfault(new_buf, &old_buf)) {
		printf("FAIL\n") ;
		get_slotname(get_my_slotid(), slot);
		printf("*** Exception while reading WIDGETID register by cpu "
                      "in SLOT %s.\n", slot);
		printf("*** Returning without doing I/O discovery\n");
		restorefault(old_buf);
		return 0;
	}

	wid_id = LD32(wid_base + WIDGET_ID);
	xwidget_part_num = XWIDGET_PART_NUM(wid_id);

	restorefault(old_buf);

	switch (xwidget_part_num) {
	case BRIDGE_WIDGET_PART_NUM:
		/*
		 * If the bridge is directly connected to the hub
		 * the we can use any widget number that we want,
		 * we try and avoid 0 as it has problems with error handling
		 * We need to use same wid number as discover_hub_console
		 */
		wid_base = SN0_WIDGET_BASE(ip27_brd_ptr->brd_nasid, 8);
		wid_id = LD32(wid_base + WIDGET_ID);
		widget_brd_ptr = bridge_discover(wid_base, diag_mode) ;
                break ;

	case XBOW_WIDGET_PART_NUM:
		widget_brd_ptr = xbow_discover(wid_base, diag_mode, ip27_brd_ptr) ;
		break ;

	case HUB_WIDGET_PART_NUM:
                /* Find xbow port num and add to graph */
		/* XXX if this hub is headless, we may want to 
		   do hubii init for it here. */
                break ;

	case XG_WIDGET_PART_NUM:
		/* xg_widget_discover calls gfx_discover if it is GFX */
		widget_brd_ptr = xg_widget_discover(wid_base,(int)xwidget_part_num, diag_mode) ;
		break;

	case HQ4_WIDGET_PART_NUM:
		/* XXX can this ever happen?  GFX with *NO* XBOW???? */
		/* Do an early init of the GFX widget */
                /* Just add this to KLCFG */
		widget_brd_ptr = gfx_widget_discover(wid_base,(int)xwidget_part_num, diag_mode) ;
                break ;

	default:
                DM_PRINTF(("New Widget %x connected to HUB.\n", wid_id));
		return 0 ;

	}

	/*
 	 * if discovery done, copy some of the info in the 'parent'
	 * ip27 board to this board.
 	 */
	if (widget_brd_ptr) {
		widget_brd_ptr->brd_module = ip27_brd_ptr->brd_module ;
		widget_brd_ptr->brd_partition = ip27_brd_ptr->brd_partition ;
		widget_brd_ptr->brd_parent = ip27_brd_ptr ;
	}
	
	printf("DONE\n") ;

	return 1 ;
}


/*
 * xbow_discover
 * Discover all the stuff that is connected to this XBOW.
 */

lboard_t *
xbow_discover(volatile __psunsigned_t xbow_base, int diag_mode,
	      lboard_t *node_lbptr)
{
	int 		link = 0;
	reg32_t		wid_id;
	volatile __psunsigned_t	wid_base;
	int		wid_type;
	lboard_t 	*xbow_lbptr, *widget_lbptr = NULL ;
	nasid_t		nasid = NASID_GET(xbow_base) ;
	int hub_link = hub_xbow_link(nasid);
	int slave = 0;

	/* Init the Midplane's KLCFG. Read in any XBOW specific
	 * info like NICs in the init_klcfg_xbow routine.
 	 */
	xbow_lbptr = init_klcfg_midplane(xbow_base, node_lbptr);
	if (xbow_get_master_hub(xbow_base) != hub_link) {
		add_klcfg_xbow_port(xbow_lbptr, hub_link, 
				    HUB_WIDGET_PART_NUM, widget_lbptr);
		xbow_lbptr->brd_flags |= DUPLICATE_BOARD;
		if (nasid != get_nasid()) {
			lboard_t *newbrd;
			nasid_t master;
			
			master = xbow_get_link_nasid(xbow_base,
						     xbow_get_master_hub(xbow_base));
			if (master == INVALID_NASID) {
				return NULL ;
			}
			newbrd = find_lboard((lboard_t *)KL_CONFIG_INFO(master),
					     KLTYPE_MIDPLANE8);
			if (newbrd == NULL) {
				return NULL ;
			}
			add_klcfg_xbow_port(newbrd, hub_link,
					    HUB_WIDGET_PART_NUM, node_lbptr);
		}
		/* 
		 * PV 691804 - VisCons boards hang with the
		 * higher HUBII_XBOW_REV2_CREDIT value.  Set hub's
		 * credits back to the lower value.  Since each
		 * xbow can be connected to more than one hub,
		 * make sure the adjustment happens for all hubs
		 * connected to the xbow.  Either hub could become
		 * the master after the kernel does its own IO
		 * discovery.  Since nasids aren't assigned when this
		 * routine is called, we have to do the credit adjustment
		 * on each separate node board (i.e. adjust the non-master
		 * board here, and the xbow master board below)
		 */
		while ((link = xbow_get_active_link(xbow_base, link)) >= 0) {
		    if (xbow_check_link_hub(xbow_base, link))
			wid_type = HUB_WIDGET_PART_NUM;
		    else {
			if ((wid_id = io_get_widgetid(nasid, link)) == -1) {
			    /* clear link status to clean out any junk status */
			    xbow_cleanup_link(xbow_base, link);
			    link++;
			    continue;
			}
			wid_type = XWIDGET_PART_NUM(wid_id);
		    }
		    wid_base = NODE_SWIN_BASE(nasid, link) ;

		    if (wid_type == HQ4_WIDGET_PART_NUM) 
			hubii_wcr_credits(NASID_GET(wid_base), 
					  HUBII_XBOW_CREDIT);
		    link++;
		}

		return xbow_lbptr;
	}
	
        /* adding code that determines the inactive links  */
        find_inactive_links(xbow_base);
	while ((link = xbow_get_active_link(xbow_base, link)) >= 0) {
		widget_lbptr = NULL;
		if (xbow_check_link_hub(xbow_base, link))
		    wid_type = HUB_WIDGET_PART_NUM;
		else {
		    if ((wid_id = io_get_widgetid(nasid, link)) == -1) {
			/* clear link status to clean out any junk status */
			xbow_cleanup_link(xbow_base, link);
			link++;
			continue;
		    }
		    wid_type = XWIDGET_PART_NUM(wid_id);
	        }

		wid_base = NODE_SWIN_BASE(nasid, link) ;		    

		switch (wid_type) {
		case HUB_WIDGET_PART_NUM:
			if ((link != hub_link) &&
			    (xbow_check_hub_net(xbow_base, link) == 0)) {
                                printf("XBow link %d:no CrayLink, skipping\n",
                                        link);
				link++;
				continue;
			}
			widget_lbptr = NULL;
				
			/* Find xbow port num and add to graph */
			/* if this hub is headless, we may want to 
			   do hubii init for it here. */
			break ;
		
		case BRIDGE_WIDGET_PART_NUM:
			widget_lbptr = bridge_discover(wid_base, diag_mode);
			break ;

		case XG_WIDGET_PART_NUM:
                /* xg_widget_discover calls gfx_discover if it is GFX */
                	widget_lbptr = xg_widget_discover(
				wid_base,wid_type, diag_mode) ;
                break;

		case HQ4_WIDGET_PART_NUM:
			/* Do an early init of the GFX widget */
			/* Just add this to KLCFG */
			widget_lbptr = gfx_widget_discover(wid_base, wid_type, diag_mode) ;
			/* 
			 * PV 691804 - VisCons boards hang with the
			 * higher HUBII_XBOW_REV2_CREDIT value.  Set hub's
			 * credits back to the lower value.  Since each
			 * xbow can be connected to more than one hub,
			 * make sure the adjustment happens for all hubs
			 * connected to the xbow.  Either hub could become
			 * the master after the kernel does its own IO
			 * discovery.  This is the adjustment for the hub
			 * which is the xbow master.
			 */
			hubii_wcr_credits(NASID_GET(wid_base), 
					  HUBII_XBOW_CREDIT);
			break ;

                case TPU_WIDGET_PART_NUM:
                        /* Just add this to KLCFG */
                        widget_lbptr = tpu_widget_discover(wid_base, diag_mode);
                        break ;

                case GSN_A_WIDGET_PART_NUM:
                case GSN_B_WIDGET_PART_NUM:
                        /* Just add this to KLCFG */
                        widget_lbptr = 
				gsn_widget_discover(wid_base, wid_type, diag_mode);
                        break ;

		default:
                        DM_PRINTF(("New Widget %x connected to HUB.\n",wid_id));
			break ;
		}

		add_klcfg_xbow_port(xbow_lbptr, link, wid_type, widget_lbptr);

		/* At this point we can run some diags upto the XBOW
		   and beyond. Check the interrupt path to this xbow.
		   Write a bad packet to this widget and check if
		   we get the interrupt. */

		/* Pass some KLCFG stuff down. */

		if (widget_lbptr)
			widget_lbptr->brd_parent = xbow_lbptr ;

		link++;
	}
	return(xbow_lbptr) ;
}




/*
 * bridge_discover
 * Find out what is there on the PCI bus.
 */

lboard_t *
bridge_discover(volatile __psunsigned_t bridge_base, int diag_mode)
{
	lboard_t	*lbptr ;
	int		board_type ;
	char 		bridge_nic_info[1024] ;
	char 		nic_laser[32]  ;
	char		brd_name[32];
	nic_t		bridge_nic = -1 ;
	char		addnl_nic = 0 ; /* additional nics flag */
	klbri_t		*brip;
        char		*c;
	bridge_t	*bridge = (bridge_t *)bridge_base ;

#ifndef SABLE
	bridge_nic_info[0] = 0 ;
	klcfg_get_nic_info((nic_data_t)
			(bridge_base + BRIDGE_NIC), bridge_nic_info) ;
#endif

        DM_PRINTF(("WID %x, Bridge NIC: %s\n", WIDGETID_GET(bridge_base),
                                                bridge_nic_info)) ;

	board_type = kl_bridge_type(bridge_nic_info, &addnl_nic, brd_name);

	switch(board_type)
	{
                case  KLTYPE_IO6:
			lbptr = init_klcfg_baseio(bridge_base) ;
			lbptr->brd_flags |= addnl_nic ;
                break ;

                case  KLTYPE_MSCSI:
			lbptr = init_klcfg_4chscsi(bridge_base) ;
                break ;

                case  KLTYPE_MENET: /* MENET card */
			lbptr = init_klcfg_menet(bridge_base) ;
                break ;

		default:
                        DM_PRINTF(("Found IO board, NIC = %s\n",
                                bridge_nic_info)) ;
			lbptr = init_klcfg_brd(bridge_base, board_type) ;
		break;
        }

	if (lbptr) {
		if(bridge_nic_info)
           	if(c=strstr(bridge_nic_info,"Laser:"))
	   	{
			lbptr->brd_nic = strtoull(c+6, 0, 16);
	   	}
		strcpy(lbptr->brd_name, brd_name);
		brip = init_klcfg_bridge(bridge_base, lbptr, 
			WIDGETID_GET(bridge_base));
		if (!brip) {
			printf("Cannot allocate for bridge component.\n");
			return 0;
		}
		if (brip->bri_mfg_nic = klmalloc_nic_off(lbptr->brd_nasid,
					strlen(bridge_nic_info))) {
			strcpy((char *)NODE_OFFSET_TO_K1(lbptr->brd_nasid,
				brip->bri_mfg_nic), bridge_nic_info) ;
		}
    		if (!(bridge->b_wid_stat & BRIDGE_STAT_PCI_GIO_N)) {
			DM_PRINTF(("Found GIO board ...\n")) ;
			return NULL ;
		}
	}
	else {
		printf("Cannot allocate for lboard in bridge_discover.\n") ;
		return 0 ; 
	}

	/* Lets do a PCI discovery. */

	pci_discover(bridge_base, lbptr, diag_mode, brip->bri_devices) ;
	return(lbptr) ;

}

/*
 * pci_discover
 * Discover all PCI devices.
 */

void
pci_discover(volatile __psunsigned_t bridge_base,
	     lboard_t *lbptr,
	     int diag_mode,
	     klpci_device_t *devices)
{
	int		npci, rc;
	reg32_t		pci_id ;
	int	wid_id = WIDGETID_GET(bridge_base);
	int		first_ioc3 = 0 ;
	klinfo_t	*ioc3_ptr;
        __uint64_t  	pci_key;
        volatile __psunsigned_t ioc3_base, pci_cfg_dev_ptr;


	/* check components like ioc3, Q logic, */

	for(npci = 0; npci< MAX_PCI_DEVS; npci++)  {
		/* check the status of the link */
		if (get_pcilink_status(bridge_base, npci, wid_id, &pci_id, 1) == 0) {
		    continue;
		}

		/* Store the device ID away for future use. */
		devices[npci].pci_device_id = pci_id;

#if defined (SABLE)
	if (npci == 2)
        	pci_id = SABLE_DISK ; /* sable disk controller */
		printf("PCI dev %x found at %d.\n", pci_id, npci) ;
#endif /* SABLE */
		/* discover pci devices */

		DM_PRINTF(("\n")) ;
		switch (pci_id) {
		case FIBERCHANNEL:
                        DM_PRINTF(("PCI slot %d     pciid %08x  %8s\n",
                               npci, pci_id, "Adaptec FibreChannel"));
			init_klcfg_compt(bridge_base, lbptr, npci, 
				KLSTRUCT_FIBERCHANNEL, 0) ;
		break ;

		case SGI_RAD:
                        DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
                               npci, pci_id, "RAD"));
			init_klcfg_compt(bridge_base, lbptr, npci, KLSTRUCT_RAD, 0) ;
			break;
		case SGI_IOC3:
                        DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
                               npci, pci_id, "IOC3"));

			rc = KLDIAG_PASSED;
			/*
			 * We only want to run the serial diag on the console.
			 * On SN00 the brd_type == KLTYPE_BASEIO
			 * and npci > 4 can be PCI enet cards which have no
			 * SIO chip.
			 */

        		pci_key = MK_SN0_KEY(NASID_GET(bridge_base), wid_id, 
					npci);
        		pci_cfg_dev_ptr = GET_PCICFGBASE_FROM_KEY(pci_key);
        		ioc3_base = GET_PCIBASE_FROM_KEY(pci_key);

			if (lbptr->brd_type == KLTYPE_BASEIO && 
				!IS_MIO_IOC3(lbptr, npci) && (npci < 4)) {
				pcicfg32_t *pcptr = (pcicfg32_t *)pci_cfg_dev_ptr ;
				/*
				 * Check if the ioc3 is initted. Headless
				 * nodes do not do get_local_console and
				 * hence are not initted. is_console looks up
				 * nvram, which will fail if not initted.
				 */

				if (pcptr->pci_addr)
				    if (is_console_device(ioc3_base,
                                        pci_cfg_dev_ptr, lbptr->brd_nic, 0,
					diag_mode)) {
				        rc = diag_serial_dma(diag_mode,bridge_base,npci);
				        kl_hubcrb_cleanup(NASID_GET(bridge_base));
				    }

				if ((SN00) && !first_ioc3) {
                                        if (!(pcptr->pci_addr))
                                            ioc3_init(bridge_base,
                                                lbptr->brd_type,
                                                npci, diag_mode) ;
					first_ioc3 = 1 ;
					sn00_copy_mod_snum(
						bridge_base, npci) ;
				}
			}

			ioc3_init(bridge_base, lbptr->brd_type, npci, diag_mode);

			if (ioc3_ptr = init_klcfg_ioc3(bridge_base, lbptr, npci))
				ioc3_ptr->diagval = rc;
			if (rc != KLDIAG_PASSED) {
				moduleid_t	module;
				int		slot;

				get_module_slot(bridge_base, &module, &slot);
				printf("diag_serial_dma: /hw/module/%d/slot/io%d: "
				" FAILED\n", module, slot);
			}
			break;

		case QLOGIC_SCSI:
                        DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
                               npci, pci_id, "QLOGIC"));
			/* 
			 * Locate the device at the same mem addr as that
			 * the PIO space start. Eg: wid 2 starts at 600000
			 * So locate that dev at 600000 also on PCI.
			 */
			kl_init_pci(bridge_base, lbptr->brd_type, npci, pci_id, diag_mode);
			init_klcfg_qscsi(bridge_base, lbptr, npci) ;
			break ;

#if defined (SABLE)
		case SABLE_DISK:
			init_klcfg_sscsi(bridge_base, lbptr, npci) ;
			break;
#endif /* SABLE */

		case UNIVERSE_VME:
                        DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
                               npci, pci_id, "UNIVME"));
			init_klcfg_compt(bridge_base, lbptr, npci, KLSTRUCT_UNKNOWN, 0) ;
			break;

		case SGI_LINC:
		{
			klinfo_t	*ki ;

                        DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
                               npci, pci_id, "SGI_LINC"));
			ki = (klinfo_t *)init_klcfg_compt(bridge_base, 
					lbptr, npci, KLSTRUCT_UNKNOWN, 1) ;
			ki->revision += 1 ; 
			break ;
		}

               case PSITECH_RAD1:
		{       static int n_rad1s = 0;
                        if (n_rad1s == 0) {
				n_rad1s ++;   /* only set up the first one */
	                	DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
	                          npci, pci_id, "PSITECH"));

       		       		kl_init_pci(bridge_base, lbptr->brd_type, npci, pci_id, diag_mode);

               			init_klcfg_compt(bridge_base, lbptr, npci, KLSTRUCT_PCI,0) ;
                        } else {
	                        DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
       		                        npci, pci_id, "UNKNOWN"));
       		                init_klcfg_compt(bridge_base, lbptr, npci, KLSTRUCT_UNKNOWN, 0) ;
                        }

                      break;
                }

		default:
                        DM_PRINTF(("PCI slot %d pciid %08x  %8s\n",
                               npci, pci_id, "UNKNOWN"));
			init_klcfg_compt(bridge_base, lbptr, npci, KLSTRUCT_UNKNOWN, 0) ;
			break ;
		}
	}

}


/*
 * Function	: get_local_console
 * Parameters	: None
 * Purpose	: to initialize the io path and return the console base.
 * Assumptions	: none.
 * Returns	: console base if any.
 */

int 
get_local_console(console_t *console, int diag_mode, 
	          int cons_dipsw, int module, int pod_flag)
{
	volatile __psunsigned_t hub_base;
	hubii_ilcsr_t	ii_csr;
	
	console->uart_base = 0;
	console->type = 0;
	console->baseio_nic = 0;

        hub_base = (__psunsigned_t)REMOTE_HUB(get_nasid(), 0);

	ii_csr.icsr_reg_value = LD(hub_base + IIO_LLP_CSR);
	if (((ii_csr.icsr_fields_s.icsr_lnk_stat & LNK_STAT_WORKING) == 0)
	    || 	(ii_csr.icsr_fields_s.icsr_llp_en == 0))
	    return -1;
    	/*
	 * initialize the hub ii and discover what is connected to hub ii.
	 */
	discover_hub_console(hub_base, console, diag_mode, cons_dipsw, pod_flag);
	
        DM_PRINTF(("glc: c->uart_base == 0x%llx\n", console->uart_base));
	if (console->uart_base)  return 0;
	return -1;
}


/*
 * Function	: discover_hub_console
 * Parameters	: hub_base -> base of hub registers.
 *		: console  -> console structure to be filled.
 * Purpose	: to initialize the hub ii path, and fill the console fields.
 * Assumptions	: None.
 * Returns	: none.
 */

void
discover_hub_console(volatile __psunsigned_t hub_base,
		     console_t *console,
		     int diag_mode,
		     int cons_dipsw, int pod_flag)
{
	volatile __psunsigned_t 	wid_base ;
	volatile reg32_t		wid_id ;
	reg32_t		xwidget_part_num;
	console_t	first_console ;
	jmp_buf		new_buf;
	void		*old_buf;
	char		slot[4];

	bzero((void *)&first_console, sizeof(console_t)) ;

        if (setfault(new_buf, &old_buf)) {
		get_slotname(get_my_slotid(), slot);
		printf("*** Exception while probing for I/O console by cpu "
                        "in SLOT %s.\n", slot);
		printf("*** WARNING: A I/O console could not be found.\n");
		restorefault(old_buf);
		return;
        }

	wid_base =  SN0_WIDGET_BASE(NASID_GET(hub_base), 0);

	wid_id = LD32(wid_base + WIDGET_ID);
	xwidget_part_num = XWIDGET_PART_NUM(wid_id);

	restorefault(old_buf);

	switch (xwidget_part_num) {
	case BRIDGE_WIDGET_PART_NUM:
		/*
		 * If the bridge is directly connected to the hub
		 * the we can use any widget number that we want,
		 * we try and avoid 0 as it has problems with error handling
		 */
		 
		wid_base =  SN0_WIDGET_BASE(NASID_GET(hub_base), 8);
		discover_bridge_console(wid_base, console, &first_console, 
					diag_mode, NIC_UNKNOWN, pod_flag);
		break;

	case XBOW_WIDGET_PART_NUM:
		discover_xbow_console(wid_base, console, &first_console, 
				      diag_mode, pod_flag);
		break;

	default:
		break;
	}
	/* If we did not find a console for any reason
           like, ConsolePath pointing to a slot with no
	   board in it, try to use the first console we find.
	*/
	if ((console->uart_base == NULL) && cons_dipsw) {
        /*
         * Each node gets a valid console base if it can
         * see an IOC3 on its xbow. But this may be a
         * first console instead of the user desired console.
         * Use this and the DIP switch setting to determine
         * if we really have a console.
         */
		memcpy((void *)console, (void *)&first_console, 
			sizeof(console_t)) ;
	}
}


/*
 * Function	: discover_xbow_console
 * Parameters	: xbow_base -> base of xbow registers.
 *		: console   -> console information to be filled.
 * Purpose	: to initialize the xbow path and fill the console fields.
 * Assumptions	: none
 * Returns	: none
 */

void
discover_xbow_console(volatile __psunsigned_t xbow_base,
		      console_t *console, console_t *fc,
		      int diag_mode, int pod_flag)
{
	int link = 0;
	int	wid_type = 0, wid_id;
	volatile __psunsigned_t	wid_base;
	int hub_link = hub_xbow_link(NASID_GET(xbow_base));

	nic_t		modnic ;

	if (!pod_flag)
	if (xbow_get_master_hub(xbow_base) != hub_link) {
		return;
	}

	modnic = xbow_get_nic(xbow_base) ;

	while ((link = xbow_get_active_link(xbow_base, link)) >= 0) {
		if (xbow_check_link_hub(xbow_base, link) == 0) {
			wid_base = 
			    SN0_WIDGET_BASE(NASID_GET(xbow_base), link);
			wid_id = io_get_widgetid(NASID_GET(xbow_base), link);
			if (wid_id == -1) {
			    wid_type = -1;
		        }
			else {
			    wid_type = XWIDGET_PART_NUM(wid_id);
		        }
		}
		else
		    wid_type = HUB_WIDGET_PART_NUM;

		switch (wid_type) {
		case BRIDGE_WIDGET_PART_NUM:
			discover_bridge_console(wid_base, console, 
						fc, 
						diag_mode, modnic, pod_flag);
			break;

		default:
			break;
		}
		link++;
	}
}

/*
 * Function	: discover_bridge_console
 * Parameters	: bridge_base -> base of bridge registers
 *		: console     -> console struture to be filled in 
 * Purpose	: to initialize the bridge io path, and fill in console fields.
 * Assumptions	: none
 * Returns	: none
 */

void
discover_bridge_console(volatile __psunsigned_t bridge_base, 
			console_t *console, console_t *fc, 
			int diag_mode, nic_t xnic, int pod_flag)
{
	int board_type;
	char 	addnl_nic ;
        char            bridge_nic_info[1024] ;
        char		nic_laser[32];
        hubii_wcr_t     *ii_wcr;

        /* unless bridge is directly connected to hub, call bridge diag */
        ii_wcr = (hubii_wcr_t *)REMOTE_HUB_ADDR(NASID_GET(bridge_base), IIO_WCR);
        if(!ii_wcr->wcr_fields_s.wcr_dir_con) {
		if (diag_bridgeSanity(diag_mode, bridge_base) != KLDIAG_PASSED) {
			int		slot;
			moduleid_t	module;

			get_module_slot(bridge_base, &module, &slot);
			printf("diag_bridgeSanity: /hw/module/%d/slot/io%d: FAILED\n",
				module, slot);
		}
        }

#ifndef SABLE
        bridge_nic_info[0] = 0 ;
        klcfg_get_nic_info((nic_data_t)
                        (bridge_base + BRIDGE_NIC), bridge_nic_info) ;
#endif
	board_type = kl_bridge_type(bridge_nic_info, &addnl_nic,
					(char *)NULL);

	parse_bridge_nic_info(bridge_nic_info, "Laser", nic_laser);

        if (board_type == KLTYPE_IO6) {
                if (diag_io6confSpace_sanity(diag_mode, bridge_base) != KLDIAG_PASSED) {
			int		slot;
			moduleid_t	module;

			get_module_slot(bridge_base, &module, &slot);
			printf("diag_io6confSpace_sanity: /hw/module/%d/slot/io%d: "
				"FAILED\n", module, slot);
		}
		console->baseio_nic = strtoull(nic_laser, 0, 16);
        }

	if (!pod_flag)
	bridge_init(bridge_base, diag_mode);

	switch (board_type) {
	case KLTYPE_IO6:
		discover_baseio_console(bridge_base, board_type, console, fc, 
					diag_mode, xnic);
		break;

	default:
		break;
	}
}


/*
 * Function	: get_ioc3_console
 * Parameters	: baseio_base -> base of the io6 registers.
 *		: console     -> console struture to be filled in 
 * Purpose	: to initialize the ioc3 part, and fill in console fields.
 * Assumptions	: none
 * Returns	: none.
 */


void
get_ioc3_console(volatile __psunsigned_t baseio_base,
		 int board_type, int npci, console_t *console, 
		 console_t *fc, int diag_mode, nic_t xnic)
{
	int	wid_id = WIDGETID_GET(baseio_base);
	__uint64_t  pci_key;
	pcicfg32_t	*pci_cfg_dev_ptr ;
	volatile __psunsigned_t ioc3_base;

	ioc3_init(baseio_base, board_type, npci, diag_mode);

	pci_key = MK_SN0_KEY(NASID_GET(baseio_base), wid_id, npci);
	pci_cfg_dev_ptr = (pcicfg32_t *) GET_PCICFGBASE_FROM_KEY(pci_key);
	ioc3_base = GET_PCIBASE_FROM_KEY(pci_key);

	if ((fc) && (fc->uart_base == NULL)) {
                fc->uart_base = ioc3_base + IOC3_SIO_UA_BASE;
		fc->baud = 9600 ; /* Force speed since console is unknown */
		fc->flag = 1 ;    /* First console */
                fc->type = SGI_IOC3;
                fc->memory_base = (__psunsigned_t)ioc3_base;
                fc->config_base = (__psunsigned_t)pci_cfg_dev_ptr;
                fc->npci = npci;
                fc->wid  = wid_id ;
	}

	if (console->uart_base) /* console found */
		return ;         /* go no further */

  	if (is_console_device(ioc3_base, 
			(__psunsigned_t)pci_cfg_dev_ptr, xnic, 1, diag_mode)) {
		console->uart_base = ioc3_base + IOC3_SIO_UA_BASE;
		console->baud = get_nvram_dbaud(ioc3_base) ;
		console->flag = 1 ;
		console->type = SGI_IOC3;
		console->memory_base = (__psunsigned_t)ioc3_base;
		console->config_base = (__psunsigned_t)pci_cfg_dev_ptr;
		console->npci = npci;
		console->wid  = wid_id ;
	}
}

/*
 * Function	: discover_baseio_console
 * Parameters	: bridge_base -> base of baseio registers.
 *		: console     -> console struture to be filled in 
 * Purpose	: to initialize the baseio board, and fill in console fields.
 * Assumptions	: none
 * Returns	: none.
 */

void
discover_baseio_console(volatile __psunsigned_t bridge_base,
			int board_type, console_t *console, console_t *fc,
			int diag_mode, nic_t xnic)
{
	int		npci;
	reg32_t		pci_id;
	int	wid_id = WIDGETID_GET(bridge_base);

	/* check components like ioc3, Q logic, */

	for(npci = 0; npci< MAX_PCI_DEVS; npci++)  {
		if (get_pcilink_status(bridge_base, npci, wid_id, &pci_id, 1)
		    == 0) {
			continue;
		}
                DM_PRINTF(("glc: pciid 0x%x in slot %d\n", pci_id, npci));
		if (pci_id != SGI_IOC3) continue;
		get_ioc3_console(bridge_base, board_type, npci, console, fc, 
				 diag_mode, xnic);
		return;
	}
	if (npci == MAX_PCI_DEVS)
                printf( "***Warning: Cannot find IOC3 on this BASEIO."
                        "May have a problem finding a console\n") ;
}

/*
 * This function is supposed to init only those registers of
 * IOC3 that are needed to access the nvram. Currently, this
 * routine does all the init that is done in ioc3_chip_init
 * by the IP27prom for Serial IO init. It looks like doing
 * this init again in the IP27prom does not break things.
 * XXX need to do only that much that is needed for nvram access.
 */

static void
early_ioc3_nvram_init(__psunsigned_t mem_base, 
		      __psunsigned_t cfg_base)
{
    struct ioc3_configregs *cb = (struct ioc3_configregs*)cfg_base;

    PCI_OUTW(&cb->pci_scr,
             PCI_INW(&cb->pci_scr) |
             PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE);
    PCI_OUTW(&cb->pci_lat, 0xff00);

    SW(mem_base + IOC3_SIO_CR,
                ((UARTA_BASE >> 3) << SIO_CR_SER_A_BASE_SHIFT) |
                ((UARTB_BASE >> 3) << SIO_CR_SER_B_BASE_SHIFT) |
                (4 << SIO_CR_CMD_PULSE_SHIFT));

    SW(mem_base + IOC3_GPDR, 0);

    SW(mem_base + IOC3_GPCR_S,
                GPCR_INT_OUT_EN | GPCR_MLAN_EN |
                GPCR_DIR_SERA_XCVR | GPCR_DIR_SERB_XCVR);

}


short
get_nvram_dbaud(volatile __psunsigned_t ioc3_base)
{
        __psunsigned_t 	nvram_base;
	char	 	bauds[16] ;
	short		baud = 2400 ;

        nvram_base = ioc3_base + IOC3_NVRAM_OFFSET;
        _cpu_get_nvram_buf(nvram_base, NVOFF_LBAUD,
                           NVLEN_LBAUD, bauds);
	baud = atoi(bauds) ;
        ST_PRINTF(0, ("***ConsoleBaud = %d\n", baud)) ;
	return baud ;
}


int
is_console_device(volatile __psunsigned_t ioc3_base, 
		  volatile __psunsigned_t cfg_base, 
		  nic_t xnic, int flag, int diag_mode)
{
	__psunsigned_t nvram_base;
	char	console_path[NVLEN_CONSOLE_PATH+2] ;
	short	self_modid = -1, path_modid = -1 ;
	short	self_slotid = -1, path_slotid = -1 ;
	nasid_t	nasid;
	char	buf[8];
	int id ;

	nasid = NASID_GET(ioc3_base);
	if (ip27log_getenv(nasid, "ForceConsole", 0, 0, 0) >= 0)
		return 1;

	nvram_base = ioc3_base + IOC3_NVRAM_OFFSET;
	/* Is the nvram OK? */
	if (check_nvram(nvram_base, xnic)) {
		/* get the ConsolePath string */
		_cpu_get_nvram_buf(nvram_base, NVOFF_CONSOLE_PATH, 
					NVLEN_CONSOLE_PATH, console_path);
                ST_PRINTF(0, ("***ConsolePath = %s\n", console_path)) ;
		/* extract the module and slot ids */
		if ((id = check_console_path(console_path)) < 0) {
                        ST_PRINTF(0, ("*** Invalid ConsolePath.\n")) ;
			/* return 0 ; XXX */
		} else if (id == 0) { /* default */
			return 1 ;
		} else {
			path_modid = id >> 16 ;
			path_slotid = id&0xffff ;
		}

		/* get own module id from nvram */
		if (SN00 || (self_modid = elsc_module_get(get_elsc())) < 0) {
			ip27log_getenv(nasid, IP27LOG_MODULE_KEY, buf, "0", 0);
			self_modid = atoi(buf);
		}
		if (!self_modid) {
			printf("New module found on nasid %d. Using as "
				"console...\n", nasid);
			return 1;
		}

		self_slotid = SLOTNUM_GETSLOT(xbwidget_to_xtslot(
				hub_slot_to_crossbow(get_my_slotid()),
				WIDGETID_GET(ioc3_base))) ;
		if ((self_modid == path_modid)) {
			if (self_slotid == path_slotid) {
				if (flag)
                                	printf(
                "Using console at /hw/module/%d/slot/io%d\n",
				       self_modid, self_slotid) ;
				return 1 ;
			} else {
				if (SN00 && (path_slotid == 25)) {
					if (flag)
                                        	printf(
                "Using console at /hw/module/%d/MotherBoard\n", self_modid);
					return(1);
				}
                                DM_PRINTF(
                ("***BaseIO(%d,%d) is not the console\n",
                                       self_modid, self_slotid)) ;
				return 0 ;
			}
		} else {
                        DM_PRINTF(("***BaseIO(%d,%d) is not the console\n",
                                self_modid, self_slotid)) ;
			return 0 ; 
		}
	}
	else {
		printf("***is_console_device : nvram is invalid\n") ;
		return 1 ;
	}

	/* BUG 408041. to be fixed
	return 1; */
}


void
set_console_node(console_t *console, nasid_t nasid)
{
	console->uart_base = CHANGE_ADDR_NASID(console->uart_base, nasid);
	console->config_base = CHANGE_ADDR_NASID(console->config_base, nasid);
	console->memory_base = CHANGE_ADDR_NASID(console->memory_base, nasid);
	console->nasid = nasid;
}


/*
 * gfx_widget_discover - Find out what is there
 */

#include <sys/xtalk/xwidget.h>
#include <sys/xtalk/xg.h>
#include <sys/xtalk/hq4.h>
#include <KONA/xg_regs.h>

lboard_t *
gfx_widget_discover(volatile __psunsigned_t gfx_wid_base, int gfx_wid_type, int diag_mode)
{ 
	widget_cfg_t	*w;
	int		wid;
	lboard_t        *lbptr;
        klgfx_t		*kg;
        char            nic_info[1024]; /* XXXmacko size arbitrarily chosen?  We may */
                                        /* need more space if we have a lot of nics. */
	int brd_type;
	char *brd_name;
	__psunsigned_t nic_addr;

        /* setup widget's widget id */
        wid = WIDGETID_GET(gfx_wid_base);
	w = (widget_cfg_t *)gfx_wid_base;
	/* clear old value of the field and write new value */
	w->w_control =  (w->w_control & ~(WIDGET_WIDGET_ID|WIDGET_LLP_XBAR_CRD) ) | 
			(wid & WIDGET_WIDGET_ID) | (3 << WIDGET_LLP_XBAR_CRD_SHFT);
#if 0
	w->w_control =  (w->w_control & ~WIDGET_WIDGET_ID) | 
			(wid & WIDGET_WIDGET_ID); 
#endif

        switch(gfx_wid_type) {

		case	XG_WIDGET_PART_NUM:

			/* check if XG is actually connected to graphics */
			if (XG_CONFIG_SUBSYSTEM_GET(*((volatile int*)(gfx_wid_base+XG_CONFIG_REG)))) {
				/* XG is NOT connected to graphics -> abort */
				return NULL;
			}
			brd_type = KLTYPE_GFX_KONA;
			brd_name = "kona";
			nic_addr = gfx_wid_base + XG_WIDGET_NIC;
			break;

		case	HQ4_WIDGET_PART_NUM:

			brd_type = KLTYPE_GFX_MGRA;
			brd_name = "mgras";
			nic_addr = gfx_wid_base + HQ4_WIDGET_NIC;
			break;

		default: /* UNKNOWN Graphics Board */

			/* should never get to here from xbow_discover - check it anyway */
			/* print warning later, when we know the slot # */
			brd_type = KLTYPE_GFX;
			brd_name = "unknown_gfx";
			nic_addr = NULL;
			break;
	}

	/* read in widget nic info */
	nic_info[0] = '\0';
	if (nic_addr)
		klcfg_get_nic_info((nic_data_t)nic_addr, nic_info);

	/* attempt instantiation of klgfx_t structure */
	if ((lbptr = init_klcfg_graphics(gfx_wid_base)) == NULL)
		return NULL;

	strcpy(lbptr->brd_name, brd_name);
	lbptr->brd_type = brd_type;

	if (kg = (klgfx_t *)find_component(lbptr, NULL, KLSTRUCT_GFX))
		if (kg->gfx_mfg_nic = klmalloc_nic_off(lbptr->brd_nasid, strlen(nic_info)))
			strcpy((char *)NODE_OFFSET_TO_K1(lbptr->brd_nasid, kg->gfx_mfg_nic), nic_info);

	if (brd_type == KLTYPE_GFX) {
		/* warn about a graphics board we don't know about */
		printf("Warning: Unknown GFX board (0x%x) at Module %d Slot %d\n",
			gfx_wid_type, lbptr->brd_module, lbptr->brd_slot);
	}

        return(lbptr);
}

/*
 * Discover the class of boards that have the XG front end.
 * Assuming that we need to write control reg and do other
 * hw init to be able to determine if it is GFX or not.
 *
 * NOTE: gfx_widget_discover has not been touched. So the
 *       gfx widget really gets initted twice. Is this OK?
 *       HDTV boards are now getting the same crossbow credits
 *	 as gfx. Is this OK?
 */
lboard_t *
xg_widget_discover(volatile __psunsigned_t xg_wid_base, 
			int xg_wid_type, int diag_mode)
{
	widget_cfg_t 	*w ;
	int		subsystem ;
        int             wid;
        lboard_t        *lbptr;
        klxthd_t        *xt;
        char            nic_info[1024];
        char            nic_tmp[32];
        __psunsigned_t	nic_addr;

        /* setup widget's widget id */
        wid = WIDGETID_GET(xg_wid_base);
        w = (widget_cfg_t *)xg_wid_base;
        /* clear old value of the field and write new value */
        w->w_control =  (w->w_control 					& 
			~(WIDGET_WIDGET_ID|WIDGET_LLP_XBAR_CRD)) 	|
                        (wid & WIDGET_WIDGET_ID) 			|
			(3 << WIDGET_LLP_XBAR_CRD_SHFT);
        /* check if XG is actually connected to xthd */
        subsystem = XG_CONFIG_SUBSYSTEM_GET(*((volatile int*)
				(xg_wid_base+XG_CONFIG_REG)));

	switch (subsystem) {
		case 0:
			return gfx_widget_discover(
					xg_wid_base, xg_wid_type, diag_mode);
			break ;	/* not really needed */

		case 1:
			break ; /* init done below */

		default:
			printf("Note: Found gfx widget with unknown subsystem\n") ;
			return NULL ;
	}

        nic_addr = xg_wid_base + XG_WIDGET_NIC;

        /* read in widget nic info */
        nic_info[0] = '\0';
        if (nic_addr)
                klcfg_get_nic_info((nic_data_t)nic_addr, nic_info);

        if ((lbptr = init_klcfg_xthd(xg_wid_base)) == NULL)
                return NULL;

	if (nic_info[0]) {
        	parse_bridge_nic_info(nic_info, "Laser", nic_tmp);
        	lbptr->brd_nic = strtoull(nic_tmp, 0, 16);

        	parse_bridge_nic_info(nic_info, "Name", nic_tmp);
        	strncpy(lbptr->brd_name, nic_tmp, 31);

        	lbptr->brd_type = KLTYPE_XTHD;
	}

        if (xt = (klxthd_t *)find_component(lbptr, NULL, KLSTRUCT_XTHD))
                if (xt->xthd_mfg_nic = klmalloc_nic_off(
				lbptr->brd_nasid, strlen(nic_info))) {
                        strcpy((char *)NODE_OFFSET_TO_K1(lbptr->brd_nasid, 
					xt->xthd_mfg_nic), nic_info);
		}

        return(lbptr);
}

void
init_tpu_widget_discover(__psunsigned_t tpu_wid_base)
{
        int wid ;
        widget_cfg_t *w ;

        /* setup widget's widget id */

        wid = WIDGETID_GET(tpu_wid_base) ;
        w = (widget_cfg_t *)tpu_wid_base ;
        /* clear old value of the field and write new value */
        w->w_control =  (w->w_control & ~WIDGET_WIDGET_ID) |
                                                (wid & WIDGET_WIDGET_ID);
}


/*
 * tpu_widget_discover - Tensor Processing Unit widget
 */

#define parse_tpu_nic_info parse_bridge_nic_info

lboard_t *
tpu_widget_discover(volatile __psunsigned_t tpu_wid_base, int diag_mode)
{
        lboard_t        *lbptr ;
        kltpu_t 	*ktpu ;

        /* Do an early init of the TPU widget */
        init_tpu_widget_discover(tpu_wid_base) ;

        /* attempt instantiation of tpu klstruct */
        lbptr = init_klcfg_tpu(tpu_wid_base);
        if (lbptr) {
		int rc;
                char tpu_wid_nic_info[1024] ;
                char nic_name[32] ;
                char nic_laser[32] ;

                tpu_wid_nic_info[0] = 0 ;

                rc = klcfg_get_nic_info((nic_data_t)(tpu_wid_base+TPU_WIDGET_NIC),
                                   tpu_wid_nic_info);
                if (rc != NIC_DONE) {
                        printf("BAD TPU NIC: status = %d (0x%x): NIC = %s\n", 
				rc, rc, tpu_wid_nic_info);
                }

                if(tpu_wid_nic_info[0]) {
                   DM_PRINTF(("WID %x, TPU NIC: %s\n",
                                                WIDGETID_GET(tpu_wid_base),
                                                tpu_wid_nic_info)) ;
                   parse_tpu_nic_info(tpu_wid_nic_info, "Laser", nic_laser);
                   lbptr->brd_nic = strtoull(nic_laser, 0, 16);

                   parse_tpu_nic_info(tpu_wid_nic_info, "Name", nic_name);
                   strncpy(lbptr->brd_name, nic_name, 31);
		   if (ktpu = (kltpu_t *)find_component(lbptr, NULL, KLSTRUCT_TPU)) {
			if (ktpu->tpu_mfg_nic = klmalloc_nic_off(lbptr->brd_nasid, 
				strlen(tpu_wid_nic_info)))
			strncpy((char *)NODE_OFFSET_TO_K1(lbptr->brd_nasid, 
					ktpu->tpu_mfg_nic),
					tpu_wid_nic_info, 1024) ;
		   }

                }
        }

        return(lbptr) ;
}

void
init_gsn_widget_discover(__psunsigned_t gsn_wid_base)
{
        int wid ;
        widget_cfg_t *w ;

        /* setup widget's widget id */

        wid = WIDGETID_GET(gsn_wid_base) ;
        w = (widget_cfg_t *)gsn_wid_base ;
        /* clear old value of the field and write new value */
        w->w_control =  (w->w_control & ~WIDGET_WIDGET_ID) |
                                                (wid & WIDGET_WIDGET_ID);
}


/*
 * gsn_widget_discover - Gigabyte System Network board (HIPPI-6400)
 */

#define parse_gsn_nic_info parse_bridge_nic_info

lboard_t *
gsn_widget_discover(volatile __psunsigned_t gsn_wid_base, 
		    int partnum,
		    int diag_mode)
{
        lboard_t        *lbptr ;
	int gsntype;

        /* Do an early init of the GSN widget */
        init_gsn_widget_discover(gsn_wid_base) ;

	gsntype = (partnum == GSN_A_WIDGET_PART_NUM) ?
                        KLSTRUCT_GSN_A : KLSTRUCT_GSN_B;
        /* attempt instantiation of gsn klstruct */
        lbptr = init_klcfg_gsn(gsn_wid_base, gsntype);

        if (lbptr) { 
	    /*
	     * Since the GSN SHAC nic is shared on the dual-board configs, 
	     * we must lock out any other cpus that might try and
	     * read it at the same time via the other port.  Use the
	     * GSN_HOST_WANTS_LOCK, GSN_FWP_WANTS_LOCK, GSN_FAVOR_FWP_LOCK 
	     * registers and Petersen's algorithm for mutual exclusion
             *  (See SHAC ASIC Specification Section 12.4.86): 
	     *
	     * Host lock routine: HOST_WANTS_LOCK = 1; FAVOR_FWP_LOCK = 1; 
	     *                    while (FWP_WANTS_LOCK && FAVOR_FWP_LOCK);
	     *
	     * FWP lock routine:  FWP_WANTS_LOCK = 1; FAVOR_FWP_LOCK = 0; 
    	     *                    while (HOST_WANTS_LOCK && !FAVOR_FWP_LOCK);
    	     *
	     * Host unlock routine: HOST_WANTS_LOCK = 0; 
	     *
	     * FWP unlock routine FWP_WANTS_LOCK = 0;
	     */
	    __psunsigned_t host_wants_lock = gsn_wid_base + GSN_HOST_WANTS_LOCK;
	    __psunsigned_t fwp_wants_lock = gsn_wid_base + GSN_FWP_WANTS_LOCK;
	    __psunsigned_t favor_fwp_lock = gsn_wid_base + GSN_FAVOR_FWP_LOCK;
	    int retries = 100;

	    if (gsntype == KLSTRUCT_GSN_A) {
		int fwp_wants = 0, favors_fwp = 0;
		SD32(host_wants_lock,1);
		SD32(favor_fwp_lock,1);
		delay(100000);
	     	while (retries-- > 0) {
		   fwp_wants = LD32(fwp_wants_lock);
		   favors_fwp = LD32(favor_fwp_lock);
	     	   if (fwp_wants && favors_fwp) {
			delay(1000000);
			DM_PRINTF(("retrying GSN NIC lock %d\n", retries));
		   } else {
			break;
		   }
		}
		if (retries > 0) {
		   DM_PRINTF(("***Debug: got lock for SHAC NIC on GSN_A board.\n"));
		   update_gsn_nic_info(gsn_wid_base, lbptr, gsntype, diag_mode);
		} else {
		   printf("***Warning: Could not obtain lock for GSN NIC.\n");
		}
		SD32(host_wants_lock,0); 	/* clear the lock */

	    } else if (gsntype == KLSTRUCT_GSN_B) {
		int host_wants = 0, favors_fwp = 0;
		SD32(fwp_wants_lock,1);
		SD32(favor_fwp_lock,0);
		delay(100000);
	     	while (retries-- > 0) {
		   host_wants = LD32(host_wants_lock);
		   favors_fwp = LD32(favor_fwp_lock);
	     	   if (host_wants && !favors_fwp) {
			delay(1000000);
			DM_PRINTF(("retrying GSN NIC lock %d\n", retries));
		   } else {
			break;
		   }
		}
		if (retries > 0) {
		   DM_PRINTF(("***Debug: got lock for SHAC NIC on GSN_B board.\n"));
		   update_gsn_nic_info(gsn_wid_base, lbptr, gsntype, diag_mode);
		} else {
		   printf("***Warning: Could not obtain lock for GSN NIC.\n");
		}
		SD32(fwp_wants_lock,0); 	/* clear the lock */
	
	    }
        }

        return(lbptr) ;
}

void
update_gsn_nic_info(volatile __psunsigned_t gsn_wid_base,
		    lboard_t *lbptr, 
		    int gsntype,
		    int diag_mode)
{
        klgsn_t 	*kgsn ;
	int rc;
	char gsn_wid_nic_info[1024] ;
	char nic_name[32] ;
	char nic_laser[32] ;

	gsn_wid_nic_info[0] = 0 ;

	rc = klcfg_get_nic_info((nic_data_t)(gsn_wid_base+GSN_L_WIDGET_NIC),
	gsn_wid_nic_info);
	if (rc != NIC_DONE) {
	   printf("***Warning: BAD GSN NIC: status = %d (0x%x): NIC = %s\n",
			rc, rc, gsn_wid_nic_info);
	   return;
	}

	if(gsn_wid_nic_info[0]) {
	   DM_PRINTF(("WID %x, GSN NIC: %s\n",
				WIDGETID_GET(gsn_wid_base),
				gsn_wid_nic_info)) ;
	   parse_gsn_nic_info(gsn_wid_nic_info, "Laser", nic_laser);
	   lbptr->brd_nic = strtoull(nic_laser, 0, 16);
	   parse_gsn_nic_info(gsn_wid_nic_info, "Name", nic_name);
	   strncpy(lbptr->brd_name, nic_name, 31);
	   if (kgsn = (klgsn_t *)find_component(lbptr, NULL, gsntype)) {
	      if (kgsn->gsn_mfg_nic = klmalloc_nic_off(lbptr->brd_nasid,
						       strlen(gsn_wid_nic_info)))
		strncpy((char *)NODE_OFFSET_TO_K1(lbptr->brd_nasid,
			kgsn->gsn_mfg_nic),
			gsn_wid_nic_info, 1024) ;
	   }
	}
	return;
}

update_old_ioprom(nasid_t nasid)
{
	lboard_t *lboard;
	klioc3_t *ioc3_compt;
	
	if (nasid == INVALID_NASID) 
	    nasid = get_nasid();

	lboard = (lboard_t *)KL_CONFIG_INFO(nasid);

	if ((lboard = find_lboard(lboard, KLTYPE_BASEIO)) == NULL) {
		printf("***Warning: Old IOprom may not work with new CPU PROM.\
Please update both to latest rev\n");
			return 0;
	}

	if ((ioc3_compt = (klioc3_t *)
		     	find_component(lboard, NULL, KLSTRUCT_IOC3)))
				ioc3_compt->ioc3_info.flags |= KLINFO_INSTALL ;
	else
                printf("***Warning: Old IOprom may not work with new CPU PROM.\
Please update both to latest rev\n");

	return 1 ;
}


void
ioc3_init(__psunsigned_t bridge_base, int board_type, int npci, int diag_mode)
{
	int	wid_id = WIDGETID_GET(bridge_base);
	__uint64_t  pci_key;
	pcicfg32_t	*pci_cfg_dev_ptr ;
	volatile __psunsigned_t ioc3_base;

	pci_key = MK_SN0_KEY(NASID_GET(bridge_base), wid_id, npci);
	pci_cfg_dev_ptr = (pcicfg32_t *) GET_PCICFGBASE_FROM_KEY(pci_key);
	/*
	 * if already initialized, do nothing.
	 */
	if (pci_cfg_dev_ptr->pci_addr)
	    return;

	ioc3_base = GET_PCIBASE_FROM_KEY(pci_key);	

	kl_init_pci(bridge_base, board_type, npci, SGI_IOC3, diag_mode);
	early_ioc3_nvram_init(ioc3_base, 
			      (__psunsigned_t)pci_cfg_dev_ptr) ;
}

/*
 * Function:		get_next_console -> Searches promcfg & klconfig to
 *			get a console. Gets a console with lowest module/slot.
 * Args:		Fills up module, slot & nasid
 * Returns:		>= 0, if console found
 *			< 0, if not found
 * Note:		Assumes both promcfg and klconfig are available
 */

int
get_next_console(pcfg_t *p, moduleid_t *module, int *slot, nasid_t *nasid)
{
	int		i, lowest_slot = 2 * MAX_XBOW_PORTS, console_found = 0;
	moduleid_t	lowest_module = MAX_MODULE_ID;
	lboard_t	*io6_ptr, *ip27_ptr;
	nasid_t		lowest_nasid;

	for (i = 0; i < p->count; i++) {
		nasid_t		hub_nasid;

		if (p->array[i].any.type != PCFG_TYPE_HUB)
			continue;

                if (!IS_RESET_SPACE_HUB(&p->array[i].hub) ||
                    (p->array[i].hub.partition != p->array[0].hub.partition))
			continue;

		hub_nasid = p->array[i].hub.nasid;
		if((p->array[i].hub.flags & PCFG_HUB_HEADLESS) ||
		(p->array[i].hub.flags & PCFG_HUB_MEMLESS)) 
		{
			continue;
		}
		   

		io6_ptr = find_lboard((lboard_t *) KL_CONFIG_INFO(hub_nasid), 
				KLTYPE_BASEIO);

		if (!io6_ptr)
			continue;

		ip27_ptr = find_lboard((lboard_t *) KL_CONFIG_INFO(hub_nasid), 
				KLTYPE_IP27);

		if (!ip27_ptr) {
			printf("*** WARNING: get_next_console: Cannot find "
				"IP27 board on nasid %d\n", hub_nasid);
			continue;
		}

		if (ip27_ptr->brd_module > lowest_module)
			continue;
		else if (ip27_ptr->brd_module < lowest_module)
			lowest_slot = 2 * MAX_XBOW_PORTS;

		while (io6_ptr) {
			if (io6_ptr->brd_type == KLTYPE_BASEIO) {
				if (SLOTNUM_GETSLOT(io6_ptr->brd_slot) <
					lowest_slot) {
					console_found = 1;
					lowest_slot = SLOTNUM_GETSLOT(
						io6_ptr->brd_slot);
					lowest_module = ip27_ptr->brd_module;
					lowest_nasid = ip27_ptr->brd_nasid;
				}
			}
			io6_ptr = KLCF_NEXT(io6_ptr);
		}
	}

	if (console_found) {
		*module = lowest_module;
		*slot = lowest_slot;
		*nasid = lowest_nasid;

		return 0;
	}
	else
		return -1;
}

#if defined (SABLE)
/*
 * init_klcfg_hw - 
 * Top level hardware discovery routine for a HUB.
 * Init HUB II and discover the IO boards. Fill up the
 * KLCFG area with the discovered information.
 */
int
init_klcfg_hw(nasid_t nasid)
{
	__psunsigned_t 	hub_base ;
	lboard_t 	*ip27_brd_ptr;
	klinfo_t	*klhubp;
	hub_error_t	*hub_err;
	int rc;
	hubreg_t    ii_csr, ii_csr1;

	if (nasid == INVALID_NASID)
	    hub_base = (__psunsigned_t) LOCAL_HUB(0) ;
	else
	    hub_base = (__psunsigned_t) REMOTE_HUB(nasid, 0) ;

	/* create the lboard struct for the node board */
	if ((ip27_brd_ptr = init_klcfg_ip27(hub_base, 0)) == NULL) {
		printf("init_klcfg_hw: nasid %d, cannot setup lboard\n",
		       nasid);
		return -1;
	}
	/*
	 * Save hub ii error registers here before they are cleared.
	 */
	if (klhubp = find_component(ip27_brd_ptr, NULL, KLSTRUCT_HUB)) {
		hub_err =HUB_ERROR_STRUCT(HUB_COMP_ERROR(ip27_brd_ptr, klhubp),
					  KL_ERROR_LOG);
		save_hub_io_error(hub_base, &hub_err->hb_io);
	}
	
	/*
	 * initialize the hub ii and discover what is connected to hub ii.
	 */

#if 0		/* XXX fixme */
	rc = kl_init_hubii(hub_base, &ii_csr, &ii_csr1);
#endif
	
	if (rc < 0) {
		/*
		 * Could not correctly initialize the hub ii.. mark it
		 * as inaccessible.
		 */
		kl_disable_board(ip27_brd_ptr);
	}
	else if (rc > 0)
	    io_discover(ip27_brd_ptr, diag_mode);

	return rc;
}

#endif /* SABLE */




static void
find_inactive_links(__psunsigned_t xbow_base)
{
	lboard_t *lbptr;
	xb_linkregs_t 	*lp;
	int nasid, link, wid;
        __psunsigned_t wid_base;
	for (link = 0; link < MAX_PORT_NUM; link++) {
                if (XBOW_WIDGET_IS_VALID(link)) {
		     lp = (xb_linkregs_t *)(xbow_base + XB_LINK_REG_BASE(link));
               if ((lp->link_aux_status & XB_AUX_STAT_PRESENT) && 
		(!(lp->link_status & XB_STAT_LINKALIVE))) 
                    {
			   nasid = NASID_GET(xbow_base);
			printf("WARNING: xbow_base: 0x%lx link: %d Widget present, "
			"but link not alive!\n", xbow_base, link);
                           if((lbptr = kl_config_alloc_board(nasid)) == NULL)
			   {
				printf("Cannot allocate board struct\n");
    				return;
			    }
			    wid_base = NODE_SWIN_BASE(nasid, link) ;
		 	    wid = WIDGETID_GET(wid_base);
			    lbptr->brd_type = KLTYPE_WEIRDIO;
			    lbptr->brd_flags |= FAILED_BOARD;
			    lbptr->brd_flags &= ~ENABLE_BOARD;
                            if(SN00)
				lbptr->brd_slot = get_node_slotid(nasid);
                	    else
			        lbptr->brd_slot = xbwidget_to_xtslot(get_node_crossbow(nasid), wid);
                            kl_config_add_board(nasid, lbptr);
			}
		} /* XBOW_WIDGET */
	} /*for*/
}
