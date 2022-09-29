#include <stdio.h>
#include <setjmp.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/addrs.h>
#include <sys/SN/agent.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/newscsi.h>
#include <sys/ql_standalone.h>
#include <sys/SN/kldiag.h>
#include <libsk.h>
#include <libkl.h>
#include <sys/SN/error.h>
#include <report.h>
#include <prom_msgs.h>
#include <rtc.h>
#include <diag_lib.h>
#include "diag_scsi.h"

#define MAKE_DIRECT_MAPPED_2GIG(x)      ((x) | 0x80000000 )
#define _PAGE_SIZE                      1024*4
#define DIAG_NUM_ATTMPT                 1000


/*
 * diag_scsi_base()
 *
 * This function will convert a given bridge base address and PCI
 * device number to a SCSI base address.
 */

static __psunsigned_t
diag_scsi_base(__psunsigned_t bridge_base,int npci)
{
    int            wid_id;
    __uint64_t     pci_key;
    __psunsigned_t scsi_base;

    wid_id    = WIDGETID_GET(bridge_base);
    pci_key   = MK_SN0_KEY(NASID_GET(bridge_base), wid_id, npci);
    scsi_base = GET_PCIBASE_FROM_KEY(pci_key);

    return(scsi_base);

}


/*
   This routine dispatches mail box commands. 
*/
   
int
diag_ql_mbox_cmd(pISP_REGS diag_isp, u_char out_cnt, 
                 u_char in_cnt, u_short reg0, u_short reg1, 
                 u_short reg2, u_short reg3, u_short reg4, u_short reg5, 
                 u_short reg6, u_short reg7, char *diag_name)
{

    int             count = 0,error = 0;
    u_short         mailbox0,hccr = 0,isr,bus_sema;


    do
    {
        hccr = diag_isp->hccr;
        rtc_sleep(10);
        count++;
        

    } while ((hccr & HCCR_HOST_INT) && (count < 1000));

    if (count == 1000)
    {
        printf("%s: Previous mbox commands not being drained ...  \n", diag_name);
        return(1);
    }

    switch (out_cnt)
    {
         case 8:
                diag_isp->mailbox7 =  reg7;
         case 7:
                diag_isp->mailbox6 =  reg6;
         case 6:
                diag_isp->mailbox5 =  reg5;
         case 5:
                if (reg0 != 0x11)
                    diag_isp->mailbox4 =  reg4;
         case 4:
                diag_isp->mailbox3 =  reg3;
         case 3:
                diag_isp->mailbox2 =  reg2;
         case 2:
                diag_isp->mailbox1 =  reg1;
         case 1:
                diag_isp->mailbox0 =  reg0;
    }
    diag_isp -> hccr = HCCR_CMD_SET_HOST_INT;


    /*
       Look for the status.
    */
    count = 0;
    bus_sema = 0;
    while ((!(bus_sema & BUS_SEMA_LOCK)) && (count < 100000))
    {
      bus_sema = diag_isp->bus_sema;
      rtc_sleep(10);
      count ++;
    }
    if (count == 100000)
    {
       printf("%s: semaphore locked .... \n", diag_name);
       return(1);
    }
    isr = 0;
    count = 0;
    while ((!(isr & BUS_ISR_RISC_INT)) && (count < 100000))
    {
        isr = diag_isp->bus_isr;
        rtc_sleep(10);
        count ++;
    }
    if (count == 100000)
    {
       printf("%s: Interrupt  failed to arrive \n", diag_name);
       return(1);
    }
   diag_isp->hccr = HCCR_CMD_CLEAR_RISC_INT;
   diag_isp->bus_sema = 0;
   return(0);
}


