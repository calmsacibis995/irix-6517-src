#include <stdio.h>
#include <setjmp.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/SN/addrs.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/SN/agent.h>
#include <sys/SN/intr.h>
#include <sys/ns16550.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/error.h>
#include <libkl.h>
#include <report.h>
#include <prom_msgs.h>
#include <rtc.h>
#include <diag_lib.h>  /* XXX - Change to SN0/kldiag.h when protos move */


#define FIFO_DEPTH       14
#define DIAG_NUM_ATTMPT  100
#define DIAG_UARTA       0
#define DIAG_UARTB       1
#define _PAGE_SIZE       1024*4
#define DMA_NUM_ENTRIES  128
#define PROM_SER_CLK_SPEED              SER_CLK_SPEED(SER_PREDIVISOR)
#define PROM_SER_DIVISOR(y)             SER_DIVISOR(y, PROM_SER_CLK_SPEED)


static uchar_t                   ring_buffer[2*_PAGE_SIZE];

void
diag_uart_initialize(ioc3_mem_t *ioc3_bp)
{
    int                            baud,clkdiv;

    baud = 9600;
    ioc3_bp->sio_cr = 0x1;
    ioc3_bp->sio_cr = 0x0;
    ioc3_bp->int_out  &= ~INT_OUT_DIAG;
    ioc3_bp->sio_cr = ((UARTA_BASE >> 3) << SIO_CR_SER_A_BASE_SHIFT) |
                 ((UARTB_BASE >> 3) << SIO_CR_SER_B_BASE_SHIFT) |
                 (0xf << SIO_CR_CMD_PULSE_SHIFT);
    ioc3_bp->gpdr = 0;
    ioc3_bp->gpcr_s = GPCR_INT_OUT_EN | GPCR_MLAN_EN | GPCR_DIR_SERA_XCVR |
                      GPCR_DIR_SERB_XCVR;
     /*
        Init UART A .
     */

    ioc3_bp->sregs.uarta.iu_msr  = 0x0;
    ioc3_bp->sregs.uarta.iu_lcr = LCR_DLAB;
    clkdiv = PROM_SER_DIVISOR(baud);

    ioc3_bp->sregs.uarta.iu_dlm = (clkdiv >> 8) & 0xff;
    ioc3_bp->sregs.uarta.iu_dll = clkdiv  & 0xff;
    ioc3_bp->sregs.uarta.iu_scr  = 0x6;
    ioc3_bp->sregs.uarta.iu_lcr =  LCR_8_BITS_CHAR | LCR_1_STOP_BITS;
    ioc3_bp->sregs.uarta.iu_ier =  0; 
    ioc3_bp->sregs.uarta.iu_mcr  = 0x3;
    ioc3_bp->sregs.uarta.iu_fcr =  FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET
                                   | FCR_XMIT_FIFO_RESET;
    ioc3_bp->sregs.uarta.iu_fcr =   FCR_ENABLE_FIFO;

    /*
       Setting for uart b.
    */
    ioc3_bp->sregs.uartb.iu_msr  = 0x0;
    ioc3_bp->sregs.uartb.iu_lcr = LCR_DLAB;
    ioc3_bp->sregs.uartb.iu_dlm = (clkdiv >> 8) & 0xff;
    ioc3_bp->sregs.uartb.iu_dll = clkdiv  & 0xff;
    ioc3_bp->sregs.uartb.iu_scr  = 0x6;
    ioc3_bp->sregs.uartb.iu_lcr =  LCR_8_BITS_CHAR | LCR_1_STOP_BITS;
    ioc3_bp->sregs.uartb.iu_ier =  0; 
    ioc3_bp->sregs.uartb.iu_mcr  = 0x3;
    ioc3_bp->sregs.uartb.iu_fcr =  FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET
                                   | FCR_XMIT_FIFO_RESET;
    ioc3_bp->sregs.uartb.iu_fcr =   FCR_ENABLE_FIFO;
    

}

/*
  Separate functions are provided to initialize UARTs A & B for
  DMA testing to allow only B to be intialized during reset
  testing so as to avoid stepping on the PROM's configuration
  of UART A for use by the console.
*/

