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

#include "sn0_fru_analysis.h"
#include <sys/reg.h>
#include <sys/SN/addrs.h>
#include <sys/SN/xbow.h>
#include <sys/PCI/bridge.h>
#if !defined(FRUTEST) && !defined(_STANDALONE)
#include <sys/PCI/pciio.h>
#endif
#if !defined( FRUTEST) && !defined(_STANDALONE)
#include <sys/SN/slotnum.h>
#include <sys/debug.h>
#endif
#if !defined(_STANDALONE)
extern int full_fru_analysis;
#endif

extern kf_rule_t	kf_rule_tab[];	     /* table of rule encodings */
extern kf_cond_t	kf_cond_tab[];	     /* table of condition encodings */
extern kf_action_set_t	kf_action_set_tab[];/* table of action set encodings */
extern confidence_t    *kf_conf_tab[];
extern hubreg_t        	kf_reg_tab[];

extern const int KF_BRIDGE_RULE_INDEX;
extern const int KF_XBOW_RULE_INDEX;

#ifdef FRUTEST

#include "../sn0_private.h"

extern kl_config_hdr_t		g_node[];
extern confidence_t		g_io_conf;
extern confidence_t		g_sn0net_conf;
extern confidence_t		g_xbow_conf;

#undef NODE_OFFSET_TO_K1(_nasid, _off)

#define NODE_OFFSET_TO_K1(_nasid, _off) (_off)

#endif

#define KF_PCI_SLAVE_FOUND		1      
#define KF_PCI_SLAVE_NOT_FOUND		0
#define KF_BRIDGE_NOT_FOUND		(-1)


#if !defined( FRUTEST ) && !defined( _STANDALONE)
#include <sys/hwgraph.h>
/*
 * This routine given a pointer to a widget board tries to get the 
 * bridge vertex handle in the hardware graph.
 */
int
kf_pci_slave_dev_present(lboard_t *board,pciio_slot_t *slot) 
{
    vertex_hdl_t	pcibr_vhdl;
    pciio_space_t	space;
    iopaddr_t		offset;

    /* The bridge vertex handle is stored in the widget's lboard
     */
    if ((pcibr_vhdl = board->brd_graph_link) == GRAPH_VERTEX_NONE)
	return(KF_BRIDGE_NOT_FOUND);	

#ifdef FRU_DEBUG
    kf_print("++Bridge vertex handle = %v\n",pcibr_vhdl);
#endif

    /* Get the slot number allocated to the pci address
     * in the bridge error status register.
     */
    *slot = pciio_error_extract(pcibr_vhdl,&space,&offset);
#ifdef FRU_DEBUG
    kf_print("++Slave slot = %d\n",*slot);
#endif

    if (*slot == PCIIO_SLOT_NONE) 
	return(KF_PCI_SLAVE_NOT_FOUND);	
    else
	return(KF_PCI_SLAVE_FOUND);
}
#else
int
kf_pci_slave_dev_present(lboard_t *board,int *slot) 
{
    board = board;
    slot = slot;
    return(-1);
}

