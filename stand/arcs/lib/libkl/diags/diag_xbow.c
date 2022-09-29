#include <stdio.h>
#include <setjmp.h>
#include <sys/xtalk/xwidget.h>
#include <sys/xtalk/xbow.h>
#include <sys/SN/agent.h>
#include <sys/SN/addrs.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/error.h>
#include <libkl.h>
#include <report.h>
#include <prom_msgs.h>
#include <rtc.h>
#include <diag_lib.h>


typedef union {
            __uint64_t   actual_addr;
            __uint32_t   addr_parts[2];
        } Intraddr;

/* use bit 1 of the xbow's port 8 link arbitration upper reg for xbow lock */
#define XBOW_LOCK_BIT 0x0002


int
xbow_lock(__psunsigned_t xbow_baseAddr, int verbose, char *diag_name)
{
   xbow_t *xbow = (xbow_t *)xbow_baseAddr;
   int retrycount = 0;

   /* delay to avoid collisions between CPUs */
   rtc_sleep(50);

   /* wait for lock to be available */
   while((xbow->xb_link(XBOW_PORT_8).link_arb_upper & XBOW_LOCK_BIT) &&
         (retrycount++ < 500))
      rtc_sleep(50);

   if(retrycount < 500)
   {
      /* got lock */
      xbow->xb_link(XBOW_PORT_8).link_arb_upper |= XBOW_LOCK_BIT;

      if(!(xbow->xb_link(XBOW_PORT_8).link_arb_upper & XBOW_LOCK_BIT))
      {
         /* failed to set lock bit */
         if(verbose)
           printf("%s: Failed to lock xbow ...\n", diag_name);

         return(0); 
      }

      /* xbow locked */
      return(1);
   }

   /* timed out waiting for lock */
   if(verbose)
      printf("%s: Xbow lock timeout ...\n", diag_name);
   return(0);
}

void
xbow_unlock(__psunsigned_t xbow_baseAddr)
{
   xbow_t *xbow = (xbow_t *)xbow_baseAddr;

   /* clear the lock bit */
   xbow->xb_link(XBOW_PORT_8).link_arb_upper &= ~XBOW_LOCK_BIT;
}


