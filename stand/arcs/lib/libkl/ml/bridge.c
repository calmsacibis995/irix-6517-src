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

#ident  "$Revision: 1.78 $"

/*
 * bridge.c - Hardware discovery of bridge. Only early init routines
 * go here. Everything else belongs in libsk/ml/bridge.c
 */

#include <sys/mips_addrspace.h>
#include <libkl.h>
#include <kllibc.h>		/* for macros like isalpha etc */
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/xbow.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/nic.h>
#include <promgraph.h>
#include <libsc.h>
#include <setjmp.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/intr.h>
#include <sys/SN/kldiag.h>


void
kl_disable_bridge_intrs(bridge_t *bp)
{
        bp->b_int_enable = 0 ;
        bp->b_int_host_err = 0 ;
        bp->b_int_rst_stat = 0xffff ;
}


int
clear_bridge_intrs(bridge_t *bp)
{
	jmp_buf fault_buf;		/* Status buffer */
	void	*prev_fault;

	if (setfault(fault_buf, &prev_fault)) {
	  printf("unable to clear bridge intrs, faulted\n"); 
	  restorefault(prev_fault);
	  return 0;
	}
	bp->b_int_rst_stat = BRIDGE_IRR_PCI_GRP_CLR;
	restorefault(prev_fault);
	return 1;
}

__uint32_t
pci_mem_base(int npci, int wid)
{
	__uint32_t mem_base;
	
	mem_base = (npci < 3) ? (npci*2+2):(npci+4) ;
	mem_base += (wid << 4);
	mem_base <<= 20;
	return mem_base;
}

/*
 * PCI inits required before accessing first pci cfg data.
 * Set up PCI Device(x) register to map the first 1/2 MB for this device.
 * May need more params like mem or io space etc.
 */

void
kl_init_pci(__psunsigned_t bridge_base,
	    int board_type,
	    int pci_dev_num,
	    int pci_id,
	    int diag_mode)
{
	pcicfg32_t	*pci_cfg_dev_ptr;
	int wid_id = WIDGETID_GET(bridge_base);
	__uint32_t mem_base = pci_mem_base(pci_dev_num, wid_id);


	pci_cfg_dev_ptr = (pcicfg32_t *) (bridge_base + 
				BRIDGE_TYPE0_CFG_DEV(pci_dev_num)) ;

	pci_cfg_dev_ptr->pci_scr |= PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE;

       	/* Make this into another function - set_pci_window */
	PUT_FIELD32((bridge_base + BRIDGE_DEVICE(pci_dev_num)), 
	     (BRIDGE_DEV_OFF_MASK|BRIDGE_DEV_DEV_IO_MEM), 0, 
		    BRIDGE_DEV_DEV_IO_MEM | 
		    (mem_base >> BRIDGE_DEV_OFF_ADDR_SHFT));


	switch (pci_id) {
	case QLOGIC_SCSI:
		pci_cfg_dev_ptr->pci_addr = mem_base;
		pci_cfg_dev_ptr->pad_0[0] = mem_base;
		break;

	case SGI_IOC3:
		pci_cfg_dev_ptr->pci_addr = mem_base;
		break;

        case PSITECH_RAD1: {
                __uint32_t mask, base = mem_base;
                __uint32_t *bar0, *bar2;
		__uint32_t bar0size, bar2size;
                nasid_t nasid = get_nasid();
                pcicfg_t *pcdp = (pcicfg_t *)pci_cfg_dev_ptr;

                /* set up big window -> PCI space mapping */
                IIO_ITTE_PUT(nasid, 0, 0, wid_id, BRIDGE_PIO32_XTALK_ALIAS_BASE);
		/* get pointers and bar sizes */
                mask = 0xf;
                bar0 = &pcdp->base_addr_regs[0];
		bar0size = 0x20000;
                bar2 = &pcdp->expansion_rom;
		bar2size = 0x0010000;

                /* set up base address register 0 */
                *bar0 = base;
		base += bar0size;
		base += (bar2size-1); /*round up to the next barsize boundry*/
		base &= ~(bar2size-1);

                /* set up base address register 2 */
                *bar2 = base | 0x01; /* exp rom enable */

        }
                break;

	default:
		pci_cfg_dev_ptr->pci_addr = mem_base;
		break;
	}


	if (pci_id == SGI_IOC3 &&
	    board_type == KLTYPE_BASEIO &&
	    pci_dev_num < 4) {
		if (diag_pcibus_sanity(diag_mode,bridge_base,pci_dev_num) != 
				KLDIAG_PASSED) {
			moduleid_t	module;
			int		slot;

			get_module_slot(bridge_base, &module, &slot);
			printf("diag_pcibus_sanity: /hw/module/%d/slot/io%d: FAILED\n",
				module, slot);
		}

		if (diag_serial_pio(diag_mode,bridge_base,pci_dev_num) != 
				KLDIAG_PASSED) {
			moduleid_t	module;
			int		slot;

			get_module_slot(bridge_base, &module, &slot);
			printf("diag_serial_pio: /hw/module/%d/slot/io%d: FAILED\n",
				module, slot);
		}
	}

}