void 
diag_uartA_init(ioc3_mem_t *ioc3_bp)
{
    int     baud,clkdiv;

    baud = 9600;
    ioc3_bp->sregs.uarta.iu_msr  = 0x0;
    ioc3_bp->sregs.uarta.iu_lcr = LCR_DLAB;
    clkdiv = PROM_SER_DIVISOR(baud);
    ioc3_bp->sregs.uarta.iu_dlm = (clkdiv >> 8) & 0xff;
    ioc3_bp->sregs.uarta.iu_dll = clkdiv  & 0xff;
    ioc3_bp->sregs.uarta.iu_scr  = 0x6;
    ioc3_bp->sregs.uarta.iu_lcr =  LCR_8_BITS_CHAR | LCR_1_STOP_BITS;
    ioc3_bp->sregs.uarta.iu_ier =  0;
    ioc3_bp->sregs.uarta.iu_mcr  = 0x3;
    ioc3_bp->sregs.uarta.iu_fcr =  FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET
                                   | FCR_XMIT_FIFO_RESET;
    ioc3_bp->sregs.uarta.iu_fcr =   FCR_ENABLE_FIFO;

}


void 
diag_uartB_init(ioc3_mem_t *ioc3_bp)
{
    int     baud,clkdiv;

    baud = 9600;
    ioc3_bp->sregs.uartb.iu_msr  = 0x0;
    ioc3_bp->sregs.uartb.iu_lcr = LCR_DLAB;
    clkdiv = PROM_SER_DIVISOR(baud);
    ioc3_bp->sregs.uartb.iu_dlm = (clkdiv >> 8) & 0xff;
    ioc3_bp->sregs.uartb.iu_dll = clkdiv  & 0xff;
    ioc3_bp->sregs.uartb.iu_scr  = 0x6;
    ioc3_bp->sregs.uartb.iu_lcr =  LCR_8_BITS_CHAR | LCR_1_STOP_BITS;
    ioc3_bp->sregs.uartb.iu_ier =  0;
    ioc3_bp->sregs.uartb.iu_mcr  = 0x3;
    ioc3_bp->sregs.uartb.iu_fcr =  FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET
                                   | FCR_XMIT_FIFO_RESET;
    ioc3_bp->sregs.uartb.iu_fcr =   FCR_ENABLE_FIFO;

}


char
diag_uart_read(ioc3_mem_t *ioc3_bp,int ch_num, char *diag_name)
{
   int i;
   char  reg_val;
   
   for (i = 0; i < DIAG_NUM_ATTMPT; i++)
   {
      if (ch_num == DIAG_UARTA)
            reg_val = ioc3_bp->sregs.uarta.iu_lsr;
      else
            reg_val = ioc3_bp->sregs.uartb.iu_lsr;
      rtc_sleep(10);
      if (reg_val & 0x1) break;
   }
   
   if (i == DIAG_NUM_ATTMPT) 
   {
       printf("%s: UART%s read timed out on LSR<Data Ready>\n", 
	      diag_name, ch_num == DIAG_UARTA ? "A" : "B");
   }
   
   if (ch_num == DIAG_UARTA)
       reg_val = ioc3_bp->sregs.uarta.iu_rbr; 
   else
       reg_val = ioc3_bp->sregs.uartb.iu_rbr;
   
   return(reg_val);
}

void 
diag_uart_write(ioc3_mem_t *ioc3_bp,char reg_val,int ch_num) 
{

  if (ch_num == DIAG_UARTA)
       ioc3_bp->sregs.uarta.iu_thr = reg_val;
  else
       ioc3_bp->sregs.uartb.iu_thr = reg_val;
}