#endif
kf_result_t
kf_bridge_analyze(lboard_t *board,klinfo_t *bridge, kf_analysis_t *curr_analysis) {
    int            i;
    kf_analysis_t  new_analysis;
    kf_result_t    rv = KF_SUCCESS;
    klbridge_err_t *bridge_err = BRI_COMP_ERROR(board,bridge);
    
    bridgereg_t    int_status = bridge_err->be_dmp.br_int_status;
    bridgereg_t    pci_err_upper  = bridge_err->be_dmp.br_pci_upper;
    bridgereg_t    ram_perr   = bridge_err->be_dmp.br_ram_perr;
    
    int            pci_master = ((pci_err_upper >> BRIDGE_PCI_DEV_NUM_SHIFT) &
				 BRIDGE_PCI_DEV_NUM_MASK);
    int		   br_dev_found;	
#if !defined(FRUTEST) && !defined(_STANDALONE)
    pciio_slot_t  slot;	
#else
    int		  slot;
#endif
    
    KF_DEBUG("\t\t\tbeginning bridge analysis...\n");

    if (!curr_analysis) {
	/* If we are starting analysis, initialize confidences to zero */
	
	BRIDGE_CONF(bridge_err) = 0;
	BRIDGE_SSRAM_CONF(bridge_err) = 0;
	BRIDGE_LINK_CONF(bridge_err) = 0;
	
	for (i = 0; i < BRIDGE_DEV_CNT; i++) {
	    BRIDGE_PCI_DEV_CONF(bridge_err,i) = 0;
	}
    }

    /* set up data for tabular rules */
    kf_conf_tab[KF_BRIDGE_CONF_INDEX] = &BRIDGE_CONF(bridge_err);
    kf_conf_tab[KF_BRIDGE_SSRAM_CONF_INDEX] = &BRIDGE_SSRAM_CONF(bridge_err);
    kf_conf_tab[KF_BRIDGE_LINK_CONF_INDEX] = &BRIDGE_LINK_CONF(bridge_err);

#if BRIDGE_ERROR_INTR_WAR
    if (bridge->revision == BRIDGE_REV_A) {	/* known bridge bug */
	/*
	 * Should never receive interrupts for these reasons on Rev 1 bridge
	 * as they are not enabled. Assert for it.
	 */
	int_status &= ~(BRIDGE_IMR_PCI_MST_TIMEOUT |
			BRIDGE_ISR_RESP_XTLK_ERR |
			BRIDGE_ISR_LLP_TX_RETRY);
    }
    if (bridge->revision < BRIDGE_REV_C) {	/* known bridge bug */
	/*
	 * This interrupt is turned off at init time. So, should never
	 * see this interrupt.
	 */
	int_status &= ~(BRIDGE_ISR_BAD_XRESP_PKT);
    }
#endif


    if (pci_err_upper & BRIDGE_PCI_DEV_MASTER_MASK) {
	/* bridge was not the master */
	kf_conf_tab[KF_BRIDGE_PCI_MASTER_CONF_INDEX] = 
	    &BRIDGE_PCI_DEV_CONF(bridge_err,pci_master);
    }
    else
	kf_conf_tab[KF_BRIDGE_PCI_MASTER_CONF_INDEX] = &BRIDGE_CONF(bridge_err);

    kf_reg_tab[KF_BRIDGE_INT_STATUS_INDEX] = int_status;
    
    
    if (curr_analysis != NULL) {
	/* we have already analyzed the error state, so report errors found */
	new_analysis = *curr_analysis;
	
	board_serial_number_get(board,new_analysis.kfa_serial_number);
	new_analysis.kfa_info[KF_BRIDGE_LEVEL].kfi_type = KFTYPE_BRIDGE;
	
	if (*kf_conf_tab[KF_BRIDGE_CONF_INDEX]) {
	    new_analysis = *curr_analysis;
	
	    new_analysis.kfa_info[KF_BRIDGE_LEVEL].kfi_type = KFTYPE_BRIDGE;
	    new_analysis.kfa_conf = *kf_conf_tab[KF_BRIDGE_CONF_INDEX];
	    kf_guess_put(&new_analysis);
	}
	if (*kf_conf_tab[KF_BRIDGE_SSRAM_CONF_INDEX]) {
	    new_analysis = *curr_analysis;
	
	    new_analysis.kfa_info[KF_BRIDGE_LEVEL].kfi_type = KFTYPE_BRIDGE;
	    new_analysis.kfa_info[KF_BRIDGE_SSRAM_LEVEL].kfi_type = KFTYPE_BRIDGE_SSRAM;
	    new_analysis.kfa_info[KF_BRIDGE_SSRAM_LEVEL].kfi_inst = 0;
	    new_analysis.kfa_conf = *kf_conf_tab[KF_BRIDGE_SSRAM_CONF_INDEX];
	    kf_guess_put(&new_analysis);
	}

	if (*kf_conf_tab[KF_BRIDGE_LINK_CONF_INDEX]) {
	    /* setup analysis information */
	    new_analysis = *curr_analysis;
	    
	    new_analysis.kfa_info[KF_BRIDGE_LEVEL].kfi_type = KFTYPE_BRIDGE;
	    new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_type = board->brd_type;
	    new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_inst = BOARD_SLOT(board);
	    new_analysis.kfa_info[KF_BRIDGE_LINK_LEVEL].kfi_type = 
		KFTYPE_BRIDGE_LINK;
	    new_analysis.kfa_conf = *kf_conf_tab[KF_BRIDGE_LINK_CONF_INDEX];
	    
	    kf_guess_put(&new_analysis);
	}
	
	for (i = 0; i < BRIDGE_DEV_CNT; i++) {
	    if (BRIDGE_PCI_DEV_CONF(bridge_err,i)) {
		new_analysis = *curr_analysis;
	
		new_analysis.kfa_info[KF_BRIDGE_LEVEL].kfi_type = KFTYPE_BRIDGE;
		new_analysis.kfa_info[KF_BRIDGE_PCI_DEV_LEVEL].kfi_type = 
		    KFTYPE_BRIDGE_PCI_DEV;
		new_analysis.kfa_info[KF_BRIDGE_PCI_DEV_LEVEL].kfi_inst = i;
		new_analysis.kfa_conf = BRIDGE_PCI_DEV_CONF(bridge_err,i);
		
		kf_guess_put(&new_analysis);
	    }
	}
	
	KF_DEBUG("\t\tfinished bridge output analysis...\n");
	
	return KF_SUCCESS;
    }
    
    /* XXX we should be analyzing the PCI devices here */
    
    if (int_status & BRIDGE_SSRAM_PERR_MASK) {
	/* if we got a valid SSRAM parity error */
	
	if (ram_perr & BRIDGE_SIZE_FAULT_MASK) {
	    /* if bad addr, don't blame SSRAM, blame software or PCI dev */
	    
	    
	    if (pci_err_upper & BRIDGE_PCI_DEV_MASTER_MASK) {
		/* if bridge was not the PCI master blame the master device */
		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_BRIDGE_PCI_MASTER_CONF_INDEX],
				      ram_perr & BRIDGE_SIZE_FAULT_MASK,
				      90);
	    }
	    else {
		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
				      ram_perr & BRIDGE_SIZE_FAULT_MASK,
				      90);
	    }
	}
	else {
	    KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_BRIDGE_SSRAM_CONF_INDEX],
				  ram_perr & BRIDGE_PAR_BYTE_MASK,
				  90);
	}
    }
    
    KF_CONDITIONAL_UPDATE(&board->brd_confidence,
			  (int_status & BRIDGE_PCI_PARITY_MASK) ||
			  (int_status & BRIDGE_PCI_SERR_MASK) ||
			  (int_status & BRIDGE_PCI_PERR_MASK),
			  80);
    
    KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_BRIDGE_PCI_MASTER_CONF_INDEX],
			  (pci_err_upper & BRIDGE_PCI_DEV_MASTER_MASK) &&
			  ((int_status & BRIDGE_PCI_SERR_MASK) ||
			   (int_status & BRIDGE_PCI_PERR_MASK)),
			  80);