#include <sys/xtalk/xwidget.h>
#include <sys/SN/slotnum.h>


void
bridge_init(__psunsigned_t bridge_base, int diag_mode)
{
	volatile reg32_t 	reg_temp ;
	bridge_t		*bp = (bridge_t *)bridge_base ;
	int 			wid ;
	xwidgetnum_t 		hub_wid_id;
	__uint32_t		br_ctrl;
	/* setup bridge's widget id */

	wid = WIDGETID_GET(bridge_base) ;
        PUT_FIELD32((bridge_base + BRIDGE_WID_CONTROL), 
		    (WIDGET_WIDGET_ID|WIDGET_LLP_XBAR_CRD), 0, 
		     wid | BRIDGE_CTRL_LLP_XBAR_CRD(3));

        /* clear all bridge regs */

        reg_temp = bp->b_wid_stat ;

	br_ctrl = bp->b_wid_control;

	/*
	 * Reset the bridge and hold it down for a while
	 * Then remove the reset. 
	 */
	bp->b_wid_control = (br_ctrl & ~BRIDGE_CTRL_RST_MASK) ;
	bp->b_wid_control;		/* inval addr bug war */
	delay(2000); /* Each count is 2 instructions  ~10ns */	
	bp->b_wid_control = br_ctrl | BRIDGE_CTRL_RST_MASK;
	bp->b_wid_control;		/* inval addr bug war */

        bp->b_wid_err_cmdword =          0 ;

	/* This is incorrect initialization of the llp config register!!
	 * BRIDGE_WID_LLP defines the llp register offset.
	 * bp->b_wid_llp  = BRIDGE_WID_LLP ; 
	 */
	kl_disable_bridge_intrs(bp) ;

	/* Set up direct map so that PCI addr 2GB is 0 phys. */
	hub_wid_id = hub_slot_to_widget(get_my_slotid());
	bp->b_dir_map = hub_wid_id << BRIDGE_DIRMAP_W_ID_SHFT;
	
	/*
	 * Set up a delay in arbitration to avoid having two transactions
	 * in a 32-cycle window.  This workaround also requires a board
	 * rework that grounds an unused PCI slot's bus request line. Only
	 * needed on rev A bridge.
	 */
	if (XWIDGET_REV_NUM(bp->b_wid_id) < 2) {
		printf("Bridge rev A: arbitration delay turned on\n");
		bp->b_arb = 0x002ff08;
	}

	/* Allocate one response buffer per device */

	bp->b_even_resp = 0xba98 ;
	bp->b_odd_resp = 0xba98 ;

	/*
	 * BUG 400739. This may not fix the problem since the problem
	 * was experienced wiht devices that do not issue retry on pio's.
	 * set the pci retry count to max. This should give us a timeout
	 * after approx 500 mics.
	 */
	bp->b_pci_bus_timeout |=  BRIDGE_TMO_PCI_RETRY_CNT_MAX;
}


/*ARGSUSED2 XXX */
int
get_pcilink_status(__psunsigned_t bridge_base, int npci, int wid_id,
		   reg32_t *pci_id, int clear)
{
	bridge_t	*bp = (bridge_t *)bridge_base ;
	pcicfg32_t	*pci_cfg_dev_ptr ;
	__int64_t	pci_key;
	jmp_buf fault_buf;		/* Status buffer */
	void	*prev_fault;

	if (clear_bridge_intrs(bp) == 0) return 0;

	pci_key = MK_SN0_KEY(NASID_GET(bridge_base), wid_id, npci);
	pci_cfg_dev_ptr=(pcicfg32_t *)GET_PCICFGBASE_FROM_KEY(pci_key);

	if (setfault(fault_buf, &prev_fault)) {
		restorefault(prev_fault);
		delay(0x2000) ; /* delay for 20 uS XXX only during init */
		clear_bridge_intrs(bp);

		return 0;
	}
	*pci_id = pci_cfg_dev_ptr->pci_id;

	restorefault(prev_fault);
        /* check the status of the read */

	delay(0x2000) ; /* delay for 20 uS XXX only during init */

 	if (bp->b_int_status & BRIDGE_ISR_PCI_MST_TIMEOUT) 
	{
		if (clear)
		    clear_bridge_intrs(bp);
		printf("Bridge timeout, pci_id 0x%x", *pci_id);
		return (0) ;
	}
	else
		return(1) ;
}