int
diag_serial_pio(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
    int                     error = 0,i,rc;
    ioc3_mem_t              *ioc3_bp;
    volatile __psunsigned_t ioc3_base;
    char                    reg_value;
    int                     mode, verbose,continue_test,return_code = 0;
    jmp_buf                 fault_buf;
    void                    *old_buf;
    int                     extLoopback = 0;
    int                     resetCheck;
    int                     count;

    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
         printf("\n=====> %s diag took an exception. <=====\n", "serial_pio");
         diag_print_exception();
         kl_log_hw_state();
         kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                           "END Hardware Error State (Forced error dump)\n");
         result_fail("serial_pio",KLDIAG_SERPIO_EXCEPTION, "Took an exception");
         restorefault(old_buf);
         return(KLDIAG_SERPIO_EXCEPTION);
    }


    mode = GET_DIAG_MODE(diag_mode);
    verbose = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE)
                    || mode == DIAG_MODE_MFG);

    continue_test = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_CONTINUE)
                    || mode == DIAG_MODE_MFG);

    resetCheck = !(diag_mode & DIAG_FLAG_NORESET);

    if (mode == DIAG_MODE_NONE)
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_PASSED);
    }

    /* check for external loopback mode */
    if(mode == DIAG_MODE_EXT_LOOP)
    {
      /* treat external loopback mode like manufacturing mode */
      /* but don't enable UART internal loopback              */
      diag_mode = DIAG_MODE_MFG;
      extLoopback = 1;
    }

    DM_PRINTF(("Running %s diag (Bridge base = 0x%08x  PCI dev = %d)\n",
	   "serial_pio", bridge_baseAddr, npci));

    if (rc = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr, npci,
                                 "serial_pio"))
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(rc);
    }
    
    ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);

    if (!ioc3_base)
    {
        error ++;
        printf("%s: could not get the ioc3 base addr \n", "serial_pio");
        result_fail("serial_pio",KLDIAG_IOC3_BASE_FAIL, " ");
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_IOC3_BASE_FAIL);
    }
    ioc3_bp = (ioc3_mem_t *) ioc3_base;
    diag_uart_initialize(ioc3_bp);
    reg_value = ioc3_bp->sregs.uarta.iu_mcr;
    if(!extLoopback)
      reg_value |= 0x10;  /* enable loopback XXX change chris*/
    else
      reg_value &= 0xEF;  /* disable internal loopback */
    ioc3_bp->sregs.uarta.iu_mcr = reg_value;
    
    if(resetCheck)
    {
      /* Check if there is any chars. */
      reg_value = ioc3_bp->sregs.uarta.iu_lsr;
      /*
        if reg_val was one then there was some chars already.
        return with error.
      */
      if (reg_value & 0x1) 
      {
         printf("%s: UARTA error. Chars in FIFO after reset ..... \n",
                "serial_pio");
         ioc3_bp->sregs.uarta.iu_mcr &= (__uint32_t)~(0x10); /* Dis. loopback */
         ioc3_bp->sregs.uartb.iu_mcr &= (__uint32_t)~(0x10); /* Dis  loopback */
         result_fail("serial_pio",KLDIAG_SER_PIO_FAIL1A, " ");
         /* Un-register diag exception handler. */
         restorefault(old_buf);
         return(KLDIAG_SER_PIO_FAIL1A);
      }
    }
    else
    {
      /* flush receive FIFO */
      /* Are there any characters in FIFO? */
      if(ioc3_bp->sregs.uarta.iu_lsr & 0x01)
        for(count = 0; count < 8; count++)
          reg_value = diag_uart_read(ioc3_bp, DIAG_UARTA, "serial_pio");
    }
    
    reg_value = ioc3_bp->sregs.uartb.iu_mcr;
    if(!extLoopback)    
      reg_value |= 0x10;  /* Enable loopback */
    else
      reg_value &= 0xEF;  /* disable internal loopback */
    ioc3_bp->sregs.uartb.iu_mcr = reg_value;

    if(resetCheck)
    {
      /* Check if there is any chars. */
      reg_value = ioc3_bp->sregs.uartb.iu_lsr;

      if (reg_value & 0x1) 
      {
        printf("%s: UARTB error. Chars in FIFO after reset ..... \n",
               "serial_pio");
        ioc3_bp->sregs.uarta.iu_mcr &= (__uint32_t)~(0x10); /* Dis. loopback */
        ioc3_bp->sregs.uartb.iu_mcr &= (__uint32_t)~(0x10); /* Dis  loopback */
        result_fail("serial_pio",KLDIAG_SER_PIO_FAIL1B, " ");
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_SER_PIO_FAIL1B);
      }
    }
    else
    {
      /* flush receive FIFO */
      /* Are there any characters in FIFO? */
      if(ioc3_bp->sregs.uartb.iu_lsr & 0x01)
        for(count = 0; count < 8; count++)
          reg_value = diag_uart_read(ioc3_bp, DIAG_UARTB, "serial_pio");
    }

    /*
      Now transmit the chars. 
    */
    for (i = 0; i < FIFO_DEPTH; i++)
    {
        reg_value = i+65;
	diag_uart_write(ioc3_bp,reg_value,DIAG_UARTA);
        rtc_sleep(10); 
	diag_uart_write(ioc3_bp,reg_value,DIAG_UARTB);
        rtc_sleep(10); 
    }
    
    /*
      Begin read and compare op.
    */
    for (i = 0; i < FIFO_DEPTH; i++)
    {
       reg_value = diag_uart_read(ioc3_bp,DIAG_UARTA, "serial_pio");
       if (reg_value != (i+65))
       {
          error++;
          return_code |= KLDIAG_SER_PIO_FAILA;
          if (verbose)
              printf("%s: UARTA sent: %d recv: %d \n", "serial_pio",
                     i+65, reg_value);
          if (!continue_test)
            break;
       }
    }

    for (i = 0; i < FIFO_DEPTH; i++)
    {
       reg_value = diag_uart_read(ioc3_bp,DIAG_UARTB, "serial_pio");
       if (reg_value != (i+65))
       {
          error++;
          return_code |= KLDIAG_SER_PIO_FAILB;
          if (verbose)
               printf("%s: UARTB sent: %d recv: %d \n", "serial_pio",
                      i+65, reg_value);
          if (!continue_test)
             break;
       }
    }
    
    if(!extLoopback)
    {
      ioc3_bp->sregs.uarta.iu_mcr &= 0xEF;  /* Disable loopback */
      ioc3_bp->sregs.uartb.iu_mcr &= 0xEF;  /* Disable loopback */
    }

    /* Un-register diag exception handler. */
    restorefault(old_buf);

    if (error)
    {
       printf("%s failed with %d errors \n", "serial_pio", error);
       result_fail("serial_pio", return_code, " ");
       return(return_code);
    }
    else
    {
       if (verbose)
          result_pass("serial_pio", diag_mode);
       return(KLDIAG_PASSED);
    }

}


