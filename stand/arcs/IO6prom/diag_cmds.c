/***********************************************************************\
*       File:           diag_cmds.c                                     *
*                                                                       *
*       Contains routines for invoking IO6 prom based POD diags via     *
*       the command line.                                               *
*                                                                       *
\***********************************************************************/

#include <stdlib.h>
#include <setjmp.h>
#include <ctype.h>
#include <libkl.h>
#include <promgraph.h>
#include <report.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/addrs.h>
#include <sys/SN/error.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/ioc3.h>
#include <sys/xtalk/xbow.h>
#include <diag_lib.h>
#include <diag_enet.h>
#include <pgdrv.h>

#ifdef SN_PDI
extern graph_hdl_t	pg_hdl ;
extern vertex_hdl_t	snpdi_rvhdl ;
#else
extern vertex_hdl_t	hw_vertex_hdl ;
extern graph_hdl_t	prom_graph_hdl ;
#endif

extern int sprintf(char *, const char *, ...);
__psunsigned_t get_ioc3_base(nasid_t nasid);

static nasid_t get_nasid_from_mod_slot(int modid, int slotid);
     

char  diag_name[128];

static int  diag_run_cmd(int argc, char **argv);
static void print_llp_regs(void);

static int enet_cmd, scsi_cmd;
 
static nasid_t nasid;

static int there_is_a_xbow; 
static xbow_t *xbow_bp;
static bridge_t *bridge_bp;

static hubii_illr_t *hub_illr;

static xbwX_stat_t *xbow_hub_port_link_status;
static xbow_aux_link_status_t *xbow_hub_port_aux_link_status;
static xbwX_stat_t *xbow_bridge_port_link_status;
static xbow_aux_link_status_t *xbow_bridge_port_aux_link_status;


/* diag_enet_cmd: Runs enet diags on a single IOC3.
   
   Syntax:
   
   diag_enet [-N <nasid>] [-m <moduleid> -s <slotid>] [-p <pcidev>] [-h] [-e] [-v] [-t <test name>] [-r | -R <count>]

     -N <nasid> :     Specify nasid number (defaults to 
                      console IO6 module).  This option 
                      supersedes the nasid implied by 
                      module and slot.
		      
     -m <moduleid> :  Specify module number (defaults to 
                      console IO6 module)

     -s <slotid> :    Specify slot name (defaults to 
                      console IO6 slot)  e.g. -s io7

     -p <pcidev> :    Specify PCI device number of target IOC3 (defaults to
                      2, which is the device number of the IOC3 on an IO6.)
      
     -h :             Run the diags in heavy mode.

     -e or -x:        Run the diags in external loopback mode. 

     -v :             Verbose mode 

     -t <testname> :  Run only specified test.  Legal values: "ssram",
                      "txclk", "phyreg", "nic", "ioc3_loop", "phy_loop",
                      "tw_loop", "ext_loop", "ext_loop_10", "ext_loop_100",
                      and "xtalk".  Default is to run all tests.
		      
     -r <count> :     Repeat test(s) <count> times; stop if any test fails.

     -R <count> :     Repeat test(s) <count> times; continue on failure.

*/
int 
diag_enet_cmd(int argc, char **argv)
{
  enet_cmd = 1;
  scsi_cmd = 0;
  return(diag_run_cmd(argc, argv));
}

static void diag_enet_usage(void)
{
  printf("Usage: diag_enet [-N <nasid>] [-m <moduleid>] [-s <slotid>] [-p <pcidev>] [-h] [-e] [-v] [-t <test name>] [-r <count>]\n");
}


/* diag_scsi_cmd: Runs SCSI diags on a single ISP1040.
   
   Syntax:
   
   diag_scsi [-N <nasid>] [-m <moduleid> -s <slotid>] [-p <pcidev>] [-h] [-v] [-t <test name>] [-r | -R <count>]

   Options are the same as diag_enet, with the following exceptions:

     - The -e option is ignored

     - The default PCI device number is 0 if a -p option is not specified.
     
     - The valid values for the -t option are "ram", "dma", and "controller".
       The default is to run all tests.  
*/

