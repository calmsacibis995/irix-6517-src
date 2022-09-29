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

#ident	"ide/godzilla/x_test/d_x_test.h:  $Revision: 1.4 $"

/*
 * x_test xbow tests header
 */

#ifndef __X_TEST_H__
#define __X_TEST_H__

/* constants */
#define NARADS_LINC_SYS 0	/* keeping c.Narad's linc_sys code as ref */
				/*  we definitely need some */
/* the code is in platte.asd:/view/narad_platte_vu/sn0/sn0_src/verif/diags/hub/asm/linc_sys */

#define MONITOR_INPUT_PACKET_BUFFER_LEVEL 3	/* code in link control*/
#define PERF_MON_MODE_SEL_SHIFT 28	/* perf. monitor mode sel=[28:29]*/
#define INPUT_PACKET_BUF_LEV_SHIFT 25	/* input packet buf lev.=[25:27]*/
#define LINK_PERF_MON_SEL_SHIFT 20	/* ctrl the perf cntr incr=[20:22]*/

#define	MON_LINK_ID_MIN		XBOW_PORT_8	/* allowed ranges */
#define	MON_LINK_ID_MAX		XBOW_PORT_F	
#define	PERF_MON_MODE_MIN	0x0
#define	PERF_MON_MODE_MAX	0x3
#define	PERF_MON_BUF_LEVEL_MIN	0x0
#define	PERF_MON_BUF_LEVEL_MAX	0x4	/* values above 0x4 will be ignored */

#define	GBR_COUNT_MAX		0x31	
#define	RR_COUNT_MAX		0x7	
#define	GBR_COUNT_MIN		0x0	
#define	RR_COUNT_MIN		0x0	

#define	LOOPBACK_GBR		0x1f	/* value for GBR[i][i], 9.0 specs */
#define	LOOPBACK_RR		0x0	/* value for RR[i][i], 9.0 specs */

/* debug stuff */
extern int Debug;
#define Dprintf(x) (Debug?printf x : 0)
#define dprintf(x,n) (Debug>=n?printf x : 0)
#define DBG_ALWS 0  /* always print */
#define DBG_FUNC 2  /* print function calls */
#define DBG_INTE 3  /* print interesting info */
#define DBG_LOTS 9  /* super verbose */

/* structure for passing arguments to the test* routines */
struct test_args{
	unsigned char perf_mon_mode[8]; /* array of perf mon select code
							in Link(x) ctrl reg*/
	unsigned char perf_mon_buf_level[8];/* perf mon input packet buf level*/
	unsigned char gbr_count[8][8];	/* GBR count/link to arb register */
	unsigned char rr_count[8][8];	/* Remainder Ring count/link (arb reg)*/
	unsigned char arb_reload_interval;	/* arb reload int */
	unsigned char mon_link_id_a;	/* link to be monitored */
	unsigned char mon_link_id_b;	/* link to be monitored */
	xbowreg_t xbow_wid_perf_ctr_a;	/* perf counter results */
	xbowreg_t xbow_wid_perf_ctr_b;	/* perf counter results */

#if NARADS_LINC_SYS
	long *t_buf_raw;	/* phys addr of tx buf */
	long *r_buf_raw;	/* phys addr of rx buf */
	long *r_stat_raw;	/* phys addr of rx status */
	long *t_stat_raw;	/* phys addr of tx status */
	long t_buf_pflags;	/* proc attributes for tx buffer */
	long r_buf_pflags;	/* proc attributes for rx buffer */
	long t_stat_pflags;	/* proc attributes for rx status */
	long r_stat_pflags;	/* proc attributes for tx status */
	long t_buf_dflags;	/* dma attributes for tx buffer */
	long r_buf_dflags;	/* dma attributes for rx buffer */
	long t_stat_dflags;      /* dma attributes for tx status */
	long r_stat_dflags;      /* dma attributes for rx status */
	long r_buf_cmdflags;    /* used to pass DMA_CMD_SMALL_BURSTS for rx_buf */
	long buf_align;		/* offset from doubleword base */
	long rand_seed;		/* seed for pseudo-random data */
	int num_bytes;		/* number of bytes to transfer */
	int rzone1;		/* dw size of red zone before rxbuf */
	int rzone2;		/* dw size of red zone after rxbuf */
	int pio_chatter;	/* flags for style of PIO traffic generator */
	int use_interrupt;	/* flag to use an interrupt */
	int cacheflags;		/* flags for MP use of buffers; note that
				 cache and memory nodes *can* differ */
	int testflags;		/* misc. flags */
	long test_number;	/* status visible on XTalk for aid in debug */
	int idma_csr_value;	/* indirect dma unit csr value */
	int idma_rdp_value;	/* irdp in bufmem for idma reads */
	int idma_bmask_expect;	/* value byte mask should have after iwdh/iwdl wrt */
	int idma_h_sz;
	int idma_h_align;
	int idma_h_data;
	int idma_l_sz;
        int idma_l_align;
	int idma_l_data;
#endif /* NARADS_LINC_SYS */
	};