void
dma_reset(ioc3_mem_t *ioc3_bp)
{
        ioc3_bp->port_a.sscr = (SSCR_RESET);
        ioc3_bp->port_b.sscr = (SSCR_RESET); /* Reset the DMA. */
        rtc_sleep(2);
        ioc3_bp->port_a.sscr = 0x1;
        ioc3_bp->port_b.sscr = 0x1; /* Reset the DMA. */
}

void
ioc3_init_serial(ioc3_mem_t *ioc3_bp)
{
  /*
    Init port a.
  */
  ioc3_bp->port_a.stpir = 0;
  ioc3_bp->port_a.srcir = 0;
  ioc3_bp->port_a.srpir = 0;
  ioc3_bp->port_a.stcir = 0;
  /*
    Init port b.
  */
  ioc3_bp->port_b.stpir = 0;
  ioc3_bp->port_b.stcir = 0;
  ioc3_bp->port_b.srpir = 0;
  ioc3_bp->port_b.srcir = 0;

  ioc3_bp->port_a.sscr =  SSCR_HFC_EN  | SSCR_RX_THRESHOLD | SSCR_HIGH_SPD;
  ioc3_bp->port_b.sscr =  SSCR_HFC_EN  | SSCR_RX_THRESHOLD | SSCR_HIGH_SPD;

}


void
clear_ring_buffers(__uint64_t *ring_buff)
{
   int         i;
   __uint64_t  *ring_buf_ptr;
   
   ring_buf_ptr = ring_buff;
   for (i = 0; i < DMA_NUM_ENTRIES*4; i++)
   {
      ring_buf_ptr[i] = 0;
   }
}


void
fill_tx_transmit_data(__uint64_t *ring_buff,int ch_num)
{
   int         i;
   __uint64_t  *ring_buf_ptr,s_val,j;
  
   s_val = TXCB_VALID;
   if (ch_num == DIAG_UARTA)
        ring_buf_ptr = ring_buff;
   else
        ring_buf_ptr = ring_buff+(2*DMA_NUM_ENTRIES);

   j = 0;
   for (i = 0; i < (DMA_NUM_ENTRIES-2); i++)
   {
      if (j == 12)
        j = 0;
 
        ring_buf_ptr[i] = (s_val | (s_val << 8) | (s_val << 16)|
                       (s_val << 24) | ((j+65) << 32) | ((j+66) << 40) |
                       ((j+67) << 48) | ((j+68) << 56));
      j+=4;
   }
}