int 
diag_scsi_cmd(int argc, char **argv)
{
  enet_cmd = 0;
  scsi_cmd = 1;
  return(diag_run_cmd(argc, argv));
}

static void diag_scsi_usage(void)
{
  printf("Usage: diag_scsi [-N <nasid>] [-m <moduleid>] [-s <slotid>] [-p <pcidev>] [-h] [-e] [-v] [-t <test name>] [-r <count>]\n");
}


static int
diag_run_cmd(int argc, char **argv)
{
  int ii, force_nasid, force_modid, force_slotid, modid, slotid, npci, 
  widget, heavy, external, verbose, diag_mode, diag_rc, test_all, test_ssram, 
  test_txclk, test_phyreg, test_nic, test_loop_ioc3, test_loop_phy, 
  test_loop_tw, test_loop_ext, test_loop_ext_10, test_loop_ext_100,  
  test_xtalk, test_scsi_ram, test_scsi_dma, test_scsi_controller, repeat_count, 
  report_interval, stop_on_error;

  __uint64_t node_wid;

  __psunsigned_t bridge_baseAddr;
  
  jmp_buf       fault_buf;  
  void         *old_buf;

  /* Set default values */
  diag_mode = DIAG_MODE_NORMAL;
  there_is_a_xbow = 1;
  force_nasid = 0;
  force_modid = 0;
  force_slotid = 0;
  modid    = 1;  
  slotid   = -1;
  if (enet_cmd) npci = 2;  /* IO6 pci device ID of IOC3 */
  if (scsi_cmd) npci = 0;  /* IO6 pci device ID of QL */
  heavy    = 0;
  external = 0;
  verbose  = 0;
  test_all = 1;
  test_ssram = 0;
  test_txclk = 0;
  test_phyreg = 0;
  test_nic = 0;
  test_loop_ioc3 = 0;
  test_loop_phy = 0;
  test_loop_tw = 0;
  test_loop_ext = 0;
  test_loop_ext_10 = 0;
  test_loop_ext_100 = 0;
  test_xtalk = 0;
  test_scsi_ram = 0;
  test_scsi_dma = 0;
  test_scsi_controller = 0;
  repeat_count = 1;
  stop_on_error = 1;

  /* Get default values for nasid, bridge_baseAddr, and widget */
  nasid = get_nasid();
  bridge_baseAddr = get_ioc3_base(nasid) & ~SWIN_SIZEMASK;
  widget = WIDGETID_GET(bridge_baseAddr);
  if (widget == 0) there_is_a_xbow = 0;

  /* Check arguments */
  for (ii = 1; ii < argc; ii++) {
    if (argv[ii][0] == '-') {
      switch (argv[ii][1]) {
      case 'n':
      case 'N':
        if (++ii == argc) {
          printf("Error: Missing nasid number\n");
	  return 0 ;
	}
	
        if (!(isdigit(argv[ii][0]))) {
          printf("Error: NASID must be a decimal number >= 1\n");
          return 0 ;
        }

        nasid = atoi(argv[ii]) ;
	force_nasid = 1;
        break ;
	
      case 'm':
        if (++ii == argc) {
          printf("Error: Missing module number\n");
	  return 0 ;
	}
	
        if (!(isdigit(argv[ii][0]))) {
          printf("Error: Module id must be a decimal number >= 1\n");
          return 0 ;
        }

        modid = atoi(argv[ii]) ;
	if (modid <= 0) {  /* XXX - check max module id */
	  printf("Error: Invalid module number %d\n", modid) ;
	  return 0 ;
	}
	force_modid = 1;
        break ;
	
      case 's':
        if (++ii == argc)
            return 0 ;
	
	if (strncmp(argv[ii], "io", 2)) {
	  printf("Error: Slotname must begin with \'io\'\n") ;
	  return 0 ;
	}
        if (!(isdigit(argv[ii][2]))) {
          printf("Error: Slotnumber must be a decimal number >= 1\n");
          return 0 ;
        }
	
	slotid = atoi(&argv[ii][2]) ;
	if ((slotid <= 0) || (slotid > 12)) {
	  printf("Error: Invalid IO slot number %d\n", slotid) ;
	  return 0 ;
	}
	force_slotid = 1;
        break ;
	
      case 'p':
        if (++ii == argc) {
          printf("Error: Missing PCI device number\n");
	  return 0 ;
	}
	
        if (!(isdigit(argv[ii][0]))) {
          printf("Error: PCI device must be a decimal number (0:7)\n");
          return 0 ;
        }

        npci = atoi(argv[ii]) ;
        break ;

      case 'h':
	heavy = 1;
        break ;

      case 'e':
      case 'x':
	external = 1;
        break ;

      case 'v':
	verbose = 1;
        break ;

      case 't':
        if (++ii == argc) {
          printf("Error: Missing test name\n");
	  return 0 ;
	}
	
	if (enet_cmd) {
	  if (!strcmp(argv[ii], "ssram")) test_ssram = 1;
	  else if (!strcmp(argv[ii], "txclk") || !strcmp(argv[ii], "tx_clk")) 
	      test_txclk = 1;
	  else if (!strcmp(argv[ii], "phyreg") || !strcmp(argv[ii], "phy_reg")) 
	      test_phyreg = 1;
	  else if (!strcmp(argv[ii], "nic")) test_nic = 1;
	  else if (!strcmp(argv[ii], "loop_ioc3") || !strcmp(argv[ii], "ioc3_loop"))
	      test_loop_ioc3 = 1;
	  else if (!strcmp(argv[ii], "loop_phy") || !strcmp(argv[ii], "phy_loop"))
	      test_loop_phy = 1;
	  else if (!strcmp(argv[ii], "loop_tw") || !strcmp(argv[ii], "tw_loop")) 
	      test_loop_tw = 1;
	  else if (!strcmp(argv[ii], "loop_ext") || !strcmp(argv[ii], "ext_loop")) 
	      test_loop_ext = 1;
	  else if (!strcmp(argv[ii], "loop_ext_10") || !strcmp(argv[ii], "ext_loop_10")) 
	      test_loop_ext_10 = 1;
	  else if (!strcmp(argv[ii], "loop_ext_100") || !strcmp(argv[ii], "ext_loop_100")) 
	      test_loop_ext_100 = 1;
	  else if (!strcmp(argv[ii], "xtalk")) test_xtalk = 1;
	  else {
	    printf("Error: There is no test named %s\n", argv[ii]);
	    printf("       Valid choices are: \n");
	    printf("            \"ssram\"\n");
	    printf("            \"txclk\"\n");
	    printf("            \"phyreg\"\n");
	    printf("            \"nic\"\n");
	    printf("            \"ioc3_loop\"\n");
	    printf("            \"phy_loop\"\n");
	    printf("            \"tw_loop\"\n");
	    printf("            \"ext_loop\"\n");
	    printf("            \"ext_loop_10\"\n");
	    printf("            \"ext_loop_100\"\n");
	    printf("            \"xtalk\"\n");
	    return 0 ;
	  }
	
	  test_all = 0;
	}
	
	if (scsi_cmd) {
	  if (!strcmp(argv[ii], "ram")) test_scsi_ram = 1;
	  else if (!strcmp(argv[ii], "dma")) test_scsi_dma = 1;
	  else if (!strcmp(argv[ii], "controller")) test_scsi_controller = 1;
	  else {
	    printf("Error: There is no test named %s\n", argv[ii]);
	    printf("       Valid choices are: \n");
	    printf("            \"ram\"\n");
	    printf("            \"dma\"\n");
	    printf("            \"controller\"\n");
	    return 0 ;
	  }
	
	  test_all = 0;
	}
	
        break ;

      case 'r':
      case 'R':
	if (argv[ii][1] == 'R') stop_on_error = 0;
	
        if (++ii == argc) {
          printf("Error: Missing repeat count\n");
	  return 0 ;
	}
	
        if (!(isdigit(argv[ii][0]))) {
          printf("Error: repeat count must be a number\n");
          return 0 ;
        }
        repeat_count = atoi(argv[ii]);
        break ;
	
      case 'f':            /* OBSOLETE */
        break ;

      default:
        if(enet_cmd)
          diag_enet_usage();
        else if(scsi_cmd)
          diag_scsi_usage();
        return 0 ;
      }
    }
    else {
      if(enet_cmd)
        diag_enet_usage();
      else if(scsi_cmd)
        diag_scsi_usage();
      return 0 ;
    }
  }

  if (ii != argc)
      printf("Extra argument at end of list ignored\n") ;

  /* Check for illegal option combinations */
  if (!there_is_a_xbow && force_slotid) {
    printf("Error: Cannot use -s option on a machine without a Xbow.\n") ;
    return 0 ;
  }

  if (force_modid && !force_slotid) {
    printf("Error: Slot must be also be specified using -s if -m option is used.\n") ;
    return 0 ;
  }

  if (force_nasid && !force_slotid && there_is_a_xbow) {
    printf("Error: Slot must be also be specified using -s if -n option is used.\n") ;
    return 0 ;
  }
  
  if (!force_nasid && !force_modid && force_slotid) {
    printf("WARNING: Slot number specified without -n or -m option; assuming module 1.\n") ;
  }
  
  /* Form diag_mode parm */
  if (external && enet_cmd) diag_mode = DIAG_MODE_EXT_LOOP; 
  else if (heavy) diag_mode = DIAG_MODE_HEAVY;

  if (verbose) diag_mode |= DIAG_FLAG_VERBOSE;

  if (heavy && verbose && scsi_cmd) diag_mode = DIAG_MODE_MFG;
  
  if (force_slotid) {
    if (!force_nasid) {
      /* Get NASID from module/slot */
      nasid = get_nasid_from_mod_slot(modid, slotid);
      if (nasid == INVALID_NASID) { 
	printf("Error: No node is associated with module %d slot io%d.\n", modid, slotid);
	return 0 ;
      }
    }
    
    /* Calculate widget number from slot */
    widget = slot_to_widget(slotid - 1) & 0xf;
    
    /* Calculate bridge base address */
    bridge_baseAddr = NODE_SWIN_BASE(nasid, widget);
  }

  bridge_bp = (bridge_t *) bridge_baseAddr;

  /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf)) {
      printf("\n=====> %s diag took an exception. <=====\n", diag_name);
      diag_print_exception();
#ifdef DEBUG
      kl_log_hw_state();
      kl_error_show_log("Hardware Error State: (Forced error dump)\n",
			"END Hardware Error State (Forced error dump)\n");
#endif
      diag_free((__uint64_t *) 0xdeadULL);  /* diag_free ignores the pointer it's passed */
      result_fail(diag_name, KLDIAG_ENET_EXCEPTION, "Took an exception"); 
      restorefault(old_buf);
      return(KLDIAG_ENET_EXCEPTION);
  }
  
  /* Read Node WID from HUB II WCR */
  node_wid = (__uint64_t) (REMOTE_HUB_L(nasid, IIO_WCR)) & 0xf;
  
  /* Get address of ILLR register */
  hub_illr = (hubii_illr_t *) REMOTE_HUB_ADDR(nasid, IIO_ILLR);

  /* Calculate XBOW addresses */
  if (there_is_a_xbow) {
    xbow_bp = (xbow_t *) NODE_SWIN_BASE(nasid, XBOW_WIDGET_ID);
    xbow_hub_port_link_status = (xbwX_stat_t *) &(xbow_bp->xb_link(node_wid).link_status);
    xbow_hub_port_aux_link_status = (xbow_aux_link_status_t *) &(xbow_bp->xb_link(node_wid).link_aux_status);
    xbow_bridge_port_link_status = (xbwX_stat_t *) &(xbow_bp->xb_link(widget).link_status);
    xbow_bridge_port_aux_link_status = (xbow_aux_link_status_t *) &(xbow_bp->xb_link(widget).link_aux_status);
  }

  /* Reset the malloc pointer.  This is necessary if someone hit ^C
     while the diags were running. */
  diag_free((__uint64_t *) 0xdeadULL);  /* diag_free ignores the pointer it's passed */
  
  /* Call selected diags */
  ii = repeat_count;
  diag_rc = KLDIAG_PASSED;
  do {

    if (enet_cmd) {
    
      if (test_all && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_all(diag_mode, bridge_baseAddr, npci);
    
      if (test_ssram && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_ssram(diag_mode, bridge_baseAddr, npci);
    
      if (test_txclk && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_tx_clk(diag_mode, bridge_baseAddr, npci);
    
      if (test_phyreg && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_phy_reg(diag_mode, bridge_baseAddr, npci);
    
      if (test_nic && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_nic(diag_mode, bridge_baseAddr, npci);
    
      if (test_loop_ioc3 && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, IOC3_LOOP);
    
      if (test_loop_phy && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, PHY_LOOP);
    
      if (test_loop_tw && (diag_rc == KLDIAG_PASSED || !stop_on_error))  
	  diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, TWISTER_LOOP);
    
      if (test_loop_ext && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, EXTERNAL_LOOP);

      if (test_loop_ext_10 && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, EXTERNAL_LOOP_10);

      if (test_loop_ext_100 && (diag_rc == KLDIAG_PASSED || !stop_on_error)) 
	  diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, EXTERNAL_LOOP_100);

      if (test_xtalk && (diag_rc == KLDIAG_PASSED || !stop_on_error)) { 

	if (heavy) report_interval = 10;
	else report_interval = 50;

	if (!(ii % report_interval) && (ii > 1)) { 
	  printf("%d/%d iterations remaining . .\n", 
		 ii, repeat_count);
	}
      
	if (ii == repeat_count) { 
	
	  printf("\nStarting retry counts:\n");
	  print_llp_regs();
	}
      
	if (!(ii % report_interval) && (ii != repeat_count)) {
	
	  print_llp_regs();
	}
      
	diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, XTALK_STRESS);
      
	if (ii == 1) {
	
	  printf("\nEnding retry counts:\n");
	  print_llp_regs();
	}
      }
    }
    
    if (scsi_cmd) {
    
      if ((test_all || test_scsi_ram) && (diag_rc == KLDIAG_PASSED || !stop_on_error))
	  diag_rc = diag_scsi_mem(diag_mode, bridge_baseAddr, npci);
      
      if ((test_all || test_scsi_dma) && (diag_rc == KLDIAG_PASSED || !stop_on_error))
	  diag_rc = diag_scsi_dma(diag_mode, bridge_baseAddr, npci);
      
      if ((test_all || test_scsi_controller) && (diag_rc == KLDIAG_PASSED || !stop_on_error))
	  diag_rc = diag_scsi_selftest(diag_mode, bridge_baseAddr, npci);
    }
    
    diag_free((__uint64_t *) 0xdeadULL);  /* diag_free ignores the pointer it's passed */
    
  } while (--ii > 0 && (diag_rc == KLDIAG_PASSED || !stop_on_error));
  
  restorefault(old_buf);
  return(0);
}


static void
print_llp_regs(void) 
{
  printf("                                  Out path    In path\n");
  printf("                                  --------    --------\n");
  printf(" ==> Hub retries                  TX = %3d    RX =   %d+%d  (CB+SN)\n",
	 IIO_WSTAT_TXRETRY_CNT((__uint64_t) (REMOTE_HUB_L(nasid, IIO_WSTAT))),
	 hub_illr->illr_fields_s.illr_cb_cnt, hub_illr->illr_fields_s.illr_sn_cnt);
  if (there_is_a_xbow) {
    printf(" ==> Xbow retries (hub side)      RX = %3d    TX = %3d   ",
	   (*((__uint32_t *)xbow_hub_port_aux_link_status) & XB_AUX_STAT_RCV_CNT ) >> 24, 
	   (*((__uint32_t *)xbow_hub_port_aux_link_status) & XB_AUX_STAT_XMT_CNT ) >> 16);
    printf("%s%s\n", 
	   (*((__uint32_t *)xbow_hub_port_link_status) & XB_STAT_RCV_CNT_OFLOW_ERR) ? 
	   "*RX overflow* " : "", 
	   (*((__uint32_t *)xbow_hub_port_link_status) & XB_STAT_XMT_CNT_OFLOW_ERR) ? 
	   "*TX overflow* " : ""); 
    printf(" ==> Xbow retries (bridge side)   TX = %3d    RX = %3d   ",
	   (*((__uint32_t *)xbow_bridge_port_aux_link_status) & XB_AUX_STAT_XMT_CNT) >> 16, 
	   (*((__uint32_t *)xbow_bridge_port_aux_link_status) & XB_AUX_STAT_RCV_CNT) >> 24);
    printf("%s%s\n", 
	   (*((__uint32_t *)xbow_bridge_port_link_status) & XB_STAT_XMT_CNT_OFLOW_ERR) ? 
	   "*TX overflow* " : "", 
	   (*((__uint32_t *)xbow_bridge_port_link_status) & XB_STAT_RCV_CNT_OFLOW_ERR) ? 
	   "*RX overflow* " : "");
  }
  printf(" ==> Bridge retries               RX = %3d    TX = %3d\n",
	 (bridge_bp->b_wid_stat & 
	  BRIDGE_STAT_LLP_REC_CNT) >> 24, 
	 (bridge_bp->b_wid_stat & 
	  BRIDGE_STAT_LLP_TX_CNT) >> 16);
}

/* This function was adapted from flash_getnasid */
nasid_t
get_nasid_from_mod_slot(int modid, int slotid)
{
  char		tmp_buf[128];
  graph_error_t	graph_err ;
  vertex_hdl_t	node_vhdl, xwidget_vhdl ;
  klinfo_t	*kliptr ;
  slotid_t	s;
  int 		x, w;

  w = slot_to_widget(slotid-1) ; /* get the composite widget no */
  x = (w&SLOTNUM_CLASS_MASK) >> 4 ; /* xbow number */
  w &= SLOTNUM_SLOT_MASK ;	/* widget number */

  /* With the xbow number known we have only 2 alternate paths to
     our slot. Check both of them out. 
     
     Xbow 0 - n0 and n2
     Xbow 1 - n1 and n3
     */

  /* Construct the graph path */

  sprintf(tmp_buf, "/module/%d/slot/n%d/node", modid, (1+x)) ;
#ifdef SN_PDI
  graph_err = hwgraph_path_lookup(snpdi_rvhdl, tmp_buf,
				  &node_vhdl, NULL) ;
#else
  graph_err = hwgraph_path_lookup(hw_vertex_hdl, tmp_buf,
				  &node_vhdl, NULL) ;
#endif
  if (graph_err != GRAPH_SUCCESS) {
    sprintf(tmp_buf, "/module/%d/slot/n%d/node", modid, (3+x)) ;
#ifdef SN_PDI
    graph_err = hwgraph_path_lookup(snpdi_rvhdl, tmp_buf,
				    &node_vhdl, NULL) ;
#else
    graph_err = hwgraph_path_lookup(hw_vertex_hdl, tmp_buf,
				    &node_vhdl, NULL) ;
#endif
    if (graph_err != GRAPH_SUCCESS) {
      printf("Unable to find link between a node and the IO slot.\n", 
	     tmp_buf) ;
      return INVALID_NASID ;
    }
  }

  /* Look for INFO and get the nasid from the struct */

#ifdef SN_PDI
  graph_err = graph_info_get_LBL(pg_hdl, node_vhdl,
				 INFO_LBL_KLCFG_INFO, NULL,
				 (arbitrary_info_t *)&kliptr) ;
#else
  graph_err = graph_info_get_LBL(prom_graph_hdl, node_vhdl,
				 INFO_LBL_KLCFG_INFO, NULL,
				 (arbitrary_info_t *)&kliptr) ;
#endif
  if (graph_err != GRAPH_SUCCESS) {
    printf("Error in promgraph. No info found for %s\n", tmp_buf) ;
    return INVALID_NASID ;
  }

  return kliptr->nasid;

}