int
diag_xbowSanity(int diag_mode,__psunsigned_t xbow_baseAddr)
{
   int                     error = 0,i;
   Intraddr       	   int_addr;
   xbowreg_t               reg_value;
   xbow_t                  *xbow = (xbow_t *) xbow_baseAddr;
   __psunsigned_t          diag_hub_id;
   volatile __psunsigned_t int_hold;
   int                     mode, verbose;
   jmp_buf                 fault_buf;
   void                    *old_buf;

   /* Register diag exception handler. */
   if (setfault(fault_buf, &old_buf)) 
   {
       printf("\n=====> %s diag took an exception. <=====\n", "xbow_sanity");
       diag_print_exception();
       kl_log_hw_state();
       kl_error_show_log("Hardware Error State: (Forced error dump)\n",
	         	 "END Hardware Error State (Forced error dump)\n");
       result_fail("xbow_sanity",KLDIAG_XBOW_EXCEPTION, "Took an exception"); 
       restorefault(old_buf);
       return(KLDIAG_XBOW_EXCEPTION);
    }
    mode = GET_DIAG_MODE(diag_mode);
    verbose = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE) 
                    || mode == DIAG_MODE_MFG);

    if (mode == DIAG_MODE_NONE)
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);

        return(KLDIAG_PASSED);
    }

    
   DM_PRINTF(("Running %s diag (Xbow address = 0x%16lx)\n", "xbow_sanity", 
	  xbow_baseAddr));

   /* lock the xbow */
   if(!xbow_lock(xbow_baseAddr, verbose, "xbow_sanity"))
   {
      /* failed to lock the xbow */
      /* Un-register diag exception handler. */
      restorefault(old_buf);

      result_fail("xbow_sanity",KLDIAG_XBOW_FAIL, " ");
      return(KLDIAG_XBOW_FAIL); 
   }

   /*
     Read the xbow id register.
   */
   reg_value = xbow->xb_wid_id;
   
   /*
        We know that the LSB of the ID is always 1.
   */
   if (!(reg_value & 1))
   {
       error++;
       printf("%s: Bad xbow id reg value 0x%08x\n", "xbow_sanity", reg_value);
   }
  
   /*
       Extract the part_num out of the id. should be 0x00
       XXX Make sure that 0 is the val.
   */
   reg_value = (reg_value >> WIDGET_PART_NUM_SHFT) & 0x00000ffff;

   if (reg_value != XBOW_WIDGET_PART_NUM)
   {
       error++;
       printf("%s: Bad Xbow id Exp:%x Recv:%x \n", "xbow_sanity",
              XBOW_WIDGET_PART_NUM,reg_value);

   }

   /*
     Perform a write and read to the interupt upper 
     and lower registers.
   */
   xbow-> xb_wid_int_lower = 0x5a5a5a5a;
   xbow-> xb_wid_int_upper = 0xa5a5a5a5;
   reg_value = xbow-> xb_wid_int_lower;
   if (reg_value != 0x5a5a5a5a)
   {
      error++;
      printf("%s: Reg write Read miscompare. Exp: %x :: Recv: %x \n", "xbow_sanity",
              0x5a5a5a5a,reg_value);
   }

   reg_value = xbow-> xb_wid_int_upper;
   reg_value &= 0xff0fffff;
   if (reg_value != 0xa50da5a5)
   {
      error++;
      printf("%s: Reg write Read miscompare. Exp: %x :: Recv: %x \n", "xbow_sanity",
              0xa50da5a5,reg_value);
   }
   xbow-> xb_wid_int_lower = 0;
   xbow-> xb_wid_int_upper = 0;

   /*
     Create a Register access error. See if the widget status
     register reflects it.
   */

   xbow->xb_wid_err_lower = 0xdeadbeef;
   /*
     The previous statement should have caused an access
     error. Now see if that was detected.
   */

   reg_value = xbow-> xb_wid_stat_clr;	/* Read and Clear status */

   if (!(reg_value & XB_WID_STAT_REG_ACC_ERR))  
   {
      error++;
      printf("%s: Register access violation not detected.\n", "xbow_sanity");
   }

   /* Read from status again to make sure that it is cleared */
   reg_value = xbow-> xb_wid_stat;

   if(((xbow-> xb_wid_id ) >> 28) >= 5) {
        /* this is XBOW 2.0 or higher */
      if (reg_value >> 6 != hub_xbow_link(NASID_GET(xbow_baseAddr))) {
        error++;
        printf("%s: Status Register Clear on Read not working!!\n", "xbow_sanity");
      }
   }
   else {
      if (reg_value != 0 )
      {
        error++;
        printf("%s: Status Register Clear on Read not working!!\n", "xbow_sanity");
      }
   }


   /*
     Now do the same with the interrupt turned on. See if it is
     caught at the host end.
   */

  diag_hub_id =  (__psunsigned_t) (REMOTE_HUB_L(NASID_GET(xbow_baseAddr),
                                   IIO_WCR)) & 0xf;
  diag_hub_id = (diag_hub_id << 16);
 
  int_addr.actual_addr = (__uint64_t )REMOTE_HUB_ADDR(NASID_GET(xbow_baseAddr),
                                                      PI_INT_PEND_MOD);
  int_addr.addr_parts[0]  |= 0x00008000 | (NASID_GET(xbow_baseAddr) << 6);

  int_addr.addr_parts[0] &= 0xffff;
  int_addr.addr_parts[0] = 0x10000000| diag_hub_id| int_addr.addr_parts[0];


  xbow->xb_wid_int_upper  = int_addr.addr_parts[0];
  xbow->xb_wid_int_lower  = int_addr.addr_parts[1];
  xbow->xb_wid_control    = XBW0_CTRL_ACCERR_INTR;

  xbow->xb_wid_err_upper  = 0;	/* Cause the error */

  reg_value = xbow-> xb_wid_stat;	/* Dummy read	*/
  reg_value = xbow-> xb_wid_stat;	/* Read status back */


  if (!(reg_value & XB_WID_STAT_REG_ACC_ERR))  
  {
      error++;
      printf("%s: Register access violation not detected.\n", "xbow_sanity");
  }
 
  int_hold = REMOTE_HUB_L(NASID_GET(xbow_baseAddr), PI_INT_PEND0);

  if ((int_hold & 0x10000) != 0x10000)
  {
      error++;
      printf("%s: Interrupt did not reach PI_INT_PEND0. exp:%lx recv: %lx .\n",
	     "xbow_sanity", 0x10000, int_hold);
  }

  reg_value = xbow-> xb_wid_stat_clr;	/* Clear only after reading */


  xbow->xb_wid_int_upper  = 0;
  xbow->xb_wid_int_lower  = 0;
  xbow->xb_wid_control    = 0;

  /* unlock xbow */
  xbow_unlock(xbow_baseAddr);

  /* Un-register diag exception handler. */
  restorefault(old_buf);
  if (error)
  {
      result_fail("xbow_sanity",KLDIAG_XBOW_FAIL, " ");
      return(KLDIAG_XBOW_FAIL); 
  }
  else
  {
     if (verbose)
          result_pass("xbow_sanity",diag_mode);
     return(KLDIAG_PASSED);
  }
}