int
diag_mbox_test(int verbose,pISP_REGS diag_isp, char *diag_name)
{
   u_short     mbox_sts[8],recv_data,seed[5];
   int         i,error = 0;
 
  for (i = 0; i < 5; i++)
     seed[i] = 0x1111 * (i + 1);
  if (diag_ql_mbox_cmd(diag_isp, 6, 6,
                        MBOX_CMD_MAILBOX_REGISTER_TEST,
                        seed[0],seed[1],seed[2],
                        seed[3],seed[4],0,0, diag_name))
   {
       printf("%s, SCSI mailbox mechanism failure.  \n", diag_name);
       return(1);
   }
   recv_data = diag_isp -> mailbox0;
   if (recv_data != 0x4000)
   {
      error ++;
      printf("%s: SCSI mailbox status error. exp: %x recv: %x \n", diag_name,
              0x4000,recv_data);
   }

   recv_data = diag_isp -> mailbox1;
   if (recv_data != seed[0])
   {
      error ++;
      if (verbose)
           printf("%s: SCSI mailbox error. exp: %x recv: %x \n", diag_name,
                   seed[0],recv_data);
   }

   recv_data = diag_isp -> mailbox2;
   if (recv_data != seed[1])
   {
      error ++;
      if (verbose)
           printf("%s: SCSI mailbox error. exp: %x recv: %x \n", diag_name,
                   seed[1],recv_data);
   }
   

   recv_data = diag_isp -> mailbox3;
   if (recv_data != seed[2])
   {
      error ++;
      if (verbose)
           printf("%s: SCSI mailbox error. exp: %x recv: %x \n", diag_name,
                   seed[2],recv_data);
   }

   recv_data = diag_isp -> mailbox4;
   if (recv_data != seed[3])
   {
      error ++;
      if (verbose)
          printf("%s: SCSI mailbox error. exp: %x recv: %x \n", diag_name,
                  seed[3],recv_data);
   }

   recv_data = diag_isp -> mailbox5;
   if (recv_data != seed[4])
   {
      error ++;
      if (verbose)
           printf("%s SCSI mailbox error. exp: %x recv: %x \n", diag_name,
                   seed[4],recv_data);
   }
   return(error);
}



int
diag_scsi_mem(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
    int             error = 0,count,i,j,scsi_ssram_len,mbox_error= 0;
    u_short         mbox_sts[8],recv_data,*pat,risc_code_addr01 = 0x1000;
    pISP_REGS       diag_isp;
    pPCI_REG        diag_isp_config;
    u_short         exp_data[] = {
                                    0x5a5a,0xa5a5,0xcccc,
                                    0x5a5a,0xa5a5,0xcccc
                                 };
    volatile __psunsigned_t scsi_base;
    int                     mode, verbose,continue_test,return_code=0;
    int                     num_loop,walk;
    jmp_buf                 fault_buf;
    void                    *old_buf;

    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
        printf("\n=====> %s diag took an exception. <=====\n", "scsi_ram");
        diag_print_exception();
        kl_log_hw_state();
        kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                          "END Hardware Error State (Forced error dump)\n");
        result_fail("scsi_ram",KLDIAG_SCSIRAM_EXCEPTION, "Took an exception");
        restorefault(old_buf);
        return(KLDIAG_SCSIRAM_EXCEPTION);
    }


    mode = GET_DIAG_MODE(diag_mode);
    verbose = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE)
                    || mode == DIAG_MODE_MFG);

    continue_test = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_CONTINUE)
                    || mode == DIAG_MODE_MFG);


    switch(mode)
    {
      case DIAG_MODE_NONE:
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_PASSED);

      case DIAG_MODE_NORMAL:
      case DIAG_MODE_HEAVY:
      case DIAG_MODE_MFG:
      case DIAG_MODE_EXT_LOOP:
        DM_PRINTF(("Running %s diag (Bridge base = 0x%08x  PCI dev = %d)\n",
               "scsi_ram", bridge_baseAddr, npci));
        break;

      default:
        printf("%s: *** WARNING ***    diag_mode parameter 0x%x unknown,"
               " returning.\n", "enet_all", diag_mode);
        return(KLDIAG_PASSED);    /* Should probably be a failure code */
    }


#if 0
    if (rc = diag_check_ql_cfg(diag_mode, bridge_baseAddr, npci, "scsi_ram"))
	return(rc);
