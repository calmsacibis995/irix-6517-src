/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_KLDIAG_H__
#define __SYS_SN_KLDIAG_H__

/*
 * NOTE:
 *	If diagval == 0x00, then component passed diags.
 *	If diagval == 0xff, the component is non-existent.
 *
 *	 For all other values of diagval, component was found but
 *	   failed diagnostics.  It is possible that diagval is
 *	   non-zero but that a component is still enabled.  In
 *	   this case, only part of a component (such as one of the
 *	   uarts on the IO6) has failed.  The diagval should be
 * 	   sufficient to determine which sub-component failed in this
 *	   case.
 */

/* Values for diagnostic modes (NB: dependent on DIP switch ordering) */

#define DIAG_MODE_NORMAL	0x00
#define DIAG_MODE_HEAVY		0x01
#define	DIAG_MODE_NONE		0x02
#define DIAG_MODE_MFG		0x03
#define DIAG_MODE_EXT_LOOP	0x04    /* Probably not dipswitch-selectable */
#define DIAG_MODE_MASK		0x0f

#define GET_DIAG_MODE(_x)	(DIAG_MODE_MASK & (_x))

/* Print extra information not needed in the field. */
#define DIAG_FLAG_VERBOSE	0x10

/*
 * The default is to stop after the first failure of a component.
 * When DIAG_FLAG_CONTINUE is set, we continue to test the device
 * after a failure has occured.
 */
#define DIAG_FLAG_CONTINUE	0x20

/*
 * The default is to check device registers for reset values.
 * When DIAG_FLAG_NORESET is set, this checking should not be
 * performed.
 */
#define DIAG_FLAG_NORESET       0x40
#define DIAG_FLAG_MASK		0xf0

#define GET_DIAG_FLAGS(_x)	(DIAG_FLAG_MASK & (_x))

/*
 * Values for using the DIP switches to stop the boot process after
 * various phases and jump to POD mode.
 */

#define DIAG_STOP_NONE		0x00	/* Load io6prom */
#define DIAG_STOP_LOCALPOD	0x01	/* Local masters go to POD */
#define DIAG_STOP_GLOBALPOD	0x02	/* Global master goes to POD */
#define DIAG_STOP_DEXPOD	0x03	/* All CPUs go to DEX POD */

#ifdef _LANGUAGE_C

/*
 * diagval_t contains the structure definitions for the
 * diagval string table.
 */

typedef struct {
    unsigned char dv_code;
    unsigned char dv_parmmask;
    unsigned char dv_fru;
    char *dv_msg;
} diagval_t;

extern diagval_t diagval_map[];

#endif

/* --------------brought from klconfig.h ---------------*/

/*
 * Standard diagnostic return values - all others indicate errors.
 */
#define KLDIAG_PASSED	0x0
#define KLDIAG_TBD	0xfd
#define KLDIAG_NOT_SET	0xfe
#define KLDIAG_NOTFOUND	0xff

#define KLINV_DISABLED	1

#define CPUST_NORESP	((unchar) 0xff)

/* --------------brought from klconfig.h ---------------*/

/*
 * Diag_parm return values  
 */
#define DIAG_PARM_TBD           0x0


/*
 * Standard diagnostic return values - all others indicate errors.
 */
#define KLDIAG_PASSED		0x0
#define KLDIAG_TBD		0xfd
#define KLDIAG_NOT_SET		0xfe
#define KLDIAG_NOTFOUND		0xff
#define KLDIAG_NOT_PRESENT	0xff

/*
 * These are failure codes for the various pod_* tests.  All pod tests
 * return zero on success and one of these codes on failure.
 */

/* L1 and L2 Cache error codes */

