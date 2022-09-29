/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1995, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * linc_sys.c; C. Narad
 *
 * This file contains the system-level tests for LINC through Bridge,
 * Hub, etc driven from a T5.  It is leverage from the widget_true_share 
 * test suite.  The model it is written for is 2newt1hub1wid2linc, 
 * so two processors can each manage one LINC forked on cpunum.
 *
 * The test runs in model 2newt1hub1wid2linc, which has 2 t5's,
 * 1 hub, 1 bridge in direct-connect, and two linc_module_1 units,
 * each of which contains a LINC, an r4k stub, sdram, a bytebus
 * stub, and a dma engine on the child bus.
 *
 * the configuration details include:
 *
 * LINC CHIPS ON THE PARENT BUS
 *
 *                    Bridge Connection
 *                        (per LINC)
 *	                    LINC0       LINC1
 *	LINC SIGNAL
 *
 *	Interrupt              1           2
 *	Request/Grant 0        1           3
 *	Request/Grant 1        2           4
 *	IDSEL               AD[34]      AD[36]
 *	
 *	Bridge address     0x02_2000   0x02_4000
 *	of PCI Config space
 *
 * Bridge PCI Config Space Device   Device 2    Device 4
 *
 */
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <libsk.h>
#include <libsc.h>
#include <uif.h>
#include <standcfg.h>

#include <sys/PCI/linc.h>


#include "linc_struct.h"
#include "pci_config.h"

#include "d_x_test.h"

/* the compiler makes me do this */
void linc_init(int cpunum,struct proc_args *parg_p);
void rand(unsigned int num_dw,long seed,volatile long *buf);
void fail_spin(unsigned char fail_code);
void stuff_descrip(int index,long host_a,long buf_a,long cnt,long cmd,struct proc_args *parg_p);
void compare(volatile char *txbuf,volatile char *rxbuf,int rzone1_sz,int data_sz,int rzone2_sz,struct proc_args *parg_p);
void test_dma(struct test_args *targ_p,struct proc_args *parg_p);
void dump_linc_args(struct linc_card* linc,int linc_num);

/* skip return 1 errors in sable */
#ifdef SABLE
#define RETURN(x)
#else
#define RETURN(x) return(x)
#endif

#define LINC_SIZE (128*1024*1024)         /* 1 linc is 128 MB long */
#define LINC_OFFSET(x) ((x+1)*LINC_SIZE)  /* put linc0 at 128MB and linc1 at 256MB */

#define LINC0_DEVNUM 2           /* linc 0 is device 2 */ 
#define LINC1_DEVNUM 4           /* linc 1 is device 4 */ 
                                 /* if linc1 then devnum = 4 else devnum = 2 */
#define LINC_DEVNUM(x) (x?LINC1_DEVNUM:LINC0_DEVNUM)  

/*
 * try to do here what linc_sys.c: linc_init() does
 *
 */
