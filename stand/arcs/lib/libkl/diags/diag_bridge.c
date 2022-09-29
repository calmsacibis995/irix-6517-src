#include <stdio.h>
#include <setjmp.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/agent.h>
#include <sys/SN/intr.h>
#include <sys/SN/addrs.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/error.h>
#include <libkl.h>
#include <report.h>
#include <prom_msgs.h>
#include <rtc.h>


typedef union {
            __uint64_t   actual_addr;
            __uint32_t   addr_parts[2];
        } Intraddr;

int
diag_bridgeSanity(int diag_mode,__psunsigned_t bridge_baseAddr)
{
    int                      error = 0,i,j;
    Intraddr                 intr_addr;
    __psunsigned_t           diag_hub_id;
    bridgereg_t              reg_value;
    bridge_t                 *bridge = (bridge_t *)bridge_baseAddr;
    volatile __uint64_t      int_hold;
    int                     mode, verbose,continue_test;
    jmp_buf                 fault_buf;
    void                    *old_buf;
    int                     count;


    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
        printf("\n=====> %s diag  took an exception. <=====\n", "bridge_sanity");
        diag_print_exception();
        kl_log_hw_state();
        kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                          "END Hardware Error State (Forced error dump)\n");
        result_fail("bridge_sanity",KLDIAG_BRIDGE_EXCEPTION, "Took an exception");
        restorefault(old_buf);
        return(KLDIAG_BRIDGE_EXCEPTION);
    }


    mode = GET_DIAG_MODE(diag_mode);
    verbose = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE)
                    || mode == DIAG_MODE_MFG);

    continue_test = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_CONTINUE)
                    || mode == DIAG_MODE_MFG);


    if (mode == DIAG_MODE_NONE)
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_PASSED);
    }

    DM_PRINTF(("Running %s diag (Bridge base = 0x%08x)\n", "bridge_sanity", bridge_baseAddr));

    /*
      To begin with read the bridge identification register.
    */
    reg_value = bridge->b_wid_id;
    
    /*
      We know that the LSB of the ID is always 1.
    */
    if (!(reg_value & 1))
    {
       error++;
       printf("%s: Bad bridge id register value (LSB != 1)\n", "bridge_sanity");
    }

    /*
       Extract the part_num out of the id. should be 0xc002.
    */
    reg_value = (reg_value >> 12) & 0x00000ffff;
   
    if (reg_value != BRIDGE_WIDGET_PART_NUM)
    {
       error++;
       printf("%s: Bad bridge id Exp:%x Recv:%x \n", "bridge_sanity",
               BRIDGE_WIDGET_PART_NUM,reg_value);
    }

    /*
       Read the bridge status register and see if the 
       PCI bit is turned on.
    */

    if (!(bridge->b_wid_stat & BRIDGE_STAT_PCI_GIO_N))
    {
       error++;
       printf("%s: PCI bit set to 0 sn status reg. \n", "bridge_sanity");
    }

    if(!(diag_mode & DIAG_FLAG_NORESET))
    {
      /*
        Read the request timeout register and verify 
        its reset val.
      */
      if(((reg_value = bridge->b_wid_req_timeout) & 0xfffff) != 0xfffff)
      {
         error++;
         printf("%s: Timeout val reg exp:%x recv: %x.\n", "bridge_sanity",
                 0xfffff,reg_value);
      }

      /*
        Read the widget control register and verify it's 
        reset val.
      */
      reg_value = bridge->b_wid_control;
      if (reg_value != 0x7f0033ff)
      {
         error++;
         printf("%s: Bridge control reg exp:%x recv: %x.\n", "bridge_sanity",
                 0x7f0033ff,reg_value);
      }

      /*
         Check to see if the device registers are up with 
         reset values.
      */
 
      if ((reg_value = bridge-> b_device[0].reg) != 0x18001000)
      {
          error++;
          printf("%s: Bad reset val in dev reg exp: %x recv: %x.\n", "bridge_sanity",
                  0x18001000,reg_value);
      }
      if ((reg_value = bridge-> b_device[1].reg) != 0x18001002)
      {
          error++;
          printf("%s: Bad reset val in dev reg exp: %x recv: %x.\n",
                  "bridge_sanity", 0x18001002,reg_value);
      }
      for ( i = 2; i < 8; i++)
      {
          if ((reg_value = bridge-> b_device[i].reg) != (0x18001002+i))
          {
             error++;
             printf("%s: Bad reset val in dev reg exp: %x recv: %x.\n", "bridge_sanity",
                    (0x18001002+i),reg_value);
          }
      }
    }

    /*
      See if interrupt status register is ok.
    */

    if (bridge->b_int_status)
    {
       error++;
       printf("%s: Interrupt status register value non-zero: 0x%08x. \n", 
	      "bridge_sanity", bridge->b_int_status); 
    }

    /*
      Perform a write to bridge register. Bridge int dest lowr reg.
      Since int are turned off this should be pretty harmless.
    */
    bridge->b_wid_int_lower = 0xa5a5a5a5;
    bridge->b_wid_int_upper = 0x5a5a5a5a;

    reg_value = bridge->b_wid_int_lower;
    if (reg_value != 0xa5a5a5a5)
    {
         error++;
         printf("%s: Bridge(wr): Exp: %x Recv:%x. \n", 
		"bridge_sanity",0xa5a5a5a5,reg_value);
    }

   /*
     Note that Upper reg. is narrower than the Lower 
     so mask off upper bits.
   */
   reg_value = bridge->b_wid_int_upper & 0xfffff;
   if (reg_value != 0xa5a5a)
   {
         error++;
         printf("%s: Bridge(wr): Exp: %x Recv:%x. \n", 
		"bridge_sanity", 0xa5a5a,reg_value);
   }

   /*
      Write to invalid address. This should cause an interrupt. The 
      interrupt is sent to memory. Then check the status.
   */
   /*
     Get the hub widget id. This is needed to formulate the 
     address.
   */
   diag_hub_id =  (__psunsigned_t) (REMOTE_HUB_L(NASID_GET(bridge_baseAddr),
                                    IIO_WCR)) & 0xf;
   diag_hub_id = (diag_hub_id << 16);
    
   /*
     The interrupt is to be sent to HUB INT_PEND reg. get its
     addr.
   */
   intr_addr.actual_addr = (__uint64_t )REMOTE_HUB_ADDR(NASID_GET(bridge_baseAddr),
                                                        PI_INT_PEND_MOD);
   /* set the IO bit and NASID */
   intr_addr.addr_parts[0]  |= 0x00008000 | (NASID_GET(bridge_baseAddr) << 6);
   intr_addr.addr_parts[0] &= 0xffff;
   intr_addr.addr_parts[0] =  diag_hub_id| intr_addr.addr_parts[0];

    bridge->b_wid_int_upper = intr_addr.addr_parts[0];
    bridge->b_wid_int_lower = intr_addr.addr_parts[1];
    bridge->b_int_enable =    BRIDGE_ISR_INVLD_ADDR;

    /*
       Now check the 122 bits of interrupt to the HUB. Note that bit 0-63
       get registered with INT_PEND0 and 63-122 with INT_PEND1. bit 1-7
       is not an INT_PEND. so we cannot check these bits.
    */
    for (i = 0; i < 20; i++)
    {
         /*
           bits 1-7 are not int_pend bits. so jump them.
         */
         if ((i > 0) && (i < 7))
            continue;

         /* tell bridge which host interrupt bit to set */
         bridge->b_int_host_err = i;

         /* force preceding write before actually generating interrupt */
         if(bridge->b_int_host_err == i)
           *(int * ) (bridge_baseAddr + 0x7c) = 0xdeadbeef;

         /* look for correct value in bridge interrupt status reg */
         for(count = 0;
             (bridge->b_int_status != BRIDGE_ISR_INVLD_ADDR) && (count < 1000);
             count++)
           rtc_sleep(100);

         if ((reg_value = bridge->b_int_status) != BRIDGE_ISR_INVLD_ADDR)
         {
             error++;
             printf("%s: Interrupt status reg bit not set:  "
                    "exp: 0x%08x  recv: 0x%08x. \n",
		    "bridge_sanity", BRIDGE_ISR_INVLD_ADDR, reg_value);
         }

         for(count = 0; count < 1000; count++)
         {
           int_hold = REMOTE_HUB_L(NASID_GET(bridge_baseAddr), PI_INT_PEND0);
           if ((int_hold & (((__uint64_t ) 1) << i)) ==
               (((__uint64_t ) 1) << i)) 
             break;
           rtc_sleep(100);
         }

         if (count == 1000)
         {
             error++;
             printf("%s: Interrupt %d did not reach PI_INT_PEND0:  "
                    "exp:%lx recv: %lx .\n",
		    "bridge_sanity", i,(((__uint64_t ) 1) << i),int_hold);
         }

         /* clear bridge rupt status and hub rupt pend regs */
         bridge->b_int_rst_stat =  BRIDGE_IRR_REQ_DSP_GRP_CLR;
         REMOTE_HUB_CLR_INTR(NASID_GET(bridge_baseAddr), i);

         if ((!continue_test) && error)
            break;
    }

    /*
       check for bits 63-122. XXX visit me. Fails on
       systems with bridge only. no xbow .....
    */