#define KLDIAG_DCACHE_DATA_WAY0	1	/* Failed dcache1 data test way0 */
#define KLDIAG_DCACHE_DATA_WAY1	2	/* Failed dcache1 data test way1 */
#define KLDIAG_DCACHE_ADDR_WAY0	3	/* Failed dcache1 address test way0 */
#define KLDIAG_DCACHE_ADDR_WAY1	4	/* Failed dcache1 address test way1 */
#define KLDIAG_SCACHE_DATA_WAY0	5	/* Failed scache1 data test way0 */
#define KLDIAG_SCACHE_DATA_WAY1	6	/* Failed scache1 data test way1 */
#define KLDIAG_SCACHE_ADDR_WAY0	7	/* Failed scache1 address test way0 */
#define KLDIAG_SCACHE_ADDR_WAY1	8	/* Failed scache1 address test way1 */
#define KLDIAG_ICACHE_DATA_WAY0	9	/* Failed icache data test way0 */
#define KLDIAG_ICACHE_DATA_WAY1	10	/* Failed icache data test way1 */
#define KLDIAG_ICACHE_ADDR_WAY0	11	/* Failed icache address test way0 */
#define KLDIAG_ICACHE_ADDR_WAY1	12	/* Failed icache address test way1 */
#define KLDIAG_DCACHE_HANG	13	/* Dcache test hung */
#define KLDIAG_SCACHE_HANG	14	/* Scache test hung */
#define KLDIAG_ICACHE_HANG	15	/* Icache test hung */
#define KLDIAG_CACHE_INIT_HANG	16	/* Cache initialization */
#define	KLDIAG_DCACHE_TAG	17	/* dcache tag ram test */
#define KLDIAG_SCACHE_TAG_WAY0	18	/* scache tag ram test way0 */
#define KLDIAG_SCACHE_TAG_WAY1	19	/* scache tag ram test way1 */
#define KLDIAG_SCACHE_FTAG	20	/* FPU scache tag ram test */
#define KLDIAG_TESTING_DCACHE	21	/* Starting dcache test. See above. */
#define KLDIAG_TESTING_ICACHE	22	/* Starting icache test.  See above */
#define KLDIAG_TESTING_SCACHE	23	/* Same for scache. */
#define KLDIAG_INITING_CACHES	24	/* Invalidate I & D caches */
#define KLDIAG_INITING_SCACHE	25	/* Invalidate I & D caches */
#define	KLDIAG_DCACHE_FAIL	26	/* Generic D-cache test failure */
#define	KLDIAG_ICACHE_FAIL	27	/* Generic I-cache test failure */

/* CPU error codes */
#define KLDIAG_OTHER_CPU_DEAD   30      /* Other CPU died on this node */
#define	KLDIAG_CPU_DISABLED	31	/* CPU disabled */
#define	KLDIAG_EARLYINIT_FAILED	32	/* failed in earlyinit */
#define	KLDIAG_UNEXPEC_EXCP	33	/* One of the CPUs took an unxecpected
					   exception */
#define	KLDIAG_DEAD_COP1	34	/* Dead Coproc1 */
#define	KLDIAG_LOCAL_ARB	35	/* Died on/before local arb */
#define	KLDIAG_MODEBIT		36	/* CPU's and PROM's modebits mismatch */

/* Memory error codes */
#define KLDIAG_PROM_CHKSUM_BAD	40  	/* Checksum in PROM is bad */
#define KLDIAG_PROM_MEMCPY_BAD	41	/* PROM copied to memory is bad */
#define KLDIAG_PROM_CPY_FAILED	42	/* PROM copy to memory failed */
#define KLDIAG_MEMTEST_FAILED   43      /* Failed Memtest for some banks */
#define KLDIAG_DIRTEST_FAILED   44      /* Failed Dirtest for some banks */
#define KLDIAG_MEMORY_DISABLED  45      /* Memory disabled by user */
#define KLDIAG_PROM_BAD_BANK0   46      /* Unuseable bank 0 */
#define KLDIAG_IP27_256MB_BANK  47      /* Unsupported 256MB bank on IP27 */ 

/* IO6 error codes */
#define KLDIAG_XBOW_FAIL           50  /* XBOW sanity on IO6 failed */
#define KLDIAG_BRIDGE_FAIL         51  /* Bridge sanity on IO6 failed */
#define KLDIAG_IOC3_BASE_FAIL      52  /* IOC3 base address not found */
#define KLDIAG_PCICONFIG_FAIL      53  /* Config space on IO6 failed. */
#define KLDIAG_PCIBUS_FAIL         54  /* PCI bus test failed. */
#define KLDIAG_SER_PIO_FAIL1A      55  /* Chars already exist in port A. */
#define KLDIAG_SER_PIO_FAIL1B      56  /* Chars already exist in port B. */
#define KLDIAG_SER_PIO_FAIL2A      57  /* Unable to transmit chars on A. */
#define KLDIAG_SER_PIO_FAIL2B      58  /* Unable to transmit chars on B. */
#define KLDIAG_SER_PIO_FAILA       59  /* Miscompares on A. */
#define KLDIAG_SER_PIO_FAILB       60  /* Miscompares on B. */
#define KLDIAG_SER_DMA_FAILA       61  /* Miscompares on A in dma mode. */
#define KLDIAG_SER_DMA_FAILB       62  /* Miscompares on B in dma mode. */
#define KLDIAG_SCSI_MBOX_FAIL      63  /* SCSI mailbox failure. */
#define KLDIAG_SCSI_SSRAM_FAIL1    64  /* Unable to load words into the ssram.*/
#define KLDIAG_SCSI_SSRAM_FAIL2    65  /* Unable to read words out of ssram. */
#define KLDIAG_SCSI_SSRAM_FAIL     66  /* Miscomapres on the scsi ssram. */
#define KLDIAG_SCSI_DMA_FAIL1      67  /* Unable to Load words into ssram. */
#define KLDIAG_SCSI_DMA_FAIL2      68  /* Unable to dump words from ssram. */
#define KLDIAG_SCSI_DMA_FAIL       69  /* DMA Miscompares from ssram. */
#define KLDIAG_SCSI_STEST_FAIL1    70  /* Unable to load selftest firmware */
#define KLDIAG_SCSI_STEST_FAIL2    71  /* Unable to execute scsi self test */
#define KLDIAG_SCSI_STEST_FAIL     72  /* Self test failed with miscompares */

