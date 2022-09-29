#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include <libsc.h>
#include <libsk.h>
#include <sys/PCI/ioc3.h>
#include <sys/RACER/IP30.h>
#include "uif.h"

#include <sys/PCI/PCI_defs.h>
#include <sys/newscsi.h>  
#include <sys/ql_standalone.h> 
#include "d_scsi.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_pci.h"
#include "d_prototypes.h"

#define MAKE_DIRECT_MAPPED_2GIG(x)      ((x) | 0x80000000 )
#define _PAGE_SIZE                      1024*4
#define DIAG_NUM_ATTMPT                 1000

/* Function to calculate SCSI base address,given the associated Bridge
   base address and the PCI device number. Might seem to be quite similar
   to ioc3 now, exists b'cos of possible changes.*/

__psunsigned_t
diag_scsi_base(__uint32_t npci)
{
    int            wid_id;
    __uint64_t     pci_key;
    __psunsigned_t scsi_base;

    scsi_base = SCSI0_PCI_DEVIO_BASE + (npci * PCI_DEVICE_SIZE);

    return(scsi_base);

}

/*
   This routine dispatches mail box commands. 
*/
   
bool_t
diag_ql_mbox_cmd(pISP_REGS diag_isp, u_char out_cnt, 
                 u_char in_cnt, u_short reg0, u_short reg1, 
                 u_short reg2, u_short reg3, u_short reg4, u_short reg5, 
                 u_short reg6, u_short reg7)
{

    int             count = 0,error = 0;
    u_short         mailbox0,hccr = 0,isr,bus_sema;


    do
    {
        PIO_REG_RD_16(&diag_isp->hccr,~0x0,hccr);
        count++;

    } while ((hccr & HCCR_HOST_INT) && (count < 1000));

    if (count == 1000)
    {
        msg_printf(ERR,"Previous commands not being drained\n");
        return(1);
    }

    switch (out_cnt)
    {
         case 8:
                PIO_REG_WR_16(&diag_isp->mailbox7 ,~0x0,  reg7);
         case 7:
                PIO_REG_WR_16(&diag_isp->mailbox6 ,~0x0,  reg6);
         case 6:
                PIO_REG_WR_16(&diag_isp->mailbox5 ,~0x0,  reg5);
         case 5:
                if (reg0 != 0x11)
                    PIO_REG_WR_16(&diag_isp->mailbox4 ,~0x0,  reg4);
         case 4:
                PIO_REG_WR_16(&diag_isp->mailbox3 ,~0x0,  reg3);
         case 3:
                PIO_REG_WR_16(&diag_isp->mailbox2 ,~0x0,  reg2);
         case 2:
                PIO_REG_WR_16(&diag_isp->mailbox1 ,~0x0,  reg1);
         case 1:
                PIO_REG_WR_16(&diag_isp->mailbox0 ,~0x0,  reg0);
    }
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_CMD_SET_HOST_INT);


    /*
       Look for the status.
    */
    count = 0;
    bus_sema = 0;
    while ((!(bus_sema & BUS_SEMA_LOCK)) && (count < 10000))
    {
      PIO_REG_RD_16(&diag_isp->bus_sema,~0x0,bus_sema);
      count ++;
    }
    if (count == 100000)
    {
       msg_printf(ERR,"semaphore locked!\n");
       return(1);
    }
    isr = 0;
    count = 0;
    while ((!(isr & BUS_ISR_RISC_INT)) && (count < 100000))
    {
	PIO_REG_RD_16(&diag_isp->bus_isr,~0x0,isr);
        DELAY(10);
        count ++;
    }
    if (count == 100000)
    {
       msg_printf(ERR,"Interrupt  failed to arrive \n");
       return(1);
    }
   PIO_REG_WR_16(&diag_isp->hccr,~0x0,HCCR_CMD_CLEAR_RISC_INT);
   PIO_REG_WR_16(&diag_isp->bus_sema,~0x0,0);
   return(0);
}

