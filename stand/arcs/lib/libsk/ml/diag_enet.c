/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* 
 *   File:  "$Id: diag_enet.c,v 1.31 1997/06/06 09:00:57 sprasad Exp $"
 *
 *   Authors: Chris Satterlee and Mohan Natarajan
 *
 *   This file contains all of the Ethernet POD diags.  A top-level
 *   function, diag_enet_all calls each of the individual diags in the
 *   order that optimizes fault isolation.  If the failure of one of the
 *   diags implies certain or probable failure of subsequent diags, the
 *   subsequent diags are not run.  If however, there are subsequent
 *   diags that do not depend on the failed part of the hardware, they
 *   -are- run.  An example is a failure of the NIC test; all the
 *   loopback tests are run using a fabricated MAC address if the NIC
 *   test fails.  NOTE: These diags do not support (i.e. ignore) the
 *   "continue flag" (DIAG_FLAG_CONTINUE).
 *
 *   Testing modes:
 *
 *     DIAG_MODE_NORMAL: 
 *                        - SSRAM test is cursory
 *                        - Loopback tests use small number of short packets
 *                        - Xtalk stress test is not run from diag_enet_all
 *     DIAG_MODE_HEAVY: 
 *                        - SSRAM test covers all locations
 *                        - Loopback tests use large number of long packets
 *                        - Xtalk stress test runs for ~5 seconds and uses
 *                          twister chip 100Mb/s loopback
 *     DIAG_MODE_MFG: 
 *                        - Same as heavy mode, but verbose mode is forced
 *     DIAG_MODE_EXT_LOOP: 
 *                        - Same as DIAG_MODE_HEAVY, -plus- . . .
 *                        - An external loopback cable is assumed, and additional
 *                          external loopback tests are performed.
 *                        - Xtalk stress test runs for ~5 seconds and uses
 *                          external 100Mb/s loopback
 *    
 *    Exceptions: 
 *    
 *       There are no expected exceptions, so none of the diags
 *       registers an exception handler.  Interrupts for anticipated
 *       error conditions are disabled and the error registers are
 *       explicitly polled. 
 * 
 * 
 * */

#ident  "$Revision: 1.31 $"

#include <stdio.h>
#include <setjmp.h>
#include <libkl.h>
#include <report.h>
#include <prom_msgs.h>
#include <rtc.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/SN/kldiag.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/xtalk/xbow.h>
#include <sys/mii.h>
#include <diag_lib.h>
#include <diag_enet.h>

#define SPEED_10       0
#define SPEED_100      1

#define SGI_PHY_ADDR      0x1f  
#define RX_FCS_SIZE       4  
#define IP_CHECKSUM_MASK  0x0000ffff
#define RSV22_MASK        0x80000000

#define NORMAL_NUM_PACKETS   10   /* Must be <= 495 */   
#define HEAVY_NUM_PACKETS   495   /* Must be <= 495 */
#define FULL_RING           512   
#define NORMAL_PACKET_LEN   301   /* Intentionally odd # */
#define HEAVY_PACKET_LEN   1501   /* Intentionally odd # */
#define MAX_PACKET_LEN     1518  
#define STALL_THRESH       1000
#define PIO_RETRY_THRESH      2
 
#define _4K_               (4*1024)
#define _64K_              (64*1024)
#define ROUND64(x,size)    (((__uint64_t)(x) & ((__uint64_t) ((size)-1))) ? (((__uint64_t)(x) + (__uint64_t)(size)) & ~((__uint64_t) ((size)-1))) : (__uint64_t)(x))
#define HI_WORD(x)         ((__uint32_t)((x) >> 32))
#define LO_WORD(x)         ((__uint32_t)((x)))

#define PASS(_name_)              if (GET_DIAG_MODE(diag_get_mode()) == DIAG_MODE_MFG) result_pass(_name_, DIAG_MODE_MFG)
  

#define PRE_PIO  retry_count = 0;\
	           if (setfault(fault_buf, &old_buf)) {\
	             retry_count++;\
	             total_retries++;\
	             if (retry_count >= PIO_RETRY_THRESH) restorefault(old_buf);\
	           }
    
#define POST_PIO if (retry_count < PIO_RETRY_THRESH) {\
	           /* Un-register exception handler. */\
	           restorefault(old_buf);\
	           }
#define  MALLOC       diag_malloc
#define  FREE         diag_free 

      

typedef struct etx_descriptor_s {
  /* Command field */
  volatile __uint32_t reserved_6:      5;  /* 31:27 */
  volatile __uint32_t chk_offset:      7;  /* 26:20 */
  volatile __uint32_t do_checksum:     1;  /*    19 */
  volatile __uint32_t buf_2_valid:     1;  /*    18 */
  volatile __uint32_t buf_1_valid:     1;  /*    17 */
  volatile __uint32_t data_0_valid:    1;  /*    16 */
  volatile __uint32_t reserved_5:      3;  /* 15:13 */
  volatile __uint32_t int_when_done:   1;  /*    12 */
  volatile __uint32_t reserved_4:      1;  /*    11 */
  volatile __uint32_t byte_cnt:       11;  /* 10:00 */
  /* Buffer count field */
  volatile __uint32_t reserved_3:      1;  /*    31 */
  volatile __uint32_t buf_2_cnt:      11;  /* 30:20 */
  volatile __uint32_t reserved_2:      1;  /*    19 */
  volatile __uint32_t buf_1_cnt:      11;  /* 18:08 */
  volatile __uint32_t reserved_1:      1;  /*     7 */
  volatile __uint32_t data_0_cnt:      7;  /*  6:00 */
  /* BUF_1 pointer field */
  volatile __uint64_t buf_ptr_1;
  /* BUF_2 pointer field */
  volatile __uint64_t buf_ptr_2;
  /* DATA_0 block */
  volatile uchar_t data0_byte[104];
} etx_descriptor_t;

typedef struct etx_64k_ring_s {
  volatile etx_descriptor_t etx_descriptor[512];
} etx_64k_ring_t;

typedef struct erx_buffer_s {
  /* First word */
  volatile __uint32_t valid:           1;  /*    31 */
  volatile __uint32_t reserved_3:      4;  /* 30:27 */
  volatile __uint32_t byte_cnt:       11;  /* 26:16 */
  volatile __uint32_t ip_checksum:    16;  /* 15:00 */
  /* Status field */
  volatile __uint32_t rsv22:           1;  /*    31 */
  volatile __uint32_t rsv21:           1;  /*    30 */
  volatile __uint32_t rsv20:           1;  /*    29 */
  volatile __uint32_t rsv19:           1;  /*    28 */
  volatile __uint32_t rsv17:           1;  /*    27 */
  volatile __uint32_t rsv16:           1;  /*    26 */
  volatile __uint32_t rsv12_3:        10;  /* 25:16 */
  volatile __uint32_t reserved_2:      1;  /*    15 */
  volatile __uint32_t rsv2_0:          3;  /* 14:12 */
  volatile __uint32_t reserved_1:      8;  /* 11:04 */
  volatile __uint32_t inv_preamble:    1;  /*     3 */
  volatile __uint32_t code_err:        1;  /*     2 */
  volatile __uint32_t frame_err:       1;  /*     1 */
  volatile __uint32_t crc_err:         1;  /*     0 */
  /* Pad, data, and unused bytes */
  volatile uchar_t pdu[1656];
} erx_buffer_t;

typedef struct erx_ring_s {
  volatile __uint64_t erx_buffer_pointer[512];
} erx_ring_t;

/* External variables */
extern char                      diag_name[128];

/* Static variables */
static int        num_packets, garbage_index = 0, enet_rc = 0;
static __uint64_t mac_address = 0xfefefefefefe;  /* Fake value used if
						    NIC test fails */
static __uint64_t *tx_ring_malloc_ptr;
static __uint64_t *tx_buf1_malloc_ptr[FULL_RING];
static __uint64_t *rx_ring_malloc_ptr;
static __uint64_t *rx_buf_malloc_ptr[FULL_RING];
static __uint64_t *garbage_pointers[FULL_RING];

static bridgereg_t old_bridge_int_enable;

/* Static functions */
static int
diag_loop_cleanup(int diag_mode,__psunsigned_t bridge_baseAddr,int npci,int type);

/* Extern function declarations */
extern void     bcopy(const void *, void *, size_t);
extern int      nic_get_eaddr(__int32_t *mcr, __int32_t *gpcr_s, char *eaddr);

/* Top level ENET test.  This routine calls the other routines in this
   file. */
int
diag_enet_all(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
  int                     ii, ssram_error = 0, phyreg_error = 0, txclk_error = 0;
  int                     nic_error = 0;
  int                     mode, verbose;
  __uint64_t              start_time;

  strcpy(diag_name, "enet_all");

  mode = GET_DIAG_MODE(diag_mode);
  verbose = (GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE) || mode == DIAG_MODE_MFG;

  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);

  start_time = rtc_time();

  switch (mode) {
  case DIAG_MODE_NORMAL:
  case DIAG_MODE_HEAVY:
  case DIAG_MODE_MFG:
  case DIAG_MODE_EXT_LOOP:
    DM_PRINTF(("Running %s diag (Bridge base = 0x%08x  PCI dev = %d  Mode = %d)\n",
	   "enet_all", bridge_baseAddr, npci, mode));
    break;
  default:
    printf("%s: *** WARNING ***    diag_mode parameter 0x%x unknown, returning. \n", "enet_all",
	   diag_mode);
    return(enet_rc=KLDIAG_PASSED); /* Should probably be a failure code */
  }
  
  /* For this diag, DIAG_MODE_MFG is equivalent to DIAG_MODE_HEAVY
     except it is verbose.  The rest of the diags are called with the
     diag_mode parameter modified to DIAG_MODE_HEAVY with
     DIAG_FLAG_VERBOSE. */
  if (mode == DIAG_MODE_MFG) diag_mode = DIAG_MODE_HEAVY | DIAG_FLAG_VERBOSE;

  /* Run SSRAM test */
  if (diag_enet_ssram(diag_mode, bridge_baseAddr, npci)) {
    
    printf("%s: SSRAM test failed.\n", "enet_all");
    ssram_error = enet_rc;
  }

  
  /* Run TX clock test (even if SSRAM test failed) */
  if (diag_enet_tx_clk(diag_mode, bridge_baseAddr, npci)) {
    
    printf("%s: Ethernet TX clock test failed.\n", "enet_all");
    txclk_error = enet_rc;
  }
  
  
  /* Run PHY register test (even if SSRAM or TX clock test failed) */
  if (diag_enet_phy_reg(diag_mode, bridge_baseAddr, npci)) {
    
    printf("%s: Ethernet PHY chip register test failed.\n", "enet_all");
    phyreg_error = enet_rc;
  }
  
  
  /* Run NIC test (even if SSRAM, TX clock, or PHY reg test failed).
     The failure of this test does not preclude ANY of the other tests,
     so we won't report its return code unless we complete all other
     tests sucessfully. */
  if (diag_enet_nic(diag_mode, bridge_baseAddr, npci)) {
    
    printf("%s: Ethernet NIC test failed.\n", "enet_all");
    nic_error = enet_rc;
  }
  

  /* If the SSRAM test failed, none of the following tests have a
     chance, so we bail out here */
  if (ssram_error) {
    
    if (verbose) 
        printf("%s: IOC3, PHY and Twister chip loopback tests not run.\n", "enet_all");
    
    return(ssram_error);
  }

  
  /* Run IOC3 internal loopback DMA test.  If this test fails, none of
     the following tests have a chance, so we bail out here. */
  if (diag_enet_loop(diag_mode, bridge_baseAddr, npci, IOC3_LOOP)) {
    
    printf("%s: IOC3 internal loopback DMA test failed.\n", "enet_all");
    return(enet_rc);
  }
  

  /* If the TX clock test failed, none of the following tests have a
     chance, so we bail out here */
  if (txclk_error) {
    
    if (verbose) 
        printf("%s: PHY and Twister chip loopback tests not run (no TX clk).\n", "enet_all");
    
    return(txclk_error);
  }

  
  /* If the PHY register test failed, none of the following tests have a
     chance, so we bail out here */
  if (phyreg_error) {
    
    if (verbose) 
        printf("%s: PHY and Twister chip loopback tests not run (bad PHY).\n", "enet_all");
    
    return(phyreg_error);
  }
  
  
  /* Run PHY chip internal loopback DMA test.  If this test fails, none
     of the following tests have a chance, so we bail out here. */
  if (diag_enet_loop(diag_mode, bridge_baseAddr, npci, PHY_LOOP)) {
    
    printf("%s: Ethernet PHY chip internal loopback DMA test failed.\n", "enet_all");
    
    if (verbose) 
        printf("%s: Twister chip loopback test not run.\n", "enet_all");
    
    return(enet_rc);
  }
  
  
  /* Run Twister chip internal loopback test.  If this test fails, none
     of the following tests have a chance, so we bail out here. */
  if (diag_enet_loop(diag_mode, bridge_baseAddr, npci, TWISTER_LOOP)) {
    
    printf("%s: Ethernet Twister chip internal loopback DMA test failed.\n", "enet_all");
    return(enet_rc);
  }
  
  
  /* Run external loopback test (if invoked in "ext_loop mode"). */
  if (mode == DIAG_MODE_EXT_LOOP) {
    
    if (diag_enet_loop(diag_mode, bridge_baseAddr, npci, EXTERNAL_LOOP)) {
      
      printf("%s: IOC3 external loopback DMA test failed.\n", "enet_all");
      return(enet_rc);
    }
  }
  
  /* Run Xtalk stress test (if invoked in heavy/mfg mode or ext_loop mode). */
  if (mode == DIAG_MODE_HEAVY || 
      mode == DIAG_MODE_MFG || 
      mode == DIAG_MODE_EXT_LOOP ) {
    
    if (diag_enet_loop(diag_mode, bridge_baseAddr, npci, XTALK_STRESS)) {
      
      printf("%s: Xtalk stress test failed.\n", "enet_all");
      return(enet_rc);
    }
  }
  
  
  /* If the NIC test failed, but everything else passed, we only now
     return with its failure code */
  if (nic_error) {
    
    return(nic_error);
  }
  
  if (verbose) 
      printf("%s: Enet diags all passed  (%lld us).\n", "enet_all",
	     rtc_time() - start_time);
  

  return(enet_rc=KLDIAG_PASSED);
}