#define KLDIAG_XBOW_EXCEPTION      80
#define KLDIAG_BRIDGE_EXCEPTION    81
#define KLDIAG_IO6CONFIG_EXCEPTION 82
#define KLDIAG_PCIBUS_EXCEPTION    83
#define KLDIAG_SERPIO_EXCEPTION    84
#define KLDIAG_SERDMA_EXCEPTION    85
#define KLDIAG_SCSIRAM_EXCEPTION   86
#define KLDIAG_SCSIDMA_EXCEPTION   87
#define KLDIAG_SCSICNTRL_EXCEPTION 88

#define KLDIAG_ENET_EXCEPTION      101 
#define KLDIAG_ENET_SSRAM_FAIL     102 
#define KLDIAG_ENET_PHYREG_FAIL    103 
#define KLDIAG_ENET_PHY_R_BUSY_TO  104 
#define KLDIAG_ENET_PHY_W_BUSY_TO  105 
#define KLDIAG_ENET_PHY_RESET_TO   106 
#define KLDIAG_ENET_NO_TXCLK       107 
#define KLDIAG_ENET_LOOP_ILL_PARM  108 
#define KLDIAG_ENET_LOOP_MALLOC    109 
#define KLDIAG_ENET_EMCR_RST_TO    110 
#define KLDIAG_ENET_EMCR_RXEN_TO   111 
#define KLDIAG_ENET_LOOP_AN_TO     112 
#define KLDIAG_ENET_LOOP_AN_ABLE   113 
#define KLDIAG_ENET_LOOP_AN_MODE   114 
#define KLDIAG_ENET_LOOP_DMA_TO1   115 
#define KLDIAG_ENET_LOOP_DMA_TO2   116 
#define KLDIAG_ENET_LOOP_DMA_TO3   117 
#define KLDIAG_ENET_LOOP_BAD_RUPT1 118 
#define KLDIAG_ENET_LOOP_BAD_RUPT2 119 
#define KLDIAG_ENET_LOOP_BAD_RUPT3 120 
#define KLDIAG_ENET_LOOP_BAD_RUPT4 121 
#define KLDIAG_ENET_LOOP_NO_RUPT   122 
#define KLDIAG_ENET_LOOP_BAD_W1    123 
#define KLDIAG_ENET_LOOP_BAD_STAT  124 
#define KLDIAG_ENET_LOOP_BAD_DATA1 125 
#define KLDIAG_ENET_LOOP_BAD_DATA2 126 
#define KLDIAG_ENET_NIC_FAIL       127 



/* Miscellaneous Hub error codes */
#define KLDIAG_NI_RCVD_LNK_ERR	150	/* Hub NI saw too many link errors */
#define KLDIAG_NI_RCVD_MISC_ERR 151	/* Hub NI saw misc error from rtr */
#define KLDIAG_NI_FAIL_ERR_DET 	152	/* Hub NI did not detect forced errs */
#define KLDIAG_HUB_FAIL_LBIST 	153	/* Hub chip failed lbist test */ 
#define KLDIAG_HUB_FAIL_ABIST 	154	/* Hub chip failed abist test */ 
#define KLDIAG_HUB_LLP_DOWN 	155	/* Hub chip's LLP interface is down */
#define KLDIAG_DISCOVERY_FAILED 156	/* Hub could not launch discovery */ 
#define KLDIAG_HUB_INTS_FAILED 	157	/* Hub chip's interrupt test failed */  
#define KLDIAG_NI_DIED		158	/* Detected an NI error in the kernel */
#define KLDIAG_HUB_BTE_FAILED	159	/* Hub chip's bte test failed */
#define KLDIAG_HUB_BTE_HUNG	160	/* Hub chip's bte hung during diags */
#define KLDIAG_RTC_ERROR        161     /* Hub real time clock error */
#define KLDIAG_RESET_WAIT       162     /* Reset propagation error */


