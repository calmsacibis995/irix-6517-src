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
 *   File:  "$Id: diag_lib.c,v 1.22 1997/08/21 01:25:26 jisu Exp $"
 *
 *   Authors: Chris Satterlee and Mohan Natarajan
 */

#ident  "$Revision: 1.22 $"

#include <stdio.h>
#include <setjmp.h>
#include <libsk.h>
#include <report.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/diagval_strs.i>
#include <sys/SN/agent.h>
#include <sys/SN/kldiag.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/mii.h>
#include <diag_lib.h>

#include "prom_leds.h"		/* XXX: Move to sys/SN */

typedef struct hled_to_kd_s {
    char                led;
    unsigned short      kldiagval;
} hled_to_kd_t;

static hled_to_kd_t hled_to_kd[] =
{ {FLED_RESETWAIT, KLDIAG_RESET_WAIT},
  {FLED_ICACHE, KLDIAG_ICACHE_FAIL},
  {FLED_DCACHE, KLDIAG_DCACHE_FAIL},
  {FLED_CP1, KLDIAG_DEAD_COP1},
  {FLED_HUBLOCAL, KLDIAG_LOCAL_ARB},
  {FLED_NOMEM, KLDIAG_PROM_BAD_BANK0},
  {FLED_DOWNLOAD, KLDIAG_PROM_MEMCPY_BAD},
  {FLED_MODEBIT, KLDIAG_MODEBIT}
};

extern __uint64_t         diag_malloc_brk;

/* Function to calculate IOCs base address, given the associated Bridge
   base address and the IOC3's PCI device number. */

__psunsigned_t
diag_ioc3_base(__psunsigned_t bridge_base,int npci)
{
    int            wid_id;
    __uint64_t     pci_key;
    __psunsigned_t ioc3_base;

    wid_id    = WIDGETID_GET(bridge_base);
    pci_key   = MK_SN0_KEY(NASID_GET(bridge_base), wid_id, npci);
    ioc3_base = GET_PCIBASE_FROM_KEY(pci_key);

    return(ioc3_base);
}


/* Check the config space registers to make sure:
     - The target is an IOC3
     - The PCI_ADDR register matches the bridge DEVICE<DEV_OFF> field
         - If it doesn't, print a warning, and force config space setup
         - Configure the IOC3 generic PIO pins
 */
int
diag_check_ioc3_cfg(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, char *diag_name)
{
  int                     mode, verbose;
  ioc3_cfg_t              *ioc3_cf_bp;
  ioc3_mem_t              *ioc3_bp;
  bridge_t                *bridge_bp;
  volatile __psunsigned_t ioc3_base;

  mode = GET_DIAG_MODE(diag_mode);
  verbose = (GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE) || mode == DIAG_MODE_MFG;
 
  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    if (verbose) printf("FAILED\n"); 
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "No IOC3 base");
    return(KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  ioc3_cf_bp = (ioc3_cfg_t *) (bridge_baseAddr + BRIDGE_TYPE0_CFG_DEV(npci)) ;
  bridge_bp = (bridge_t *) bridge_baseAddr;
  
  if (ioc3_cf_bp->pci_id != 0x000310a9) {
    if (verbose) printf("FAILED\n"); 
    printf("%s: Target PCI device is not an IOC3 (PCI_ID = 0x%08x) \n", 
	   diag_name, ioc3_cf_bp->pci_id);
    result_fail(diag_name, KLDIAG_IOC3_BASE_FAIL, "Target is not an IOC3");
    return(KLDIAG_IOC3_BASE_FAIL);
  }

  if ((ioc3_cf_bp->pci_addr != bridge_bp->b_device[npci].reg << BRIDGE_DEV_OFF_ADDR_SHFT) || 
       ((ioc3_cf_bp->pci_scr & 0x6) != 0x6)) {
    
    printf("\n%s: WARNING  ** Target IOC3 not initialized . . forcing initialization **\n",
	   diag_name);
    
    /* Write to DEV_OFF field of the appropriate device register. */
    bridge_bp->b_device[npci].reg &= ~BRIDGE_DEV_OFF_MASK;
    bridge_bp->b_device[npci].reg |= ((WIDGETID_GET(bridge_baseAddr) << 4) | (BRIDGE_DEVIO(npci) >> BRIDGE_DEV_OFF_ADDR_SHFT));

    /* Initialize IOC3 config regs */
    ioc3_cf_bp->pci_scr = 0xffff0006;  /* Clear error bits; enable mem space and mastership */
    ioc3_cf_bp->pci_lat = 0x0000ff00;  /* Latency timer = 255 */
    ioc3_cf_bp->pci_addr = bridge_bp->b_device[npci].reg << BRIDGE_DEV_OFF_ADDR_SHFT;
    
  }
  
  /* Normal clocks for NIC */ 
  ioc3_bp->int_out &= ~INT_OUT_DIAG;
  
  /* Enable standard IO6 GPIO pins */
  ioc3_bp->gpcr_s = GPCR_INT_OUT_EN | GPCR_MLAN_EN | GPCR_DIR_SERA_XCVR | 
      GPCR_DIR_SERB_XCVR | GPCR_DIR_PHY_RST; 
  
  return(0);
}