/* SSRAM test (originally written by Mohan).  In normal mode, an address
   walk test is done.  This covers faults on all signals between the
   SSRAM and the IOC3.  A parity error test is also done in normal mode;
   this covers the parity generation and checking logic on the IOC3.  In
   heavy mode, all locations are tested.  We may later add a super-heavy
   mode, but it will probably be #ifdef'd out for the normal PROM. */
int
diag_enet_ssram(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
  int                     error = 0,wid_id,pat_ind,j,i,loop_cnt;
  pcicfg32_t              *pci_cfg_dev_ptr;
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  volatile __uint32_t     *ioc3_srambp,*ioc3_sramp;
  volatile __uint32_t     *ioc3_sramph,*ioc3_srampl;
  __uint32_t              reg_value,wr_val,exp_val;
  __uint32_t              memtest_pat[] = 
      {
        0x25555, 0x2aaaa, 0x25a5a,
        0x25555, 0x2aaaa, 0x25a5a,
        0x25555, 0x2aaaa, 0x25a5a,
      };
  int                     mode, verbose;
  __uint64_t              start_time, finish_time;

  strcpy(diag_name, "enet_ssram");
  
  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;

  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  if (verbose) 
    printf("Running %s diag (Bridge base = 0x%08x  PCI dev = %d) ... ",
	   diag_name, bridge_baseAddr, npci);


  start_time = rtc_time();

  if (enet_rc = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr, npci, diag_name))
      return(enet_rc);
    
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    error ++;
    if (verbose) printf("FAILED\n"); 
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  /* Enable Parity checking and disable DMA.  */
  ioc3_bp->eregs.emcr |=  EMCR_RAMPAR;
  ioc3_bp->eregs.emcr &=  ~(EMCR_TXDMAEN | EMCR_TXEN | EMCR_RXDMAEN | EMCR_RXEN);
  
  /* Set the ioc3_base pointer to the starting loc.  of ioc3 ssram.  */
  ioc3_srambp = (__uint32_t  *) (ioc3_base + IOC3_RAM_OFF);
  ioc3_sramp = ioc3_srambp;

  /* First do the write part of address walk on ssram.  */
  i = 0;
  while  ((__psunsigned_t) ioc3_sramp < 
          (ioc3_base + IOC3_RAM_OFF + IOC3_SSRAM_LEN)) {
    
    exp_val = (i | (i << 8) | (i << 16)) & 0xffff;
    *(ioc3_sramp) = exp_val | 0x20000; 
    ioc3_sramp =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (1<<(i+2)));

    i++;
  }

  /* Now begin the reads and compare.  */
  i = 0;
  ioc3_sramp = ioc3_srambp;
  while  ((__psunsigned_t) ioc3_sramp < 
          ((ioc3_base + IOC3_RAM_OFF + IOC3_SSRAM_LEN))) {
    
    reg_value = *ioc3_sramp ;
    exp_val = (i | (i << 8) | (i << 16)) & 0xffff;
    if ((reg_value & 0x2ffff) != (exp_val & 0xffff)) {
      
      if (++error == 1) {
	if (verbose) printf("FAILED\n");
	result_fail(diag_name, KLDIAG_ENET_SSRAM_FAIL, "Failed address walk test");
      }
      printf("%s: ssram address walk error. Exp: %05x Recv: %05x \n", diag_name,
             (exp_val & 0xfffff),(reg_value & 0x2ffff));
    }
    ioc3_sramp =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (1<<(i+2)));
    i++;
  }
  
  /* Test parity error checking.  Test all four cases with the PAR_TEST
     bit (bit 17) = 0. */
  ioc3_sramp = ioc3_srambp;
  for (i = 0; i < 4; i++) {
    
    switch (i) {
    case 0:
      wr_val  = 0x0aa55;   /* Good parity = 0 */
      exp_val = wr_val;
      break;
    case 1:
      wr_val  = 0x157aa;   /* Good parity = 1 */
      exp_val = wr_val;
      break;
    case 2:
      wr_val  = 0x0bad1;   /* Bad parity = 0  */
      exp_val = wr_val | 0x20000;  /* error bit must be set */
      break;
    case 3:
      wr_val  = 0x1bad0;   /* Bad parity = 1  */
      exp_val = wr_val | 0x20000;  /* error bit must be set */
      break;
    } 
  
    *(ioc3_sramp) = wr_val;
  
    reg_value = *ioc3_sramp ;
  
    if ((reg_value & 0x3ffff) != exp_val) {
    
      if (++error == 1) {
	if (verbose) printf("FAILED\n");
	result_fail(diag_name, KLDIAG_ENET_SSRAM_FAIL, "Failed parity test");
      }
      printf("%s: ssram parity test error. Exp: %05x Recv: %05x \n", diag_name,
             exp_val,(reg_value & 0x3ffff));
    }
  }
  
  if (mode == DIAG_MODE_NORMAL) { 
    
    finish_time = rtc_time();
    
    if (error) {
      printf("%s: IOC3 ssram test failed .... \n", diag_name);
      goto ssram_fail;
    }
    
    else if (verbose) printf("passed (%lld us).\n",
			     finish_time - start_time);
    

    PASS(diag_name);
    
    return(enet_rc=KLDIAG_PASSED);
  }

  /* Perform the ssram mem data pattern test.  Heavy mode only. */
  for (j = 0; j < 3; j++) {
    
    ioc3_srampl = ioc3_srambp;
    ioc3_sramph =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (IOC3_SSRAM_LEN-4));

    /* Perform the writes. Note that we alternate between high and
       low mem to stress. Note that we set IOC3_SSRAM_LEN - 24 as
       the limit b'cos the last block will have common entries
       between high and low mem block.  */
    for (i= 0; i < (IOC3_SSRAM_LEN - 24);i+=24) {
      
      for (pat_ind = 0; pat_ind <3;pat_ind++) {
        
        *(ioc3_srampl)   = memtest_pat[j+pat_ind];
        *(ioc3_sramph)   = memtest_pat[j+pat_ind];
        ioc3_srampl++;
        ioc3_sramph--;
      }
    }
    /* Now read and verify.  */
    ioc3_srampl = ioc3_srambp;
    ioc3_sramph =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (IOC3_SSRAM_LEN-4));
    for (i= 0; i < (IOC3_SSRAM_LEN -24);i+=24) {
      
      for (pat_ind = 0; pat_ind <3;pat_ind++) {
        
        reg_value = *(ioc3_srampl);
        if ((reg_value & 0x2ffff) != (memtest_pat[j+pat_ind] & 0xffff)) {
          
          if (++error == 1) {
	    if (verbose) printf("FAILED\n");
	    result_fail(diag_name, KLDIAG_ENET_SSRAM_FAIL, "SSRAM data miscompare");
	  }
          if (error < 25)
              printf("%s:  **Data miscompare(A)**  offset: %05x  exp: %05x recv: %05x \n", diag_name,
                     i/2, memtest_pat[j+pat_ind] & 0xffff, (reg_value & 0x2ffff));
          
          else if (error == 25) {
            
            printf("%s: Too many SSRAM errors . . . abandoning test.\n", diag_name);
            goto ssram_fail;
          }
        }
        ioc3_srampl++;
      }

      for (pat_ind = 0; pat_ind <3;pat_ind++) {
        
        reg_value = *(ioc3_sramph);
        if ((reg_value & 0x2ffff) != (memtest_pat[j+pat_ind] & 0xffff)) {
          
          if (++error == 1) {
	    if (verbose) printf("FAILED\n");
	    result_fail(diag_name, KLDIAG_ENET_SSRAM_FAIL, "SSRAM data miscompare");
	  }
          if (error < 25)
              printf("%s:  **Data miscompare(B)**  offset: %05x exp: %05x recv: %05x \n", diag_name,
                     (IOC3_SSRAM_LEN-(i/2)), memtest_pat[j+pat_ind] & 0xffff,
                     (reg_value & 0x2ffff));
          
          else if (error == 25) {
            
            printf("%s: Too many SSRAM errors . . . abandoning test.\n", diag_name);
            goto ssram_fail;
          }
        }
        ioc3_sramph--;
      }
    }
  }
  /* offset location 0x1fffc & 0x1fff8 were not tested.  test them.  */
  ioc3_srampl =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (0x1ff8));
  ioc3_sramph =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (0x1ffc));
  for (i = 0; i < 2;i++) {
    
    *(ioc3_srampl) = memtest_pat[i];
    *(ioc3_sramph) = memtest_pat[i+1];
    reg_value = *(ioc3_srampl);
    if ((reg_value & 0x2ffff) != (memtest_pat[i] & 0xffff)) {
      
      if (++error == 1) {
	if (verbose) printf("FAILED\n");
	result_fail(diag_name, KLDIAG_ENET_SSRAM_FAIL, "SSRAM data miscompare");
      }
      if (error < 25)
          printf("%s:  **Data miscompare(C)**  offset: %05x exp: %05x recv: %05x \n", diag_name,
                 (0x1ff8), memtest_pat[i] & 0xffff), (reg_value & 0x2ffff);
      else if (error == 25) {
	
	printf("%s: Too many SSRAM errors . . . abandoning test.\n", diag_name);
	goto ssram_fail;
      }
    }
    reg_value = *(ioc3_sramph);
    if ((reg_value & 0x2ffff) != (memtest_pat[i+1] & 0xffff)) {
      
      if (++error == 1) {
	if (verbose) printf("FAILED\n");
	result_fail(diag_name, KLDIAG_ENET_SSRAM_FAIL, "SSRAM data miscompare");
      }
      if (error < 25)
          printf("%s:  **Data miscompare(D)**  offset: %05x exp: %05x recv: %05x \n", diag_name,
                 (0x1ffc), memtest_pat[i+1] & 0xffff, (reg_value & 0x2ffff));
      else if (error == 25) {
	
	printf("%s: Too many SSRAM errors . . . abandoning test.\n", diag_name);
	goto ssram_fail;
      }
    }
  }
  
  finish_time = rtc_time();
    
  if (verbose) 
    printf("passed (%lld us).\n", finish_time - start_time);
  
  PASS(diag_name);
  
  return(enet_rc=KLDIAG_PASSED);
  
 ssram_fail:
  return(enet_rc=KLDIAG_ENET_SSRAM_FAIL);
}

 
/* PHY chip register test. */ 
int
diag_enet_phy_reg(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  int                     timeout, error = 0, reg_num, rc;
  __uint32_t              reg_value, wr_value, exp_val, exp_mask;
  int                     phy_addr, mode, verbose;
  __uint64_t              start_time, finish_time;

#if 0
  int                    *uninitialized;  /* Temporary */
#endif

  strcpy(diag_name, "enet_phy_reg");
  
  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;
  
  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  if (verbose) 
    printf("Running %s diag (Bridge base = 0x%08x  PCI dev = %d) ... ",
	   diag_name, bridge_baseAddr, npci);
  
#if 0
  *uninitialized = 5;  /* Temporary: force exception */
#endif

  start_time = rtc_time();
  
  if (enet_rc = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr,npci, diag_name))
      return(enet_rc);
    
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  /* Hard reset the PHY */
  ioc3_bp->gppr_5 = 0;
  reg_value = ioc3_bp->gppr_5;
  ioc3_bp->gpcr_s = GPCR_PHY_RESET;
  reg_value = ioc3_bp->gpcr_s;
  rtc_sleep(10);
  ioc3_bp->gppr_5 = 1;
  reg_value = ioc3_bp->gppr_5;
  
  if (SN00){
    /* Note: timing problem requires double writes to LED reg */
    LOCAL_HUB_S(MD_LED0, 0x0);
    LOCAL_HUB_S(MD_LED0, 0x0);
    rtc_sleep(10);
    LOCAL_HUB_S(MD_LED0, 0x1);
    LOCAL_HUB_S(MD_LED0, 0x1);
  }

  rtc_sleep(500);
  
  /* Read PHY register 0 (BMCR) at expected PHY address.  If this
     returns all 1's, there's a problem. */
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              0, &reg_value)) return(rc);

  if (reg_value == 0xffff) {
    if (++error == 1) {
      if (verbose) printf("FAILED\n");
      result_fail(diag_name, KLDIAG_ENET_PHYREG_FAIL, "Register 0 = 0xffff");
    }
    printf("%s: Read of PHY register 0 \
failed (returned 0xffff).\n", diag_name);
    goto phyreg_fail;
  }

  /* Read all of the registers, verifying their reset values. XXX - have
     to make sure to account for autonegotiation initiated by a partner;
     this could cause many of the expected values to change. */

  /* Read the PHY Identifier Registers (regs 2 & 3) and Silicon Revision
     Register (reg 22) first.  If their values do not indicate a known
     revision of a National DP83840, skip the rest. */
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              2, &reg_value)) return(rc); /* Reg 2 */
  if (reg_value != 0x2000) {
    printf("\n%s: Warning - PHY Identifier register #1 value \
0x%04x is not supported by this diag!\n", diag_name, 
           reg_value);
  }
  
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              3, &reg_value)) return(rc); /* Reg 3 */
  if (reg_value != 0x5c00 && reg_value != 0x5c01) {
    printf("\n%s: Warning - PHY Identifier register #2 value \
0x%04x is not supported by this diag!\n", diag_name, 
           reg_value);
  }
  
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              22, &reg_value)) return(rc); /* Reg 22 */
  if (reg_value != 0x0000 && reg_value != 0x0001) {
    printf("\n%s: Warning - PHY Silicon Revision Register value \
0x%04x is not supported by this diag!\n", diag_name, 
           reg_value);
  }
  
  /* I'm just going to make unexpected reset values generate warning
     messages, since this is too easy to get wrong.  I don't want a
     failure here to prevent using Enet; if something is really broken,
     we'll find out about it soon enough, and these messages will
     help. */
  for (reg_num = 0; reg_num < 32; reg_num++) {
    
    if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                                reg_num, &reg_value)) return(rc);
    switch (reg_num) {
    case 0:
      exp_val = 0x3100;
      exp_mask = 0xff80;
      break;
    case 1:
      exp_val = 0x7809;
      exp_mask = 0xf83f;
      break;
    case 2:
      exp_val = 0x2000;
      exp_mask = 0xffff;
      break;
    case 3:
      exp_val = 0x5c00;
      exp_mask = 0xfffe;  /* 0x5c00 or 0x5c01 are supported */
      break;
    case 4:
      exp_val = 0x01e1;
      exp_mask = 0xe3ff;
      break;
    case 5:
      exp_val = 0x0000;
      exp_mask = 0xe3ff;
      break;
    case 6:
      exp_val = 0x0000;
      exp_mask = 0x001f;
      break;
    case 7:   /* Reserved */
    case 8:   /* Reserved */
    case 9:   /* Reserved */
    case 10:  /* Reserved */
    case 11:  /* Reserved */
    case 12:  /* Reserved */
    case 13:  /* Reserved */
    case 14:  /* Reserved */
    case 15:  /* Reserved */
    case 16:  /* Reserved */
    case 17:  /* Reserved */
      continue;
    case 18:
      exp_val = 0x0000;
      exp_mask = 0x0000;   /* XXX - temporary ?? fails in mfg oven machines w/100 half Hub */
      break;
    case 19:
      exp_val = 0x0000;
      exp_mask = 0x0000;   /* XXX - temporary ?? fails in mfg oven machines w/100 half Hub */
      break;
    case 20:  /* Reserved */
      continue;
    case 21:
      exp_val = 0x0000;
      exp_mask = 0xffff;
      break;
    case 22:
      exp_val = 0x0000;
      exp_mask = 0xfffe;   /* 0x0000 or 0x0001 are supported */ 
      break;
    case 23:
      exp_val = 0x8040;
      exp_mask = 0xb8d6;   /* F_CONNECT is 0 in rev A and 1 in rev B */
      break;
    case 24:
      exp_val = 0x0000;
      exp_mask = 0x7b1e;
      break;
    case 25:
      exp_val = 0x005f;
      exp_mask = 0x001f;
      break;
    case 26:  /* Reserved */
      continue;
    case 27:
      exp_val = 0x0000;
      exp_mask = 0x0200;
      break;
    case 28:
      exp_val = 0x0039;
      exp_mask = 0x003d;
      break;
    default:  /* Reserved */
      continue;
    }
  
    if ((reg_value & exp_mask) != (exp_val & exp_mask)) {
      printf("%s: Warning - unexpected Reg %d reset value,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", diag_name, reg_num, exp_val, exp_mask, reg_value);
    }
  }
  
  /* Write some writeable register bits and verify.  These are the regs
     actually written by the diags and by the driver. */
  
  /****************  Register 0 ********************/
  wr_value = MII_R0_LOOPBACK | MII_R0_SPEEDSEL | 
      MII_R0_AUTOEN | MII_R0_ISOLATE | 
          MII_R0_RESTARTAUTO | MII_R0_DUPLEX | 
              MII_R0_COLLTEST;  /* Set all writable, non-self-clearing bits */

  exp_mask = 0xff80;
  
  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                               0, wr_value)) return(rc);
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              0, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    if (++error == 1) {
      if (verbose) printf("FAILED\n");
      result_fail(diag_name, KLDIAG_ENET_PHYREG_FAIL, "Reg 0 bit didn't set");
    }
    printf("%s: Reg 0 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", diag_name, wr_value, exp_mask, reg_value);
  }
  
  wr_value = 0x0;               /* Now clear them */  
  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                               0, wr_value)) return(rc);
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              0, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    if (++error == 1) {
      if (verbose) printf("FAILED\n");
      result_fail(diag_name, KLDIAG_ENET_PHYREG_FAIL, "Reg 0 bit didn't clear");
    }
    printf("%s: Reg 0 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", diag_name, wr_value, exp_mask, reg_value);
  }
  
  /****************  Register 23 ********************/
  wr_value = 0x0020; /* Just test this bit */
  exp_mask = 0x0020;
  
  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                               23, wr_value)) return(rc);
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              23, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    if (++error == 1) {
      if (verbose) printf("FAILED\n");
      result_fail(diag_name, KLDIAG_ENET_PHYREG_FAIL, "Reg 23 bit didn't set");
    }
    printf("%s: Reg 23 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", diag_name, wr_value, exp_mask, reg_value);
  }
  
  wr_value = 0x0;               /* Now clear it */  
  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                               23, wr_value)) return(rc);
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              23, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    if (++error == 1) {
      if (verbose) printf("FAILED\n");
      result_fail(diag_name, KLDIAG_ENET_PHYREG_FAIL, "Reg 23 bit didn't clear");
    }
    printf("%s: Reg 23 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", diag_name, wr_value, exp_mask, reg_value);
  }
  
  /****************  Register 24 ********************/
  wr_value = 0x0b00;            /* Just test loopback related bits */
  exp_mask = 0x0b00;            /* Just test loopback related bits */
  
  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                               24, wr_value)) return(rc);
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              24, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    if (++error == 1) {
      if (verbose) printf("FAILED\n");
      result_fail(diag_name, KLDIAG_ENET_PHYREG_FAIL, "Reg 24 bit didn't set");
    }
    printf("%s: Reg 24 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", diag_name, wr_value, exp_mask, reg_value);
  }
  
  wr_value = 0x0;               /* Now clear them */  
  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                               24, wr_value)) return(rc);
  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                              24, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    if (++error == 1) {
      if (verbose) printf("FAILED\n");
      result_fail(diag_name, KLDIAG_ENET_PHYREG_FAIL, "Reg 24 bit didn't clear");
    }
    printf("%s: Reg 24 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", diag_name, wr_value, exp_mask, reg_value);
  }

  /* Reset the PHY (clean up) */
  if (rc = diag_enet_phy_reset(diag_mode, bridge_baseAddr, npci))
      return(rc);
      
  finish_time = rtc_time();
  
  if (verbose) printf("passed (%lld us).\n",
		      finish_time - start_time);

  PASS(diag_name);
  
  return(enet_rc=KLDIAG_PASSED);

 phyreg_fail:
  return(enet_rc=KLDIAG_ENET_PHYREG_FAIL);
}