int
init_linc(int port,bridge_t *bridge, struct linc_card *linc){
  
  int temp;
  int linc_num;
  struct linc_chip* lp;


  dprintf(("init_linc()\n"),DBG_FUNC);

  /* set widget id */
  linc->widget_id = port;

  /* get widget's base addr - based on widget id */
  linc->widget_base = K1_HUGE_WIDGET(linc->widget_id);

  for (linc_num=0 ; linc_num<=1 ; linc_num++) {

    /* set linc chip ptr */
    lp = &linc->linc_chip[linc_num];

    /* linc config space */
    lp->config_base = (volatile struct pci_config *)(linc->widget_base+
						     BRIDGE_TYPE0_CFG_DEV(LINC_DEVNUM(linc_num)));

    /* linc base addr */
    lp->linc_base = linc->widget_base + BRIDGE_PCI_MEM64_BASE + LINC_OFFSET(linc_num);

    /* set up stuff in linc addr space */
    lp->linc_reg = (volatile struct linc_misc_regs *)(lp->linc_base + LINC_MISC_REGS_ADDR);
    lp->dma[0] = (volatile struct linc_dma *)(lp->linc_base + LINC_DMA0_ADDR);
    lp->dma[1] = (volatile struct linc_dma *)(lp->linc_base + LINC_DMA1_ADDR);
    lp->bufmem = (volatile unsigned char *)(lp->linc_base + 0x0);
    lp->mbox = (volatile long *)(lp->linc_base + LINC_MAILBOX_ADDR);
    lp->cpci_mem = (volatile int*)(lp->linc_base + LINC_CPCI_PIO_ADDR);
    lp->cpci_mem_swiz = (volatile int*)(lp->linc_base + LINC_CPCI_PIO_BSWAP_ADDR);

    /* add pointer for mem_space access to config regs. */
  
    /* read PCI_CONFIG_VENDOR_DEVICE_ID */
#define VENDOR_ID 0x000210a9
    temp = lp->config_base->vendor_dev_id;
    if (temp != VENDOR_ID) {
      printf("Error: Got 0x%X from PCI CONFIG VENDOR DEVICE ID, expected 0x%X\n",
	     temp,VENDOR_ID);
      RETURN(1);
    }
  
    /* read REV etc.  */
#define REV 0xff000000
    temp = lp->config_base->rev_id_class;
    if (temp != REV) {
      printf("Error: Got 0x%X from PCI CONFIG REV, expected 0x%X\n",
	     temp,REV);
      RETURN(1);
    }
  
    /* set latency timer */
#define LATENCY_1 0x00800000
    temp = lp->config_base->header_latency;
    if (temp != LATENCY_1) {
      printf("Error: Got 0x%X from PCI CONFIG LATENCY, expected 0x%X\n",
	     temp,LATENCY_1);
      RETURN(1);
    }
    lp->config_base->header_latency = 0x00004000;
#define LATENCY_2 0x00804000
    temp = lp->config_base->header_latency;
    if (temp != LATENCY_2) {
      printf("Error: Got 0x%X from PCI CONFIG LATENCY, expected 0x%X\n",
	     temp,LATENCY_2);
      RETURN(1);
    }
    
    /* set base address for pci - linc0 at 128MB, linc1 at 256MB ... */
    lp->config_base->base_address = LINC_OFFSET(linc_num);
    
    /* enable mem space & master response, and verify reset values */
    temp = (LINC_PCCSR_MASTER_EN | LINC_PCCSR_MEM_SPACE | LINC_PCCSR_PAR_RESP_EN |
	    LINC_PCCSR_SERR_EN);
    lp->config_base->conf_cmd_stat = temp;
    temp |= 0x1 << LINC_PCCSR_DEVSEL_TIMING_SHFT; /* rd-only value */
    if (lp->config_base->conf_cmd_stat != temp) {
      printf("Error: got 0x%X from PCI CONFIG conf_cmd_stat, expected 0x%X\n",
	     lp->config_base->conf_cmd_stat,temp);
      RETURN(1);
    }
    
    /* 
     * enable bufmem (do not init bufmem, handled in verilog)
     */
    
    /* write all 0's to bufmem ctl, read back */
    temp = 0x0;
    lp->linc_reg->bmc  = temp;
    if (lp->linc_reg->bmc != temp) {
      printf("Error: got 0x%X from LINC REG bmc, expected 0x%X\n",
	     lp->linc_reg->bmc,temp);
      RETURN(1);
    }
    
    /* write all 1's to bufmem ctl(except rfsh_en), read back */
    temp = 0xffffffff;
    temp &= ~(LINC_BMC_REF_EN);
    temp &= ~(LINC_BMC_PAR_CHK_EN);
    lp->linc_reg->bmc  = temp;
    if (lp->linc_reg->bmc != temp) {
      printf("Error: got 0x%X from LINC REG bmc, expected 0x%X\n",
	     lp->linc_reg->bmc,temp);
      RETURN(1);
    }    
    
    /* init bufmem_ctl values */
    temp = (2 << LINC_BMC_CAS_LATENCY_SHFT) | (1024 << LINC_BMC_REFRESH_SHFT) |
      (5 << LINC_BMC_REFR_TO_ACT_SHFT) | (3 << LINC_BMC_PR_TO_ACT_SHFT) |
	(3 << LINC_BMC_ACT_TO_PR_SHFT) | (3 << LINC_BMC_RAS_TO_CAS_SHFT);
    
    /* no rfsh enable yet */
    lp->linc_reg->bmc  = temp;
    
    /* issue precharge */
    lp->linc_reg->bmo = LINC_BMO_DO_PRECH;
    while (lp->linc_reg->bmo & LINC_BMO_DO_PRECH) {}
    
    /* do mode set operation */
    lp->linc_reg->bmo  = (LINC_BMO_MODE_SET | (3 << LINC_BMO_MODE_BURST_SHFT) |
			      (2 << LINC_BMO_MODE_LAT_SHFT) | (0 << LINC_BMO_MODE_WRAP_SHFT));
    while (lp->linc_reg->bmo & LINC_BMO_MODE_SET) {}
    
    /* issue two rfsh ops */
    lp->linc_reg->bmo = LINC_BMO_DO_REF;
    while (lp->linc_reg->bmo & LINC_BMO_DO_REF) {}
    
    lp->linc_reg->bmo = LINC_BMO_DO_REF;
    while (lp->linc_reg->bmo & LINC_BMO_DO_REF) {}
    
    /* enable rfsh */
    temp |= LINC_BMC_REF_EN;
    lp->linc_reg->bmc  = temp;
    if (lp->linc_reg->bmc != temp) {
      printf("Error: got 0x%X from LINC REG bmc, expected 0x%X\n",
	     lp->linc_reg->bmc,temp);
      RETURN(1);
    }
    
    /* dbg */
    if (Debug >= DBG_INTE)
      dump_linc_args(linc,linc_num);
   
  }
  
  /* success */
  return(0);
  
}

