/*
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
#ident "stand/arcs/ide/godzilla/enet/enet_diags.c: $revision 1.1$"

#include "sys/types.h"
#include <stdio.h>
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "uif.h"
#include <setjmp.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/mii.h>
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_enet.h"
#include "d_prototypes.h"

#define SGI_PHY_ADDR	0x1		/* on-board PHY */
/* PHY defines */
#define PHY_QS6612X	0x0181441       /* Quality TX */
#define PHY_ICS1889	0x0015F41       /* ICS FX */
#define PHY_ICS1890	0x0015F42       /* ICS TX */
#define PHY_DP83840	0x20005C0       /* National TX */

/* Extern function declarations */
extern int  nic_get_eaddr(__int32_t *mcr, __int32_t *gpcr_s, char *eaddr);

static __uint64_t mac_address;
static ioc3_mem_t *ioc3_bp;
static volatile __psunsigned_t ioc3_base;


/* PHY chip register test. */ 
int
enet_phy_reg()
{
  int                     timeout, error = 0, reg_num, rc;
  __uint32_t              reg_value, wr_value, exp_val, exp_mask;


  msg_printf(SUM,"Testing ethernet PHY chip registers ....\n");

  /* Read PHY register 0 (BMCR) at expected PHY address.  If this
     returns all 1's, there's a problem. */

  if (rc = enet_phy_read(0, &reg_value)) return(rc);

  if (reg_value == 0xffff) {
    if (++error == 1)
    	msg_printf(ERR,"enet_phy_reg: Read of PHY register 0 \
		failed (returned 0xffff).\n");
    goto phyreg_fail;
  }

  /* Reset the PHY. */
  if (rc = enet_phy_reset())
      return(rc);
      

  /* Read all of the registers, verifying their reset values. XXX - have
     to make sure to account for autonegotiation initiated by a partner;
     this could cause many of the expected values to change. */

  /* Read the PHY Identifier Registers (regs 2 & 3) */
  if (rc = enet_phy_read(2, &reg_value)) return(rc); /* Reg 2 */
  if (reg_value != 0x0015) {
    msg_printf(ERR,"\nenet_phy_reg: PHY Identifier register #1 value \
0x%04x is not supported by this diag!\n", 
           reg_value);
    error++;
  }
  
  if (rc = enet_phy_read(3, &reg_value)) return(rc); /* Reg 3 */
  if ( (reg_value & 0xfff0) != 0xf420 ) {
    msg_printf(ERR,"\nenet_phy_reg: PHY Identifier register #2 value \
0x%04x is not supported by this diag!\n", 
           reg_value);
    error++;
  }
  
  /* I'm just going to make unexpected reset values generate warning
     messages, since this is too easy to get wrong.  I don't want a
     failure here to prevent using Enet; if something is really broken,
     we'll find out about it soon enough, and these messages will
     help. */
  for (reg_num = 0; reg_num < 22; reg_num++) {
    
    if (rc = enet_phy_read(reg_num, &reg_value)) return(rc);
    switch (reg_num) {
    case 0:
      exp_val = 0x3000;
      exp_mask = 0xff80;
      break;
    case 1:
      exp_val = 0x7809;
      exp_mask = 0xf83f;
      break;
    case 2:
      exp_val = 0x0015;
      exp_mask = 0xffff;
      break;
    case 3:
      exp_val = 0xf420;
      exp_mask = 0xfff0;  /* 0x5c00 or 0x5c01 are supported */
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
      continue;
    case 16:
      exp_val = 0x0058;
      exp_mask = 0xffff;
      break;
    case 17:
      exp_val = 0x8000;
      exp_mask = 0xf000;
      break;
    case 18:
      exp_val = 0x0010;
      exp_mask = 0xffff;
      break;
/* Some systems show 0x4001 so we skip bit 0 check */
    case 19:
      exp_val = 0x4000;
      exp_mask = 0xfffe;
      break;
    case 20:  /* Reserved */
    case 21:  /* Reserved */
    case 22:  /* Reserved */
    default:  /* Reserved */
      continue;
    }
  
    if ((reg_value & exp_mask) != exp_val) {
      msg_printf(ERR,"enet_phy_reg: unexpected Reg %d reset value,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", reg_num, exp_val, exp_mask, reg_value);
    error++;
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
  
  if (rc = enet_phy_write(0, wr_value)) return(rc);
  if (rc = enet_phy_read(0, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    
    if (++error == 1) 
    msg_printf(ERR,"enet_phy_reg: Reg 0 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", wr_value, exp_mask, reg_value);
  }
  
  wr_value = 0x0;               /* Now clear them */  
  if (rc = enet_phy_write(0, wr_value)) return(rc);
  if (rc = enet_phy_read(0, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    
    if (++error == 1) 
    msg_printf(ERR,"enet_phy_reg: Reg 0 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", wr_value, exp_mask, reg_value);
  }
  
  /****************  Register 16 ********************/
  wr_value = 0x0020; /* Just test this bit */
  exp_mask = 0x0020;
  
  if (rc = enet_phy_write(16, wr_value)) return(rc);
  if (rc = enet_phy_read(16, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    
    if (++error == 1 ) 
    msg_printf(ERR,"enet_phy_reg: Reg 16 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", wr_value, exp_mask, reg_value);
  }
  
  wr_value = 0x0;               /* Now clear it */  
  if (rc = enet_phy_write(16, wr_value)) return(rc);
  if (rc = enet_phy_read(16, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    
    if (++error == 1 )
    msg_printf(ERR,"enet_phy_reg: Reg 16 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", wr_value, exp_mask, reg_value);
  }
  
  /****************  Register 18 ********************/
  wr_value = 0x0000;            /* enable loopback */
  exp_mask = 0x0010;

  if (rc = enet_phy_write(18, wr_value)) return(rc);
  if (rc = enet_phy_read(18, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    
  if (++error == 1)
    msg_printf(ERR,"enet_phy_reg: Reg 18value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", wr_value, exp_mask, reg_value);
  }
  
  wr_value = 0x0010;               /* disable loopback */  
  if (rc = enet_phy_write(18, wr_value)) return(rc);
  if (rc = enet_phy_read(18, &reg_value)) return(rc);
  if (reg_value != wr_value & exp_mask) {
    
    if (++error == 1 ) 
    msg_printf(ERR,"enet_phy_reg: Reg 18 value wrong,  \
exp: 0x%04x  mask: 0x%04x  got: 0x%04x\n", wr_value, exp_mask, reg_value);
  }

  /* Reset the PHY (clean up) */
  if (rc = enet_phy_reset())
      return(rc);
      
  if (error == 0) 
  	msg_printf(SUM,"Testing ethernet PHY chip registers passed\n");
  else
	msg_printf(SUM,"Testing ethernet PHY chip registers failed\n");

  
  return(error);

phyreg_fail:
  return(1);
}


/* Phy register read routine.  This is a test in itself, since it
   can time out before or during the read. */
int
enet_phy_read(int reg,  __uint32_t *val)
{
  int                     timeout;
  
  /* Get IOC3 base pointer */
  ioc3_base = PHYS_TO_K1(IOC3_PCI_DEVIO_BASE);
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  /* Wait until not busy or timeout */
  timeout = 1000;
  while (ioc3_bp->eregs.micr & MICR_BUSY) {
    us_delay(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      msg_printf(ERR, "Timed out on MICR_BUSY before read.\n");
      goto phy_read_timeout;
    }
  }

  /* Write MICR with PHY address, register offset, and read trigger set */
  ioc3_bp->eregs.micr = MICR_READTRIG |
      (SGI_PHY_ADDR << MICR_PHYADDR_SHIFT) | reg;

  /* Wait until not busy or timeout */
  timeout = 1000;
  while (ioc3_bp->eregs.micr & MICR_BUSY) {
    us_delay(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      msg_printf(ERR, "Timed out on MICR_BUSY during read.\n");
      goto phy_read_timeout;
    }
  }

  /* Get register value from MIDR_R */
  *val = ioc3_bp->eregs.midr_r & MIDR_DATA_MASK;

  return(0);

phy_read_timeout:
  msg_printf(ERR,"PHY read busy timeout\n");
  return(1);

}


/* Phy register write routine.  This is a test in itself, since it
   can time out before or during the write. */
int
enet_phy_write(int reg,  u_short val)
{
  int                     timeout;

  /* Get IOC3 base pointer */
  ioc3_base = PHYS_TO_K1(IOC3_PCI_DEVIO_BASE);
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  /* Wait until not busy or timeout */
  timeout = 1000;
  while (ioc3_bp->eregs.micr & MICR_BUSY) {
    us_delay(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      msg_printf(ERR, "Timed out on MICR_BUSY before write.\n");
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
    us_delay(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      msg_printf(ERR, "Timed out on MICR_BUSY during write.\n");
      goto phy_write_timeout;
    }
  }

  return(0);

phy_write_timeout:
  msg_printf(ERR, "PHY write busy timeout\n");
  return(1);

}


/* Phy register RMW with OR */
int
enet_phy_or(int reg,  u_short val)
{
  int        rc;
  __uint32_t temp;

  if (rc = enet_phy_read(reg, &temp)) return(rc);

  temp |= val;

  if (rc = enet_phy_write(reg, (u_short) temp)) return(rc);
  return(0);
}


/* Phy register RMW with AND */
int
enet_phy_and(int reg,  u_short val)
{
  int        rc;
  __uint32_t temp;

  if (rc = enet_phy_read(reg, &temp)) return(rc);

  temp &= val;

  if (rc = enet_phy_write(reg, (u_short) temp)) return(rc);
  return(0);
}


/* Phy reset */
int
enet_phy_reset()
{
  int        timeout, rc;
  __uint32_t reg_value;

  /* Reset the PHY. */
  reg_value = MII_R0_RESET;
  enet_phy_write(0, (u_short) reg_value);
  
  timeout = 1000;
  while (reg_value & MII_R0_RESET) {
    
    if (rc = enet_phy_read(0, &reg_value)) return(rc);

    DELAY(1000);  /* 1 ms */
    if (timeout-- <= 0) {
      msg_printf(ERR,"enet_phy_reset: Timed out waiting for PHY to reset.\n");
      return(1);
    }
  }
  
  return(0);
}

/* TX clock test. */ 
int
enet_tx_clk()
{
  
  msg_printf(SUM,"Testing ENET TX_CLK  ....\n");
    
  /* Read ETCSR<NO_TX_CLK>.  If it's set, there's a problem. */

  if (ioc3_bp->eregs.etcsr & ETCSR_NOTXCLK) {
    msg_printf(ERR,"enet_tx_clk: IOC3 is not seeing the PHY TX clock. \n");
    return(1);
  }
  
  msg_printf(SUM,"Testing ENET TX_CLK passed\n");

  return(0);
}


/* NIC test. */ 
int
enet_nic()
{
  __uint64_t              start_time, finish_time;

  msg_printf(SUM,"Testing ethernet MAC address NIC  ....\n"); 

    
  /* Call function to read MAC address from NIC.  This function provides 
     all error checking (presence, CRC, etc).  */
  if (nic_get_eaddr((__int32_t *)(&ioc3_bp->mcr), 
		    (__int32_t *)(&ioc3_bp->gpcr_s), 
		    ((char *)(&mac_address) + 2))) 
      return(1);
  
   msg_printf(SUM,"Testing ethernet MAC address NIC passed \n");
  
  return(0);
}
bool_t
enet_test(__uint32_t argc, char **argv)
{

  /* Get IOC3 base pointer */
  ioc3_base = PHYS_TO_K1(IOC3_PCI_DEVIO_BASE);
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  d_errors = 0;
  d_errors += enet_tx_clk();
  d_errors += enet_nic();
  d_errors += enet_phy_reg();
  d_errors += enet_loop(IOC3_LOOP);
  d_errors += enet_loop(PHY_LOOP);
/*
  d_errors += enet_loop(EXTERNAL_LOOP);
  d_errors += enet_loop(XTALK_STRESS);
*/
#ifdef NOTNOW
  REPORT_PASS_OR_FAIL(d_errors, "ENET ", D_FRU_IP30);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_ETHERNET_0000], d_errors );
}
enet_external_lpbk(__uint32_t argc, char **argv)
{
	d_errors = 0;
	d_errors += enet_loop(EXTERNAL_LOOP);
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "ENET ", D_FRU_IP30);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_ETHERNET_0001], d_errors );
}		
enet_stress_test(__uint32_t argc, char **argv)
{
	d_errors = 0;
	d_errors += enet_loop(XTALK_STRESS);
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "ENET ", D_FRU_IP30);
#endif
  REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_ETHERNET_0002], d_errors );
}		

#ifdef	DEBUG

int
enet_phy_probe()
{
  int i,val;
  __uint32_t r2, r3;

  for (i = 0; i < 32; i++) {
    enet_phy_read(2, &r2);
    enet_phy_read(3, &r3);
    val = (r2 << 12) | (r3 >> 4);
    switch (val) {
        case PHY_QS6612X:
                msg_printf(SUM, "\nQuality QS6612X found\n");
                return(0);
        case PHY_ICS1889:
                msg_printf(SUM, "\nICS ICS1889 found\n");
                return(0);
        case PHY_ICS1890:
                msg_printf(SUM, "\nICS ICS1890 found\n");
                return(0);
        case PHY_DP83840:
                msg_printf(SUM, "\nNational DP83840 found\n");
                return(0);
    }
  }

  msg_printf(ERR, "PHY not found\n");
  return (1);

}
#endif	/* DEBUG */