/* Phy register read routine.  This is a test in itself, since it
   can time out before or during the read. */
int
diag_enet_phy_read(int diag_mode,__psunsigned_t bridge_baseAddr,
                   int npci, int reg,  __uint32_t *val)
{
  int                     timeout;
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  int                     mode, verbose;

  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;
  
  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  /* Wait until not busy or timeout */
  timeout = 1000;
  while (ioc3_bp->eregs.micr & MICR_BUSY) {
    rtc_sleep(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      if (verbose) printf("FAILED\n"); 
      printf("%s: Timed out on MICR_BUSY before read.\n", diag_name);
      goto phy_read_timeout;
    }
  }
  
  /* Write MICR with PHY address, register offset, and read trigger set */
  ioc3_bp->eregs.micr = MICR_READTRIG | 
      (SGI_PHY_ADDR << MICR_PHYADDR_SHIFT) | reg;
  
  /* Wait until not busy or timeout */
  timeout = 1000;
  while (ioc3_bp->eregs.micr & MICR_BUSY) {
    rtc_sleep(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      if (verbose) printf("FAILED\n"); 
      printf("%s: Timed out on MICR_BUSY during read.\n", diag_name);
      goto phy_read_timeout;
    }
  }
  
  /* Get register value from MIDR_R */
  *val = ioc3_bp->eregs.midr_r & MIDR_DATA_MASK;
  
  return(enet_rc=KLDIAG_PASSED);
  
 phy_read_timeout:
  result_fail(diag_name, KLDIAG_ENET_PHY_R_BUSY_TO, "PHY read busy timeout");
  return(enet_rc=KLDIAG_ENET_PHY_R_BUSY_TO);
}