#define PCI_ABORT(_r)		((_r >> 15) & 0x1) /* abort bit in int status */
#define PCI_MASTER_TOUT(_r)	((_r >> 11) & 0x1) /* pci master timeout 
						    * bit in int status
						    */
#define PCI_RETRY_CNT(_r)	((_r >> 10) & 0x1) /* pci retry count bit in 
						    * int status
						    */	
    br_dev_found = kf_pci_slave_dev_present(board,&slot);
    /* br_dev_found == -1 implies there are problems getting the
     * bridge vertex itself in the hwgraph.
     * br_dev_found == 1 implies there is a pci slave device at pci
     *			 slot "slot".
     * br_dev_found == 0 implies there is no pci slave device at pci
     *			 slot "slot".
     */
    if (br_dev_found != KF_BRIDGE_NOT_FOUND) {
	if (br_dev_found == KF_PCI_SLAVE_FOUND) {
	    kf_conf_tab[KF_BRIDGE_PCI_DEV_CONF_INDEX] = 
		&BRIDGE_PCI_DEV_CONF(bridge_err,slot);

	    KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_BRIDGE_PCI_DEV_CONF_INDEX],
				  PCI_ABORT(int_status) ||
				  PCI_MASTER_TOUT(int_status) ||
				  PCI_RETRY_CNT(int_status),
				  90);
	} else
	    KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
				  PCI_MASTER_TOUT(int_status) ||
				  PCI_RETRY_CNT(int_status),
				  90);
    }
			      

			  
    /* check bridge's tabluar rules */
    rv  = kf_rule_tab_analyze(KF_BRIDGE_RULE_INDEX);
    
    KF_DEBUG("\t\t\tfinished bridge analysis...\n");
    
    return rv;
}