/*
 * dump debug routine 
 */
void dump_linc_args(struct linc_card* linc,int linc_num) {

  struct linc_chip* lp;

  lp = &linc->linc_chip[linc_num];

  printf("=====================================\n");
  printf("dumping for linc chip #%d\n",linc_num);
  printf("widget id       = 0x%X\n",linc->widget_id);
  printf("widget_base     = 0x%X\n",linc->widget_base);
  printf("linc_base       = 0x%X\n",lp->linc_base);
  printf("config_base     = 0x%X\n",lp->config_base);
  printf("linc_reg        = 0x%X\n",lp->linc_reg);
  printf("dma0            = 0x%X\n",lp->dma[0]);
  printf("dma1            = 0x%X\n",lp->dma[1]);
  printf("bufmem          = 0x%X\n",lp->bufmem);
  printf("mbox            = 0x%X\n",lp->mbox);
  printf("cpci_mem        = 0x%X\n",lp->cpci_mem);
  printf("cpci_mem_swiz   = 0x%X\n",lp->cpci_mem_swiz);
  printf("=====================================\n");

}

#define MAX_DMA_SIZE (128*1024-4) /* 128k-4 in bytes */
#define MAX_XFER_size (4*MAX_DMA_SIZE) /* current max xfer size
					  supported in linc_dma() */
/* for dma direction */
#define HOST_TO_LINC  0x1  /* host -> linc dma */      
#define LINC_TO_HOST  0x2  /* linc -> host dma */

struct dma_args {
  int dest_port;         /* destination widget id */
  __uint64_t xtalk_addr; /* 47:0 addr bits */
  char* bufmem;
  int size;
  int direction;
};

/*
 * do dma
 *
 */
int
linc_dma(struct linc_card* linc, int linc_num, struct dma_args* dma_args,
	 int dma_engine) {

  struct linc_chip* lp;
  volatile struct linc_dma *dma;
  __uint32_t dtc;
  int num_of_xfers;
  int count;

  if (! ((linc_num == 0) || (linc_num == 1)) ) {
    printf("linc_dma_setup: Error linc has no linc chip #%d\n",linc_num);
    return(1);
  }

  if (! ((dma_engine == 0) || (dma_engine == 1)) ) {
    printf("linc_dma_setup: Error linc has no dma engine #%d\n",dma_engine);
    return(1);
  }
    
  /* get ptr to linc chip */
  lp = &linc->linc_chip[linc_num];

  /* get ptr to dma regs */
  dma = lp->dma[dma_engine];
  
  /* 1. reset dma engine */
  dma->dcsr |= LINC_DCSR_RESET;

  /* 2. enable dma */
  dma->dcsr |= LINC_DCSR_EN_DMA;

  /* 3. enable interupts by setting mask bits to 0 */
#define LINC_CIMR_DMA0_INT 0x1
#define LINC_CIMR_DMA1_INT 0x2
  if (dma_engine)
    lp->linc_reg->cimr &=  ~(__uint32_t)LINC_CIMR_MASK4_W(LINC_CIMR_DMA1_INT);
  else
    lp->linc_reg->cimr &=  ~(__uint32_t)LINC_CIMR_MASK4_W(LINC_CIMR_DMA0_INT);
  
  /* 4. load high PPCI address */ 
  /* XXX check this - dont look right XXX */
  /* put xtalk addr bits 47:32 into bottom of dhpa */
#define LINC_DPHA_XTALK_ADDR_SHIFT 32
  dma->dhpa = (__uint32_t) ((dma_args->xtalk_addr & 0x0000FFFF00000000) >> 
    LINC_DPHA_XTALK_ADDR_SHIFT);
  /* set dest port */
#define LINC_DPHA_TARGET_ID_SHIFT 28  
  dma->dhpa |= (dma_args->dest_port << LINC_DPHA_TARGET_ID_SHIFT);
  /* turn on prefetch */
#define LINC_DPHA_PREFETCH 0x08000000
  dma->dhpa |= LINC_DPHA_PREFETCH;

  /* 5. load lower PPCI address */
  dma->dlpa = (__uint32_t) (dma_args->xtalk_addr & 0x00000000FFFFFFFF);

  /* 6. load bufmem addr */
  dma->dbma = (__uint32_t) dma_args->bufmem;

  /* need to break up into MAX_DMA_SIZE'd xfers */
  num_of_xfers = dma_args->size / MAX_DMA_SIZE;
  if (dma_args->size % MAX_DMA_SIZE != 0)
    num_of_xfers++;

  /* currently only support 4 dmas in a row */
  if (num_of_xfers > 4) {
    printf("linc_dma: Error: size = %d too large for current implementation\n",
	   dma_args->size);
    return(1);
  }

  /* 7. load DTC reg w/ byte count, direction & control info */
  /* every DTC store goes into a 4-wide fifo */
  for (count=1 ; count<=num_of_xfers ; count++) {

    /* init value for dtc reg */
    dtc = 0x0;

    /* set direction of dma */
    if (dma_args->direction == LINC_TO_HOST)
      dtc |= LINC_DTC_TO_PARENT;
    else if (dma_args->direction != HOST_TO_LINC) {
      printf("linc_dma: Error: bad direction %d\n",
	   dma_args->direction);
      return(1);
    }

    /* set chaining - dont chain on first xfer */
    if (count > 1) {
      dtc |= LINC_DTC_CHAIN_BA; /* buffer addr */
      dtc |= LINC_DTC_CHAIN_PA; /* ppci addr */
      dtc |= LINC_DTC_CHAIN_CS; /* chain checksum */
    }

    /* save checksum */
    dtc |= LINC_DTC_SAVE_CS;

    /* use D64 */
    dtc |= LINC_DTC_D64;

    /* set size */
    if (count == num_of_xfers) 
      dtc |= LINC_DTC_WORD_CNT_W(dma_args->size % MAX_DMA_SIZE);
    else
      dtc |= LINC_DTC_WORD_CNT_W(MAX_DMA_SIZE);

    /* writting the dtc register starts the dma */
    dma->dtc = dtc;
  }


  /* success */
  return(0);

}