/* Phy register write routine.  This is a test in itself, since it
   can time out before or during the write. */
int
diag_enet_phy_write(int diag_mode,__psunsigned_t bridge_baseAddr,
                   int npci, int reg,  u_short val)
{
  int                     timeout;
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  int                     mode, verbose;

  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;
  
  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  /* Wait until not busy or timeout */
  timeout = 1000;
  while (ioc3_bp->eregs.micr & MICR_BUSY) {
    rtc_sleep(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      if (verbose) printf("FAILED\n"); 
      printf("%s: Timed out on MICR_BUSY before write.\n", diag_name);
      goto phy_write_timeout;
    }
  }
  
  /* Write MIDR_W with data */
  ioc3_bp->eregs.midr_w = val;
  
  /* Write MICR with PHY address, register offset, and read trigger clear */
  ioc3_bp->eregs.micr = (SGI_PHY_ADDR << MICR_PHYADDR_SHIFT) | reg;
  
  /* Wait until not busy or timeout */
  timeout = 1000;
  while (ioc3_bp->eregs.micr & MICR_BUSY) {
    rtc_sleep(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      if (verbose) printf("FAILED\n"); 
      printf("%s: Timed out on MICR_BUSY during write.\n", diag_name);
      goto phy_write_timeout;
    }
  }
  
  return(enet_rc=KLDIAG_PASSED);
  
 phy_write_timeout:
  result_fail(diag_name, KLDIAG_ENET_PHY_W_BUSY_TO, "PHY write busy timeout");
  return(enet_rc=KLDIAG_ENET_PHY_W_BUSY_TO);
}


/* Phy register RMW with OR */
int
diag_enet_phy_or(int diag_mode,__psunsigned_t bridge_baseAddr,
                   int npci, int reg,  u_short val)
{
  int        rc;
  __uint32_t temp;

  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                         reg, &temp)) return(rc);

  temp |= val;

  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                         reg, (u_short) temp)) return(rc);
  return(enet_rc=KLDIAG_PASSED);
}


/* Phy register RMW with AND */
int
diag_enet_phy_and(int diag_mode,__psunsigned_t bridge_baseAddr,
                   int npci, int reg,  u_short val)
{
  int        rc;
  __uint32_t temp;

  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                         reg, &temp)) return(rc);

  temp &= val;

  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci, 
                         reg, (u_short) temp)) return(rc);
  return(enet_rc=KLDIAG_PASSED);
}


/* Phy reset */
int
diag_enet_phy_reset(int diag_mode,__psunsigned_t bridge_baseAddr,
                   int npci)
{
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  int                     timeout, rc;
  __uint32_t              reg_value;
  int                     mode, verbose;

  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;

  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  /* Hard reset the PHY */
  ioc3_bp->gppr_5 = 0;
  reg_value = ioc3_bp->gppr_5;
  ioc3_bp->gpcr_s = GPCR_PHY_RESET;
  reg_value = ioc3_bp->gpcr_s;
  rtc_sleep(10);
  ioc3_bp->gppr_5 = 1;
  reg_value = ioc3_bp->gppr_5;
  
  if (SN00){
    /* Note: timing problem requires double writes to LED reg */
    LOCAL_HUB_S(MD_LED0, 0x0);
    LOCAL_HUB_S(MD_LED0, 0x0);
    rtc_sleep(10);
    LOCAL_HUB_S(MD_LED0, 0x1);
    LOCAL_HUB_S(MD_LED0, 0x1);
  }

  rtc_sleep(500);
  
  /* Soft reset the PHY. */
  reg_value = MII_R0_RESET;
  if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci,
                      0, (u_short) reg_value)) return(rc);
  
  timeout = 1000;
  while (reg_value & MII_R0_RESET) {
    
    if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
                                0, &reg_value)) return(rc);

    rtc_sleep(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      if (verbose) printf("FAILED\n"); 
      result_fail(diag_name, KLDIAG_ENET_PHY_RESET_TO, "PHY reset timeout");
      printf("%s: Timed out waiting for PHY to reset.\n", diag_name);
      return(enet_rc=KLDIAG_ENET_PHY_RESET_TO);
    }
  }
  
  return(enet_rc=KLDIAG_PASSED);
}


/* TX clock test. */ 
int
diag_enet_tx_clk(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  int                     mode, verbose;
  __uint64_t              start_time, finish_time;

  strcpy(diag_name, "enet_tx_clk");
  
  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;
  
  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  if (verbose) 
    printf("Running %s diag (Bridge base = 0x%08x  PCI dev = %d) ... ",
	   diag_name, bridge_baseAddr, npci);
  
  start_time = rtc_time();
    
  if (enet_rc = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr,npci, diag_name))
      return(enet_rc);
    
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  /* Read ETCSR<NO_TX_CLK>.  If it's set, there's a problem. */
  if (ioc3_bp->eregs.etcsr & ETCSR_NOTXCLK) {
    
    if (verbose) printf("FAILED\n"); 
    result_fail(diag_name, KLDIAG_ENET_NO_TXCLK, "No TX clock");
    printf("%s: IOC3 is not seeing the PHY TX clock. \n", diag_name);
    return(enet_rc=KLDIAG_ENET_NO_TXCLK);
  }
  
  finish_time = rtc_time();
    
  if (verbose)
      printf("passed (%lld us).\n",
	     finish_time - start_time);

  PASS(diag_name);
  
  return(enet_rc=KLDIAG_PASSED);
}


/* DMA loopback tests. */ 
int
diag_enet_loop(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, int type)
{
  int                     ii, packet, packet_len, dw_index;
  int                     shift_amt, byte, remaining_bytes, speed;
  int                     try_again, timeout, start_timeout, dma_hung, retry_count; 
  int                     total_retries, xtalk_stress_loop, rx_stall_counter, tx_stall_counter, rc;
  __uint64_t              tx_ring_base;
  __uint64_t              bigend_mac_address;
  __uint64_t              rx_ring_base;
  __uint64_t              cmd_buf_cnt;
  __uint64_t              nasid;
  __uint64_t              node_wid;
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  __uint32_t              rx_off, reg_value, reg0_value, reg1_value;
  __uint32_t              reg4_value, reg5_value, reg6_value;
  __uint32_t              br_isr, eisr, ercir, etcir, prev_ercir, prev_etcir;
  char                    type_string[128];
  erx_buffer_t            scratch_rx_buf;
  erx_buffer_t            *actual;
  erx_buffer_t            *expected;
  int                     mode, verbose;
  __uint64_t              start_time, finish_time, stress_time;
  jmp_buf                 fault_buf;  
  void                    *old_buf;

  bridge_t                *bridge_bp;
  
  static __uint64_t       rx_buf_pointer[FULL_RING];  /* Must be static to avoid stack overflow */
  static __uint64_t       tx_buf1_pointer[FULL_RING]; /* Must be static to avoid stack overflow */
  
  static __uint64_t       xtalk_pattern[8] = { 0x6955555555555555,
					       0x5569555555555555,
					       0x5555695555555555,
					       0x5555556955555555,
					       0x5555555569555555,
					       0x5555555555695555,
					       0x5555555555556955,
					       0x5555555555555569  };

  garbage_index = 0;
  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;
  
  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  strcpy(diag_name, "diag_enet_loop");
  
  switch (type) {
  case IOC3_LOOP:
    strcpy(type_string, "IOC3 LOOPBACK");
    strcpy(diag_name, "enet_ioc3_loop");
    break;
  case PHY_LOOP:
    strcpy(type_string, "PHY LOOPBACK");
    strcpy(diag_name, "enet_phy_loop");
    break;
  case TWISTER_LOOP:
    strcpy(type_string, "TWISTER LOOPBACK");
    strcpy(diag_name, "enet_tw_loop");
    break;
  case EXTERNAL_LOOP:
    strcpy(type_string, "EXTERNAL LOOPBACK");
    strcpy(diag_name, "enet_ext_loop");
    break;
  case EXTERNAL_LOOP_10:
    strcpy(type_string, "EXTERNAL LOOPBACK");
    strcpy(diag_name, "enet_10_ext_loop");
    break;
  case EXTERNAL_LOOP_100:
    strcpy(type_string, "EXTERNAL LOOPBACK");
    strcpy(diag_name, "enet_100_ext_loop");
    break;
  case XTALK_STRESS:
    strcpy(type_string, "XTALK STRESS");
    strcpy(diag_name, "enet_xtalk_stress");
    break;
  default:
    printf("%s: ERROR illegal loopback type parm (%d).\n",diag_name, type);
    return(enet_rc=KLDIAG_ENET_LOOP_ILL_PARM);
  }

  if (verbose) 
    printf("Running %s diag (Bridge base = 0x%08x  PCI dev = %d) ... ",
	   diag_name, bridge_baseAddr, npci);
  
  if (enet_rc = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr,npci, diag_name))
      return(enet_rc);
    
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  start_time = rtc_time();
    
  /* Read Node WID from HUB II WCR */
  nasid = NASID_GET(bridge_baseAddr);
  node_wid = (__uint64_t) (REMOTE_HUB_L(nasid, IIO_WCR)) & 0xf;

  /* Determine the number of packets to be generated and the length of
     each packet, based on the mode parameter (normal vs heavy). */
  num_packets = (mode == DIAG_MODE_NORMAL) ? 
      NORMAL_NUM_PACKETS : HEAVY_NUM_PACKETS;
  
  if (type == XTALK_STRESS) num_packets = FULL_RING;
  
  packet_len = (mode == DIAG_MODE_NORMAL)  ? 
      NORMAL_PACKET_LEN : HEAVY_PACKET_LEN; 

  /* Allocate memory for the TX descriptor ring.  For simplicity we'll
     use the large ring size even in normal mode.  We need to
     over-allocate by 64K in order to assure ring alignment. */
  if (!(tx_ring_malloc_ptr = 
        (__uint64_t *) MALLOC(sizeof (etx_64k_ring_t) + _64K_))) {
    
    if (verbose) printf("FAILED\n"); 
    result_fail(diag_name, KLDIAG_ENET_LOOP_MALLOC, "Malloc failure");
    printf("%s: Couldn't allocate memory for TX descriptor ring.\n", diag_name);
    return(enet_rc=KLDIAG_ENET_LOOP_MALLOC);
  }

  tx_ring_base = ROUND64(tx_ring_malloc_ptr, _64K_); /* Round up to 64K boundary */

#if 0
  printf("tx_ring_base = 0x%016llx\n",tx_ring_base);