kf_result_t
kf_xbow_analyze(lboard_t *board, klxbow_t *xbow,
		kf_analysis_t *curr_analysis) {
    kf_result_t   rv = KF_SUCCESS;
    int           src_id;
    int           i;
    lboard_t      *widget_board;
    lboard_t      *src_widget_board;
    klinfo_t      *bridge;
    xbowreg_t	xbow_status;
    xbowreg_t	err_cmd;
    xbowreg_t	link_status;
    klxbow_err_t  *xbow_err = XBOW_COMP_ERROR(board,(klinfo_t*)xbow);
    kf_analysis_t new_analysis;

    
    xbow_status = xbow_err->xe_dmp.xb_status;
    err_cmd = xbow_err->xe_dmp.xb_err_cmd;

    if (!curr_analysis) {
        /* If we are starting analysis, initialize confidences to zero */

	XBOW_CONF(xbow_err) = 0;
    }
  
    /* set up data for tabular rules */
    kf_conf_tab[KF_XBOW_CONF_INDEX] = &XBOW_CONF(xbow_err);
    kf_reg_tab[KF_XBOW_WIDGET_0_STATUS_INDEX] = xbow_status;

    if ((curr_analysis != NULL) && (*kf_conf_tab[KF_XBOW_CONF_INDEX])) {
	/* if we suspect the xbow */

	/* setup analysis information */
	new_analysis = *curr_analysis;

	new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_type = KFTYPE_XBOW;

	/* for hw graph output we need the slot of the master hub */
	new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_inst = 
	   BOARD_SLOT(XBOW_PORT_BOARD(xbow,xbow->xbow_master_hub_link));

	new_analysis.kfa_conf = *kf_conf_tab[KF_XBOW_CONF_INDEX];
	kf_guess_put(&new_analysis);
    }
  
    /* Do a depth first analysis, so go to widgets first */
    for(i = BASE_XBOW_PORT; i < MAX_PORT_NUM; i++) {

	/* skip invalid widget numbers */
	if (!XBOW_WIDGET_IS_VALID(i))
	    continue;

	/* if there is a widget attached to this port */
	if (XBOW_PORT_IS_ENABLED(xbow,i)) {

	    widget_board = XBOW_PORT_BOARD(xbow,i);
      
	    KF_ASSERT(widget_board);
#if !defined(FRUTEST) && !defined(_STANDALONE)
	    ASSERT_ALWAYS(widget_board);
#endif
      
	    /* set up data for use in tabular rules  */
	    kf_reg_tab[KF_XBOW_LINK_STATUS_INDEX] = 
		xbow_err->xe_dmp.xb_link_status[i];

	    if (curr_analysis != NULL) {
		/* setup analysis information */
		new_analysis = *curr_analysis;
	    }

	    switch (widget_board->brd_type) {
	    case KLTYPE_IP27:
		/* IP27's will analyze themselves */
		continue;
		
	    case KLTYPE_GSN_A:
	    case KLTYPE_GSN_B:
		    /* Skip the GSN */
		continue;

	    case KLTYPE_GFX:
		/* XXX not quite sure what to do here */
		/* (can't really do much of anything) */
		continue;

	    default:

		    if (KLCLASS(widget_board->brd_type) == KLCLASS_IO) {
			    bridge = find_first_component(widget_board,
							  KLSTRUCT_BRI);
			    KF_ASSERT(bridge);
		
			    if (curr_analysis != NULL) {
				    new_analysis.kfa_info[KF_WIDGET_LEVEL].
					    kfi_type = widget_board->brd_type;
				    new_analysis.kfa_info[KF_WIDGET_LEVEL].
					    kfi_inst = BOARD_SLOT(widget_board);
		    
				    rv = kf_bridge_analyze(widget_board,bridge,
							   &new_analysis);
			    }
			    else {
				    rv = kf_bridge_analyze(widget_board,bridge,
							   curr_analysis);
			    }
		    }
		    else {
#ifdef DEBUG
			    kf_print("unrecognized widget board type\n");
#endif
			    continue;
		    }
		break;
	    }

	    if (curr_analysis != NULL) {

		if ((widget_board->brd_confidence) && 
		    (widget_board->brd_type != KLTYPE_IP27)) {
		    /* if it's not the IP27 add to the list */

		    /* setup analysis information */
		    new_analysis = *curr_analysis;

		    new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_type =
			widget_board->brd_type;
		    new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_inst = 
			BOARD_SLOT(widget_board);
		    new_analysis.kfa_conf = widget_board->brd_confidence;

		    kf_guess_put(&new_analysis);
		}
	    }
	
	    /* if we are not in output phase, check the link */
	    else {
		/* check the link status register */
		/* subtract the base because only lower ports are used */
		link_status = xbow_err->xe_dmp.xb_link_status[i - 
							     BASE_XBOW_PORT];

		/* this is a special rule for down rev xbows */
		if (xbow->xbow_info.struct_version < 3) {
			KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_XBOW_CONF_INDEX],
					      (link_status & 
					       XBOW_LINK_OVER_ALLOC_MASK) &&
					      (link_status & 
					       XBOW_LINK_LLP_REC_ERR_MASK),
					      FRU_FLAG_CONF);
		}
                    
		/* illegal destination bit set */
		KF_CONDITIONAL_UPDATE(&widget_board->brd_confidence,
				      link_status & XBOW_LINK_ILL_DEST_MASK,
				      70);

		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
				      link_status & XBOW_LINK_ILL_DEST_MASK,
				      60);
		
		/* input over allocation error bit set */
		KF_CONDITIONAL_UPDATE(&widget_board->brd_confidence,
				      link_status & 
				      XBOW_LINK_OVER_ALLOC_MASK,
				      70);

		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_SOFTWARE_CONF_INDEX],
				      link_status & 
				      XBOW_LINK_OVER_ALLOC_MASK,
				      60);
                    
		/* max transmitter retry set */
		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_BRIDGE_LINK_CONF_INDEX],
				      link_status & 
				      XBOW_LINK_MAX_TRANS_RETRY_MASK,
				      80);
		
		/* connection time-out error bit set */
		KF_CONDITIONAL_UPDATE(&XBOW_CONF(xbow_err),
				      (link_status & 
				       XBOW_LINK_CONN_TOUT_ERR_MASK),
				      90);
		
		/* packet time-out error source bit set */
		KF_CONDITIONAL_UPDATE(&widget_board->brd_confidence,
				      link_status & 
				      XBOW_LINK_PKT_TOUT_SRC_MASK,
				      70);
		
		KF_CONDITIONAL_UPDATE(kf_conf_tab[KF_BRIDGE_LINK_CONF_INDEX],
				      link_status & 
				      XBOW_LINK_PKT_TOUT_SRC_MASK,
				      70);
	    } /* else */
	}/* if XBOW_PORT_IS_ENABLED */
    } /* for */

    if (curr_analysis == NULL) {
	/* now analyze xbow error state */
    
	if (xbow_status & 0x2) {    
	    /* get source widget id */
	    src_id =  (err_cmd >> XBOW_SRC_WIDGET_SHIFT) & XBOW_SRC_WIDGET_MASK;

	    
	    /* if the source id is a valid wiget and there is a device attached */
	    if (XBOW_WIDGET_IS_VALID(src_id) && 
		XBOW_PORT_IS_ENABLED(xbow,src_id)) {
		klbridge_err_t *bridge_err;

		src_widget_board = XBOW_PORT_BOARD(xbow,src_id);
		
		/* crosstalk error */
		KF_CONDITIONAL_UPDATE(&src_widget_board->brd_confidence,
				      xbow_status & 0x2,
				      70);
		
		/* Skip if we are looking at GSN */
		if ((KLCLASS(widget_board->brd_type) == KLCLASS_IO) &&
		    ! ((widget_board->brd_type == KLTYPE_GSN_A) ||	
		       (widget_board->brd_type == KLTYPE_GSN_B))) {
		    bridge = find_first_component(src_widget_board, KLSTRUCT_BRI);

		    KF_ASSERT(bridge);

		    bridge_err = BRI_COMP_ERROR(src_widget_board,bridge);

		    KF_CONDITIONAL_UPDATE(&BRIDGE_LINK_CONF(bridge_err),
					  xbow_status & 0x2,
					  70);
		}
	    }
	}
    
	/* check xbow's tabluar rules */
	rv  = kf_rule_tab_analyze(KF_XBOW_RULE_INDEX);

	KF_DEBUG("\t\tfinished xbow analysis...\n");
    }
    else {
	KF_DEBUG("\t\tfinished xbow output analysis...\n");
    }

    return rv;
}