#endif

    if (verbose) printf("%s:  Running SCSI mailbox test.\n", "scsi_ram");
    
    scsi_base = diag_scsi_base(bridge_baseAddr,npci);
    diag_isp = (pISP_REGS) scsi_base;
    
    scsi_ssram_len = (0xffff - 0x1000)/2; /*  U divide by 2 bcos we write
                                              half words .... */
    diag_isp_config = (pPCI_REG) (bridge_baseAddr + 
                                  BRIDGE_TYPE0_CFG_DEV(npci)) ;
    /*
       SCSI config space setup.
    */
    diag_isp_config->Latency_Timer = 0x40;
    diag_isp_config->Cache_Line_Size = 0x40;
    diag_isp_config->Command = MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE;
    

    /*
        SCSI initialization.
    */
    diag_isp->bus_icr =  ICR_ENABLE_RISC_INT | ICR_SOFT_RESET;
    rtc_sleep(10);
    diag_isp->hccr =  HCCR_CMD_RESET;
    diag_isp->hccr =  HCCR_CMD_RELEASE;
    diag_isp->bus_config1 =  0x4;
    diag_isp->bus_sema =  0x0;

    mbox_error = diag_mbox_test(verbose,diag_isp, "scsi_ram");
    if (mbox_error)
    {
        return_code = KLDIAG_SCSI_MBOX_FAIL;
        printf("%s: SCSI mailbox test failed .... \n", "scsi_ram");
    }
    else
    {
        if (verbose)
            printf("%s: SCSI mailbox test passed .... \n", "scsi_ram");
    }

    if (verbose) printf("%s: Testing SCSI RAM \n", "scsi_ram");

    if (mode == DIAG_MODE_NORMAL)
    {
        walk = 1;
        num_loop = 1;
    }
    else
    {
        walk = 0;
        num_loop = 3;

    }

    /*
      First do the writes.
    */
    for (j = 0; j < num_loop; j++)
    {
        pat = exp_data +j;
        i = 0;
        count = 0;
        while (count < scsi_ssram_len)
        {
            if (walk)
            {
                if (!count)
                   count = 1;
                else
                   count *= 2;
            }
            else
                count ++;

            if (diag_ql_mbox_cmd(diag_isp, 3, 3, MBOX_CMD_WRITE_RAM_WORD,
                            (u_short) (risc_code_addr01 + count), *(pat+i),
                                0, 0, 0, 0, 0, "scsi_ram"))
            {
                /* Un-register diag exception handler. */
                restorefault(old_buf);
                printf("%s: QL write data failure at %d word \n", "scsi_ram", count);
                return_code |= KLDIAG_SCSI_SSRAM_FAIL1;
                return(return_code);
            }
            i++;
            if (i == 3)
                i = 0;
        }

        /*
          Now do the reads.
        */
        i = 0;
        count = 0;
        while ( count < scsi_ssram_len)
        {
            if (walk)
            {
                if (!count)
                   count = 1;
                else
                   count *= 2;
            }
            else
                count ++;
            if (diag_ql_mbox_cmd(diag_isp, 2, 3, MBOX_CMD_READ_RAM_WORD,
                            (u_short) (risc_code_addr01 + count), 0,
                            0, 0, 0, 0, 0, "scsi_ram"))
            {
                printf("%s: QL read data failure at %d word \n", "scsi_ram", count);
                return_code |= KLDIAG_SCSI_SSRAM_FAIL2;
                result_fail("scsi_ram",return_code, " ");
                /* Un-register diag exception handler. */
                restorefault(old_buf);
                return(return_code);
            }
            recv_data = diag_isp->mailbox2;
            if (recv_data != *(pat+i))
            {
               error++;
               if (verbose)
                     printf("%s: QL wrote: %x read:%x at addr: %x \n", "scsi_ram", *(pat+i),
                               recv_data,count);
               if (!continue_test)
               {
                   return_code |= KLDIAG_SCSI_SSRAM_FAIL;
                   result_fail("scsi_ram",return_code, " ");
                   /* Un-register diag exception handler. */
                   restorefault(old_buf);
                   return(return_code);
               }
            }
            i++;
            if (i == 3)
                i = 0;
        }
    }
    /* Un-register diag exception handler. */
    restorefault(old_buf);
    if ((error) || (mbox_error))
    {
       if (error)
           return_code |= KLDIAG_SCSI_SSRAM_FAIL;
       result_fail("scsi_ram",return_code, " ");
       return(return_code);
    }
    else
    {
       if (verbose)
            result_pass("scsi_ram",diag_mode);
       return(KLDIAG_PASSED);
    }
}