#endif
  
  /* Write the base address to the ETBR_H and ETBR_L registers.  Since
     we are using 64K rings, the RING_SIZE bit (bit 0) is also set.
     Bits 63:60 are the node WID.  All other attribute bits are set to
     zero. */
  ioc3_bp->eregs.etbr_h  = 
      (TO_PHYS(tx_ring_base) | (node_wid << 60)) >> 32;

  ioc3_bp->eregs.etbr_l  = 
      TO_PHYS(tx_ring_base) | 0x1;
  
  
  /* Calculate TX descriptor Command and Buffer Count fields.  
     They are the same for all packets: 
     CHK_OFFSET    = 0   
     DO_CHECKSUM   = 0  
     BUF_2_VALID   = 0 
     BUF_1_VALID   = 1
     DATA_0_VALID  = 0
     INT_WHEN_DONE = 0
     BYTE_CNT      = packet_len
     DATA_0_CNT    = 0
     BUF_1_CNT     = packet_len
     BUF_1_CNT     = 0
     */
  cmd_buf_cnt = ((__uint64_t) 1 << 49) | /* BUF_1_VALID */
      (__uint64_t) packet_len << 32 | /* BYTE_CNT    */ 
          (__uint64_t) packet_len << 8; /* BUF_1_CNT   */
  
  /* Allocate TX buffers and initialize the TX descriptor ring entries */
  for (packet = 0; packet < num_packets; packet++) {
    
    /* Allocate memory for the TX buffer.  For simplicity, all of the
       data will be in a BUF_1 area (i.e. no DATA_0 or BUF_2).  The
       hardware only requires that BUF_1 buffers be halfword aligned,
       but we force doubleword alignment so we can do doubleword writes
       (overallocate and round up).  The hardware DOES require that the
       buffer not cross a 16K boundary, so we test for this case, and do
       another malloc if the first try did cross a 16K boundary. */
    do {
      
      if (!(tx_buf1_malloc_ptr[packet] = (__uint64_t *) MALLOC(packet_len + 8))) {
        
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_MALLOC, "Malloc failure");
        printf("%s: Couldn't allocate memory for TX packet %d buffer.\n", diag_name, packet);
        return(enet_rc=KLDIAG_ENET_LOOP_MALLOC);
      }
      
      tx_buf1_pointer[packet] = ROUND64(tx_buf1_malloc_ptr[packet], 8);

      /* Test for 16K crossing */
      if ((tx_buf1_pointer[packet] & 0x3fff) + packet_len > 0x4000) {

	if (garbage_index >= FULL_RING - 1) {
	  if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_MALLOC, "Malloc failure");
	  printf("%s: Too many garbage pointers (TX packet %d).\n", diag_name, 
		 packet);
	  return(enet_rc=KLDIAG_ENET_LOOP_MALLOC);
	}
	
	garbage_pointers[garbage_index++] = tx_buf1_malloc_ptr[packet];
        try_again = 1;
      }
      
      else try_again = 0;
      
    } while (try_again);

#if 0
    printf("tx_buf1_pointer[%d] = 0x%016llx\n", packet, tx_buf1_pointer[packet]);
