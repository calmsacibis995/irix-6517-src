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
 *   File:  "$Id: diag_lib.c,v 1.2 1997/03/24 18:58:57 udayh Exp $"
 *
 *   Authors: Chris Satterlee and Mohan Natarajan
 */

#ident  "$Revision: 1.2 $"

#include <stdio.h>
#include <setjmp.h>
#include <libsk.h>
#include <report.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/SN/kldiag.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/mii.h>
#include <diag_lib.h>

extern __uint64_t         diag_malloc_brk;


int
diag_check_pci_scr(int diag_mode,__psunsigned_t bridge_baseAddr,int npci, char *diag_name)
{

  int pci_error = 0;
  __uint32_t pci_scr, pci_err_addr_l, pci_err_addr_h;
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;

  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
  
  if (!ioc3_base) {
    
    printf("%s: could not get the ioc3 base addr \n", diag_name);
    return(KLDIAG_IOC3_BASE_FAIL);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  pci_scr = ioc3_bp->pci_scr;

  if (pci_scr & (PCI_SCR_RX_SERR | 
		 PCI_SCR_DROP_MODE | 
		 PCI_SCR_SIG_PAR_ERR |
		 PCI_SCR_SIG_TAR_ABRT |
		 PCI_SCR_RX_TAR_ABRT |
		 PCI_SCR_SIG_MST_ABRT |
		 PCI_SCR_SIG_SERR |
		 PCI_SCR_PAR_ERR)) {
    pci_error = 1;
  }
      
  if (pci_scr & PCI_SCR_RX_SERR)
      printf("   ==>    PCI_SCR<RX_SERR> is \
active:  The IOC3 detected SERR# from another agent while it was master.\n");
  
  if (pci_scr & PCI_SCR_DROP_MODE)
      printf("   ==>    PCI_SCR<DROP_MODE> is \
active:  The IOC3 detected a parity error on an address/command phase \
or it detected a data phase parity error on a PIO write.\n");
  
  if (pci_scr & PCI_SCR_SIG_PAR_ERR)
      printf("   ==>    PCI_SCR<SIG_PAR_ERR> is \
active:  The IOC3, as a master, either detected or was signalled a \
parity error.\n");
  
  if (pci_scr & PCI_SCR_SIG_TAR_ABRT)
      printf("   ==>    PCI_SCR<SIG_TAR_ABRT> is \
active:  The IOC3 signalled a target abort.\n");
  
  if (pci_scr & PCI_SCR_RX_TAR_ABRT)
      printf("   ==>    PCI_SCR<RX_TAR_ABRT> is \
active:  The IOC3 received a target abort.\n");
  
  if (pci_scr & PCI_SCR_SIG_MST_ABRT)
      printf("   ==>    PCI_SCR<SIG_MST_ABRT> is \
active:  The IOC3 issued a master abort.\n");
  
  if (pci_scr & PCI_SCR_SIG_SERR)
      printf("   ==>    PCI_SCR<SIG_SERR> is \
active:  The IOC3 asserted SERR# (address/command parity error).\n");
  
  if (pci_scr & PCI_SCR_PAR_ERR)
      printf("   ==>    PCI_SCR<PAR_ERR> is \
active:  The IOC3 detected a parity error.\n");
  
  if (pci_error) {
    pci_err_addr_h = ioc3_bp->pci_err_addr_h;
    pci_err_addr_l = ioc3_bp->pci_err_addr_l;
  }
  
  if ((pci_err_addr_l & PCI_ERR_ADDR_VLD) && pci_error) {
    
    printf("   ==>    PCI_ERR_ADDR_L<VALID> is active: info is valid.\n");
    printf("   ==>    PCI_ERR_ADDR_H value is 0x%08x\n",pci_err_addr_h);
    printf("   ==>    PCI_ERR_ADDR_L value is 0x%08x\n",pci_err_addr_l);
    
    switch ((pci_err_addr_l & PCI_ERR_ADDR_MST_ID_MSK) >> 1) {
    case 0:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 0   (Serial A TX)\n");
      break;
      
    case 1:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 1   (Serial A RX)\n");
      break;
      
    case 2:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 2   (Serial B TX)\n");
      break;
      
    case 3:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 3   (Serial B RX)\n");
      break;
      
    case 4:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 4   (Parallel port)\n");
      break;
      
    case 8:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 8   (Enet TX descriptor read)\n");
      break;
      
    case 9:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 9   (Enet TX BUF1 read)\n");
      break;
      
    case 10:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 10  (Enet TX BUF2 read)\n");
      break;
      
    case 11:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 11  (Enet RX buffer pointer ring read)\n");
      break;
      
    case 12:
      printf("   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 12  (Enet RX buffer write)\n");
      break;
      
    default:
      
      break;
      
    }
    
    if (pci_err_addr_l & PCI_ERR_ADDR_MUL_ERR)
	printf("   ==>    PCI_ERR_ADDR_L<PCI_ERR_ADDR_MUL_ERR> is active: Another master had \
an error after this info froze.\n");
    
    printf("   ==>    Error address:  0x%08x%08x \n",
	   pci_err_addr_h, pci_err_addr_l & 0xffffff80);
  }
  
  else if (pci_error)
      printf("   ==>    PCI_ERR_ADDR_L<VALID> is not active.\n"); 
  
  return(KLDIAG_PASSED);
}