/*
 * pseudo-random number generator that generates "num_dw" doublewords
 * in memory at "buf".
 */
void rand(unsigned int num_dw,long seed,volatile long *buf){
  
  long data = 0x0a0b0c0d0e0f0102;
  
  for(num_dw++;num_dw > 0; num_dw--){
    *buf++ = seed;
    seed += data;
  }

}


/*
 * error 
 */
void fail_spin(unsigned char fail_code){
  printf("failed ... fail_code = 0x%X\n",fail_code);
  while (1) {}      /* stop on failure */
}


/*
 * do the descriptor dance
 */
void stuff_descrip(int index, long host_a, long buf_a, long cnt, long cmd,
		   struct proc_args *parg_p){
/* NULL */
}

/* 
 * compares the tx buffer contents with the rx buffer contents;
 * rx_buf and tx_buf are always mutually aligned; the red zone
 * starts at a double-word aligned location rzone1_sz below
 * the start of rx_buf.  rzone1 and rzone2 bytes are tested for
 * 0x11's above and below (respectively) rx_buf.
 * 
 */
void compare(volatile char *txbuf,volatile char *rxbuf,int rzone1_sz,int data_sz,
	     int rzone2_sz,struct proc_args *parg_p){

  long temp_l;
  unsigned char temp_b;
  volatile char *temp_ptr;

  /* test leading red zone for zeros */
  
  temp_ptr = (volatile char *)((long)rxbuf - rzone1_sz);
  while(rzone1_sz > 8){
    temp_l = *(long *)temp_ptr;
    if(temp_l != 0x1111111111111111){
      parg_p->stub_stat[5] = (long)temp_ptr;
      parg_p->stub_stat[5] = temp_l;
      parg_p->stub_stat[5] = 0x1111111111111111;
      fail_spin(0x03);
    }
    temp_ptr += 8;
    rzone1_sz -= 8;
  }
  while(rzone1_sz > 0){
    temp_b = *temp_ptr++;
    if(temp_b != 0x11){
      parg_p->stub_stat[5] = (long)temp_ptr;
      parg_p->stub_stat[5] = (unsigned long)temp_b;
      parg_p->stub_stat[5] = 0x11;
      fail_spin(0x04);
    }
    rzone1_sz -= 1;
  }
  
  /* compare rx_buf and tx_buf data; start with leading bytes */
  while((((long)txbuf & 0x7) != 0) && (data_sz > 0)){
    if(*txbuf++ != *rxbuf++){
      parg_p->stub_stat[5] = (long)--rxbuf;
      parg_p->stub_stat[5] = (unsigned long)*rxbuf;
      parg_p->stub_stat[5] = (long)--txbuf;
      parg_p->stub_stat[5] = (unsigned long)*txbuf;
      fail_spin(0x05);
    }
    data_sz -= 1;
  }
  /* now the doublewords */
  while(data_sz > 8){
    if(*(long *)txbuf != *(long *)rxbuf){
      parg_p->stub_stat[5] = (long)rxbuf;
      parg_p->stub_stat[5] = *(long*)rxbuf;
      parg_p->stub_stat[5] = (long)txbuf;
      parg_p->stub_stat[5] = *(long*)txbuf;
      fail_spin(0x06);
    }
    txbuf += 8;
    rxbuf += 8;
    data_sz -= 8;
  }
  /* now the trailing bytes */
  while(data_sz > 0){
    if(*txbuf++ != *rxbuf++){
      parg_p->stub_stat[5] = (long)--rxbuf;
      parg_p->stub_stat[5] = (unsigned long)*rxbuf;
      parg_p->stub_stat[5] = (long)--txbuf;
      parg_p->stub_stat[5] = (unsigned long)*txbuf;
      fail_spin(0x07);
    }
    data_sz -= 1;
  }
  /* now compare the rx_buf trailing red_xone */
  while((((long)rxbuf & 0x7) != 0) && (rzone2_sz > 0)){
    temp_b = *rxbuf++;
    if(temp_b != 0x11){
      parg_p->stub_stat[5] = (long)temp_ptr;
      parg_p->stub_stat[5] = (unsigned long)temp_b;
      parg_p->stub_stat[5] = 0x11;
      fail_spin(0x08);
    }
    rzone2_sz -= 1;
  }
  /* now the doublewords */
  while(rzone2_sz > 8){
    temp_l = *(long *)rxbuf;
    if(temp_l != 0x1111111111111111){
      parg_p->stub_stat[5] = (long)temp_ptr;
      parg_p->stub_stat[5] = temp_l;
      parg_p->stub_stat[5] = 0x1111111111111111;
      fail_spin(0x09);
    }
    rxbuf += 8;
    rzone2_sz -= 8;
  }
  /* now the trailing bytes */
  while(rzone2_sz > 0){
    temp_b = *rxbuf++;
    if(temp_b != 0x11){
      parg_p->stub_stat[5] = (long)temp_ptr;
      parg_p->stub_stat[5] = (unsigned long)temp_b;
      parg_p->stub_stat[5] = 0x11;
      fail_spin(0x0a);
    }
    rzone2_sz -= 1;
  }
  
}