#endif
  
    /* Write the first DW (the Command and Buffer Count fields) */
    (*((__uint64_t *)tx_ring_base + packet * 16)) = cmd_buf_cnt;
    
    /* Write the BUF_PTR_1 field.  Bits 63:60 are the node WID.  All
       other attribute bits are set to 0. */
    (*((__uint64_t *)tx_ring_base + packet * 16 + 1)) = 
        TO_PHYS(tx_buf1_pointer[packet]) | node_wid << 60;
    
    /* It should be OK to leave the rest of the TX ring entry (BUF_PTR_2
       and DATA_0) uninitialized.  XXX - may want to revisit this
       decision. */
  }
  
  /* Generate DMA data, and fill TX buffers. 
   * 
   * The first 6 bytes of data are the Target MAC address, and the next
   * 6 bytes are the Source MAC address.  Both addresses are the same
   * since we are doing loopback.  If the NIC test passed, we use the
   * MAC address from the NIC.  If the NIC test failed, we will use a
   * fabricated MAC address.  This should never make its way onto a real
   * network, but its value is chosen such that there is virtually no
   * chance of its being the address of another legitimate station if it
   * did.  The value is:
   * 
   *    0  1  2  3  4  5  
   * +-------------------+
   * | FE FE FE FE FE FE |  Fabricated MAC address (if NIC test fails)
   * +-------------------+
   * 
   * The MAC address returned by nic_get_eaddr is little endian as are
   * the addresses in the packet.  The value written to the EMAR_H and
   * EMAR_L registers are big endian.
   * 
   * The target and source MAC addresses take up the first 12 bytes of
   * the packet.  The 13th byte is the loopback type code (0 = IOC3, 1 =
   * PHY, 2 = TWISTER, 3 = EXT).  The 14th byte is the speed (0 =
   * 10Mb/s, 1 = 100 Mb/s).  The 15th and 16th bytes are the packet number.
   * 
   * The data patterns are chosen to find SSO problems on the PCI bus
   * and XTALK LLP.  PCI stress paterns are the default, but XTALK LLP
   * stress patterns are used in the xtalk stress mode.  To stress PCI,
   * the data alternates between words of all ones and one walking zero,
   * and the complement of this, i.e. a typical double-word would be
   * 0xfffeffff00010000.
   * 
   * e.g. The first 8 DWs of a PCI stress packet (100Mb/s Twister
   * loopback) might look like:
   * 
   * fefefefefefefefe fefefefe020101ac
   * ffffffff00000000 7fffffff80000000
   * bfffffff40000000 dfffffff20000000
   * efffffff10000000 f7ffffff08000000
   * 
   * The bytes in the last partial DW of the packet are written with
   * 0x5a.
   *
   * The pattern for LLP stress is a "walking 69" (seriously!), and is
   * intended to generate a similar effect on the LLP SSD. */
  
  for (packet = 0; packet < num_packets; packet++) {
    
    /* First DW */
    dw_index = 0;
    
    (*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
        (mac_address << 16) | 
            (mac_address >> 32);
     
    /* Second DW (filled later - see while loop below) */
    
    /* Remaining DWs */
    dw_index = 2;

    
    if(type != XTALK_STRESS) {
      /* Generate PCI stress pattern  XXX: since this part of the packet is
	 identical for all PCI stress packets, I should just calculate the
	 contents once, stick it in an array, and then copy the array into
	 the buffer.  This would speed up checking too.  */
      shift_amt = 32;
    
      while (dw_index < packet_len/8) {

	(*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
	    0xffffffff00000000 & 
		~(((__uint64_t) 1 << (shift_amt + 32))) |
		    (((__uint64_t) 1 << (shift_amt)));
        
	shift_amt = shift_amt ? --shift_amt : 32; /* 32->0, 32->0,  ... */

	dw_index++;
      }
    
      /* Fill the remaining partial DW with bytes of 0x5a */
      remaining_bytes = packet_len - dw_index*8;
    
      for (byte = 0; byte < remaining_bytes; byte++) {
      
	(*((uchar_t *)tx_buf1_pointer[packet] + dw_index*8 + byte)) = 0x5a;
      }
    }
    else {
      /* Fill data with xtalk stress pattern */
      while (dw_index < packet_len/8) {

	(*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) =
	    xtalk_pattern[dw_index % 8];
        
	dw_index++;
      }
    
      /* Fill the remaining partial DW with bytes of 0x55 */
      remaining_bytes = packet_len - dw_index*8;
    
      for (byte = 0; byte < remaining_bytes; byte++) {
      
	(*((uchar_t *)tx_buf1_pointer[packet] + dw_index*8 + byte)) = 0x55;
      }
    }
  }
  
  /* Allocate memory for RX buffer pointer ring. */
  if (!(rx_ring_malloc_ptr = (__uint64_t *) MALLOC(sizeof (erx_ring_t) + _4K_))) {
    if (verbose) printf("FAILED\n"); 
    result_fail(diag_name, KLDIAG_ENET_LOOP_MALLOC, "Malloc failure");
    printf("%s: Couldn't allocate memory for RX pointer ring.\n", diag_name);
    return(enet_rc=KLDIAG_ENET_LOOP_MALLOC);
  } 
    
  rx_ring_base = ROUND64(rx_ring_malloc_ptr, _4K_); /* Round up to 4K boundary */

#if 0
  printf("rx_ring_base = 0x%016llx\n",rx_ring_base);
#endif
  
  /* Write the base address to the ERBR_H and ERBR_L registers.  Bits
     63:60 are the node WID.  All other attribute bits are set to
     zero. */
  ioc3_bp->eregs.erbr_h  = 
      (TO_PHYS(rx_ring_base) | (node_wid << 60)) >> 32;

  ioc3_bp->eregs.erbr_l  = TO_PHYS(rx_ring_base);
  
  for (packet = 0; packet < num_packets; packet++) {
    
    /* Allocate memory for RX buffers.  Buffers are 1664 bytes, but must
       be 128-byte aligned, so we overallocate and round up.  The
       hardware also requires that the buffer not cross a 16K boundary,
       so we test for this case, and do another malloc if the first try
       did cross a 16K boundary. */
    do {
      
      if (!(rx_buf_malloc_ptr[packet] = 
            (__uint64_t *) MALLOC(sizeof (erx_buffer_t) + 128))) {
        
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_MALLOC, "Malloc failure");
        printf("%s: Couldn't allocate memory for RX packet %d buffer.\n", diag_name, packet);
        return(enet_rc=KLDIAG_ENET_LOOP_MALLOC);
      }
      
      rx_buf_pointer[packet] = ROUND64(rx_buf_malloc_ptr[packet], 128); /* Round up to 128B boundary */

      /* Test for 16K crossing */
      if ((rx_buf_pointer[packet] & 0x3fff) + sizeof (erx_buffer_t) > 0x4000) {
        
	if (garbage_index >= FULL_RING - 1) {
	  if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_MALLOC, "Malloc failure");
	  printf("%s: Too many garbage pointers (RX packet %d).\n", diag_name, 
		 packet);
	  return(enet_rc=KLDIAG_ENET_LOOP_MALLOC);
	}
	
	garbage_pointers[garbage_index++] = rx_buf_malloc_ptr[packet];
        try_again = 1;
      }
      
      else try_again = 0;
      
    } while (try_again);
    
#if 0
    printf("rx_buf_pointer[%d] = 0x%016llx\n", packet, rx_buf_pointer[packet]);
#endif
  
    /* Write the ring entry.  Bits 63:60 are the node WID.  All other
       attribute bits are set to 0. */
    (*((__uint64_t *)rx_ring_base + packet)) = 
        TO_PHYS(rx_buf_pointer[packet]) | (node_wid << 60);
    
  }

  /* Enable Bridge error interrupts */
  bridge_bp = (bridge_t *) bridge_baseAddr;
  old_bridge_int_enable = bridge_bp->b_int_enable;
  
  bridge_bp->b_int_rst_stat = 0x7f;      /* Clear all error interrupts  */
  bridge_bp->b_int_enable = 0xffc1ff00;  /* Enable all interrupts,
					    except for LLP errors and
					    device interrupts (which
					    will set even if disabled) */

  /* For IOC3 internal loopback and Twister chip loopback, we only want
     to test 100Mb/s operation.  For PHY chip loopback and external
     loopback, we want to test both 10 and 100 Mb/s operation. */
  if (type == TWISTER_LOOP || 
      type == IOC3_LOOP || 
      type == EXTERNAL_LOOP_100 || 
      type == XTALK_STRESS) 
      speed = SPEED_100; 
  else speed = SPEED_10;
  
  while (speed <= SPEED_100) {

    if (type == EXTERNAL_LOOP_10 && speed == SPEED_100) break; 

#if 0
    printf("\n%s: Testing %s at %s.\n", type_string, speed ? "100Mb/s" : "10Mb/s");
#endif
    
    for (packet = 0; packet < num_packets; packet++) {
      
      /* Write to second TX BUF1 DW.  This was deferred until now so we
         can indicate the speed. */
      dw_index = 1;
    
      (*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
          mac_address << 32 |
              type << 24 |
                  speed << 16 |
                      packet;
      
      /* Clear the first DW in each RX buffer to clear valid bit. */
      (*(__uint64_t *)rx_buf_pointer[packet]) = 0;
    }
  
    /* Reset IOC3 Ethernet  */
    ioc3_bp->eregs.emcr = EMCR_RST;
  
    timeout = 10;
  
    while (!(ioc3_bp->eregs.emcr & EMCR_ARB_DIAG_IDLE)) {
    
      rtc_sleep(1000);         /* 1 ms */
      if (timeout-- <= 0) {
      
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_EMCR_RST_TO, "EMCR reset timeout");
        printf("%s: Timed out on EMCR_ARB_DIAG_IDLE trying to reset enet.\n", diag_name);
	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        return(enet_rc=KLDIAG_ENET_EMCR_RST_TO);
      }
    }
  
    ioc3_bp->eregs.emcr  = 0x0;
    ioc3_bp->eregs.etcir = 0x0;
    ioc3_bp->eregs.etpir = 0x0;
    ioc3_bp->eregs.ercir = 0x0;
    ioc3_bp->eregs.erpir = 0x0;

    /* Set appropriate EMCR bits */
    rx_off = 0x4 << EMCR_RXOFF_SHIFT; /* No pad bytes, for simplicity */
    ioc3_bp->eregs.emcr  = EMCR_DUPLEX | EMCR_PADEN | 
	rx_off | EMCR_RAMPAR | EMCR_BUFSIZ;
    
    /* Initialize ERCSR */
    ioc3_bp->eregs.ercsr  = ERCSR_THRESH_MASK; /* Max thresh value */
  
    /* Initialize ERBAR */
    ioc3_bp->eregs.erbar  = ERBAR_BARRIER_BIT << ERBAR_RXBARR_SHIFT;
  
    /* Write big endian MAC address to EMAR_L and EMAR_H */
    bigend_mac_address =  (mac_address & 0x00000000000000ff) << 40;
    bigend_mac_address |= (mac_address & 0x000000000000ff00) << 24;
    bigend_mac_address |= (mac_address & 0x0000000000ff0000) <<  8;
    bigend_mac_address |= (mac_address & 0x00000000ff000000) >>  8;
    bigend_mac_address |= (mac_address & 0x000000ff00000000) >> 24;
    bigend_mac_address |= (mac_address & 0x0000ff0000000000) >> 40;
    ioc3_bp->eregs.emar_l  = LO_WORD(bigend_mac_address);
    ioc3_bp->eregs.emar_h  = HI_WORD(bigend_mac_address);

    
    /* Enable TX_EMPTY interrupt */
    ioc3_bp->eregs.eier  = EISR_TXEMPTY;

    /* Initialize ETCSR.  Values from kern/master.d/if_ef */
    ioc3_bp->eregs.etcsr  =  21 |    /* IPGT  = 21 */ 
	21 << ETCSR_IPGR1_SHIFT |    /* IPGR1 = 21 */ 
	    21 << ETCSR_IPGR2_SHIFT; /* IPGR2 = 21 */
    
    /* Loopback type-specific configuration */
    if (type == IOC3_LOOP) {
      
      /* Set the loopback bit in the EMCR */
      ioc3_bp->eregs.emcr |= EMCR_LOOPBACK;
    }
    
    else {
      
      /* Reset the PHY */
      if (rc = (diag_enet_phy_reset(diag_mode, bridge_baseAddr, npci))) {
        if (verbose) {
	  printf("FAILED\n"); 
	  printf("%s: Phy reset failed, abandoning test.\n", diag_name);
	}
        return(rc);
      }

      if (type == EXTERNAL_LOOP) {  /* Note that we skip testing AN if
        			       the loopback type is one of the
        			       specified speed/duplex types */
        
        /* Test auto-negotiation */
        if (rc = diag_enet_phy_or(diag_mode, bridge_baseAddr, npci,
                                  0, (u_short) MII_R0_RESTARTAUTO)) return(rc);
        
        timeout = 2000;  /* 2 seconds */
	
	reg1_value = 0;

        while (!(reg1_value & MII_R1_AUTODONE)) {
	  
          rtc_sleep(1000);
          
          if(timeout-- <= 0) {
            
            if (verbose) printf("FAILED\n"); 
	    result_fail(diag_name, KLDIAG_ENET_LOOP_AN_TO, "Auto-negotiation timeout");
            printf("%s: Phy did not complete auto-negotiation\n", diag_name);
            printf("                with itself via external loopback cable.\n");
	    if (verbose) {
	      printf("                  ==>   Reg 0 = 0x%04x\n", reg0_value);
	      printf("                  ==>   Reg 1 = 0x%04x\n", reg1_value);
	      printf("                  ==>   Reg 4 = 0x%04x\n", reg4_value);
	      printf("                  ==>   Reg 5 = 0x%04x\n", reg5_value);
	      printf("                  ==>   Reg 6 = 0x%04x\n", reg6_value);
	      printf("                  \n");
	      printf("            *** Are you sure there is an external loopback? ***\n");
	    }
	    diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
            return(enet_rc=KLDIAG_ENET_LOOP_AN_TO);
            
          }
	  
	  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
				      0, &reg0_value)) return(rc);
	  
	  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci,
				      1, &reg1_value)) return(rc);
	  
	  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
				      4, &reg4_value)) return(rc);
	  
	  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
				      5, &reg5_value)) return(rc);
	  
	  if (rc = diag_enet_phy_read(diag_mode, bridge_baseAddr, npci, 
				      6, &reg6_value)) return(rc);
	  
        }
               
        if (!(reg6_value & MII_R6_LPNWABLE)) {
	  
	  if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_AN_ABLE, "AN_ABLE not set after AN");
	  printf("%s: PHY reg 6 LP_AN_ABLE bit not set after auto-negotiation.\n", diag_name);
	  if (verbose) {
	    printf("                  ==>   Reg 0 = 0x%04x\n", reg0_value);
	    printf("                  ==>   Reg 1 = 0x%04x\n", reg1_value);
	    printf("                  ==>   Reg 4 = 0x%04x\n", reg4_value);
	    printf("                  ==>   Reg 5 = 0x%04x\n", reg5_value);
	    printf("                  ==>   Reg 6 = 0x%04x\n", reg6_value);
	    printf("                  \n");
	    printf("            *** Are you sure there is an external loopback? ***\n");
	  }
	  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
	  return(enet_rc=KLDIAG_ENET_LOOP_AN_ABLE);
	}
        
        if ((reg4_value & 0x3e0) != (reg5_value & 0x3e0)) {
	  
	  if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_AN_MODE, "Mode bits wrong after AN");
 	  printf("%s: PHY reg 5 mode support bits (9:5) do not\n", diag_name);
 	  printf("%s: match reg 4 mode support bits after\n", diag_name);
 	  printf("%s: autonegotiation.  Reg 4: 0x%04x  Reg 5: 0x%04x\n", diag_name,
		 reg4_value, reg5_value);

	  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
	  return(enet_rc=KLDIAG_ENET_LOOP_AN_MODE);
	}
        
        if (rc = (diag_enet_phy_reset(diag_mode, bridge_baseAddr, npci))) {
          if (verbose) printf("FAILED\n"); 
          printf("%s: Phy reset failed after autonegotiation, \
abandoning test.\n", diag_name);
          return(rc);
        }
      } 

      /* Set speed and duplex in PHY register 0 */
      if (type == PHY_LOOP && speed == SPEED_10) {
        
        /* Set the 10BT_LPBK bit in the LBREMR register (reg 24, bit 11) */
        if (rc = diag_enet_phy_or(diag_mode, bridge_baseAddr, npci,
                                  24, (u_short) 0x0800)) return(rc);
        
        reg_value = MII_R0_LOOPBACK | MII_R0_DUPLEX;
      }
      
      else if (type == PHY_LOOP && speed == SPEED_100)
          reg_value = MII_R0_LOOPBACK | MII_R0_SPEEDSEL | MII_R0_DUPLEX;
      
      else if (((type == TWISTER_LOOP  || 
		 type == XTALK_STRESS  || 
		 type == EXTERNAL_LOOP) && speed == SPEED_100) ||
	       type == EXTERNAL_LOOP_100)
          reg_value = MII_R0_SPEEDSEL | MII_R0_DUPLEX;
      
      else if ((type == EXTERNAL_LOOP && speed == SPEED_10) ||
	       type == EXTERNAL_LOOP_10)
          reg_value = MII_R0_DUPLEX;
      
      else {
        if (verbose) printf("FAILED\n"); 
        printf("%s: PROGRAMMING ERROR!!!!", diag_name);
      }
      
      if (rc = diag_enet_phy_write(diag_mode, bridge_baseAddr, npci,
                                   0, (u_short) reg_value)) return(rc);

      /* Set Twister loopback mode in PHY register 24, if type is
         TWISTER_LOOP: bit9 = 0, bit8 = 1 */
      if (type == TWISTER_LOOP || (type == XTALK_STRESS && mode != DIAG_MODE_EXT_LOOP)) {
        
        /* Clear bit 9 */
        if (rc = diag_enet_phy_and(diag_mode, bridge_baseAddr, npci,
                                   24, (u_short) ~0x0200)) return(rc);
        /* Set bit 8 */
        if (rc = diag_enet_phy_or(diag_mode, bridge_baseAddr, npci,
                                  24, (u_short) 0x0100)) return(rc);
      }

      /* Set F_CONNECT (Force disconnect func. bypass) bit in reg 23 */
      if (rc = diag_enet_phy_or(diag_mode, bridge_baseAddr, npci, 
                                23, (u_short) 0x0020)) return(rc);
      
      /* The PHY seems to need some time to adjust to changes, so we
         insert a delay here.  The problem is worse when there is an
         external loop cable.  This is presumably due to
         autonegotiation.  */
      if (mode == DIAG_MODE_EXT_LOOP || 
	  type == EXTERNAL_LOOP      || 
	  type == EXTERNAL_LOOP_10   || 
	  type == EXTERNAL_LOOP_100) 
	  rtc_sleep(200000);  /* 200 ms */
      else rtc_sleep(1000);  /* 1 ms */
      
    }
    
    /* Initialize ERPIR and ETPIR */
    ioc3_bp->eregs.erpir = 511 << 3; /* No need to be precise, this
                                        will work for all cases */
    
    if (type == XTALK_STRESS) 
	ioc3_bp->eregs.etpir = 495 << 7;
    else 
	ioc3_bp->eregs.etpir = num_packets << 7;

    /* Enable RX DMA */
    ioc3_bp->eregs.emcr |= 
        (EMCR_RXEN    | 
         EMCR_RXDMAEN);
    
    /* Enable TX DMA */
    ioc3_bp->eregs.emcr |= 
        (EMCR_TXEN    | 
         EMCR_TXDMAEN);

    /* For the Xtalk stress test, we just sit in a loop checking the
       EISR and Bridge ISR for errors and chasing the consume pointers.
       There is no result checking of the data, and checking the xtalk
       error registers on the Hub, Xbow, and Bridge is left either to an
       exception handler or the external caller of this function.  In
       normal mode, we try to sustain this for about one second, and
       about five seconds in heavy mode. */
    if (type == XTALK_STRESS) {
      if (mode == DIAG_MODE_HEAVY || mode == DIAG_MODE_EXT_LOOP) 
	  stress_time = 5000000;
      else stress_time = 1000000;
    
      prev_ercir = 0xffffffff;
      prev_etcir = 0xffffffff;;
      dma_hung = 0;
      xtalk_stress_loop = 0;
      rx_stall_counter = 0;
      tx_stall_counter = 0;

      /* Clear error bits in PCI SCR */
      ioc3_bp->pci_scr |= 0xffff0000;

      total_retries = 0;
      do {

	/* PIOs in this loop are silently retried if they get a bus
	   error.  The PRE_PIO and POST_PIO macros implement the retry.
	   This is necessary because the heavy traffic of the DMA
	   occasionally causes PIO timeouts.  The number of retries are
	   reported as a warning, but not until the loop has completed,
	   since the latency of the printf will cause us to get behind,
	   and get a TX_EMPTY interrupt. */
        PRE_PIO;
        ercir = ioc3_bp->eregs.ercir;
	etcir = ioc3_bp->eregs.etcir;
	POST_PIO;
	
	if (ercir != prev_ercir) {
	  if (rx_stall_counter > 100) 
	      printf("%s: *WARNING* RX CONSUME took %d iterations of the Xtalk stress loop to increment.", 
		     diag_name, rx_stall_counter); 
	  rx_stall_counter = 0;
	}
	else rx_stall_counter++;
	
	if (etcir != prev_etcir) tx_stall_counter = 0;
	else tx_stall_counter++;
	
	if ((ercir == prev_ercir) && (rx_stall_counter > STALL_THRESH)) {
	  
	  if (verbose) printf("FAILED\n"); 
	  printf("%s: RX CONSUME did not increment from Xtalk stress loop %d to %d.\n", diag_name,
		 xtalk_stress_loop - STALL_THRESH, xtalk_stress_loop);

	  dma_hung = 1;
	}
    
	if ((etcir == prev_etcir) && (tx_stall_counter > STALL_THRESH)) {
	  if (verbose && !dma_hung) printf("FAILED\n"); 
	  printf("%s: TX CONSUME did not increment from Xtalk stress loop %d to %d.\n", diag_name,
		 xtalk_stress_loop - STALL_THRESH, xtalk_stress_loop);
	  dma_hung = 1;
	}

	if (dma_hung) {
	  if (verbose) {
	    printf("%s: Bridge response buffer status: 0x%08x\n", diag_name, bridge_bp->b_resp_status);
	    diag_check_pci_scr(diag_mode, bridge_baseAddr, npci, diag_name);
	    diag_enet_dump_regs(diag_mode, bridge_baseAddr, npci);
#ifdef DEBUG
	    crbx(NASID_GET(bridge_baseAddr), printf);
#endif
	  }
	  result_fail(diag_name, KLDIAG_ENET_LOOP_DMA_TO1, "DMA timeout in stress loop");
	  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
	  return(enet_rc=KLDIAG_ENET_LOOP_DMA_TO1); 
	}
	
      
	/* Check EISR and Bridge ISR for unexpected interrupts; none is
           expected here */
	PRE_PIO;
	eisr = ioc3_bp->eregs.eisr;
	br_isr = bridge_bp->b_int_status;
	POST_PIO;
	
	if (eisr) {
        
	  if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_RUPT1, "Unexpected IOC3 interrupt");
	  printf("%s: Unexpected EISR condition in Xtalk stress loop %d.\n", diag_name,
		 xtalk_stress_loop);
	  printf("%s:   EISR value = 0x%08x\n", diag_name, eisr);

	  diag_check_pci_scr(diag_mode, bridge_baseAddr, npci, diag_name);
	  diag_enet_dump_regs(diag_mode, bridge_baseAddr, npci);

	  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
	  enet_rc=KLDIAG_ENET_LOOP_BAD_RUPT1;
	}

	if (br_isr) {
        
	  if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_RUPT2, "Unexpected Bridge interrupt");
	  printf("%s: Unexpected Bridge ISR condition in Xtalk stress loop %d.\n", diag_name,
		 xtalk_stress_loop);
	  printf("%s:   Bridge ISR value = 0x%08x\n", diag_name, (__uint64_t) br_isr);

	  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
	  enet_rc=KLDIAG_ENET_LOOP_BAD_RUPT2;
	}
	if (enet_rc) return(enet_rc);
	
	prev_ercir = ercir;
	prev_etcir = etcir;
	
	PRE_PIO;
	ioc3_bp->eregs.erpir = ioc3_bp->eregs.ercir + (495 << 3);
	ioc3_bp->eregs.etpir = ioc3_bp->eregs.etcir + (495 << 7);
	POST_PIO;

	xtalk_stress_loop++;
	rtc_sleep(xtalk_stress_loop);
	
      } while (rtc_time() - start_time < stress_time);
      
    }
    
    
    
    /* Wait for IOC3 to think it's done (TX_EMPTY).  Time out if it
       doesn't complete in the appropriate amount of time.  Check for
       unexpected interrupts in the polling loop. */
    timeout = (num_packets * packet_len)/100;  /* ~ 1us/byte @ 10Mb/s */
    
    if (type == IOC3_LOOP) timeout = timeout/7; /* IOC3 loop is actually 66.67 Mb/s */
    
    else timeout = (speed == SPEED_100) ? timeout/10 : timeout;

    timeout += 500;  /* Needed for small num_packets (increased from 3
    			to 500 to fix PV 409913.  */
    
    start_timeout = timeout;
    
    while (!(ioc3_bp->eregs.eisr & EISR_TXEMPTY)) {
      
      /* In xtalk stress mode, we have to keep kicking RX_PRODUCE so we
         don't get an RX OFLO */
      if (type == XTALK_STRESS) 
	  ioc3_bp->eregs.erpir = ioc3_bp->eregs.ercir + (495 << 3);
      
      rtc_sleep(100);          /* 100 us */
      if (timeout-- <= 0) {
        
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_DMA_TO2, "DMA t.o. polling TX_EMPTY");
        printf("%s: Timed out waiting for TX_EMPTY \
interrupt (%s:  %s).\n", diag_name, type_string, speed ? "100Mb/s" : "10Mb/s");
        
	if (verbose) {
	  diag_check_pci_scr(diag_mode, bridge_baseAddr, npci, diag_name);
	  diag_enet_dump_regs(diag_mode, bridge_baseAddr, npci);
#ifdef DEBUG
	  crbx(NASID_GET(bridge_baseAddr), printf);
#endif
	}
	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        return(enet_rc=KLDIAG_ENET_LOOP_DMA_TO2);
      }

      /* Check EISR for unexpected interrupts; the only expected
         interrupt is TX_EMPTY.  XXX: this will be enhanced to diagnose
         the problem. */
      if (ioc3_bp->eregs.eisr & ~EISR_TXEMPTY) {
        
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_RUPT3, "Unexpected IOC3 interrupt");
        printf("%s: Unexpected EISR condition while waiting \
for TX_EMPTY (%s:  %s).\n", diag_name, type_string, speed ? "100Mb/s" : "10Mb/s");
        printf("%s:   EISR value = 0x%08x\n", diag_name, ioc3_bp->eregs.eisr);
        printf("%s:   Elapsed time = %d us\n", diag_name, 
               (start_timeout - timeout) * 100);
        
	diag_check_pci_scr(diag_mode, bridge_baseAddr, npci, diag_name);
	diag_enet_dump_regs(diag_mode, bridge_baseAddr, npci);

	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        enet_rc=KLDIAG_ENET_LOOP_BAD_RUPT3;
      }
      if (enet_rc) return(enet_rc);
    }

    /* Check Bridge ISR for interrupt  */
    if (!(bridge_bp->b_int_status & (1 << npci))) {  /* Not sure if this
							is valid for all board types */
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_NO_RUPT, "Missing bridge interrupt");
        printf("%s: Bridge ISR bit %d did not set after TX_EMPTY \
interrupt (%s:  %s).\n", diag_name, npci, type_string, speed ? "100Mb/s" : "10Mb/s");
	printf("%s:   ==> Bridge ISR = 0x%08x\n", diag_name, bridge_bp->b_int_status);
        
	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        return(enet_rc=KLDIAG_ENET_LOOP_NO_RUPT);
    }

    /* Clear the TX_EMPTY bit in the EISR, and disable the interrupt */
    ioc3_bp->eregs.eisr = EISR_TXEMPTY;
    ioc3_bp->eregs.eier = 0;

    /* Wait for the valid bit to be set in the RX buffer for the last
       packet.  XXX also should set up for an RX_THRESH_INT when the
       last packet is received. */
    timeout = 10;         /* This is generous */
    while (!((*(__uint64_t *)rx_buf_pointer[num_packets - 1]) & 
             (__uint64_t) 0x8000000000000000)) {
      
      rtc_sleep(100);          /* .1 ms */
      if (timeout-- <= 0) {
        
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_DMA_TO3, "DMA t.o. polling valid bit");
        printf("%s: Timed out polling RX buffer valid \
bit. (%s:  %s).\n", diag_name, type_string, speed ? "100Mb/s" : "10Mb/s");
        
        for (packet = 0; packet < num_packets; packet++) {
          
#if 0
          printf("   Packet %d  DW[0:2] : %016llx  %016llx  %016llx\n",
                 packet,
                 (*(__uint64_t *)rx_buf_pointer[packet]),
                 (*((__uint64_t *)rx_buf_pointer[packet] + 1)),
                 (*((__uint64_t *)rx_buf_pointer[packet] + 2)));
#else

          /* XXX - could add analysis here of whether a packet was
             dropped. */
          if (!((*(__uint64_t *)rx_buf_pointer[packet]) & 
                (__uint64_t) 0x8000000000000000)) {
	    printf("%s: %d of %d packets were received.\n", diag_name,
		   packet, num_packets); 
	    break;
	  }
#endif
        }
        
        
	if(verbose) {
	  diag_check_pci_scr(diag_mode, bridge_baseAddr, npci, diag_name);
	  diag_enet_dump_regs(diag_mode, bridge_baseAddr, npci);
#ifdef DEBUG
	  crbx(NASID_GET(bridge_baseAddr), printf);
#endif
	}
	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        return(enet_rc=KLDIAG_ENET_LOOP_DMA_TO3);
      }

      /* Check EISR for unexpected interrupts; the only expected
         interrupt is TX_EMPTY.  XXX: this will be enhanced to diagnose
         the problem. */
      if (ioc3_bp->eregs.eisr & ~EISR_TXEMPTY) {
        
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_RUPT4, "Unexpected IOC3 interrupt");
        printf("%s: Unexpected EISR condition while waiting \
for RX buffer valid bit (%s:  %s).\n", diag_name, type_string, speed ? "100Mb/s" : "10Mb/s");
        printf("%s: EISR value = 0x%08x\n", diag_name, ioc3_bp->eregs.eisr);
        
	diag_check_pci_scr(diag_mode, bridge_baseAddr, npci, diag_name);
	diag_enet_dump_regs(diag_mode, bridge_baseAddr, npci);

	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        return(enet_rc=KLDIAG_ENET_LOOP_BAD_RUPT4);
      }
    }

    /* For each TX descriptor, build an image of the expected RX buffer
     * from the TX buffer, and then compare with the actual contents.  If
     * there are discrepancies, report them.  Note that there are two
     * fields that are treated as "don't cares" in the comparison:
     *  
     *       - The IP checksum field in the first word
     *       - The four FCS (CRC) bytes at the end
     *
     * XXX - this is modelled after the way EVIL does result checking.
     *       This could be redone to be faster.
     * */
    for (packet = 0; 
	 packet < num_packets && type != XTALK_STRESS; 
	 packet++) {
      
      /* Fill the RX first word */
      scratch_rx_buf.valid           = 1;
      scratch_rx_buf.reserved_3      = 0;
      
      if (type != IOC3_LOOP) 
          scratch_rx_buf.byte_cnt = packet_len + RX_FCS_SIZE;
      else 
          /* No FCS on IOC3 loopback */
          scratch_rx_buf.byte_cnt = packet_len;
      
      scratch_rx_buf.ip_checksum = 0xFADE; /* Not compared */
      
      /* Fill the RX Status field */
      if (type != IOC3_LOOP) { 
        
        scratch_rx_buf.rsv22        = 0; /* Carrier event previously seen */ 
        scratch_rx_buf.rsv21        = 1; /* Good packet received */
        scratch_rx_buf.rsv20        = 0; /* Bad packet received */
        scratch_rx_buf.rsv19        = 0; /* Long event previously seen */
        scratch_rx_buf.rsv17        = 0; /* Broadcast  */
        scratch_rx_buf.rsv16        = 0; /* Broadcast/Multicast  */
        scratch_rx_buf.rsv12_3      = scratch_rx_buf.byte_cnt >> 3;
        scratch_rx_buf.reserved_2   = 0;
        scratch_rx_buf.rsv2_0       = scratch_rx_buf.byte_cnt;
        scratch_rx_buf.reserved_1   = 0;
        scratch_rx_buf.inv_preamble = 0;
        scratch_rx_buf.code_err     = 0;
        scratch_rx_buf.frame_err    = 0;
        scratch_rx_buf.crc_err      = 0;
      }
        
      else {                    /* IOC3 loopback.  All MAC status bits will be zero. */
          
        scratch_rx_buf.rsv22        = 0;
        scratch_rx_buf.rsv21        = 0;
        scratch_rx_buf.rsv20        = 0;
        scratch_rx_buf.rsv19        = 0;
        scratch_rx_buf.rsv17        = 0;
        scratch_rx_buf.rsv16        = 0;
        scratch_rx_buf.rsv12_3      = 0;
        scratch_rx_buf.reserved_2   = 0;
        scratch_rx_buf.rsv2_0       = 0;
        scratch_rx_buf.reserved_1   = 0;
        scratch_rx_buf.inv_preamble = 0;
        scratch_rx_buf.code_err     = 0;
        scratch_rx_buf.frame_err    = 0;
        scratch_rx_buf.crc_err      = 0;
      }
     
      /* Copy packet data from BUF1 to data part of scratch RX buffer */
      bcopy((void *)tx_buf1_pointer[packet], (void *)scratch_rx_buf.pdu, packet_len);

      /* Fill the data part a DW at a time */      
      for (dw_index = 0; dw_index < packet_len/8; dw_index++) {
        
        (*((__uint64_t *)scratch_rx_buf.pdu + dw_index))  = 
            (*((__uint64_t *) tx_buf1_pointer[packet] + dw_index));
      }

      
      /* Fill the remaining partial DW with bytes of 0x5a */
      remaining_bytes = packet_len - dw_index*8;
    
      for (byte = 0; byte < remaining_bytes; byte++) {
        
        scratch_rx_buf.pdu[dw_index*8 + byte] = 0x5a;
      
      }

      actual   = (erx_buffer_t *) rx_buf_pointer[packet];
      expected = &scratch_rx_buf;

      /* Check the first word, masking off IP checksum */
      if ((*((__int32_t *)(actual))   & ~IP_CHECKSUM_MASK) !=
          (*((__int32_t *)(expected)) & ~IP_CHECKSUM_MASK)) {
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_W1, "Miscompare: first word");
        printf("%s: RX data miscompare on packet %d first word.\n", diag_name, packet);
        printf("%s:     ==> exp: 0x%04x  recv: 0x%04x (%s:  %s).\n", diag_name, (*((__int32_t *)(expected))), 
               (*((__int32_t *)(actual))),
               type_string, speed ? "100Mb/s" : "10Mb/s");
        
	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        return(enet_rc=KLDIAG_ENET_LOOP_BAD_W1);
      }
      
      /* Check the status word, masking off Carrier Event Previously Seen */
      if ((*((__int32_t *)(actual) + 1)   & ~RSV22_MASK) !=
          (*((__int32_t *)(expected) + 1) & ~RSV22_MASK)) {
        if (verbose) printf("FAILED\n"); 
	result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_STAT, "Miscompare: status word");
        printf("%s: RX data miscompare on packet %d status word.\n", diag_name, packet);
        printf("%s:     ==> exp: 0x%04x  recv: 0x%04x (%s:  %s).\n", diag_name, (*((__int32_t *)(expected)+1)), 
               (*((__int32_t *)(actual)+1)),
               type_string, speed ? "100Mb/s" : "10Mb/s");
        
	diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
        return(enet_rc=KLDIAG_ENET_LOOP_BAD_STAT);
      }
      
      /* Check the data */
      for (dw_index = 1; dw_index < packet_len/8 + 1; dw_index++) {
        if ( *((__int64_t *)(actual) + dw_index) !=
            (*((__int64_t *)(expected) + dw_index)) ) {
          if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_DATA1, "Miscompare: data doubleword");
          printf("%s: RX data miscompare on packet %d DW %d.\n", diag_name, packet, dw_index);
          printf("%s:     ==> exp: 0x%08x  recv: 0x%08x (%s:  %s).\n", diag_name,
                 (*((__int64_t *)(expected)+dw_index)), 
                 (*((__int64_t *)(actual)+dw_index)),
                 type_string, speed ? "100Mb/s" : "10Mb/s");

	  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
          return(enet_rc=KLDIAG_ENET_LOOP_BAD_DATA1);
        }
      }

      remaining_bytes = packet_len - dw_index*8;
      for (byte = dw_index*8; byte < dw_index*8 + remaining_bytes; byte++) {
        
        if (actual->pdu[byte] != 0x5a) {
          if (verbose) printf("FAILED\n"); 
	  result_fail(diag_name, KLDIAG_ENET_LOOP_BAD_DATA2, "Miscompare: ending bytes");
          printf("%s: RX data miscompare on packet %d byte %d.\n", diag_name, packet, byte);
          printf("%s:     ==> exp: 0x%02x  recv: 0x%02x (%s:  %s).\n", diag_name, 0x5a,
                 actual->pdu[byte],
                 type_string, speed ? "100Mb/s" : "10Mb/s");

	  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
          return(enet_rc=KLDIAG_ENET_LOOP_BAD_DATA2);
        }
      }
      
      /* Four bytes of FCS are tacked onto the end, but we don't check
         it */
    }

    speed++;
  }

  diag_loop_cleanup(diag_mode, bridge_baseAddr, npci, type);
  
  finish_time = rtc_time();
    
  if (verbose)
      printf("passed (%lld us).\n",
	     finish_time - start_time);
  
  if (verbose && type == XTALK_STRESS && total_retries)  
      printf("%s: WARNING: %d PIO%s timed out and %s successfully retried.\n", diag_name, 
	     total_retries, total_retries == 1 ? "" : "s", 
	     total_retries == 1 ? "was" : "were");

  PASS(diag_name);
  
  return(enet_rc=KLDIAG_PASSED);
}