int
diag_scsi_dma(int diag_mode,__psunsigned_t bridge_base,int npci)
{
    int                     error = 0,count,i,j,scsi_ssram_len;
    u_short                 mbox_sts[8],recv_data,*pat,risc_code_addr01=0x1000;
    pISP_REGS               diag_isp;
    pPCI_REG                diag_isp_config;
    volatile __psunsigned_t scsi_base;
    paddr_t                 src_phy_ptr,pagesz = _PAGE_SIZE;
    paddr_t                 src_buf_ptr;
    ushort                  *src_buf;
    bridge_t                *bridge_ptr = (bridge_t *)bridge_base;
    bridgereg_t             dir_map_save;

    /*
       src_pat is setup such that no munging is required.
    */
    u_short                 src_pat[] = {
                                           0x5555,0x5555,0xaaaa,0xaaaa,
                                           0xcccc,0xcccc
                                        };
    int                     mode, verbose,continue_test;
    jmp_buf                 fault_buf;
    void                    *old_buf;

    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
        printf("\n=====> %s diag took an exception. <=====\n", "scsi_dma");
        diag_print_exception();
        kl_log_hw_state();
        kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                          "END Hardware Error State (Forced error dump)\n");
        result_fail("scsi_dma",KLDIAG_SCSIDMA_EXCEPTION, "Took an exception");
        restorefault(old_buf);
        return(KLDIAG_SCSIDMA_EXCEPTION);
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

    DM_PRINTF(("Running %s diag (Bridge base = 0x%08x  PCI dev = %d)\n",
	   "scsi_dma", bridge_base, npci));

#if 0
    if (rc = diag_check_ql_cfg(diag_mode, bridge_base, npci, "scsi_dma"))
	return(rc);
