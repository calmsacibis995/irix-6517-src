#include <stdio.h>
#include <setjmp.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/addrs.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/error.h>
#include <report.h>
#include <prom_msgs.h>
#include <libkl.h>
#include <diag_lib.h>

int
diag_pcibus_sanity(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
    int                     error = 0,wid_id,i,rc;
    volatile __psunsigned_t ioc3_base;
    ioc3_mem_t              *ioc3_bp;
     __uint32_t             reg_value;
    int                     mode, verbose,continue_test;
    jmp_buf                 fault_buf;
    void                    *old_buf;
    int                     resetCheck;


    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
        printf("\n=====> %s diag took an exception. <=====\n", "pcibus_sanity");
        diag_print_exception();
        kl_log_hw_state();
        kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                          "END Hardware Error State (Forced error dump)\n");
        result_fail("pcibus_sanity",KLDIAG_PCIBUS_EXCEPTION,
                    "Took an exception");
        restorefault(old_buf);
        return(KLDIAG_PCIBUS_EXCEPTION);
    }


    mode = GET_DIAG_MODE(diag_mode);
    verbose = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE)
                    || mode == DIAG_MODE_MFG);
    resetCheck = !(diag_mode & DIAG_FLAG_NORESET);

    continue_test = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_CONTINUE)
                     || mode == DIAG_MODE_MFG);

    if (mode == DIAG_MODE_NONE)
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_PASSED);
    }


    DM_PRINTF(("Running %s diag (Bridge base = 0x%08x  PCI dev = %d)\n",
           "pcibus_sanity", bridge_baseAddr, npci));

    if (rc = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr, npci,
                                 "pcibus_sanity"))
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
	return(rc);
    }
    
    ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
    if (!ioc3_base)
    {
        error ++;
        printf("%s: Could not get the ioc3 base addr \n", "pcibus_sanity");
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        result_fail("pcibus_sanity",KLDIAG_IOC3_BASE_FAIL, " ");
        return(KLDIAG_IOC3_BASE_FAIL);
    }
    ioc3_bp = (ioc3_mem_t *) ioc3_base;

    if(resetCheck)
    {
      /*
        Make sure ethernet low hashed address has reset value.
      */
      reg_value = ioc3_bp->eregs.ehar_l;
      if (reg_value != 0)
      {
        error++;
        printf("%s: IOC3 enet hashed low addr reg reset val exp: "
               "0x%08x recv: 0x%08x \n", "pcibus_sanity", (__uint64_t) 0x0,
               (__uint64_t) reg_value);
      }
    }

    /*
       Now do a walking one through ethernet hashed 
       address low reg.
    */
    for (i = 0; i < 32;i++)
    {
         ioc3_bp->eregs.ehar_l = (1 << i);
         reg_value = ioc3_bp->eregs.ehar_h;  /* Dummy Read */
         reg_value = ioc3_bp->eregs.ehar_l;
         if (reg_value != (1 << i))
         {
              error++;
              if (verbose)
                   printf("%s: PCI walk one test.  exp: 0x%08x recv: 0x%08x \n",
                          "pcibus_sanity", (__uint64_t) (1 << i),
                          (__uint64_t) reg_value);
              if (!continue_test) 
                 break;
         }
    }

    /*
       Now do a walking zero on ethernet hashed 
       address low reg.
    */
    ioc3_bp->eregs.ehar_l = 0xffffffff;
    reg_value = ioc3_bp->eregs.ehar_h; /* dummy read */
    reg_value = ioc3_bp->eregs.ehar_l;
    if (reg_value != 0xffffffff)
    {
         error++;
         if (verbose)
              printf("%s: PCI walk zero test. exp: 0x%08x recv: 0x%08x \n",
                     "pcibus_sanity", (__uint64_t) 0xffffffff,
                     (__uint64_t) reg_value);
    }
    for (i = 0; i < 32; i++)
    {
        ioc3_bp->eregs.ehar_l = (~(1 << i));
         reg_value = ioc3_bp->eregs.ehar_h;  /* Dummy Read */
         reg_value = ioc3_bp->eregs.ehar_l;
         if (reg_value != (~(1 << i)))
         {
              error++;
              if (verbose)
                   printf("%s: PCI walk zero test. exp: 0x%08x recv: 0x%08x \n",
                          "pcibus_sanity", (~((__uint64_t) 1 << i)),
                          (__uint64_t) reg_value);
              if (!continue_test)
                 break;
         }
    }

    /* Clear ehar_l register so it is in its reset state */
    ioc3_bp->eregs.ehar_l = 0;

    
    /* Un-register diag exception handler. */
    restorefault(old_buf);

    if (error)
    {
       result_fail("pcibus_sanity",KLDIAG_PCIBUS_FAIL, " ");
       return(KLDIAG_PCIBUS_FAIL);
    }
    else
    {
       if (verbose)
           result_pass("pcibus_sanity",diag_mode);
       return(KLDIAG_PASSED);
    }
       
}