static int
diag_loop_cleanup(int diag_mode,__psunsigned_t bridge_baseAddr,int npci,int type)
{

  int packet, ii, rc;
  
  ioc3_mem_t              *ioc3_bp;
  bridge_t                *bridge_bp;
  xbow_t                  *xbow_bp;
  /* REFERENCED */
  xbowreg_t               temp;  
#if 0
  int *junk;
#endif

  /* Free allocated memory. */
  FREE(tx_ring_malloc_ptr);
  FREE(rx_ring_malloc_ptr);
  for (packet = 0; packet < num_packets; packet++) {
    FREE(tx_buf1_malloc_ptr[packet]);
    FREE(rx_buf_malloc_ptr[packet]);
  }
  for (ii = 0; ii < garbage_index; ii++) {
    FREE(garbage_pointers[garbage_index]);
  }

#if 0
  junk = (__uint64_t *) MALLOC(_64K_); 
  printf("junk = 0x%016llx\n",junk);
  FREE(junk);
#endif

  /* Reset IOC3 ethernet */
  ioc3_bp = (ioc3_mem_t *) diag_ioc3_base(bridge_baseAddr,npci);
  ioc3_bp->eregs.emcr = EMCR_RST;
  rtc_sleep(1000);
  ioc3_bp->eregs.emcr  = 0x0;
  ioc3_bp->eregs.etcir = 0x0;
  ioc3_bp->eregs.etpir = 0x0;
  ioc3_bp->eregs.ercir = 0x0;
  ioc3_bp->eregs.erpir = 0x0;
  

  /* Reset the PHY */
  if (type != IOC3_LOOP) 
      if (rc = diag_enet_phy_reset(diag_mode, bridge_baseAddr, npci))
	  return(rc);

  /* Restore the Bridge INT_ENABLE register */
  bridge_bp = (bridge_t *) bridge_baseAddr;
  bridge_bp->b_int_enable = old_bridge_int_enable;

  /* Clear the Xbow Widget 0 Status Register */
  xbow_bp = (xbow_t *) NODE_SWIN_BASE(NASID_GET(bridge_baseAddr), XBOW_WIDGET_ID);
  temp = xbow_bp->xb_wid_stat_clr;  /* Clear on read */

  /* Clear the Xbow Error Command Word and Error Upper/Lower regs */
  xbow_bp->xb_wid_err_cmdword = 0;  /* Clear on write */

  return(KLDIAG_PASSED);
}