#endif

    scsi_base = diag_scsi_base(bridge_base,npci);

    diag_isp = (pISP_REGS) scsi_base;
   
    /*
     * The ISP1040B is designed for a 64K-word external SRAM.
     * However, conflicting information on whether all boards
     * using an ISP1040B actually have a 64K-word or a 32K-word.
     * SRAM installed lead us to use 32K as the size to ensure
     * compatibility with all boards.  In any case, the first 4K
     * words of the ISP1040B RISC processor's address space are
     * mapped to internal ROM.  So the usable length of the SRAM
     * is 28K.
     */
    scsi_ssram_len = 0x7000;

    diag_isp_config = (pPCI_REG) (bridge_base +
                                  BRIDGE_TYPE0_CFG_DEV(npci)) ;
    diag_isp_config->Latency_Timer = 0x40;
    diag_isp_config->Cache_Line_Size = 0x40;
    diag_isp_config->Command = MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE;
   

    diag_isp->bus_icr =  ICR_ENABLE_RISC_INT | ICR_SOFT_RESET;
    rtc_sleep(10);
    diag_isp->hccr =  HCCR_CMD_RESET;
    diag_isp->hccr =  HCCR_CMD_RELEASE;
    /*
     * Due to a bug in the QLogic ISP1040B affecting the DUMP RAM command,
     * we set burst mode to 128 bytes as part of a workaround.
     */
    diag_isp->bus_config1 =  0x44;
    diag_isp->bus_sema =  0x00;

    src_buf = (ushort *) diag_malloc(sizeof(ushort) * (64*1024));

    src_buf = (ushort * ) ((paddr_t)(src_buf+pagesz)  &
                        ~(paddr_t)(pagesz - (paddr_t)1));

    i = 0;
    for ( count = 0; count < scsi_ssram_len; count++)
    {
        src_buf[count] = src_pat[i];
        i = (i + 1) % 6;
    }

    src_buf_ptr = (paddr_t) src_buf;
    
    /*
       Make the bridge unserstand that we are operating in
       32 bit space.
    */
    src_phy_ptr = MAKE_DIRECT_MAPPED_2GIG(src_buf_ptr); 

    /* Put bits 47:31 of DMA address in Bridge Direct Mapping Reg. */
    /* This way, the NASID will be added to the 32-bit PCI address. */
    dir_map_save = bridge_ptr->b_dir_map;
    bridge_ptr->b_dir_map =
            ((LOCAL_HUB_L(IIO_WCR) & 0xF) << BRIDGE_DIRMAP_W_ID_SHFT) |
            ((src_buf_ptr >> BRIDGE_DIRMAP_OFF_ADDRSHFT) & 0x1FFFF);

    if (diag_ql_mbox_cmd(diag_isp, 5, 5,
                         MBOX_CMD_LOAD_RAM,
                         (short) risc_code_addr01,
                         (short) ((u_long) src_phy_ptr >> 16) & 0xFFFF,
                         (short) ((u_long) src_phy_ptr & 0xFFFF),
                         scsi_ssram_len, 0, 0, 0, "scsi_dma"))
    {
       printf("%s: LOAD SSRAM command failed \n", "scsi_dma");
       bridge_ptr->b_dir_map = dir_map_save;
       diag_free(src_buf);
       result_fail("scsi_dma",KLDIAG_SCSI_DMA_FAIL1, " ");
       /* Un-register diag exception handler. */
       restorefault(old_buf);
       return(KLDIAG_SCSI_DMA_FAIL1);
    }

    for ( count = 0; count < scsi_ssram_len; count++)
        src_buf[count] = 0;

    /*
     * Because of a bug in the QLogic ISP1040B affecting the DUMP RAM command,
     * we only dump 128 bytes of data in this test rather than the full
     * size of the SRAM.
     */
    scsi_ssram_len = 64;  /* 64 16-bit words = 128 bytes */

    /*
     * In order to verify that something close to the full LOAD
     * completed, dump a 64-word (128-byte) chunk of data near the end
     * of the SRAM. We don't pick the very last 64-word block, because
     * we want to use a block where our 6-word repeating test pattern
     * restarts.  In other words, we want to start the DUMP at a
     * boundary that is both 64-word aligned and aligned on a
     * a 6-word boundary where our pattern restarts.
     */
    risc_code_addr01 = 0x7F00;

    if (diag_ql_mbox_cmd(diag_isp, 5, 5,
                         MBOX_CMD_DUMP_RAM,
                         (short) risc_code_addr01,
                         (short) ((u_long) src_phy_ptr >> 16) & 0xFFFF,
                         (short) ((u_long) src_phy_ptr & 0xFFFF),
                         scsi_ssram_len, 0, 0, 0, "scsi_dma"))
    {
       printf("%s: DUMP SSRAM command failed \n", "scsi_dma");
       bridge_ptr->b_dir_map = dir_map_save;
       diag_free(src_buf);
       result_fail("scsi_dma",KLDIAG_SCSI_DMA_FAIL2, " ");
       /* Un-register diag exception handler. */
       restorefault(old_buf);
       return(KLDIAG_SCSI_DMA_FAIL2);
    }

    /*
       Now compare.
    */
    i = 0;
    for ( count = 0; count < scsi_ssram_len; count++)
    {
        if (src_buf[count] !=  src_pat[i])
       {
           error++;
           if (verbose)
           {
              printf("%s DMA failure exp %x recv %x at %d \n", "scsi_dma",
                      src_pat[i],*(src_buf+count),count);
           }
           if (!continue_test)
           {
               bridge_ptr->b_dir_map = dir_map_save;
               diag_free(src_buf);
               printf("%s: DMA test failed .... \n", "scsi_dma");
               /* Un-register diag exception handler. */
               restorefault(old_buf);
               result_fail("scsi_dma",KLDIAG_SCSI_DMA_FAIL, " ");
               return(KLDIAG_SCSI_DMA_FAIL);
           }
           
       }
        
        i = (i + 1) % 6;
    }

    bridge_ptr->b_dir_map = dir_map_save;
    diag_free(src_buf);
    /* Un-register diag exception handler. */
    restorefault(old_buf);

    if (error)
    {
        result_fail("scsi_dma",KLDIAG_SCSI_DMA_FAIL, " ");
        return(KLDIAG_SCSI_DMA_FAIL);
    }
 
    else
    {
        if (verbose)
            result_pass("scsi_dma",diag_mode);
        return(KLDIAG_PASSED);
    }

}