bool_t
diag_mbox_test(pISP_REGS diag_isp)
{
   u_short     mbox_sts[8],recv_data,seed[5];
   int         i,error = 0;
 
  for (i = 0; i < 5; i++)
     seed[i] = 0x1111 * (i + 1);
  if (diag_ql_mbox_cmd(diag_isp, 6, 6,
                        MBOX_CMD_MAILBOX_REGISTER_TEST,
                        seed[0],seed[1],seed[2],
                        seed[3],seed[4],0,0))
   {
       msg_printf(ERR,"QL: mbox mechanism failure.  \n");
       return(1);
   }
   PIO_REG_RD_16(&diag_isp->mailbox0,~0x0,recv_data);

   if (recv_data != 0x4000)
   {
      error ++;
      msg_printf(ERR,"SCSI mailbox status error. exp: %x recv: %x \n",
              0x4000,recv_data);
   }

   PIO_REG_RD_16(&diag_isp->mailbox1,~0x0,recv_data);
   if (recv_data != seed[0])
   {
      error ++;
      msg_printf(ERR,"SCSI mailbox error. exp: %x recv: %x \n",
                   seed[0],recv_data);
   }

   PIO_REG_RD_16(&diag_isp->mailbox2,~0x0,recv_data);
   if (recv_data != seed[1])
   {
      error ++;
      msg_printf(ERR,"SCSI mailbox error. exp: %x recv: %x \n",
                   seed[1],recv_data);
   }
   

   PIO_REG_RD_16(&diag_isp->mailbox3,~0x0,recv_data);
   if (recv_data != seed[2])
   {
      error ++;
      msg_printf(ERR,"SCSI mailbox error. exp: %x recv: %x \n",
                   seed[2],recv_data);
   }

   PIO_REG_RD_16(&diag_isp->mailbox4,~0x0,recv_data);
   if (recv_data != seed[3])
   {
      error ++;
      msg_printf(ERR,"SCSI mailbox error. exp: %x recv: %x \n",
                  seed[3],recv_data);
   }

   PIO_REG_RD_16(&diag_isp->mailbox5,~0x0,recv_data);
   if (recv_data != seed[4])
   {
      error ++;
      msg_printf(ERR,"SCSI mailbox error. exp: %x recv: %x \n",
                   seed[4],recv_data);
   }
   return(error);
}