/* NIC test. */ 
int
diag_enet_nic(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  int                     mode, verbose;
  __uint64_t              start_time, finish_time;

  strcpy(diag_name, "enet_nic");
  
  mode = GET_DIAG_MODE(diag_mode);
  verbose = GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE;
  
  if (mode == DIAG_MODE_NONE) return(enet_rc=KLDIAG_PASSED);
  
  if (verbose) 
    printf("Running %s diag (Bridge base = 0x%08x  PCI dev = %d) ... ",
	   diag_name, bridge_baseAddr, npci);
  
  start_time = rtc_time();
    
  if (enet_rc = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr,npci, diag_name))
      return(enet_rc);
    
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  /* Call function to read MAC address from NIC.  This function provides 
     all error checking (presence, CRC, etc).  */
  if (nic_get_eaddr((__int32_t *)(&ioc3_bp->mcr), 
		    (__int32_t *)(&ioc3_bp->gpcr_s), 
		    ((char *)(&mac_address) + 2))) { 
    result_fail(diag_name, KLDIAG_ENET_NIC_FAIL, " ");
    return(enet_rc=KLDIAG_ENET_NIC_FAIL);
  }
  

  finish_time = rtc_time();
    
  if (verbose)
      printf("passed (%lld us)   MAC address = 0x%012llx\n",
	     finish_time - start_time, mac_address);
  
  PASS(diag_name);
  
  return(enet_rc=KLDIAG_PASSED);
}

/* Dump IOC3 Enet registers.  This is for debugging.  We may want to
   ifdef it out of the production code. */
int
diag_enet_dump_regs(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;

#ifdef DEBUG
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(enet_rc=KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  
  printf("\n%s: Target IOC3 registers:\n", diag_name); 
  printf("%s   EMCR  : 0x%08lx   EISR  : 0x%08lx   EIER  : 0x%08lx\n", diag_name, 
         (__uint64_t) ioc3_bp->eregs.emcr, 
	 (__uint64_t) ioc3_bp->eregs.eisr, 
	 (__uint64_t) ioc3_bp->eregs.eier);
  
  printf("%s   ERCSR : 0x%08lx   ERBR_H: 0x%08lx   ERBR_L: 0x%08lx\n", diag_name, 
         (__uint64_t) ioc3_bp->eregs.ercsr, 
	 (__uint64_t) ioc3_bp->eregs.erbr_h, 
	 (__uint64_t) ioc3_bp->eregs.erbr_l);
  
  printf("%s   ERBAR : 0x%08lx   ERCIR : 0x%08lx   ERPIR : 0x%08lx\n", diag_name, 
         (__uint64_t) ioc3_bp->eregs.erbar, 
	 (__uint64_t) ioc3_bp->eregs.ercir, 
	 (__uint64_t) ioc3_bp->eregs.erpir);
  
  printf("%s   ETCSR : 0x%08lx   ERSR  : 0x%08lx   ETCDC : 0x%08lx\n", diag_name, 
         (__uint64_t) ioc3_bp->eregs.etcsr, 
	 (__uint64_t) ioc3_bp->eregs.ersr, 
	 (__uint64_t) ioc3_bp->eregs.etcdc);
  
  printf("%s   ETBR_H: 0x%08lx   ETBR_L: 0x%08lx   ETCIR : 0x%08lx\n", diag_name, 
         (__uint64_t) ioc3_bp->eregs.etbr_h, 
	 (__uint64_t) ioc3_bp->eregs.etbr_l, 
	 (__uint64_t) ioc3_bp->eregs.etcir);
  
  printf("%s   ETPIR : 0x%08lx   EBIR  : 0x%08lx   EMAR_H: 0x%08lx\n", diag_name, 
         (__uint64_t) ioc3_bp->eregs.etpir, 
	 (__uint64_t) ioc3_bp->eregs.ebir, 
	 (__uint64_t) ioc3_bp->eregs.emar_h);
  
  printf("%s   EMAR_L: 0x%08lx   EHAR_H: 0x%08lx   EHAR_L: 0x%08lx\n", diag_name, 
         (__uint64_t) ioc3_bp->eregs.emar_l, 
	 (__uint64_t) ioc3_bp->eregs.ehar_h, 
	 (__uint64_t) ioc3_bp->eregs.ehar_l);
  
#endif

  return(0);
}