int
check_rx_data(int verbose,int continue_test,__uint64_t *ring_buff,
	      int ch_num,ioc3_mem_t  *ioc3_bp, char *diag_name)
{
   int         i,error = 0;
   __uint64_t  *ring_buf_ptr,exp_data,j,mask,s_mask;
  
#define MAX_RX_DATA_ERRORS 4

    /*
       Upper four bytes is DATA. Lower is status. 0xc0 as status
       is for DATA & MODEM VALID indicator.
    */
    mask =   0xffffffffc0c0c0c0; 
    s_mask = 0xc0;
    if (ch_num == DIAG_UARTA)
    {
         if (verbose)
             printf("%s: Data Compare for UARTA..... \n", diag_name);
         ring_buf_ptr = ring_buff+ DMA_NUM_ENTRIES;
    }
    else
    {
         if (verbose)
             printf("%s: Data Compare for UARTB..... \n", diag_name);
         ring_buf_ptr = ring_buff+ (3*DMA_NUM_ENTRIES);
    }
   j = 0;

   for (i = 0; i < (DMA_NUM_ENTRIES-2) && error < MAX_RX_DATA_ERRORS; i++)
   {
      if (j == 12)
        j = 0;

        
        exp_data = (s_mask | (s_mask << 8) | (s_mask << 16)|
                       (s_mask << 24) | ((j+65) << 32) | ((j+66) << 40) |
                       ((j+67) << 48) | ((j+68) << 56));

        if (exp_data != (ring_buf_ptr[i] & mask))
        {
            error++;
            if (verbose)
                printf("%s: Serial DMA exp: %lx recv:%lx \n", 
                      diag_name, exp_data, ring_buf_ptr[i] & mask);
            if (!continue_test)
               break;
        }
        j+=4;
   }
   if (error)
   {
      if (verbose)
           printf("%s:  %d errors out of %d compares \n", 
		  diag_name, error,DMA_NUM_ENTRIES-2);
   }
   return(error);
}

__uint64_t diag_vtop(__uint64_t addr)
{
   __uint64_t phy_addr = (addr & 0xfffff) | 0x1a00000;
   return(phy_addr);
}


void
setup_dma(ioc3_mem_t  *ioc3_bp,__uint64_t *rbuff_ptr,int  ch_num )
{

    __uint32_t  diag_hub_id,i;

    rbuff_ptr = (__uint64_t *) diag_vtop((__uint64_t) rbuff_ptr);

    ioc3_bp->sbbr_l = (ioc3reg_t) (((__uint64_t) rbuff_ptr) & 0xffffffff);
    diag_hub_id     =  (__psunsigned_t) (REMOTE_HUB_L(NASID_GET(ioc3_bp),
                                                      IIO_WCR)) & 0xf;

    ioc3_bp->sbbr_h = (ioc3reg_t) (((((__uint64_t) rbuff_ptr >> 32) )  & 0xff)
                                    | (diag_hub_id << 28));

    ioc3_bp->port_a.srtr = 0x00000fff;
    ioc3_bp->port_a.sscr = SSCR_HFC_EN | SSCR_DMA_EN | SSCR_HIGH_SPD |
                           SSCR_RX_THRESHOLD;

    ioc3_bp->port_b.srtr = 0x00000fff;
    ioc3_bp->port_b.sscr = SSCR_HFC_EN | SSCR_DMA_EN | SSCR_HIGH_SPD |
                           SSCR_RX_THRESHOLD;

    ioc3_bp->sio_ir = 0xffffffff;
 
}
   