kf_result_t
kf_io_analyze(nasid_t nasid, kf_analysis_t *curr_analysis) {	
    lboard_t 	  *board;
    kf_result_t	  rv;
    klxbow_t      *xbow = NULL, *xbow2 = NULL;
    kf_analysis_t new_analysis;

#if !defined(_STANDALONE)
     /* This allows us to set an mtune variable to disable IO analysis. */
     if (!full_fru_analysis)
       return KF_SUCCESS;
#endif

    KF_DEBUG("beginning IO analysis...\n");

#ifdef FRUTEST
    board = find_lboard((lboard_t *)g_node[nasid].ch_board_info,
			KLTYPE_MIDPLANE8);
#else
    board	= find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
			      KLTYPE_MIDPLANE8);
#endif /* ifdef FRUTEST */

    if (board) {
	if (board->brd_flags & DUPLICATE_BOARD) {
	    /* we've already done this one */
	    return KF_SUCCESS;
	}
	xbow = (klxbow_t*)find_first_component(board,KLSTRUCT_XBOW);
    }

    if (xbow) {
	/* find if second xbow is attached */
	xbow2 = (klxbow_t*)find_component(board,(klinfo_t*)xbow,KLSTRUCT_XBOW);

	KF_DEBUG("\t\tbeginning xbow 1 analysis...\n");
	rv = kf_xbow_analyze(board,xbow,curr_analysis);
	
	if (xbow2) {
	    KF_DEBUG("\t\tbeginning xbow 2 analysis...\n");
	    rv = kf_xbow_analyze(board,xbow2,curr_analysis);
	}
    }
    else {
	/* no xbow attached so check if we have a baseio attached to the hub */
#ifdef FRUTEST
	board = find_lboard((lboard_t *)g_node[nasid].ch_board_info,
			    KLTYPE_BASEIO);
#else
	board = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),KLTYPE_BASEIO);
#endif /* ifdef FRUTEST */
    
	if (board) {
	    klinfo_t *bridge = find_first_component(board,KLSTRUCT_BRI);


	    /* setup data for tabular rules */
	    kf_conf_tab[KF_XBOW_CONF_INDEX] = 0;

	    if (curr_analysis != NULL) {
		/* setup output data */
		new_analysis = *curr_analysis;
		new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_type = 
		    KFTYPE_BASEIO;
		new_analysis.kfa_info[KF_WIDGET_LEVEL].kfi_inst = 
		    BOARD_SLOT(board);

		rv = kf_bridge_analyze(board,bridge,&new_analysis);
	    }
	    else {
		rv = kf_bridge_analyze(board,bridge,curr_analysis);
	    }
	}
	else {
	    /* no io is attached so we're done */
	    rv = KF_SUCCESS;
	}
    }

    KF_DEBUG("finished IO analysis...\n");

    return rv;
}