/* Router error codes */
#define KLDIAG_NI_RTR_TIMEOUT 	200	/* Hub got no response from rtr */	
#define KLDIAG_NI_RTR_BAD_RPLY	201	/* Hub got overrun/mismatch rply */	
#define KLDIAG_NI_RTR_HW_ERROR	202	/* Hub got hw err accessing rtr */	
#define KLDIAG_NI_RTR_PROT_ERR	203	/* Hub got protection err from rtr*/	
#define KLDIAG_NI_RTR_INV_VEC	204	/* Hub got invalid vec err from rtr*/	
#define KLDIAG_RTR_FAIL_ERR_DET	205	/* Rtr did not detect forced errors */ 
#define KLDIAG_RTR_RCVD_LNK_ERR	206	/* Rtr saw too many link errors */
#define KLDIAG_RTR_FAIL_LBIST	207	/* Rtr failed lbist test */ 
#define KLDIAG_RTR_FAIL_ABIST	208	/* Rtr failed abist test */

#define KLDIAG_FRU_LOCAL	209	/* FRU analysis on the local node*/
#define KLDIAG_FRU_ALL		210	/* FRU analysis on all nodes */

#define	KLDIAG_RTC_DIST		211	/* Failed RTC distribution */

/* Generic error codes */

#define KLDIAG_NOCPU		240	/* We've got no CPU. */
#define KLDIAG_RETURNING	241	/* Returning to POD mode. */
#define KLDIAG_EXC_GENERAL	242	/* PROM panic. */
#define KLDIAG_EXC_ECC		243	/* PROM panic. */
#define KLDIAG_EXC_TLB		244	/* PROM panic. */
#define KLDIAG_EXC_XTLB		245	/* PROM panic. */
#define KLDIAG_EXC_UNIMPL	246	/* PROM panic. */
#define KLDIAG_EXC_CACHE	247	/* PROM panic. */
#define KLDIAG_NMI		248	/* Got an NMI */
#define KLDIAG_NMIPROM		249	/* Got an NMI */
#define KLDIAG_NMIBADMEM	250	/* Got an NMI */
#define KLDIAG_NMICORRUPT	251	/* Got an NMI */
#define KLDIAG_NMISECOND	252	/* Got an NMI */
#define KLDIAG_DEBUG		253	/* Going to POD mode at user request */

/* MEM_DISABLE reason */
#define	MEM_DISABLE_AUTO	1
#define	MEM_DISABLE_USR		2
#define MEM_DISABLE_FRU		3	/* Disabled by FRU */
#define MEM_DISABLE_256MB	4	/* Disabled 256MB bank on IP27 board */

/* SCSI_FAIL codes */
#define	SCSI_MEM_FAIL_MASK	(1 << 0)
#define	SCSI_DMA_FAIL_MASK	(1 << 1)
#define	SCSI_STEST_FAIL_MASK	(1 << 2)

#define NUM_SCSI_FAIL_CODES     3

#ifdef _LANGUAGE_C
/* Diag function prototypes */

extern int diag_xbowSanity(int diag_mode,__psunsigned_t xbow_baseAddr);
extern int diag_bridge_config_check(int diag_mode,__psunsigned_t bridge_baseAddr);
extern int diag_pcibus_sanity(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
extern int diag_serial_pio(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
extern int diag_serial_dma(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
extern int diag_scsi_mem(int diag_mode,__psunsigned_t bridge_baseAddr,int npci);
extern int diag_scsi_dma(int diag_mode,__psunsigned_t bridge_base,int npci);
extern int diag_scsi_selftest(int diag_mode,__psunsigned_t bridge_base,int npci);
extern int diag_bridgeSanity(int diag_mode, __psunsigned_t bridge_baseAddr);
extern int diag_io6confSpace_sanity(int diag_mode, __psunsigned_t bridge_baseAddr);
#endif	/* _LANGUAGE_C */

#endif /* __SYS_SN_KLDIAG_H__ */
