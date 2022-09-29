#include <stdio.h>
#include <setjmp.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/addrs.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/error.h>
#include <libkl.h>
#include <report.h>
#include <prom_msgs.h>

extern int get_pcilink_status(__psunsigned_t,int,int,reg32_t *,int);


int
ioc3_config_verify(int verbose,__psunsigned_t bridge_base, int pci_dev_num, char *diag_name, int resetCheck)
{
    int               error =0;
    pcicfg32_t        *pci_cfg_dev_ptr;
    __uint32_t        reg_value;
    
    pci_cfg_dev_ptr = (pcicfg32_t *) (bridge_base +
                                BRIDGE_TYPE0_CFG_DEV(pci_dev_num)) ;

   if(resetCheck)
   {
     /*
        Now read the config registers and verify the reset
        values. dev id has already been verified.
     */
     reg_value = pci_cfg_dev_ptr->pci_scr;
     if (reg_value != 0x02800000)
     {
        error++;
        printf("%s: IOC3 PCI_SCR reg reset val exp: %x recv: %x \n", diag_name,
               0x02800000,reg_value);

     }
     reg_value = pci_cfg_dev_ptr->pci_rev;

     if (verbose)
        printf("%s: IOC3 revision = %x \n", diag_name, reg_value);

     reg_value = pci_cfg_dev_ptr->pci_addr;
     if (reg_value != 0)
     {
         error++;
         printf("%s: IOC3 PCI_ADDR reg reset val exp: %x recv: %x \n",
                diag_name, 0x0,reg_value);
     }
   }

   /*
     Do a write and read to the upper 12 bits of base addr 
     register.
   */

   pci_cfg_dev_ptr->pci_addr = 0x5a500000;
   reg_value = pci_cfg_dev_ptr->pci_rev; /* Dummy read to clear the bus */
   reg_value = pci_cfg_dev_ptr->pci_addr;

   if (reg_value != 0x5a500000)
   {
       error++;
       printf("%s: IOC3  PCI_ADDR reg write read error. wrote: %x read: %x \n", diag_name,
               0x5a500000,reg_value);
     
   }
   pci_cfg_dev_ptr->pci_addr = 0;
   return(error);
   
}

int
ql_config_verify(int verbose, __psunsigned_t bridge_base, int pci_dev_num, char *diag_name, int resetCheck)
{
    int               error =0;
    pcicfg32_t        *pci_cfg_dev_ptr;
    __uint32_t        reg_value;

    pci_cfg_dev_ptr = (pcicfg32_t *) (bridge_base +
                                BRIDGE_TYPE0_CFG_DEV(pci_dev_num)) ;

   if(resetCheck)
   {
     /*
        Now read the config registers and verify the reset
        values. dev id has already been verified.
     */
     /*
       Note that when we read scr reg the command and status
       come together. See SCSI manual for reset vals. 
     */
     reg_value = pci_cfg_dev_ptr->pci_scr;

     if (reg_value != 0x02000000)
     {
         error++;
         printf("%s: PCI device %d QL command status reg reset val exp: "
                "%x recv: %x \n", diag_name, pci_dev_num, 0x02000000,reg_value);
     }
     /*
       Note that when we read rev reg the rev and class code
       come together. 
     */

     reg_value = pci_cfg_dev_ptr->pci_rev;
     if (verbose)
         printf("%s: PCI device %d QL class code and revision read as %x \n",
                diag_name, pci_dev_num, reg_value);

     reg_value = pci_cfg_dev_ptr->pci_addr;
     if (reg_value != 0x1)
     {
         error++;
         printf("%s: PCI device %d QL base address reg reset val exp: "
                "%x recv: %x \n", diag_name, pci_dev_num, 0x1,reg_value);
     }
   }

   pci_cfg_dev_ptr->pci_addr = 0x005a5a5a;
   reg_value = pci_cfg_dev_ptr->pci_rev; /* Dummy read */
   reg_value = pci_cfg_dev_ptr->pci_addr;
   if ((reg_value & 0x00ffffff) != 0x005a5a5a)
   {
       error++;
       printf("%s: PCI device %d QL base addr write read error. wrote: %x read: %x \n",
               diag_name, pci_dev_num, 0x005a5a5a,reg_value);
   }
   return(error);
}

int
diag_io6confSpace_sanity(int diag_mode,__psunsigned_t bridge_baseAddr)
{
    int               error = 0,npci,wid_id;
    reg32_t           pci_id ;
    int               ioc3_found=0,num_qlogic = 0;
    int               mode, verbose;
    jmp_buf                 fault_buf;
    void                    *old_buf;
    int               resetCheck;

    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
        printf("\n=====> %s diag took an exception. <=====\n",
               "io6config_space");
        diag_print_exception();
        kl_log_hw_state();
        kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                          "END Hardware Error State (Forced error dump)\n");
        result_fail("io6config_space", KLDIAG_IO6CONFIG_EXCEPTION,
                    "Took an exception");
        restorefault(old_buf);
        return(KLDIAG_IO6CONFIG_EXCEPTION);
    }

    mode = GET_DIAG_MODE(diag_mode);
    verbose = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE)
                    || mode == DIAG_MODE_MFG);
    resetCheck = !(diag_mode & DIAG_FLAG_NORESET);

    if (mode == DIAG_MODE_NONE)
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_PASSED);
    }

    DM_PRINTF(("Running %s diag  (Bridge base = 0x%08x)\n",
	   "io6config_space", bridge_baseAddr));

    wid_id = WIDGETID_GET(bridge_baseAddr);

    for(npci = 0; npci<  MAX_PCI_DEVS; npci++)
    {
        if (get_pcilink_status(bridge_baseAddr, npci, wid_id, &pci_id, 1) == 0) 
        {
            continue;
        }
        
        switch(pci_id)
        {
            case SGI_IOC3:
                ioc3_found ++ ;
                error += ioc3_config_verify(verbose,bridge_baseAddr,npci,
                                            "io6config_space", resetCheck);
                break;
            case QLOGIC_SCSI:
                num_qlogic++;
                error += ql_config_verify(verbose,bridge_baseAddr,npci,
                                          "io6config_space", resetCheck);
                break;
            default:
                break;
        }
    }

    if (!ioc3_found) 
    {
       printf("%s: No IOC3 found on BASEIO.\n", "io6config_space");
       error++;
    }
    if (num_qlogic < 2)
    {
       printf("%s: Found %d Qlogic devices BASEIO; expected %d or more.\n",
               "io6config_space", num_qlogic, 2);
       error++;
    }

    /* Un-register diag exception handler. */
    restorefault(old_buf);
    
    if (error)
    {
        result_fail("io6config_space",KLDIAG_PCICONFIG_FAIL, " ");
        return(KLDIAG_PCICONFIG_FAIL);
         
    }
    else
    {
        if (verbose)
            result_pass("io6config_space",diag_mode);
        return(KLDIAG_PASSED);
    }
}