bool_t
ql_scsi_mem(__uint32_t npci)
{
    int             error = 0,count,i,j,scsi_ssram_len, mbox_error= 0;
    u_short         mbox_sts[8],recv_data,*pat,risc_code_addr01 = 0x1000;
    pISP_REGS       diag_isp;
    pPCI_REG        diag_isp_config;
    u_short         exp_data[] = {
                                    0x5a5a,0xa5a5,0xcccc,
                                    0x5a5a,0xa5a5,0xcccc
                                 };
    volatile __psunsigned_t scsi_base;
    int                     mode; 
    int 	rd_val;
    msg_printf(SUM,"Testing SCSI mailbox\n");

    scsi_base = diag_scsi_base(npci);  
    diag_isp = (pISP_REGS) scsi_base;
    
    scsi_ssram_len = (0xffff - 0x1000)/2; /*  U divide by 2 bcos we write
                                              half words .... */

    /*
       SCSI config space setup.
    */

    diag_isp_config = (pPCI_REG) (BRIDGE_BASE +
                                  BRIDGE_TYPE0_CFG_DEV(npci)) ;
    msg_printf(DBG,"scsi_base = 0x%x, scsi_cfg_base = 0x%x\n",scsi_base,diag_isp_config);

    PIO_REG_WR_8(&diag_isp_config->Latency_Timer,~0x0,0x40);
    PIO_REG_WR_8(&diag_isp_config->Cache_Line_Size,~0x0,0x40);
    PIO_REG_WR_16(&diag_isp_config->Command,~0x0,MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE);

    /*
        SCSI initialization.
    */

    PIO_REG_WR_16(&diag_isp->bus_icr, ~0x0, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
    DELAY(10);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_CMD_RESET);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_CMD_RELEASE);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_WRITE_BIOS_ENABLE);
    PIO_REG_WR_16(&diag_isp->bus_config1, ~0x0, CONF_1_BURST_ENABLE);
    PIO_REG_WR_16(&diag_isp->bus_sema, ~0x0, 0x0);

    mbox_error = diag_mbox_test(diag_isp);
    if (mbox_error)
    {
        msg_printf(ERR,"SCSI mailbox test failed\n");
	return(-1);
    }
    else
    {
        msg_printf(SUM,"SCSI mailbox test passed\n");
    }

    msg_printf(SUM,"Testing SCSI ssram memory\n");
    msg_printf(SUM,"  Address Uniqueness test \n");
    /* Address uniqueness test */
    /*
      First do the writes.
    */
    for (count = 0; count < scsi_ssram_len;count++)
    {
            if (diag_ql_mbox_cmd(diag_isp, 3, 3, MBOX_CMD_WRITE_RAM_WORD,
                            (u_short) (risc_code_addr01 + count), count,
                                0, 0, 0, 0, 0))
            {
                msg_printf(ERR,"QL: write data failure at %d word \n",count);
                return(-1);
            }
     }

     /*
         Now do the reads.
     */
     for (count = 0; count < scsi_ssram_len;count++)
     {
            if (diag_ql_mbox_cmd(diag_isp, 2, 3, MBOX_CMD_READ_RAM_WORD,
                            (u_short) (risc_code_addr01 + count), 0,
                            0, 0, 0, 0, 0))
            {
                msg_printf(ERR,"QL: read data failure at %d word \n",count);
                return(-1);
            }
	    PIO_REG_RD_16(&diag_isp->mailbox2, ~0x0, recv_data);
            if (recv_data != count)
            {
               error++;
               msg_printf(ERR,"QL: wrote: %x read:%x at addr: %x \n",count,
                               recv_data,count);
               return(-1);
            }
     }
    msg_printf(SUM,"  Mixed Patterns test(0x5a5a,0xa5a5,0xcccc)\n");
    /* Patterns test */
    /*
      First do the writes.
    */
    for (j = 0; j < 3; j++)
    {
        pat = exp_data +j;
        i = 0;
        for (count = 0; count < scsi_ssram_len;count++)
        {
            if (diag_ql_mbox_cmd(diag_isp, 3, 3, MBOX_CMD_WRITE_RAM_WORD,
                            (u_short) (risc_code_addr01 + count), *(pat+i),
                                0, 0, 0, 0, 0))
            {
                msg_printf(ERR,"QL: write data failure at %d word \n",count);
                return(-1);
            }
            i++;
            if (i == 3)
                i = 0;
        }

        /*
          Now do the reads.
        */
        i = 0;
        for (count = 0; count < scsi_ssram_len;count++)
        {
            if (diag_ql_mbox_cmd(diag_isp, 2, 3, MBOX_CMD_READ_RAM_WORD,
                            (u_short) (risc_code_addr01 + count), 0,
                            0, 0, 0, 0, 0))
            {
                msg_printf(ERR,"QL: read data failure at %d word \n",count);
                return(-1);
            }
	    PIO_REG_RD_16(&diag_isp->mailbox2, ~0x0, recv_data);
            if (recv_data != *(pat+i))
            {
               error++;
               msg_printf(ERR,"QL: wrote: %x read:%x at addr: %x \n",*(pat+i),
                               recv_data,count);
               return(-1);
            }
            i++;
            if (i == 3)
                i = 0;
        }
    }
    if ((error) || (mbox_error))
    {
       msg_printf(ERR,"SCSI ram test failed\n");
       return(-1);
    }
    else
    {
       msg_printf(SUM,"SCSI ram test passed\n");
       return(0);
    }
}


