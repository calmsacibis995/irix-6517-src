 /*
 * ioc3_sram.c :
 *      ioc3 SSRAM Tests
 *
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S GOVERNMENT RESTRICTED RIGHTS LEGEND:
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


/*
 * ioc3_sram.c - ioc3 Sram  Tests 
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_ioc3.h"
#include "d_pci.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_prototypes.h"
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/RACER/IP30.h>

/* Function to calculate SCSI base address,given the associated Bridge
   base address and the PCI device number. Might seem to be quite similar
   to ioc3 now, exists b'cos of possible changes.*/

__psunsigned_t
diag_ioc3_base(__psunsigned_t bridge_base,int npci)
{
    int            wid_id;
    __uint64_t     pci_key;
    __psunsigned_t scsi_base;

    scsi_base = IOC3_PCI_DEVIO_BASE + (npci * PCI_DEVICE_SIZE);

    return(scsi_base);

}


/* This perform following tasks:
   1. Address walking test.
   2. A parity error test
   3. Check out all locations 
*/
bool_t
ioc3_sram_test(__psunsigned_t bridge_baseAddr, int npci)
{
  int                     error = 0,wid_id,pat_ind,j,i,loop_cnt;
  pcicfg32_t              *pci_cfg_dev_ptr;
  ioc3_mem_t              *ioc3_bp;
  volatile __psunsigned_t ioc3_base;
  volatile __uint32_t     *ioc3_srambp,*ioc3_sramp;
  volatile __uint32_t     *ioc3_sramph,*ioc3_srampl;
  __uint32_t              reg_value,wr_val,exp_val, tmp;
  __uint32_t              memtest_pat[] =
      {
        0x25555, 0x2aaaa, 0x25a5a,
        0x25555, 0x2aaaa, 0x25a5a,
        0x25555, 0x2aaaa, 0x25a5a,
      };

  msg_printf(SUM,"Testing ioc3_sram ....\n");


  /* Get IOC3 base pointer */
  ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);

  msg_printf(DBG,"ioc3_base =0x%x\n",ioc3_base);
  if (!ioc3_base) {

    error ++;
    msg_printf(ERR,"ioc3_sram_test: could not get the ioc3 base addr \n");
    return(-1);
  }
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  /* Enable Parity checking.  */
  
  /* ioc3_bp->eregs.emcr |=  EMCR_RAMPAR; */
  PIO_REG_RD_32((&ioc3_bp->eregs.emcr),~0x0,tmp);
  tmp |= EMCR_RAMPAR;
  PIO_REG_WR_32((&ioc3_bp->eregs.emcr) ,~0x0,  tmp);

  /* Set the ioc3_base pointer to the starting loc.  of ioc3 ssram.  */
  ioc3_srambp = (__uint32_t  *) (ioc3_base + IOC3_RAM_OFF);
  ioc3_sramp = ioc3_srambp;

  /* First do the write part of address walk on ssram.  */
  i = 0;
  while  ((__psunsigned_t) ioc3_sramp <=
          (ioc3_base + IOC3_RAM_OFF + IOC3_SSRAM_LEN)) {

    exp_val = (1 << i) & 0xffff;
    /* *(ioc3_sramp) = exp_val | 0x20000; */
    PIO_REG_WR_32(ioc3_sramp, ~0x0, exp_val | 0x20000);
    ioc3_sramp =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (1<<(i+2)));
    i++;
  }

  /* Now begin the reads and compare.  */
  i = 0;
  ioc3_sramp = ioc3_srambp;
  while  ((__psunsigned_t) ioc3_sramp <
          ((ioc3_base + IOC3_RAM_OFF + IOC3_SSRAM_LEN))) {

    PIO_REG_RD_32(ioc3_sramp,~0x0,reg_value);
    PIO_REG_WR_32(ioc3_sramp ,~0x0,  reg_value);
    exp_val = (1 << i)  & 0xffff;
    if ((reg_value & 0x2ffff) != (exp_val & 0xffff)) {

      if (++error == 1) msg_printf(ERR,"FAILED\n");
      msg_printf(ERR,"ioc3_sram_test: sram address walk error. Exp: %05x Recv: %05x \n",
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

    PIO_REG_WR_32(ioc3_sramp ,~0x0,  wr_val);

    PIO_REG_RD_32(ioc3_sramp,~0x0,reg_value);

    if ((reg_value & 0x3ffff) != exp_val) {


      if (++error == 1) msg_printf(ERR,"FAILED\n");
      msg_printf(ERR,"ioc3_sram_test: ssram parity test error. Exp: %05x Recv: %05x \n",
             exp_val,(reg_value & 0x3ffff));
    }
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

        PIO_REG_WR_32(ioc3_srampl ,~0x0,  memtest_pat[j+pat_ind]);
        PIO_REG_WR_32(ioc3_sramph ,~0x0,  memtest_pat[j+pat_ind]);
        ioc3_srampl++;
        ioc3_sramph--;
      }
    }
    /* Now read and verify.  */
    ioc3_srampl = ioc3_srambp;
    ioc3_sramph =  (__uint32_t *) ((__psunsigned_t) ioc3_srambp + (IOC3_SSRAM_LEN-4));
    for (i= 0; i < (IOC3_SSRAM_LEN -24);i+=24) {

      for (pat_ind = 0; pat_ind <3;pat_ind++) {

        PIO_REG_RD_32(ioc3_srampl,~0x0,reg_value);
        if ((reg_value & 0x2ffff) != (memtest_pat[j+pat_ind] & 0xffff)) {

          if (++error == 1) msg_printf(ERR,"FAILED\n");
          if (error < 25)
              msg_printf(ERR,"ioc3_sram_test:  **Data miscompare(A)**  offset: %05x  exp: %05x recv: %05x \n",
                     i/2, memtest_pat[j+pat_ind] & 0xffff, (reg_value & 0x2ffff));

          else if (error == 25) {

            msg_printf(SUM,"Too many SSRAM errors . . . abandoning test.\n");
            goto ssram_fail;
          }
        }
        ioc3_srampl++;
      }

      for (pat_ind = 0; pat_ind <3;pat_ind++) {

        PIO_REG_RD_32(ioc3_sramph,~0x0,reg_value);
        if ((reg_value & 0x2ffff) != (memtest_pat[j+pat_ind] & 0xffff)) {

          if (++error == 1) msg_printf(ERR,"FAILED\n");
          if (error < 25)
              msg_printf(ERR,"ioc3_sram_test:  **Data miscompare(B)**  offset: %05x exp: %05x recv: %05x \n",
                     (IOC3_SSRAM_LEN-(i/2)), memtest_pat[j+pat_ind] & 0xffff,
                     (reg_value & 0x2ffff));

          else if (error == 25) {

            msg_printf(SUM,"Too many SSRAM errors . . . abandoning test.\n");
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


    PIO_REG_WR_32(ioc3_srampl ,~0x0,  memtest_pat[i]);
    PIO_REG_WR_32(ioc3_sramph ,~0x0,  memtest_pat[i+1]);

    PIO_REG_RD_32(ioc3_srampl,~0x0,reg_value);
    if ((reg_value & 0x2ffff) != (memtest_pat[i] & 0xffff)) {

      if (++error == 1) msg_printf(ERR,"FAILED\n");
      if (error < 25)
          msg_printf(ERR,"ioc3_sram_test:  **Data miscompare(C)**  offset: %05x exp: %05x recv: %05x \n",
                 (0x1ff8), (memtest_pat[i] & 0xffff), (reg_value & 0x2ffff));
      else if (error == 25) {

        msg_printf(SUM,"Too many SSRAM errors . . . abandoning test.\n");
        goto ssram_fail;
      }
    }
    PIO_REG_RD_32(ioc3_sramph,~0x0,reg_value);
    if ((reg_value & 0x2ffff) != (memtest_pat[i+1] & 0xffff)) {

      if (++error == 1) msg_printf(ERR,"FAILED\n");
      if (error < 25)
          msg_printf(ERR,"ioc3_sram_test:  **Data miscompare(D)**  offset: %05x exp: %05x recv: %05x \n",
                 (0x1ffc), memtest_pat[i+1] & 0xffff, (reg_value & 0x2ffff));
      else if (error == 25) {

        msg_printf(SUM,"Too many SSRAM errors . . . abandoning test.\n");
        goto ssram_fail;
      }
    }
  }


  msg_printf(DBG,"ioc3 sram test passed .... \n");

  return(error);

 ssram_fail:
    return(-1);
}
bool_t
ioc3_sram(__uint32_t argc, char **argv)
{
	d_errors = 0;
	d_errors = ioc3_sram_test(BRIDGE_BASE , 0) ;
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "IOC3 SRAM", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_IOC3_0002], d_errors );
}