#if 0

    j = 0;
    for (i = 64; i < 122; i++)
    {
         bridge->b_int_host_err = i; /* Set the bit. Exp val is 1 << i. */
         /*
            Now create the FAULT.
         */
         *(bridge_baseAddr + 0x7c) = 0xdeadbeef;
         reg_value = bridge-> b_int_status;
         if (reg_value != BRIDGE_ISR_INVLD_ADDR)
         {
             error++;
             printf("%s: Interrupt %d did not occur.\n", "bridge_sanity", i);
         }

         int_hold = REMOTE_HUB_L(NASID_GET(bridge_baseAddr), PI_INT_PEND1);

         if (int_hold !=  (((__uint64_t ) 1) << j))
         {
             error++;
             printf("%s: Interrupt status reg bit not set:  exp: 0x%08x  recv: 0x%08x. \n",
		    "bridge_sanity", BRIDGE_ISR_INVLD_ADDR, reg_value);
         }
         bridge->b_int_rst_stat =  BRIDGE_IRR_REQ_DSP_GRP_CLR;
         REMOTE_HUB_CLR_INTR(NASID_GET(bridge_baseAddr), i);
         j ++;
    }
#endif

    /*
      Cleanup.
    */
    bridge->b_int_enable    = 0;
    bridge->b_wid_int_upper = 0;
    bridge->b_wid_int_lower = 0;
    /* Un-register diag exception handler. */
    restorefault(old_buf);

    if (error)
    {
         result_fail("bridge_sanity",KLDIAG_BRIDGE_FAIL, " ");
         printf("%s:  %d errors occured .... \n", "bridge_sanity", error);
         return(KLDIAG_BRIDGE_FAIL);
    }
    else 
    {
        if (verbose)
             result_pass("bridge_sanity",diag_mode);
        return(KLDIAG_PASSED);
    } 
}