int
kl_bridge_type(char *bridge_nic_info, char *addnl_nic, char *brd_name)
{
	char 	part_number[32] ;
	char 	board_name[32] ;
	int	rv = 0 ;
	char	field_name[32] ;

	/* Get the board name */
	if (brd_name) {
		strcpy(field_name, "Name") ;
		parse_bridge_nic_info(bridge_nic_info, field_name, board_name);
		strcpy(brd_name, board_name);
	}

	if ((bridge_nic_info == NULL) || (*bridge_nic_info == 0))
		return KLTYPE_IO6;

	strcpy(field_name, "Part") ;

	while (bridge_nic_info) {
		part_number[0] = 0 ;

		bridge_nic_info = 
		parse_bridge_nic_info(bridge_nic_info, field_name, part_number) ;
		/* Check if the bridge corresponds to one in the xbox */
		if (!strncmp(part_number, XBOX_BRIDGE_PART_NUM, 9) ||
		    !strncmp(part_number, XBOX_XTOWN_PART_NUM, 9)) {
			rv = KLTYPE_HAROLD ;
		} 
		else
		if (!strncmp(part_number, BASEIO_PART_NUM, 9) ||
		    !strncmp(part_number, BASEIO_SERVER_PART_NUM, 9)) {
			rv = KLTYPE_IO6 ;
		}
		else
		if (!strncmp(part_number, MSCSI_PART_NUM, 9) ||
		    !strncmp(part_number, MSCSI_B_PART_NUM, 9) ||
		    !strncmp(part_number, MSCSI_B_O_PART_NUM, 9)) {
			rv = KLTYPE_MSCSI ;
		}
		else
		if (!strncmp(part_number, MENET_PART_NUM, 9)) {
			rv = KLTYPE_MENET ;
		}
		else
		if (!strncmp(part_number, FC_PART_NUM, 9)) {
			rv = KLTYPE_FC ;
		}
		else
		if (!strncmp(part_number, HAROLD_PART_NUM, 9)) {
			rv = KLTYPE_HAROLD ;
		}
		else
		if (!strncmp(part_number, LINC_PART_NUM, 9)) {
			rv = KLTYPE_LINC ;
		}
		else
		if (!strncmp(part_number, MIO_PART_NUM, 9)) {
			*addnl_nic |= SECOND_NIC_PRESENT ; /* KLTYPE_MIO */
		}
		else
		if (!strncmp(part_number, SN00_PART_NUM, 9)) {
			rv = KLTYPE_IO6;
		}
		else
		if (!strncmp(part_number, SN00A_PART_NUM, 9)) {
			rv = KLTYPE_IO6;
		}
		else
		if (!strncmp(part_number, SN00B_PART_NUM, 9)) {
			rv = KLTYPE_IO6;
		}
		else
		if (!strncmp(part_number, SN00C_PART_NUM, 9)) {
			rv = KLTYPE_IO6;
		}
		else
		if (!strncmp(part_number, SN00D_PART_NUM, 9)) {
			rv = KLTYPE_IO6;
		}
		else
		if (!strncmp(part_number, SN00_XTOWN_PART_NUM, 9)) {
			rv = KLTYPE_IO6;
		}
		else
		if (!strncmp(part_number, HPCEX_PART_NUM, 9)) {
			rv = KLTYPE_VME;
		}
		else {
		    if (!rv) {
		      
			if (SN00 && !is_xbox_config(get_nasid())) {
				/* For boards with no NICs programmed on speedo
				 * without xbox.
				 */
				printf("\nSN00:Found NO NICs. Assume BASEIO. part is %s\n", part_number) ;
				rv = KLTYPE_IO6;
			} else {
				char buf[128];

                                printf("\nFound an io Board with nic %s.\n", part_number) ;
				if (ip27log_getenv(get_nasid(), "nonicbaseio", buf,
						   0, 0) >= 0) {
					printf("\nFound NO NICs. Assuming BASEIO board\n") ;
					rv = KLTYPE_IO6;
				} else {
					rv = KLTYPE_WEIRDIO ;
				}
			}
		    }
		}
	}

	return rv ;
}