int
diag_scsi_selftest(int diag_mode,__psunsigned_t bridge_base,int npci)
{
    int                     error = 0,count;
    u_short                 mbox_sts[8];
    pISP_REGS               diag_isp;
    pPCI_REG                diag_isp_config;
    volatile __psunsigned_t scsi_base;
    int                     mode, verbose;
    jmp_buf                 fault_buf;
    void                    *old_buf;

    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
        printf("\n=====> %s diag took an exception. <=====\n", "scsi_controller");
        diag_print_exception();
        kl_log_hw_state();
        kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                          "END Hardware Error State (Forced error dump)\n");
        result_fail("scsi_controller",KLDIAG_SCSICNTRL_EXCEPTION, "Took an exception");
        restorefault(old_buf);
        return(KLDIAG_SCSICNTRL_EXCEPTION);
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
    DM_PRINTF(("Running %s diag (Bridge base = 0x%08x  PCI dev = %d)\n",
	   "scsi_controller", bridge_base, npci));

#if 0
    if (rc = diag_check_ql_cfg(diag_mode, bridge_base, npci, "scsi_controller"))
	return(rc);
#endif

    scsi_base = diag_scsi_base(bridge_base,npci);
    diag_isp = (pISP_REGS) scsi_base;


    diag_isp_config = (pPCI_REG) (bridge_base +
                                  BRIDGE_TYPE0_CFG_DEV(npci)) ;
    diag_isp_config->Latency_Timer = 0x40;
    diag_isp_config->Cache_Line_Size = 0x40;
    diag_isp_config->Command = MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE;


    diag_isp->bus_icr =  ICR_ENABLE_RISC_INT | ICR_SOFT_RESET;
    rtc_sleep(10);
    diag_isp->hccr =  HCCR_CMD_RESET;
    diag_isp->hccr =  HCCR_CMD_RELEASE;
    diag_isp->hccr =  HCCR_WRITE_BIOS_ENABLE;
    diag_isp->bus_config1 =  CONF_1_BURST_ENABLE;
    diag_isp->bus_sema =  0x0;

    for (count = 0; count < diag_risc_code_length01;count++)
    {
        if (diag_ql_mbox_cmd(diag_isp, 3, 3, MBOX_CMD_WRITE_RAM_WORD,
                            (u_short) (diag_risc_code_addr01 + count), 
                             diag_risc_code01[count], 0, 0, 0, 0, 0, "scsi_controller"))
        {
             printf("%s:  write data failure at %d word \n", "scsi_controller", count);
             result_fail("scsi_controller",KLDIAG_SCSI_STEST_FAIL1, " ");
             /* Un-register diag exception handler. */
             restorefault(old_buf);
             return(KLDIAG_SCSI_STEST_FAIL1);
        }
    }
    if (diag_ql_mbox_cmd(diag_isp, 2, 1,
                        MBOX_CMD_EXECUTE_FIRMWARE,
                        diag_risc_code_addr01,
                        0, 0, 0, 0, 0, 0, "scsi_controller"))
    {
         printf("%s: EXECUTE FIRMWARE Command Failed. \n", "scsi_controller");
         result_fail("scsi_controller",KLDIAG_SCSI_STEST_FAIL1, " ");
         /* Un-register diag exception handler. */
         restorefault(old_buf);
         return(KLDIAG_SCSI_STEST_FAIL2);
    }

    count =  0;
    while ((diag_isp->mailbox0 != 0x4000) && (count < DIAG_NUM_ATTMPT))
    {
        rtc_sleep(50);
        count++;
    }
    
    /* Un-register diag exception handler. */
    restorefault(old_buf);

    if (count == DIAG_NUM_ATTMPT)
    {
       printf("%s: Bad status val ==> %x .... \n", "scsi_controller", diag_isp->mailbox0);
       printf("%s: Failing Test # ==> %x .... \n", "scsi_controller", diag_isp->mailbox1);
       printf("%s: Failing Data   ==> %x .... \n", "scsi_controller", diag_isp->mailbox2);
       result_fail("scsi_controller",KLDIAG_SCSI_STEST_FAIL, " ");
       return(KLDIAG_SCSI_STEST_FAIL);
         
    }
    if (verbose)
       result_pass("scsi_controller",diag_mode);
    return(KLDIAG_PASSED);
               
}