bool_t
ql_scsi_dma(__uint32_t npci)
{
    int                     error = 0,count,i,j,scsi_ssram_len;
    u_short                 mbox_sts[8],recv_data,*pat,risc_code_addr01=0x1000;
    pISP_REGS               diag_isp;
    pPCI_REG                diag_isp_config;
    volatile __psunsigned_t scsi_base;
    paddr_t                 src_phy_ptr,dst_phy_ptr,pagesz = _PAGE_SIZE;
    paddr_t                 src_buf_ptr;
    ushort                  *src_buf,*dst_buf;
    /*
       src_pat is setup uch that no munging is required.
    */
    u_short                 src_pat[] = {
                                           0x5555,0x5555,0xaaaa,0xaaaa,
                                           0xcccc,0xcccc
                                        };
    int                     mode, verbose,continue_test;
    
    msg_printf(SUM,"Testing SCSI DMA.\n");

    init_ide_malloc(2 * sizeof(ushort) * (64*1024));	/* big 4 alignment */

    scsi_base = diag_scsi_base(npci);
    diag_isp = (pISP_REGS) scsi_base;
   
    scsi_ssram_len = ((0xffff - 0x1000)/ 2) -1; /*  U divide by 2 bcos we write
                                              half words .... */
    diag_isp_config = (pPCI_REG) (BRIDGE_BASE +
                                  BRIDGE_TYPE0_CFG_DEV(npci)) ;
   
    msg_printf(DBG,"scsi_base = 0x%x, scsi_cfg_base = 0x%x\n",scsi_base,diag_isp_config);

    PIO_REG_WR_8(&diag_isp_config->Latency_Timer,~0x0,0x40);
    PIO_REG_WR_8(&diag_isp_config->Cache_Line_Size,~0x0,0x40);
    PIO_REG_WR_16(&diag_isp_config->Command,~0x0,MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE);


    PIO_REG_WR_16(&diag_isp->bus_icr, ~0x0, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
    DELAY(10);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_CMD_RESET);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_CMD_RELEASE);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_WRITE_BIOS_ENABLE);
    PIO_REG_WR_16(&diag_isp->bus_config1, ~0x0, 0x4);
    PIO_REG_WR_16(&diag_isp->bus_sema, ~0x0, 0x0);

    src_buf = ide_align_malloc(sizeof(ushort) * (64*1024),pagesz);
    i = 0;
    for ( count = 0; count < scsi_ssram_len; count++)
    {
       *(src_buf+count) = src_pat[i];
       i++;
       if (i == 6)
         i = 0;
    }

    src_buf_ptr = (paddr_t) src_buf;
    
    /*
       Make the bridge unserstand that we are operating in
       32 bit space.
    */
    src_phy_ptr = MAKE_DIRECT_MAPPED_2GIG(src_buf_ptr); 

    if (diag_ql_mbox_cmd(diag_isp, 5, 5,
                            MBOX_CMD_LOAD_RAM,
                            (short) risc_code_addr01,
                            (short) ((u_int) src_phy_ptr >> 16),
                            (short) ((u_int) src_phy_ptr & 0xFFFF),
                            scsi_ssram_len,
                            0, 0, 0))
     {
        msg_printf(ERR,"SCSI LOAD SSRAM command failed \n");
        return(1);
     }

     /*
        Now compare.
     */
    i = 0;
    for ( count = 0; count < scsi_ssram_len; count++)
    {
       if (*(src_buf+count) !=  src_pat[i])
       {
           error++;
              msg_printf(ERR,"SCSI DMA fialure exp %x recv %x at %d \n",
                      src_pat[i],*(src_buf+count),count);
               return(1);
           
       }
        
       i++;
       if (i == 6)
         i = 0;
    }

    if (error)
    {
         msg_printf(ERR,"SCSI DMA test FAILED.\n");
         return(1);
    }
 
    else
    {
        msg_printf(SUM,"SCSI DMA test passed.\n");
        return(0);
    }

}