/* Rudimentary memory allocation function for POD diags.  We simply
   start at a base address of DIAG_BASE (defined in sn0addrs.h), and
   increment our pointer by the amount requested.  We return NULL if the
   pointer would exceed DIAG_BASE + DIAG_SIZE.  Otherwise we return the
   value of the pointer before it was incremented.  The return value is
   an UNALIGNED PHYSICAL ADDRESS in Cacheable Exclusive Write space. */

void *
diag_malloc(size_t size) 
{
  
  __uint64_t current_brk;

  if (diag_malloc_brk < DIAG_BASE) {
    printf("diag_malloc:  ERROR:  diag_malloc_brk (0x%016x) < DIAG_BASE (0x%016x)!!\n",
	   diag_malloc_brk, DIAG_BASE); 
    return(NULL);
  }
  
  if (diag_malloc_brk + size > DIAG_BASE + DIAG_SIZE) return(NULL);
  
  current_brk = diag_malloc_brk; 

  diag_malloc_brk += size;

  return((void *) (NODE_OFFSET(get_nasid()) | TO_CAC(current_brk)));
    
}


/* Rudimentary memory free function for POD diags.  Although it takes a
   pointer as a parameter, it actually frees ALL memory allocated by
   diag_malloc. */

void
diag_free(void *diag_malloc_ptr) 
{
  
  diag_malloc_brk = DIAG_BASE;
  
}


machreg_t	diag_get_epc(void);
machreg_t	diag_get_cause(void);
machreg_t	diag_get_badvaddr(void);

void
diag_print_exception(void)
{
  /* XXX - figure out how to get function name symbol and offset */
  printf(" EPC    : 0x%016lx\n", diag_get_epc());
  printf(" BadVA  : 0x%016lx\n", diag_get_badvaddr());
  printf(" Cause  : 0x%016lx\n", diag_get_cause());
}

static int mode = 0xdeadbeef;

int
diag_get_mode(void)
{
    return mode;
}

void
diag_set_mode(int arg)
{
    mode = arg;
}

#ifndef LBYTEU
#define	LBYTEU(caddr) \
	(u_char) ((*(uint *) ((__psunsigned_t) (caddr) & ~3) <<         \
	((__psunsigned_t) (caddr) & 3) * 8) >> 24)
#endif

char *get_diag_string(uint diagcode)
{
    int num_entries;
    int i;

    num_entries = sizeof(diagval_map) / sizeof(diagval_t);

    for (i = 0; i < num_entries; i++)
        if (LBYTEU(&diagval_map[i].dv_code) == diagcode)
            return diagval_map[i].dv_msg;

#if 0
    printf("diagcode = %d\n", diagcode);
#endif

    return "Unknown";
}

/*
 * Returns valid kldiag values for early init failures
 */

unsigned short
hled_get_kldiag(char hled)
{
    int         i, num;

    num = sizeof(hled_to_kd) / sizeof(hled_to_kd_t);

    for (i = 0; i < num; i++)
        if (hled == hled_to_kd[i].led)
            return hled_to_kd[i].kldiagval;

    return KLDIAG_EARLYINIT_FAILED;
}

int
hled_tdisable(char hled)
{
    switch (hled) {
        case FLED_NOMEM:
        case FLED_DOWNLOAD:
            return 1;
        default:
            return 0;
    }
}