int
diag_serial_dma(int diag_mode,__psunsigned_t bridge_baseAddr,int npci)
{
    int                     error = 0,error_a= 0,error_b = 0,wid_id,i,loop_cnta,loop_cntb;
    ioc3_mem_t              *ioc3_bp;
    volatile __psunsigned_t ioc3_base;
    __uint64_t              *rbuff_ptr;
    __psunsigned_t          pagesz=_PAGE_SIZE;
    uchar_t                 reg_value;
    volatile unsigned int   loop1,loop2;
    int                     mode, verbose,continue_test,return_code = 0;
    jmp_buf                 fault_buf;
    void                    *old_buf;
    int                     extLoopback = 0;
    int                     initUARTA;

    /* Register diag exception handler. */
    if (setfault(fault_buf, &old_buf))
    {
        printf("\n=====> %s diag took an exception. <=====\n", "serial_dma");
        diag_print_exception();
        kl_log_hw_state();
        kl_error_show_log("Hardware Error State: (Forced error dump)\n",
                          "END Hardware Error State (Forced error dump)\n");
        result_fail("serial_dma",KLDIAG_SERDMA_EXCEPTION, "Took an exception");
        restorefault(old_buf);
        return(KLDIAG_SERDMA_EXCEPTION);
    }

    mode = GET_DIAG_MODE(diag_mode);
    verbose = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_VERBOSE)
                    || mode == DIAG_MODE_MFG);

    continue_test = ((GET_DIAG_FLAGS(diag_mode) & DIAG_FLAG_CONTINUE)
                    || mode == DIAG_MODE_MFG);

    initUARTA = diag_mode & DIAG_FLAG_NORESET;

    if (mode == DIAG_MODE_NONE)
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(KLDIAG_PASSED);
    }

    if(mode == DIAG_MODE_EXT_LOOP)
    {
      /* treat external loopback mode like manufacturing mode */
      /* but don't enable UART internal loopback              */
      mode = DIAG_MODE_MFG;
      extLoopback = 1;
    }
    
    DM_PRINTF(("\nRunning %s diag (Bridge base = 0x%08x  PCI dev = %d)\n",
	   "serial_dma", bridge_baseAddr, npci));

    if (return_code = diag_check_ioc3_cfg(diag_mode, bridge_baseAddr, npci,
                                          "serial_dma"))
    {
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        return(return_code);
    }
    
    rtc_sleep(100000);

    ioc3_base = diag_ioc3_base(bridge_baseAddr,npci);
    if (!ioc3_base)
    {
        error ++;
        printf("%s: PCI: could not get the ioc3 base addr \n", "serial_dma");
        /* Un-register diag exception handler. */
        restorefault(old_buf);
        result_fail("serial_dma",KLDIAG_IOC3_BASE_FAIL, " ");
        return(KLDIAG_IOC3_BASE_FAIL);
    }
    

    ioc3_bp   = (ioc3_mem_t *) ioc3_base;
    /*
       Make sure there is nothing in the fifo .
       give a delay.
    */

    rbuff_ptr = (__uint64_t *)((__psunsigned_t)(ring_buffer+pagesz)  & 
                        ~(__psunsigned_t)(pagesz - (__psunsigned_t)1));
    /*
       Reset and fill up the host buffers.
    */
    clear_ring_buffers(rbuff_ptr);
    fill_tx_transmit_data(rbuff_ptr,DIAG_UARTA);
    fill_tx_transmit_data(rbuff_ptr,DIAG_UARTB);

    /*
       Set up the IOC3 registers.
    */
    ioc3_bp->int_out  &= ~INT_OUT_DIAG;
    ioc3_bp->sio_cr = ((UARTA_BASE >> 3  ) << SIO_CR_SER_A_BASE_SHIFT) |
                 ((UARTB_BASE >> 3 ) << SIO_CR_SER_B_BASE_SHIFT) |
                 (0x4 << SIO_CR_CMD_PULSE_SHIFT);

    ioc3_bp->gpdr = 0;
    ioc3_bp->gpcr_s = GPCR_INT_OUT_EN | GPCR_MLAN_EN | GPCR_DIR_SERA_XCVR |
                      GPCR_DIR_SERB_XCVR;

    diag_uartB_init(ioc3_bp);
    if(initUARTA)
      diag_uartA_init(ioc3_bp);

    /*
        Setup Super IO registers. A & B.
    */
    ioc3_bp->sregs.uarta.iu_ier =  0; 
    ioc3_bp->sregs.uarta.iu_scr  = 0x8;
    ioc3_bp->sregs.uarta.iu_mcr  = 0x3;
    ioc3_bp->sregs.uarta.iu_fcr =  FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET
                                   | FCR_XMIT_FIFO_RESET;
    ioc3_bp->sregs.uarta.iu_fcr =   FCR_ENABLE_FIFO;
    
    ioc3_bp->sregs.uartb.iu_ier =  0; 
    ioc3_bp->sregs.uartb.iu_scr  = 0x8;
    ioc3_bp->sregs.uartb.iu_mcr  = 0x3;
    ioc3_bp->sregs.uartb.iu_fcr =  FCR_ENABLE_FIFO | FCR_RCV_FIFO_RESET
                                   | FCR_XMIT_FIFO_RESET;
    ioc3_bp->sregs.uartb.iu_fcr =   FCR_ENABLE_FIFO;

    dma_reset(ioc3_bp);
    ioc3_init_serial(ioc3_bp);

    if(!extLoopback)
    {
      /*
         Enable loop back Channel A. 
      */
      reg_value = ioc3_bp->sregs.uarta.iu_mcr;
      reg_value |= 0x10; 
      ioc3_bp->sregs.uarta.iu_mcr = reg_value;
      ioc3_bp->port_a.shadow = (__uint32_t)reg_value << 24;

      /*
         Enable loop back Channel B. 
      */
      reg_value = ioc3_bp->sregs.uartb.iu_mcr;
      reg_value |= 0x10; 
      ioc3_bp->sregs.uartb.iu_mcr = reg_value;
      ioc3_bp->port_b.shadow = (__uint32_t)reg_value << 24;
    }
    else
    {
      /*
        Disable loopback Channel A
      */
      reg_value = ioc3_bp->sregs.uarta.iu_mcr;
      reg_value &= 0xEF;
      ioc3_bp->sregs.uarta.iu_mcr = reg_value;
      ioc3_bp->port_a.shadow = (__uint32_t)reg_value << 24;

      /*
        Disable loopback Channel B
      */
      reg_value = ioc3_bp->sregs.uartb.iu_mcr;
      reg_value &= 0xEF;
      ioc3_bp->sregs.uartb.iu_mcr = reg_value;
      ioc3_bp->port_b.shadow = (__uint32_t)reg_value << 24;
    }

    /*
       Write to the Transmit Produce index register 
       to fire the DMA. We wrote 126 indexed entries.
       so write one past the index to stpir.
    */
    ioc3_bp->port_a.stpir = 127 << 3;
    ioc3_bp->port_b.stpir = 127 << 3;
    setup_dma(ioc3_bp,rbuff_ptr,DIAG_UARTA);

    /*
       Wait for the DMA to complete
    */
   loop_cnta = 0;
    while ((*(rbuff_ptr + 2*DMA_NUM_ENTRIES - 3) & 0x80808080) != (0x80808080) && (loop_cnta < 50000))
    {
            rtc_sleep(100);
            loop_cnta ++;
    }
   loop_cntb = 0;
    while ((*(rbuff_ptr + 4*DMA_NUM_ENTRIES - 3) & 0x80808080) != (0x80808080) && (loop_cntb < 50000))
    {
            rtc_sleep(100);
            loop_cntb ++;
    }
    /*
       Now disable DMA and Loop back. For both A and B.
    */
    ioc3_bp->port_a.sscr = 0;
    ioc3_bp->sregs.uarta.iu_mcr &= 0xEF;
    ioc3_bp->port_b.sscr = 0;
    ioc3_bp->sregs.uartb.iu_mcr &= 0xEF;
    if (loop_cnta == 50000)
    {
         printf("%s: Timeout on uartA dma ....\n", "serial_dma");
    }
    
    if (loop_cntb == 50000)
    {
         printf("%s: Timeout on uartB dma ....\n", "serial_dma");
    }
    error_a = check_rx_data(verbose,continue_test,rbuff_ptr,DIAG_UARTA,ioc3_bp, "serial_dma");
    if (error_a)
       return_code = KLDIAG_SER_DMA_FAILA;
    error_b = check_rx_data(verbose,continue_test,rbuff_ptr,DIAG_UARTB,ioc3_bp, "serial_dma");
    if (error_b)
       return_code |= KLDIAG_SER_DMA_FAILB;
    error = error_a + error_b;

    /* Un-register diag exception handler. */
    restorefault(old_buf);
    
    if (error)
    {
        result_fail("serial_dma",return_code, " ");
        return(return_code);
    }
    else
    {
        if (verbose)
           result_pass("serial_dma",diag_mode);
        return(KLDIAG_PASSED);
    }


}