bool_t
ql_scsi_selftest(__uint32_t npci)
{
    int                     error = 0,count;
    u_short                 mbox_sts[8];
    pISP_REGS               diag_isp;
    pPCI_REG                diag_isp_config;
    volatile __psunsigned_t scsi_base;
    int                     mode;
    u_short                 rd_data;

    msg_printf(SUM,"Performing self test on SCSI contoller\n");

    scsi_base = diag_scsi_base(npci);
    diag_isp = (pISP_REGS) scsi_base;


    diag_isp_config = (pPCI_REG) (BRIDGE_BASE +
                                  BRIDGE_TYPE0_CFG_DEV(npci)) ;

    PIO_REG_WR_8(&diag_isp_config->Latency_Timer,~0x0,0x40);
    PIO_REG_WR_8(&diag_isp_config->Cache_Line_Size,~0x0,0x40);
    PIO_REG_WR_16(&diag_isp_config->Command,~0x0,MEMORY_SPACE_ENABLE | BUS_MASTER_ENABLE);


    PIO_REG_WR_16(&diag_isp->bus_icr, ~0x0, ICR_ENABLE_RISC_INT | ICR_SOFT_RESET);
    DELAY(10);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_CMD_RESET);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_CMD_RELEASE);
    PIO_REG_WR_16(&diag_isp->hccr, ~0x0, HCCR_WRITE_BIOS_ENABLE);
    PIO_REG_WR_16(&diag_isp->bus_config1, ~0x0, CONF_1_BURST_ENABLE);
    PIO_REG_WR_16(&diag_isp->bus_sema, ~0x0, 0x0);

    for (count = 0; count < diag_risc_code_length01;count++)
    {
        if (diag_ql_mbox_cmd(diag_isp, 3, 3, MBOX_CMD_WRITE_RAM_WORD,
                            (u_short) (diag_risc_code_addr01 + count), 
                             diag_risc_code01[count], 0, 0, 0, 0, 0))
        {
             msg_printf(ERR,"QL: write data failure at %d word \n",count);
             return(1);
        }
    }
    if (diag_ql_mbox_cmd(diag_isp, 2, 1,
                        MBOX_CMD_EXECUTE_FIRMWARE,
                        diag_risc_code_addr01,
                        0, 0, 0, 0, 0, 0))
    {
         msg_printf(ERR,"EXECUTE FIRMWARE Command Failed. \n");
         return(1);
    }

    count =  0;
    PIO_REG_RD_16(&diag_isp->mailbox0, ~0x0, rd_data);
    while ((rd_data != 0x4000) && (count < DIAG_NUM_ATTMPT))
    {
        DELAY(50);
        PIO_REG_RD_16(&diag_isp->mailbox0, ~0x0, rd_data);
        count++;
    }
    
    if (count == DIAG_NUM_ATTMPT)
    {
       msg_printf(ERR,"SCSI self test failed. mbox val %x \n",diag_isp->mailbox0);
       return(1);
         
    }
       msg_printf(SUM,"SCSI self test passed\n");
       return(0);
}

bool_t
ql_test(int argc, char **argv)
{
	__uint32_t cntr_no = 0;

	d_errors = 0;

	if (argc > 3) {
		msg_printf(SUM, "usage: %s scsi_controller_no[0|1]\n", argv[0]);
		return(-1);
	}

	if (argc == 2) {
		cntr_no = atoi(*(++argv));
		switch(cntr_no) {
		case 0:
		case 1:
			msg_printf(SUM,"\nTesting Qlogic SCSI controller #%d.\n",cntr_no);
			d_errors = ql_scsi_selftest(cntr_no) + ql_scsi_mem(cntr_no) + ql_scsi_dma(cntr_no);	

			break;

		default:
			msg_printf(SUM,"Illegal SCSI controller number %d\n",
				   cntr_no);
			return(-1);
		}
	}
	else if (argc == 1) {
		msg_printf(SUM,"Testing Qlogic SCSI controller #0.\n");
	     	d_errors = ql_scsi_dma(0) + ql_scsi_selftest(0) + ql_scsi_mem(0);
		msg_printf(SUM,"\nTesting Qlogic SCSI controller #1.\n");
	     	d_errors += ql_scsi_dma(1) + ql_scsi_selftest(1) + ql_scsi_mem(1);

	}
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "QLOGIC SCSI CONTROLLER", D_FRU_IP30);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_SCSI_0000], d_errors );
}
