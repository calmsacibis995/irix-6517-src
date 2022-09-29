/***********************************************************************\
*       File:           diag_cmds.c                                     *
*                                                                       *
*       Contains routines for invoking IO6 prom based POD diags via     *
*       the command line.                                               *
*                                                                       *
\***********************************************************************/

/*XXX*/
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
/*XXX*/

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <ctype.h>
#if XXX_PORT
#include <sys/KLEGO/kldiag.h>
#include <sys/KLEGO/klconfig.h>
#include <libkl.h>
#include "../lib/libkl/diags/diag_lib.h"
#include "../lib/libsk/ml/diag_enet.h"
#endif
#include <sys/PCI/bridge.h>
#include <sys/xtalk/xbow.h>
#include <diag_enet.h>
#include "diag_port.h"


extern int _symmon;

/* diag_enet_cmd: Runs enet diags on a single IOC3.
   
   Syntax:
   
   diag_enet [-n <nasid>] [-m <moduleid> -s <slotid>] [-p <pcidev>] [-h] [-e] [-v] [-t <test name>] [-r <count>]

----for KLEGO----
     -n <nasid> :     Specify nasid number (defaults to 
                      console IO6 module).  This option 
                      supersedes the nasid implied by 
                      module and slot.
		      
     -m <moduleid> :  Specify module number (defaults to 
                      console IO6 module)

     -s <slotid> :    Specify slot name (defaults to 
                      console IO6 slot)  e.g. -s io7
                      
----for SR------
     -w <widid> :     Widget port # of enet card, default to
     		      baseIO ioc3, 15

     -p <pcidev> :    Specify PCI device number of target IOC3 (defaults to
                      2, which is the device number of the IOC3 on an IO6.)
      
     -h :             Run the diags in heavy mode.

     -e or -x:        Run the diags in external loopback mode. 

     -v :             Verbose mode 

     -t <testname> :  Run only specified test.  Legal values: "ssram",
                      "txclk", "phyreg", "nic", "ioc3_loop", "phy_loop",
                      "tw_loop", "ext_loop", "ext_loop_10", and "ext_loop_100".
                      Default is to run all tests.
		      
     -r <count> :     Repeat test <count> times.

*/
int
diag_enet_cmd(int argc, char **argv)
{
  int ii, npci, widget, heavy, external, 
  verbose, diag_mode, diag_rc, test_all, test_ssram, 
  test_txclk, test_phyreg, test_nic, test_loop_ioc3, test_loop_phy, 
  test_loop_tw, test_loop_ext, test_loop_ext_10, test_loop_ext_100,  
  test_xtalk, repeat_count, report_interval;

#if SN0
  int force_nasid, modid, slotid;
  __uint64_t nasid, node_wid;
#endif

  __psunsigned_t bridge_baseAddr;

  xbow_t *xbow_bp;
  bridge_t *bridge_bp;
  
#if SN0
  hubii_illr_t *hub_illr;
      
  xbwX_stat_t *xbow_hub_port_link_status;
  xbow_aux_link_status_t *xbow_hub_port_aux_link_status;
#elif IP30
  xbwX_stat_t *xbow_heart_port_link_status;
  xbow_aux_link_status_t *xbow_heart_port_aux_link_status;
#endif
  xbwX_stat_t *xbow_bridge_port_link_status;
  xbow_aux_link_status_t *xbow_bridge_port_aux_link_status;

  jmp_buf       fault_buf;  
  void         *old_buf;

  /* Set default values */
  diag_mode = DIAG_MODE_NORMAL;
#if SN0
  force_nasid = 0;
  modid    = 1;  /* XXX - needs to change to module id of console */
  slotid   = 1;  /* XXX - needs to change to slot id of console IO6 */
#elif IP30
  widget = XBOW_PORT_F;
#endif
  npci     = 2;  /* P1 IO6 pci device ID of IOC3 */
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
  repeat_count = 1;

  /* Check arguments */
  for (ii = 1; ii < argc; ii++) {
    if (argv[ii][0] == '-') {
      switch (argv[ii][1]) {
#ifdef KLEGO
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

#elif IP30
      case 'w':
        if (++ii == argc) {
          printf("Error: Missing widget port number\n");
	  return 0 ;
	}
	
        if (!(isdigit(argv[ii][0]))) {
          printf("Error: widget port must be a decimal number >= 1\n");
          return 0 ;
        }

        widget = atoi(argv[ii]) ;
	if (widget <= XBOW_PORT_8 && widget > XBOW_PORT_F) {
	  printf("Error: widget port must be > XBOW_PORT_8 && <= XBOW_PORT_F\n");
	  widget = XBOW_PORT_F;
	  return 0 ;
	}
        break ;

      case 'd':
      case 'D': {
	void dump_ioc3_regs(__psunsigned_t, __psunsigned_t);

	bridge_baseAddr = K1_MAIN_WIDGET(widget); 
	/* BRIDGE_IOC3_ID is 2 for IO6 and IP30 baseIO bridges */
	dump_bridge_regs((bridge_t *)bridge_baseAddr); 
	dump_ioc3_regs(bridge_baseAddr+BRIDGE_DEVIO(BRIDGE_IOC3_ID),
		bridge_baseAddr+BRIDGE_TYPE0_CFG_DEV(BRIDGE_IOC3_ID)); 
	diag_enet_dump_regs(-1, bridge_baseAddr, BRIDGE_IOC3_ID); 
	return(0);
      }
#endif
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
	
        break ;

      case 'r':
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
	
      default:
        printf("Error: Unrecognized flag: '%s'\n",
               argv[ii]);
        return 0 ;
      }
    }
    else {
      printf("Error: Unrecognized flag: '%s'\n",
	     argv[ii]);
      return 0 ;
    }
  }

  if (ii != argc)
      printf("Extra argument at end of list ignored\n") ;

#if SN0
  /* Check for proper range of values for module id and slot id */
  if ((modid <= 0)) {  /* XXX - check max module id */
    printf("Error: Invalid module number %d\n", modid) ;
    return 0 ;
  }
  
  if ((slotid <= 0) || (slotid > 12)) {
    printf("Error: Invalid IO slot number %d\n", slotid) ;
    return 0 ;
  }
#endif

  /* Form diag_mode parm */
  if (external) diag_mode = DIAG_MODE_EXT_LOOP; 
  else if (heavy) diag_mode = DIAG_MODE_HEAVY;

  if (verbose) diag_mode |= DIAG_FLAG_VERBOSE; 
  
#if SN0
  /* Calculate bridge base address based on module and slot numbers */
  if (modid != 1) {
    printf("Error: Only module 1 is currently supported.\n");
  }
  
  /* XXX - add code to find NASID from module/slot */
  if (!force_nasid) {
    if (slotid < 7) nasid = 0;
    else nasid = 1;
  }

  /* Read Node WID from HUB II WCR */
  node_wid = (__uint64_t) (REMOTE_HUB_L(nasid, IIO_WCR)) & 0xf;
  
  /* Get address of ILLR register */
  hub_illr = (hubii_illr_t *) REMOTE_HUB_ADDR(nasid, IIO_ILLR);

  /* Calculate widget number from slot */
  widget = slot_to_widget(slotid - 1) & 0xf;

  /* Calculate XBOW addresses */
  xbow_bp = (xbow_t *) NODE_SWIN_BASE(nasid, XBOW_WIDGET_ID);
  xbow_hub_port_link_status = (xbwX_stat_t *) &(xbow_bp->xb_link(node_wid).link_status);
  xbow_hub_port_aux_link_status = (xbow_aux_link_status_t *) &(xbow_bp->xb_link(node_wid).link_aux_status);
  xbow_bridge_port_link_status = (xbwX_stat_t *) &(xbow_bp->xb_link(widget).link_status);
  xbow_bridge_port_aux_link_status = (xbow_aux_link_status_t *) &(xbow_bp->xb_link(widget).link_aux_status);
  
  /* Calculate bridge base address */
  bridge_baseAddr = NODE_SWIN_BASE(nasid, widget);
#else
    xbow_bp = XBOW_K1PTR; 
    xbow_heart_port_link_status = (xbwX_stat_t *) &(xbow_bp->xb_link(HEART_ID).link_status);
    xbow_heart_port_aux_link_status = (xbow_aux_link_status_t *) &(xbow_bp->xb_link(HEART_ID).link_aux_status);
  xbow_bridge_port_link_status = (xbwX_stat_t *) &(xbow_bp->xb_link(widget).link_status);
  xbow_bridge_port_aux_link_status = (xbow_aux_link_status_t *) &(xbow_bp->xb_link(widget).link_aux_status);
  
  /* Calculate bridge base address */
  bridge_baseAddr = K1_MAIN_WIDGET(widget); 

printf("bridge_baseAddr = 0x%x\n", bridge_baseAddr);
#endif

  bridge_bp = (bridge_t *) bridge_baseAddr;

#ifdef XXX_PORT_EXCEPTIONS
  /* Register diag exception handler. */
  if (!_symmon) {
    if (setfault(fault_buf, &old_buf)) {
      printf("=====> Enet diag took an exception. <=====\n");
      diag_print_exception();
#if 0  /* Pending resolution of PV bug 405779 */
      kl_log_hw_state();
      kl_error_show_log("Hardware Error State: (Forced error dump)\n",
			"END Hardware Error State (Forced error dump)\n");
#endif
      diag_free((__uint64_t *) 0xdeadULL);  /* diag_free ignores the pointer it's passed */
      restorefault(old_buf);
      return(KLDIAG_ENET_EXCEPTION);
    }
  }
#endif
  
  /* Call selected diags */
  ii = repeat_count;

  if (test_xtalk) {
      if (heavy && repeat_count > 10) report_interval = 10;
      else if (repeat_count > 50) report_interval = 50;
      else report_interval = repeat_count;
  }

  diag_rc = KLDIAG_PASSED;
  do {
    
    if (test_all && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_all(diag_mode, bridge_baseAddr, npci);
    
    if (test_ssram && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_ssram(diag_mode, bridge_baseAddr, npci);
    
    if (test_txclk && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_tx_clk(diag_mode, bridge_baseAddr, npci);
    
    if (test_phyreg && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_phy_reg(diag_mode, bridge_baseAddr, npci);
    
    if (test_nic && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_nic(diag_mode, bridge_baseAddr, npci);
    
    if (test_loop_ioc3 && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, IOC3_LOOP);
    
    if (test_loop_phy && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, PHY_LOOP);
    
    if (test_loop_tw && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, TWISTER_LOOP);
    
    if (test_loop_ext && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, EXTERNAL_LOOP);

    if (test_loop_ext_10 && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, EXTERNAL_LOOP_10);

    if (test_loop_ext_100 && diag_rc == KLDIAG_PASSED) 
	diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, EXTERNAL_LOOP_100);

    if (test_xtalk && diag_rc == KLDIAG_PASSED) { 
      diag_rc = diag_enet_loop(diag_mode, bridge_baseAddr, npci, XTALK_STRESS);


      if (!((ii - 1) % report_interval) && (ii > 1)) { 
	printf("%d/%d iterations remaining . .\n", 
	       ii-1, repeat_count);
      }
      
      if ((ii == repeat_count && repeat_count >= report_interval) || 
	  (!((ii - 1) % report_interval) && (repeat_count >= report_interval))) {
      
	printf("                                  Out path    In path\n");
	printf("                                  --------    --------\n");
#if SN0
	printf(" ==> Hub retries                  TX = %3d    RX =   %d+%d  (CB+SN)\n",
	       IIO_WSTAT_TXRETRY_CNT((__uint64_t) (REMOTE_HUB_L(nasid, IIO_WSTAT))),
	       hub_illr->illr_fields_s.illr_cb_cnt, hub_illr->illr_fields_s.illr_sn_cnt);
	printf(" ==> Xbow retries (hub side)      RX = %3d    TX = %3d   ",
	       (*((__uint32_t *)xbow_hub_port_aux_link_status) & XB_AUX_STAT_RCV_CNT ) >> 24, 
	       (*((__uint32_t *)xbow_hub_port_aux_link_status) & XB_AUX_STAT_XMT_CNT ) >> 16);
	printf("%s%s\n", 
	       (*((__uint32_t *)xbow_hub_port_link_status) & XB_STAT_RCV_CNT_OFLOW_ERR) ? 
	       "*RX overflow* " : "", 
	       (*((__uint32_t *)xbow_hub_port_link_status) & XB_STAT_XMT_CNT_OFLOW_ERR) ? 
	       "*TX overflow* " : ""); 
#elif IP30
	printf(" ==> Xbow retries (heart side)    RX = %3d    TX = %3d   ",
	       (*((__uint32_t *)xbow_heart_port_aux_link_status) & XB_AUX_STAT_RCV_CNT ) >> 24, 
	       (*((__uint32_t *)xbow_heart_port_aux_link_status) & XB_AUX_STAT_XMT_CNT ) >> 16);
	printf("%s%s\n", 
	       (*((__uint32_t *)xbow_heart_port_link_status) & XB_STAT_RCV_CNT_OFLOW_ERR) ? 
	       "*RX overflow* " : "", 
	       (*((__uint32_t *)xbow_heart_port_link_status) & XB_STAT_XMT_CNT_OFLOW_ERR) ? 
	       "*TX overflow* " : ""); 
#endif
	printf(" ==> Xbow retries (bridge side)   TX = %3d    RX = %3d   ",
	       (*((__uint32_t *)xbow_bridge_port_aux_link_status) & XB_AUX_STAT_XMT_CNT) >> 16, 
	       (*((__uint32_t *)xbow_bridge_port_aux_link_status) & XB_AUX_STAT_RCV_CNT) >> 24);
	printf("%s%s\n", 
	       (*((__uint32_t *)xbow_bridge_port_link_status) & XB_STAT_XMT_CNT_OFLOW_ERR) ? 
	       "*TX overflow* " : "", 
	       (*((__uint32_t *)xbow_bridge_port_link_status) & XB_STAT_RCV_CNT_OFLOW_ERR) ? 
	       "*RX overflow* " : ""); 
	printf(" ==> Bridge retries               RX = %3d    TX = %3d\n",
	       (bridge_bp->b_wid_stat & 
		BRIDGE_STAT_LLP_REC_CNT) >> 24, 
	       (bridge_bp->b_wid_stat & 
		BRIDGE_STAT_LLP_TX_CNT) >> 16);
      }
    }
  } while (--ii > 0 && diag_rc == KLDIAG_PASSED);
  
#ifdef XXX_PORT_EXCEPTIONS
  if (!_symmon) {
    /* Un-register diag exception handler. */
    restorefault(old_buf);
  }
#endif

  return(0);
}