void test_dma(struct test_args *targ_p,struct proc_args *parg_p){
  
  int temp;
  long temp_l;

  /* this suite will do simple DMA reads and writes.
   * due to the read-retry bug we cannot simply use DMA
   * to move data down to bufmem & back yet; instead
   * PIO must be used for now.
   *
   * start test numbers at 100.
   */
  
  /* pio down one block of data, d32, no swizzle, dma0, dma to host, compare.
   * repeat with D64.
   * repeat with d32, bswap.
   * repeat with d32, bswap, bridge swap.
   * repeat with d64, bswap.
   * repeat with d64, bswap, bridge swap.
   * repeat with d64, wswap.
   * repeat with d64, wswap, bridge swap.
   * repeat with d64, wswap, bswap.
   * repeat with d64, wswap, bswap, bridge swap.
   *	(use dma1 for some; interrupts for some; vary alignment).
   *
   * in the interest of saving PIO sim time, will use one 256-byte
   * block of bufmem with one init value, and will dma it repeatedly
   * with different starting bufmem offset to different host buffers.
   *
   * all of these tests involve stuffing the dma command queue
   * and polling depth, enqueuing ops as fast as possible.
   */
  
  /* dma read one block of data, d32, no swizzle, pio read, compare.
   * repeat with D64.
   * repeat with d32, bswap.
   * repeat with d32, bswap, bridge swap.
   * repeat with d64, bswap.
   * repeat with d64, bswap, bridge swap.
   * repeat with d64, wswap.
   * repeat with d64, wswap, bridge swap.
   * repeat with d64, wswap, bswap.
   * repeat with d64, wswap, bswap, bridge swap.
   *	(use dma1 for some; interrupts for some; prefetch for some; vary alignment).
   *
   * we can use dma wrt to send back results, saving pio sim time.
   */
 xyzzy:
  temp = temp+1;

}