/* munge all local variables into a structure, then declare the structure
 * inside of main to get per-processor copies of these on the stack for mthread.
 */
struct proc_args{

/* Anouchka defs begin */
	volatile unsigned char index;		/* widget index 8-f */
	volatile unsigned char link;		/* link index 0-7 */
	volatile xbowreg_t x_mask;		/* xbow reg mask */
	volatile xbowreg_t xb_link_ctrl;	/* xbow link ctrl reg */
	volatile xbowreg_t xbow_wid_arb_reload;	/* xbow arb reload interval */
	volatile xbowreg_t xb_link_arb_lower;	/* xbow link arb lower */
	volatile xbowreg_t xb_link_arb_upper;	/* xbow link arb upper */
	volatile xbowreg_t xb_link_arb;		/* generic xbow link arb */
	volatile xbowreg_t xbow_wid_perf_ctr_a;	/* perf counter var */
	volatile xbowreg_t xbow_wid_perf_ctr_b;	/* perf counter var */

					/* for testing only: */
	volatile unsigned char i;		/* widget index for test only*/
	volatile unsigned char j;		/* widget index for test only*/
	volatile unsigned int errors;		/* # of err for test only*/
	volatile xbowreg_t xb_test;		/* xbow reg for test only*/
	volatile xbowreg_t xb_test_2;		/* xbow reg for test only*/
/* Anouchka defs end */

/* Chuck defs begin */
	volatile struct pci_config 	*config_base; /* pci config space on linc */
	volatile struct linc_misc_regs	*linc_reg; /* misc reg's */
	volatile struct linc_dma	*dma0;
	volatile struct linc_dma	*dma1;
	volatile unsigned char		*bufmem;
	volatile long			*mbox;
	volatile int			*cpci_mem;
	volatile int			*cpci_mem_swiz;
	volatile long *stub_stat;	/* array of status words in stub */
	volatile long *start_reg;	/* pointer to kick-start register */
	volatile long *test_num_start;	/* pointer to start status word */
	volatile long *test_num_end;	/* pointer to end status word */
	volatile long *testfaildata;	/* make bad cmpr info visible */
	unsigned int interrupt_count;	/* count of interrupts taken */
	int cpunum;			/* per-hub cpu number */
	int remote_cpu;			/* flag indicates remote cpu for multi-hub */
	unsigned long tx_stat_data; 	/* data written back for status */
	unsigned long rx_stat_data;
	int interrupt_bit;		/* variable to pass interrupt to use */
/* Chuck defs end */

	};


/* 2 linc chips / linc card */
struct linc_chip {
  int                            linc;        /* linc #, 2 linc chips on a linc card */  
  __uint64_t                     linc_base;
  volatile struct pci_config 	 *config_base;/* pci config space on linc */
  volatile struct linc_misc_regs *linc_reg;   /* misc reg's */
  volatile struct linc_dma	 *dma[2];
  volatile unsigned char	 *bufmem;
  volatile long		         *mbox;
  volatile int		         *cpci_mem;
  volatile int		         *cpci_mem_swiz;
};

/* a linc card */
struct linc_card {
  int                             widget_id;
  __uint64_t                      widget_base;
  struct linc_chip linc_chip[2];
};

#endif	/* __X_TEST_H__ */

