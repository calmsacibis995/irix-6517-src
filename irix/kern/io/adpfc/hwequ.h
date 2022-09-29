/***************************************************************************
*									   *
* Copyright 1997 Adaptec, Inc.,  All Rights Reserved.			   *
*									   *
* This software contains the valuable trade secrets of Adaptec.  The	   *
* software is protected under copyright laws as an unpublished work of	   *
* Adaptec.  Notice is for informational purposes only and does not imply   *
* publication.	The user of this software may make copies of the software  *
* for use with parts manufactured by Adaptec or under license from Adaptec *
* and for no other use.							   *
*									   *
***************************************************************************/

/**************************************************************************
*  file name :
*		 hwequ.h.h
*
*  edit history :
*		   6/21/95    orig. creation  ja
*
*  Notes:
*	   emerald address map (equates and defintions) are located in this
*  file. It includes all of configuration as well device space registers.
*  Most of these registers are for Sequence Use only during the normal oper-
*  ation. HIM is not required to know their state. 
*		   
*
*
*
*
****************************************************************************/

#ifndef _HWEQU_H
#define _HWEQU_H

/********* AIC-EMERALD Receive PayLoad Buffer & Manager Register Set */

#define RPB_RPG_DATA0	0xF0	    /* all regsiters in mode 2 */
#define RPB_RPG_DATA1	0xF1
#define RPB_WPG_SEL	0xF2
#define RPB_WPG_ADR	0xF3
#define RPB_RPG_SEL	0xF4
#define RPB_RPG_ADR	0xF5
#define RPB_RPG_STAT0	0xF6
#define RPB_RPG_STAT1	0xF7
#define RPB_RPG_BCNT0	0xF8
#define RPB_RPG_BCNT1	0xF9

#define RPB_PLD_SIZE	0xFA
/*	sizes possible on Emerald	*/
#define RPB_PLD_128	0
#define RPB_PLD_256	1
#define RPB_PLD_512	2
#define RPB_PLD_1024	3
#define RPB_PLD_2048	4

/*	mode 2					*/
#define RPB_BUF_CTL	0xFB
#define RPB_BUF_CTL_DEFAULT 0x80
#define RPB_CLR_BSTAT0	0xFC	  /* Write only */
#define RPB_BUF_STAT0	0xFC	  /* Read only	*/
#define RPB_BUF_STAT1	0xFD
#define RPB_RSVRD0	0xFE
#define RPB_RSVRD1	0xFF
#define RPB_CLR_BUF	0x04	/* self clearing */


/******** AIC-EMERALD Send PayLoad Buffer & Manager Register Set */

#define SPB_WPG_DATA0	0xE0	 /* registers in mode 0  */
#define SPB_WPG_DATA1	0xE1
#define SPB_WPG_SEL	0xE2
#define SPB_WPG_ADR	0xE3
#define SPB_RPG_SEL	0xE4
#define SPB_RPG_ADR	0xE5
#define SPB_WPG_STAT	0xE6
#define SPB_RPG_STAT	0xE7
#define SPB_WPG_BCNT0	0xE8
#define SPB_WPG_BCNT1	0xE9
#define SPB_BUF_STAT0	0xEA
#define SPB_BUF_STAT1	0xEB

#define HST_XRDY_CNT0	  0xEC
#define HST_XRDY_CNT1	  0xED
#define HST_XRDY_CNT2	  0xEE
#define HST_XRDY_CNT3	  0xEF

#define HST_CFG_CTL		0xF0
#define SPB_AUTO_LOAD		0x10
#define SPB_PAYLOAD_SIZE_128	0x00
#define SPB_PAYLOAD_SIZE_256	0x01
#define SPB_PAYLOAD_SIZE_512	0x02
#define SPB_PAYLOAD_SIZE_1024	0x03
#define SPB_PAYLOAD_SIZE_2048	0x04

#define SPB_BUF_CTL	  0xF0		   /* mode 1 & 5 */
#define SPB_CLR_BUFF	  0x02

#define SFC_XRDY_CNT0	  0xF4
#define SFC_XRDY_CNT1	  0xF5
#define SFC_XRDY_CNT2	  0xF6
#define SFC_XRDY_CNT3	  0xF7
#define SPB_SFC_STAT	  0xF1
#define SFC_PLD_XFR_CNT0  0xF2
#define SFC_PLD_XFR_CNT1  0xF3

/******** AIC-EMERALD Command Channel Register Set and bit definitions */

#define TCB_QSIZE    0x69		/* Mode 4 */
#define TCB_QSIZE00  0x01
#define TCB_QSIZE01  0x02
#define TCB_QSIZE02  0x04
#define TCB_QSIZE03  0x08
#define TCB_1024_QSIZE (TCB_QSIZE00 | TCB_QSIZE01 | TCB_QSIZE02)

#define MODE_PTR     0x2C
#define MODEPTR		0x2C

#define HNTCB_QOFF	0x2A
#define HNTCB_QOFF0  0x2A
#define HNTCB_QOFF1  0x2B

#define SNTCB_QOFF   0x6A		/* Mode 4 */
#define SNTCB_QOFF0  0x6A
#define SNTCB_QOFF1  0x6B

#define SDTCB_QOFF0  0x6C
#define SDTCB_QOFF1  0x6D

#define CMC_DMA_CTL	0x8E	       /* Mode 4 */
#define CC_DMAEN	0x80
#define CM_DMAEN	0x20
#define CP_DMAEN	0x10
#define CMC_DIR		0x02
#define CMC_BUFFRST	0x01

#define CMC_STATUS0  0x8C
#define CMC_CCDONE   0x80
#define CMC_CMDONE   0x40
#define CMC_CPDONE   0x20
#define CMC_DONE     0x10

#define CMC_STATUS1  0x8D
#define TCB_AVAIL    0x80
#define CRTC_SEQ_INT 0x40
#define SNTCB_ROLOVR 0x20
#define SDTCB_ROLOVR 0x10

#define CC_DMA_ADR0  0x90
#define CC_DMA_ADR1  0x91

#define CC_DMA_CNT0  0x92
#define CC_DMA_CNT1  0x93

#define CM_DMA_CNT0  0x94
#define CM_DMA_CNT1  0x95


#define CMC_HCTL     0x22
#define HPAUSETOP    0x80
#define HPAUSETOPACK 0x80
#define HPAUSE	     0x40
#define HPAUSEACK    0x40
#define IOPAGE	     0x20 
#define ACC_TOKEN    0x10	    /* semaphore (Lock) */
#define POWRDN	     0x08  
#define INTEN	     0x04
#define FC_BLKRST    0x02
#define CHIPRST      0x01
#define CHIPRSTACK   0x01
#define CMC_HCTL_RESET_MASK   0x43

#define POST_STAT0	0x24
#define ALC_INT		0x08
#define PS_HPAUSETOPACK 0x02
#define PS_HPAUSEPACK	0x01

#define POST_STAT1   0x25
#define PCI_ERR_INT  0x80
#define HW_ERR_INT   0x40
#define SW_ERR_INT   0x20
#define SEQ_INT      0x10
#define EXT_INT      0x08
#define CRTC_HST_INT 0x04
#define BRKINT	     0x02
#define SEQ_LIP_INT  0x01

#define POST_STAT1_HW_BAD (PCI_ERR_INT | HW_ERR_INT | SW_ERR_INT)
#define POST_STAT1_INTERRUPT (PCI_ERR_INT | HW_ERR_INT | SW_ERR_INT | \
BRKINT | CRTC_HST_INT | SEQ_LIP_INT)

#define IPEN0	       0x98		   /* Mode 4 */
#define ALC_INT_IPEN   0x08
#define HPAUSETOP_IPEN 0x02
#define HPAUSE_IPEN    0x01
#define IPEN0_INIT_STATE	ALC_INT_IPEN

#define IPEN1	       0x99
#define IPEN2		0x9A
#define IPEN3		0x9B
#define PCIER_INTIPEN  0x80
#define HWERR_INTIPEN  0x40
#define SW_ERR_INTIPEN 0x20
#define SEQ_INT_IPEN   0x10
#define EXT_INTIPEN    0x08
#define CRTCHST_INTIPE 0x04
#define BRKINT_IPEN    0x02
#define SEQ_LIP_INTPEN 0x01
/* when CIP_DMAEN is on, turn on
bits in IPEN1 for which you want
interrupts in both A0 and A1 */

/* When CIP_DMAEN is off, no error status
posting, turn off all bits for which
you want interrupts, except for ALC_INT and
HWERR_INT */

#define IPEN1_INIT_STATE	0xe7

#define CLRINT0      0x24
#define CLR_ALC_INT  0x08
#define CLR_INT0_ALL 0xFF

#define CLRINT1      0x25
#define CLR_PCI_ERR  0x80
#define CLR_PAR_ERR  0x40
#define CLR_SW_INT   0x20
#define CLR_SEQ_INT  0x10
#define CLR_OTHR_INT 0x04
#define CLR_BRK_INT  0x02
#define CLR_INT1_ALL 0xFF

#define CMC_HST_RTC0 0x28
#define CMC_HST_RTC1 0x29

/* in CMC_HST_RTC1	*/
#define CIP_DMAEN	0x20
/* necessary for DMA postings and IRQ will follow */

#define CRTC_HST_EN  0x80
#define CRTC_HPERIOD 0x40
#define CRTC_DMAEN   0x20

#define CMC_SEQ_RTC0  0x9C	     /* Mode 4 */

#define CMC_SEQ_RTC1  0x9D
#define CRTC_SEQ_EN   0x80
#define CRTC_SPERIOD  0x40
#define CLR_CRTC_SINT 0x20
#define CMC_SEQ_INT	 0x10
#define SW_ACT_INT    0x08
#define CMC_SEQ_LIP_INT   0x04
#define SEQ_RTC_PER09 0x02
#define SEQ_RTC_PER08 0x01

#define CMC_BADR     0x9E	    /* Mode 4 */
#define CMC_BDAT0    0x20	    /* all Modes */
#define CMC_BDAT1    0x21   

#define MODE_PTR	0x2C	   

#define SRC_CMD_MODE0	0x01
#define SRC_CMD_MODE1	0x02
#define SRC_CMD_MODE2	0x04

#define DST_CMD_MODE3	0x10
#define DST_CMD_MODE4	0x20
#define DST_CMD_MODE5	0x40

#define CMC_MODE0	0x00
#define CMC_MODE1	(SRC_CMD_MODE0 | DST_CMD_MODE3)
#define CMC_MODE2	(SRC_CMD_MODE1 | DST_CMD_MODE4)
#define CMC_MODE3	(SRC_CMD_MODE0 | SRC_CMD_MODE1 | DST_CMD_MODE3 | DST_CMD_MODE4)
#define CMC_MODE4	(SRC_CMD_MODE2 | DST_CMD_MODE5)
#define CMC_MODE5	(SRC_CMD_MODE0 | SRC_CMD_MODE2 | DST_CMD_MODE3 | DST_CMD_MODE5)
#define CMC_MODE6	0x66
#define CMC_MODE7	0x77
#define CMC_MODE_ANY	0xff

#define EVENT	     0x2D
#define MODE5_EVENT  0x20
#define MODE4_EVENT  0x10
#define MODE3_EVENT  0x08
#define MODE2_EVENT  0x04
#define MODE1_EVENT  0x02
#define MODE0_EVENT  0x01

#define ERROR	     0x23
#define CRPARERR     0x80
#define MPARERR      0x40
#define CMC_RAM_PERR 0x20
#define DRHPARERR    0x10
#define DSSPARERR    0x08
#define DRCPARERR    0x04
#define SQPARERR     0x02
#define ILLOPCODE    0x01

/******** AIC-EMERALD HostDevice Register Set */


#define HS_DMA_HADR0   0x84	  /* Mode 0 */
#define HS_DMA_HADR1   0x85
#define HS_DMA_HADR2   0x86
#define HS_DMA_HADR3   0x87
#define MODE_SEL       0x14

#define HR_DMA_HADR0   0x84	  /* Mode 2 */
#define HR_DMA_HADR1   0x85
#define HR_DMA_HADR2   0x86
#define HR_DMA_HADR3   0x87

#define CP_DMA_HADR0   0x84	   /* Mode 4 */
#define CP_DMA_HADR1   0x85
#define CP_DMA_HADR2   0x86
#define CP_DMA_HADR3   0x87

#define CIP_DMA_HADR0  0x74	   /* Mode 4 */
#define CIP_DMA_HADR1  0x75
#define CIP_DMA_HADR2  0x76
#define CIP_DMA_HADR3  0x77

#define HS_DMA_LADR0   0x80	   /* Mode 0 */
#define HS_DMA_LADR1   0x81
#define HS_DMA_LADR2   0x82
#define HS_DMA_LADR3   0x83

#define HR_DMA_LADR0   0x80	   /* Mode 2 */
#define HR_DMA_LADR1   0x81
#define HR_DMA_LADR2   0x82
#define HR_DMA_LADR3   0x83

#define CP_DMA_LADR0   0x80	  /* mode 4  */
#define CP_DMA_LADR1   0x81
#define CP_DMA_LADR2   0x82
#define CP_DMA_LADR3   0x83

#define CIP_DMA_LADR0  0x70	   /* Mode 4 */
#define CIP_DMA_LADR1  0x71
#define CIP_DMA_LADR2  0x72
#define CIP_DMA_LADR3  0x73

#define HS_DMA_CNT0    0x88	  /* Mode 0 */
#define HS_DMA_CNT1    0x89
#define HS_DMA_CNT2    0x8A

#define HS_DMA_CTL     0x8C
#define HS_STATUS      0x8D

#define HR_DMA_CNT0    0x88	  /* Mode 2 */
#define HR_DMA_CNT1    0x89
#define HR_DMA_CNT2    0x8A

#define SG_CACHEPTR    0x8B

#define HR_DMA_CTL     0x8C
#define HR_STATUS      0x8D
#define HR_RSVD0	0x8E
#define HR_RSVD1	0x8F
#define HR_RSVD2	0x90
#define HR_RSVD3	0x91
#define HR_RSVD4	0x92
#define HR_RSVD5	0x93
#define HS_RSVD0	0x8E
#define HS_RSVD1	0x8F
#define SNAP_SGL_LADR0		0x90
#define SNAP_SGL_LADR1		0x91
#define SNAP_SGL_LADR2		0x92
#define SNAP_SGL_LADR3		0x93
#define SNAP_SH_LADR0		0x94
#define SNAP_SH_LADR1		0x95
#define SNAP_SH_LADR2		0x96
#define SNAP_SH_LADR3		0x97
#define SNAP_SH_CNT0		0x98
#define SNAP_SH_CNT1		0x99
#define SNAP_SH_CNT2		0x9A
#define SNAP_BPTR		0x9B

#define CP_DMA_CNT0	0x88	   /* Mode 4 */
#define CP_DMA_CNT1	0x89	   /* Mode 4 */
#define CP_DMA_CNT2	0x8a

/* #define CIP_DMA_CNT0   reserved, not addressable Mode 4 */

#define HCOMMAND0	0x7C
#define MPARCKEN	0x80
#define DPARCKEN	0x40
#define DUAL_IRQ	0x02
#define HOLD_DMA_CH	0x01

#define HCOMMAND1			0x7D
#define CLINE_XFER			0x80
#define STOPONPERR			0x40
#define MASTER_CTL_64			0x20
#define MASTER_CTL_32			0x10
#define MASTER_CTL_DYNAMIC		0x00
#define PREQ_CTL_REQ0_ALL		0x00
#define PREQ_CTL_REQ0_HR_REQ1_HS_CMD	0x04
#define PREQ_CTL_REQ0_HR_CMD_REQ1_HS	0x08
#define PREQ_CTL_REQ0_HS_REQ2_HS_CMD	0x0c

#define SNAP_RV_LADR0		0x70
#define SNAP_RV_LADR1		0x71
#define SNAP_RV_LADR2		0x72
#define SNAP_RV_LADR3		0x73
#define SNAP_RV_CNT0		0x74
#define SNAP_RV_CNT1		0x75
#define SNAP_RV_CNT2		0x76

/* mode 1 or 5		*/
#define SNAP_RV_CACHEPTR     0x77
#define SNAP_RV_CACHE_PTR     0x77

#define SNAP_RV_SGL_LADR0	0x78
#define SNAP_RV_SGL_LADR1	0x79
#define SNAP_RV_SGL_LADR2	0x7A
#define SNAP_RV_SGL_LADR3	0x7B
#define SNAP_RSVD0		0x7C
#define SNAP_RSVD1		0x7D
#define CSDATL			0x7E
#define CSDATH			0x7F
#define SFUNCT	      0x8A
#define CP_RSVD		0x8B

/******** AIC-EMERALD Memory Port Register Set	***********/ 

#define LPDAT0		0x30
#define LPDAT1		0x31
#define LPPTR		0x32
#define GENPORT       0x33
#define PVARPTR       0x34
#define GVARDAT_START	0x38
#define SGEDAT0			0x60
#define SGEDAT1			0x61
#define SG_CACHEPTR_ACT		0x62

#define PVAR_DAT0     0x00    /* Memory address  128 bytes */	 
#define PVAR_DAT1     0x01   
#define TCB_DAT0      0x80    /* Memory address 128 bytes */ 
#define TCB_DAT1      0x81    

#define EVARPTR0		0x66
#define EVARPTR1		0x67
#define SEEDAT0			0x68
#define SEEDAT1			0x69
#define SEEADR			0x6A
#define     OP3			0x40	       /* Opcode 3			   */
#define     OP4			0x80	       /* Opcode 4			   */
#define SEECTL			0x6B
#define     START		0x01	       /* Start SEEPROM Operation (write)  */
#define     BUSY		0x01	       /* SEEPROM Busy Pending		   */
#define     OP0			0x10	       /* Opcode 0			   */
#define     OP1			0x20	       /* Opcode 1			   */
#define     OP2			0x40	       /* Opcode 2			   */

#define M135_EVARPTR0 0x62     /* Mode 1,3,5	 */
#define M135_SGEPORT1 0x63

#define TCBPTR	   0x64		/* Mode 0,1,2,3,4,5 */
#define TCBPTR0    0x64		/* Mode 0,1,2,3,4,5 */
#define TCBPTR1    0x65

#define M0_SGCACHPTR 0x62      /* Mode 0 */
#define M2_SGCACHPTR 0x62      /* Mode 2 */
#define DMATCBINDX   0x60      /* Mode 4 */
#define TRACESEL     0x61      /* Mode 4 */
#define SIZE_SELECT  0x62      /* Mode 4 */
#define PVAR_PAGE_MASK	 0x03  /* pvar page mask	    */
#define EVAR_PAGE_MASK	 0x38  /* evar page mask	    */
#define SGEL_SIZE8   0x80      /* 8 elements, 16 bytes each */
#define SGEL_SIZE16  0x40      /* 16 elements,8  bytes each */
#define EVAR_SIZE256 0x08      /* evar table size = 256     */
#define EVAR_SIZE512 0x10      /* evar table size = 512     */
#define EVAR_SIZE102 0x18      /* evar table size = 1024    */
#define PVAR_PAGE2   0x00      /* pvar page	  = 2	    */
#define PVAR_PAGE4   0x01      /* pvar pages	  = 4	    */
#define PVAR_PAGE8   0x02      /* pvar pages	  = 8	    */
#define PVAR_PAGE16  0x03      /* pvar pages	  = 16	     */

#define MVAR_DAT0	0x40	  /* Mode 0,1,2,3,4,5 32 bytes	*/
#define MVAR_DAT1	0x41

#define EVAR_DAT0    0x60      /* Mode 1,3,5 */
#define EVAR_DAT1    0x61

#define CMP_SAVE_VAL0  0xB0    /* Mode 4     */
#define CMP_SAVE_VAL1  0xB1
#define CMP_SAVE_VAL2  0xB2
#define CMP_SAVE_VAL3  0xB3

#define TOV_BASE_CNT0  0xB4    /* Mode 4     */
#define TOV_BASE_CNT1  0xB5
#define TOV_BASE_CNT2  0xB6
#define TOV_BASE_CNT3  0xB7

#define ED_TOV_VAL0   0xB8     /* Mode 4 */
#define ED_TOV_VAL1   0xB9
#define ED_TOV_VAL2   0xBA
#define ED_TOV_VAL3   0xBB

#define RA_TOV_VAL0   0xBC     /* Mode 4 */
#define RA_TOV_VAL1   0xBD
#define RA_TOV_VAL2   0xBE
#define RA_TOV_VAL3   0xBF

#define DMATCBINDX	0x60
#define TRACESEL	0x61
#define SIZESELECT	0x62

#define CLK_GEN_CTL	0x63
#define QUARTER_REF_CLK 0
#define HALF_REF_CLK	1	
#define FULL_REF_CLK	2
#define DISABLE_REF_CLK 3
#define TX_CLK_CTL_MASK 0xfc

#define TCBPTR0		0x64
#define TCBPTR1		0x65

#define CLKGEN_CTL_20_BIT	0x80
#define CLKGEN_CTL_FREQ_LOW	0x40
#define CLKGEN_CLKOUT_MASK	0x0c
#define CLKGEN_TXCLK_CTRL_MASK	0x03
#define CLKGEN_CLKOUT_SHFT	0x02



/******** AIC-EMERALD Send Frame Control Register Set ***********/ 

#define OUR_AIDA0    0x68
#define OUR_AIDA1    0x69
#define OUR_AIDA2    0x6A
#define OUR_AIDA_CFG 0x6B
#define OUR_AIDA_VALID 1

#define OUR_AIDB0    0x6C
#define OUR_AIDB1    0x6D
#define OUR_AIDB2    0x6E
#define OUR_AIDB_CFG  0x6F

#define OUR_XID0     0x80
#define OUR_XID1     0x81
#define THEIR_XID0   0x82
#define THEIR_XID1   0x83

#define THEIR_AID0   0x84
#define THEIR_AID1   0x85
#define THEIR_AID2   0x86
#define RCTL	     0x87

#define CS_CTL	     0x88
#define DELIMITER    0x89

#define F_CTL2	     0x8E

#define TYPE	     0x8F

#define F_CTL0	     0x90
#define F_CTL1	     0x91
#define DF_CTL	     0x92
#define SSEQ_ID			0x93

#define SSEQ_CNT0		0x94
#define SSEQ_CNT1		0x95
#define PARAMETER0   0x96
#define PARAMETER1   0x97

#define PARAMETER2   0x98
#define PARAMETER3   0x99
#define BB_CREDITL   0x9A
#define BB_CREDITH   0x9B

#define SEQ_CFG1     0x9C
#define SFC_RSVD0    0x9D

#define SEQ_CFG0     0xB6
#define DIS_FULL_DUPLEX  0x02
#define SEQ_CFG2     0xB7

#define LINK_CTRL_CTL	0xB8
#define AVAIL_DBUF_CNT	0xB9
#define LINK_CTRL_DATL	0xBA
#define LINK_CTRL_DATH	0xBB
#define LINK_CTRL_WAD	0xBC
#define LINK_CTRL_RAD	0xBE

#define UNACK_CNT0   0xB0
#define UNACK_CNT1   0xB1
#define UNACK_CNT2   0xB2
#define SFC_RSVD1    0x9D

#define EE_CDT_CNTL  0xB4
#define EE_CDT_CNTH  0xB5

#define LNKCTRL_CTL  0xB8
#define AVDATBUF_CNT 0xB9

#define LNKCTRL_DATL 0xBA
#define LNKCTRL_DATH 0xBB
#define LNKCTRL_WADR 0xBC
#define LNKCTRL_RADR 0xBE

#define SPC_PB_DATL  0xC0
#define SPC_PB_DATH  0xC1
#define SPC_PB_WAD   0xC2
#define SPC_PB_RAD   0xC3

#define SFC_CMD0     0xC4
#define DISABLE_ALC_CONTROL 0x08
#define REPEAT_FRAME 0x20

#define SFC_CMD1     0xC5
#define SFC_STATUS0  0xC6

#define SFC_STATUS1  0xC7

#define AVAIL_RCV_BUF  0xC8
#define OUR_BB_CDTH    0xC9
#define BB_CDT_CNTL	0xCA
#define BB_CDT_CNTH    0xCB
#define AVAIL_BB_CDTL  0xCC
#define AVAIL_BB_CDTH  0xCD

#define RRDY_SEND_CNT  0xCE

#define ORDR_CTRL	0xD0
#define SEND_OS_CONT	0x80
#define LIPf7f7		0x28
#define LIPf8f7		0x29
#define LIPf7ALPA	0x2a
#define LIPf8ALPA	0x2b
#define LIPfx		0x2c
#define ENC_CTRL	0xD1
#define IDLES		0x00

#define AL_LAT_TIME		0xD2
#define AL_LAT_TIME_DEFAULT	0x60
/* was 0xff */

#define ACK_WAIT_TIME  0xD3

#define GENERAL_CTRL   0xCF
#define AL_LAT_TIMER_EN 0x80

#define SOF0	       0xD4
#define SOF1	       0xD5
#define SOF2	       0xD6
#define SOF3	       0xD7

#define EOF_NRD0       0xD8
#define EOF_NRD1       0xD9
#define EOF_NRD2       0xDA
#define EOF_NRD3       0xDB

#define EOF_PRD0       0xDC
#define EOF_PRD1       0xDD
#define EOF_PRD2       0xDE
#define EOF_PRD3       0xDF

#define CRC_DAT0       0xE0
#define CRC_DAT1       0xE1
#define CRC_DAT2       0xE2
#define CRC_DAT3       0xE3

#define OS_DAT0        0xE4
#define OS_DAT1        0xE5
#define OS_DAT2        0xE6
#define OS_DAT3        0xE7

#define SFC_SM	       0xE8
#define FIXED_PATTERN  0xE9

#define SERDES_CTRL	0xEA
#define FAULT_INT_EN	0x20
#define LKNUSE_INT_EN	0x10
#define LOCK_REF_CLK_L	0x08
#define LRC_EN		0x04
#define SYNC_EN		0x02
#define LOOP_BACK_EN	0x01


#define SERDES_STATUS	0xEB
#define SERDES_LKNUSE	0x08
#define SERDES_FAULT	0x10
/******* ALC block registers within SFC  ******/

#define ALC_LPSM_CMD		0xA0
#define RESET_LPSM	0x80

#define ALC_LPSM_STATUS_1      0xA1
#define LIP_RCV      0x80
#define LPB_RCV      0x40
#define LPE_RCV      0x20
#define ARBF0_RCV    0x10
#define LPSM_STATE_MSK 0x0f
#define OPEN_INIT    0x09
#define LPSM_STATUS1_LIP_MASK	0xe0
#define LPSM_STATUS1_RESET	0xf0

#define ALC_LPSM_STATUS_2      0xA2
#define SYNC_SPIN    0x40
#define SYNC_CLK     0x20
#define LPSM_STATUS2_SYNC_MASK	0x60

#define ALC_LPSM_STATUS_3      0xA3
#define LPSM_ERR     0x10
#define ALC_HW_INT   0x10
#define ALC_LPSM_STATE		0x0F
#define ALC_LPSM_F7F7	0x08
#define ALC_LPSM_F8F7	0x09
#define ALC_LPSM_F7ALPS 0x0A
#define ALC_LPSM_F8ALPS 0x0B
#define ALC_LPSM_ALPDALPS 0x0C

#define ALC_SRC_PA	    0xA7 
#define ALC_AL_PA	    0x68	 /* low byte of OUR_AIDA0 */
#define AL_TIME_OUT	 0xA4	/* ALC timeout	- unit = 154ms */
				/* default = 30ms, set to c0h for lip */
#define DEFAULT_LIP_TIMEOUT 0x60

#define AL_MK_TP	0xA5
#define ALC_SOURCE_PA		0xA7
#define ALC_MISC_CNTRL		0xA8
#define ENABLE_SYNC_C_S		0x01



/******** AIC-EMERALD Receive Frame Control Register Set ***********/ 

#define DAT_MASKA   0xE8
#define DAT_MASKB   0xE9

#define DAT_MSKC   0xEA
/* bit patterns in DAT_MSKC	*/
#define DAT_MK_FC16		0x80
#define DAT_MK_FC19		0x10
#define DAT_MK_FC20		0x08
#define DAT_MK_FC21		0x04
#define DAT_MK_FC22		0x02

#define MSKC_SEQ_INIT_OFF	DAT_MK_FC16		
#define MSKC_END_SEQ_OFF	DAT_MK_FC19		
#define MSKC_LAST_SEQ_OFF	DAT_MK_FC20		
#define MSKC_FIRST_SEQ_OFF	DAT_MK_FC21		
#define MSKC_SEQ_CONTEXT_OFF	DAT_MK_FC22		
#define DAT_MSKC_DEFAULT	(MSKC_SEQ_INIT_OFF | MSKC_END_SEQ_OFF |MSKC_LAST_SEQ_OFF | \
				MSKC_FIRST_SEQ_OFF	| MSKC_SEQ_CONTEXT_OFF)
#define DAT_MSKD   0xEB 
/* bit patterns in DAT_MSKD	*/
#define DAT_MK_RESVD	0x80
#define DAT_MK_FC3	0x40	/* Disable offset present bit in F_CTL */
#define DAT_MSKD_DEFAULT	(DAT_MK_RESVD | DAT_MK_FC3)

#define ACK_MASKA   0xEC
#define ACK_MASKB   0xED
#define ACK_MASKC   0xEE
#define ACK_MASKD   0xEF 

#define DAT_STATUS     0xE6
#define ACK_STATUS     0xE7
 
#define TAG_RXIDA      0x5E
#define TAG_RXIDB      0x5F

#define DAT_HD_THRAIDA 0x90
#define DAT_HD_THRAIDB 0x91
#define DAT_HD_THRAIDC 0x92
#define DAT_HD_RSVD	0x93

#define DAT_HD_FCTLA   0x94
#define DAT_HD_FCTLB   0x95
#define DAT_HD_FCTLC   0x96
#define DAT_HD_TYPE    0x97

#define DAT_HD_DFCTL	0x98
#define DAT_HD_SEQID	0x99
#define DAT_HD_SEQCNT0	0x9A
#define DAT_HD_SEQCNT1	0x9B

#define DAT_HD_THRXIDA	0x9C
#define DAT_HD_THRXIDB	0x9D
#define DAT_HD_OURXIDA	0x9E
#define DAT_HD_OURXIDB	0x9F

#define DAT_HD_PARAMA	0xA0
#define DAT_HD_PARAMB	0xA1
#define DAT_HD_PARAMC	0xA2
#define DAT_HD_PARAMD	0xA3

#define DAT_HD_RCTL	0xA4
#define DAT_HD_DELIMT	0xA5
#define DAT_HD_EVENT	0xA6

#define DAT_EP_FCTLC	0x66
#define DAT_EP_TYPE	0x8F

#define DAT_EP_SEQCNT0	0x8C
#define DAT_EP_SEQCNT1	0x8D
#define DAT_EP_DFCTL	0x8A
#define DAT_EP_SEQID	0x8B

#define DAT_EP_THRXIDA	0x82
#define DAT_EP_THRXIDB	0x83
#define DAT_EP_OURXIDA	0x80
#define DAT_EP_OURXIDB	0x81
#define DAT_EP_THRAIDA	0x84
#define DAT_EP_THRAIDB	0x85
#define DAT_EP_THRAIDC	0x86
#define DAT_EP_RCTL	0x87
#define DAT_EP_CS_CTL	0x88
#define DAT_EP_DELIMT	0x89
#define DAT_EP_EPCTL	0x8A

#define ACK_HD_FCTLA	0xC4
#define ACK_HD_FCTLB	0xC5
#define ACK_HD_FCTLC	0xC6
#define ACK_HD_TYPE	0xC7

#define ACK_HD_SEQCNT0	0xCA
#define ACK_HD_SEQCNT1	0xCB
#define ACK_HD_DFCTL	0xC8
#define ACK_HD_SEQID	0xC9

#define ACK_HD_THRXIDA	0xCC
#define ACK_HD_THRXIDB	0xCD
#define ACK_HD_OURXIDA	0xCE
#define ACK_HD_OURXIDB	0xCF

#define ACK_HD_PARAMA	0xD0
#define ACK_HD_PARAMB	0xD1
#define ACK_HD_PARAMC	0xD2
#define ACK_HD_PARAMD	0xD3

#define ACK_HD_RCTL	0xD4
#define ACK_HD_DELIMT	0xD5
#define ACK_HD_EVENT	0xD6

#define ACK_EP_SIDA	0xE0
#define ACK_EP_SIDB	0xE1
#define ACK_EP_SIDC	0xC2
#define ACK_EP_TYPE	0xB7

#define ACK_EP_THRXIDA	0xAA
#define ACK_EP_THRXIDB	0xAB

#define ACK_EP_OURXIDA		0xA8
#define ACK_EP_OURXIDB		0xA9

#define ACK_EP_THRAIDA		0xAC
#define ACK_EP_THRAIDB		0xAD
#define ACK_EP_THRAIDC		0xAE
#define ACK_EP_EPCTL		0xB2

#define ACK_EP_DELIMT	0xB1
#define ACK_EP_EPCTL	0xB2

#define RFC_HD_INDX	0xB8
#define RFC_HD_CTL	0xB9
#define ACK_HD_CTL	0xFC

#define RFC_STATUSA	0xE0
#define RFC_LINKERR	0x08
#define RFC_STATUSB	0xE2
#define RFC_STATUSC	0xE3
#define RFC_STATUSD	0xE4
#define RFC_STATUSE    0xE5
#define DAT_MSKA		0xE8
#define DAT_MSKB		0xE9
#define DAT_MSKC		0xEA
#define DAT_MSKD		0xEB
#define ACK_MSKA		0xEC
#define ACK_MSKB		0xED
#define ACK_MSKC		0xEE
#define ACK_MSKD		0xEF
#define VC_ID_DAT0		0xF0
#define VC_ID_DAT1		0xF1

#define LESB_WD0B0		0xE8
#define LESB_WD0B1		0xE9
#define LESB_WD0B2		0xEA
#define LESB_WD0B3		0xEB
#define LESB_WD1B0		0xEC
#define LESB_WD1B1		0xED
#define LESB_WD1B2		0xEE
#define LESB_WD1B3		0xEF
#define LESB_WD2B0		0xF0
#define LESB_WD2B1		0xF1
#define LESB_WD2B2		0xF2
#define LESB_WD2B3		0xF3
#define LESB_WD3B0		0xF4
#define LESB_WD3B1		0xF5
#define LESB_WD3B2		0xF6
#define LESB_WD3B3		0xF7
#define LESB_WD4B0		0xF8
#define LESB_WD4B1		0xF9
#define LESB_WD4B2		0xFA
#define LESB_WD4B3		0xFB
#define LESB_WD5B0		0xFC
#define LESB_WD5B1		0xFD
#define LESB_WD5B2		0xFE
#define LESB_WD5B3		0xFF

#define RFC_MODE	0xBA
#define INTLPBK		0x01
#define DAT_TCB_PTR	0xBB
#define DAT_CP_RCTL	0xBC
#define DAT_CP_FCTLH	0xBD
#define DAT_CP_CSCTL	0xBE
#define ACK_HD_THRAIDA		0xC0
#define ACK_HD_THRAIDB		0xC1
#define ACK_HD_THRAIDC		0xC2
#define LSTSQ		0x80
#define LSTFM		0x40
#define HOLD		0x10
#define CLEAR		0x04
#define RFC_MODE_DEFAULT (LSTSQ | LSTFM | HOLD | CLEAR)

#define LINK_ERRST	0xE1
#define LINK_ERRSTATUS	0xE1
/* Bits in LINK_ERRST						*/
#define LOSIG		0x01
#define LOSYNTOT	0x02
#define LINKRST		0x20
#define LINKEN		0x40 /* RFC will execute elasticity buffer operations,
				loss of signal detection and loss of lock
				detection */
  

/**************** AIC-EMERALD Sequencer register set ********/

/* Note that these #defines must be maintained in sync with front.c	*/

#define SEQCTL	   0x00
#define PERRORDIS  0x80
#define ALUMOVSEL1 0x40
#define FAILDIS    0x20
#define ALUMOVSEL0 0x10
#define SAVSTKDEN  0x08
#define STEP	   0x04
#define SEQRESET   0x02
#define LOADRAM    0x01

#define SEQCTL_LOADRAM	(LOADRAM)
#define SEQCTL_RUN	( ALUMOVSEL1)
#define SEQCTL_RESET	( ALUMOVSEL1 | SEQRESET)

#define FLAGS	   0x01
#define SEQADDR    0x02      /* 16 bit register */
#define SEQADDR0   0x02      /* 8 bit register */
#define SEQADDR1   0x03      /* 8 bit register */
#define ACCUM	   0x04      /* 16 bit register */
#define ACCUM0	   0x04      /* 8 bit register */
#define ACCUM_RSVD 0x05
#define SINDEX	   0x06      /* 16 bit register */
#define SINDEX0    0x06      /* 8 bit register */
#define SINDEX1    0x07      /* 8 bit register */
#define DINDEX	   0x08      /* 16 bit register */
#define DINDEX0    0x08      /* 8 bit register */
#define DINDEX1    0x09      /* 8 bit register */
#define BRKADDR0   0x0A      /* 16 bit register */
#define BRKADDR1   0x0B      /* 16 bit register */
#define BRKDIS	   0x80
#define ALLONES    0x0C
#define ALLONES0   0x0C
#define ALLONES1   0x0D
#define ALLZEROS   0x0E
#define ALLZEROS0  0x0E
#define ALLZEROS1  0x0F
#define SINDER	   0x18       /* 16 bit register */
#define DINDER	   0x1A       /* 16 bit register */
#define FUNCTION0 0x10	
#define FUNCTIONONE0 0x10 
#define FUNCTION1 0x11	  
#define FUNCTIONONE1 0x11 
#define STACK	   0x12       /* 16 bit register */
#define STACK0	   0x12       /* 8 bit register */
#define STACK1	   0x13       /* 8 bit register */
#define SEQRAM	   0x14       /* 16 bit register */
#define SEQRAM0    0x14       /* 8 bit register */
#define SEQRAM1    0x15       /* 8 bit register */
#define TILPADDR   0x16       /* 16 bit register */
#define TILPADDR0  0x16       /* 16 bit register */
#define TILPADDR1  0x17       /* 16 bit register */
#define TILPDIS 0x80
#define TILPADDR_TRACE_MASK	0x60
#define SINDIR	   0x18
#define SINDIR0    0x18
#define SINDIR1    0x19
#define SINDIR_RSVD 0x19
#define DINDIR	   0x1a
#define DINDIR_RSVD 0x1b
#define EXFLRADR 0x1c
#define EXFLRADR0 0x1c
#define EXFLRADR1 0x1d


/**************** AIC-EMERALD Configuration Space register Set ****/
/* configuration space */

#define CONFIG_ADDRESS 0x0CF8
#define CONFIG_DATA    0x0CFC

#define VEN_DEV_ID     0x000
#define    HA_ID_HI    0x04

#define VENDORID1      0x001
#define    HA_ID_LO    0x90

#define DEVICEID0      0x002
#define    HA_PROD_HI  0x60

#define DEVICEID1      0x003
#define    HA_PROD_LO 0x11


#define COMMAND0      0x004
#define ISPACEEN      0x01
#define MSPACEEN      0x02
#define MASTEREN      0x04
#define SPCYCEN       0x08
#define MWRICEN       0x10
#define VSNOOPEN      0x20
#define PERRESPEN     0x40
#define WAITCTLEN     0x80

#define COMMAND1      0x005

#define STATUS0       0x006

#define STATUS1       0x007
#define STATUS1_DPR	0x01	/* Data Parity Reported		*/
#define STATUS1_STA	0x08	/* Signal Target Abort		*/
#define STATUS1_RTA	0x10	/* Reported Target Abort	*/
#define STATUS1_RMA	0x20	/* Reported Master Abort	*/
#define STATUS1_SERR	0x40	/* Serr asserted		*/
#define STATUS1_DPE	0x80	/* Data Parity Error		*/

#define DEVREVID      0x008
#define PROGINFC      0x009
#define SUBCLASS      0x00A
#define BASECLASS     0x00B

#define CACHESIZE     0x00C
#define LATTIME       0x00D
#define DEFAULT_LATENCY_CLOCK_COUNT	0xf0
#define HDRTYPE       0x00E

#define BIST	      0x00F
#define PCI_BIST_CAPABLE 0x80
#define START_BIST	0x40
#define MTPE_BIST_FAIL 0x01
#define RPB_BIST_FAIL 0x02
#define SPB_BIST_FAIL 0x04
#define PCI_BIST_MASK 0x0f


#define BASEA0		     0x010
#define BASEADDR00    0x010
#define BASEADDR01    0x011
#define BASEADDR02    0x012
#define BASEADDR03    0x013

#define BASEA1		     0x014
#define BASEADDR10    0x014
#define BASEADDR11    0x015
#define BASEADDR12    0x016
#define BASEADDR13    0x017

#define BASEA2		     0x018
#define BASEADDR20    0x018
#define BASEADDR21    0x019
#define BASEADDR22    0x01A
#define BASEADDR23    0x01B

#define BASEA3		     0x01c

#define SYS_VEN_ID	     0x02c
#define SUBSYSVENDID0 0x02C
#define SUBSYSVENDID1 0x02D
#define SUBSYSID0     0x02E
#define SUBSYSID1     0x02F

#define EXROMCTL	     0x030
#define EXROMCTL0     0x030
#define EXROMCTL1     0x031
#define EXROMCTL2     0x032
#define EXROMCTL3     0x033

#define INT_GNT_LAT	     0x03c
#define INTLINSEL     0x03C
#define INTPINSEL     0x03D
#define MINGNT	      0x03E
#define MAXLAT	      0x03F

#define DEVCONFIG     0x040
#define DEVSTATUS0    0x041
#define DEVSTATUS1    0x042
#define PCIERRORGEN   0x043

/* DEVCONFIG bits for initialization				*/
#define CIP_RD_DIS		0x02
#define DACEN			0x04
#define MRDCEN			0x40

#define ROMPTR0		     0x044
#define CFGBUSHOLD0   0x044
#define CFGBUSHOLD1   0x045
#define CFGBUSHOLD2   0x046
#define CFGBUSHOLD3   0x047

#define DMA_ERROR	0x048
#define DMA_ERROR0	0x048
#define DMA_ERROR1	0x049
#define DMA_ERROR2	0x04a

/* Bit definitions for DMA_ERROR0		*/
#define HS_DMA_RTA	0x02
#define HS_DMA_RMA	0x04
#define HS_DMA_DPE	0x08
#define HR_DMA_DPR	0x10
#define HR_DMA_RTA	0x20
#define HR_DMA_RMA	0x40

/* Bit definitions for DMA_ERROR1		*/
#define CP_DMA_DPR	0x01
#define CP_DMA_RTA	0x02
#define CP_DMA_RMA	0x04
#define CP_DMA_DPE	0x08
#define CIP_DMA_DPR	0x10
#define CIP_DMA_RTA	0x20
#define CIP_DMA_RMA	0x40
#define CIP_DMA_DPE	0x80

/* Bit definitions for DMA_ERROR2		*/
#define T_DPE		0x10

/*	masks for error types in 3 above bytes	*/
#define DMA_ERROR0_DPR_MASK	0x10
#define DMA_ERROR1_DPR_MASK	0x11

#define DMA_ERROR0_RTA_MASK	0x22
#define DMA_ERROR1_RTA_MASK	0x22

#define DMA_ERROR0_RMA_MASK	0x44
#define DMA_ERROR1_RMA_MASK	0x44

#define DMA_ERROR0_DPE_MASK	0x08
#define DMA_ERROR1_DPE_MASK	0x88
#define DMA_ERROR2_DPE_MASK	0x10

#define HST_LIP_CTL_HI	0x04B
#define HST_REQ_LIP	 0x01
#define HST_REQ_CLS	 0x02

#define ID_EMERALD    0x1160

/**************** LINK Codes ***************************/
#define EME_RLIP    0x00

#define EME_BA_LINK 0x80
#define EME_ABTS    0x81
#define EME_RMC     0x82

#define EME_EXT_LINK 0x20
#define EME_ABTX     0x06 
#define EME_LOGIN    0x03 
#define EME_PRLI     0x20 
#define EME_PRLO     0x20 
#define EME_LOGO     0x05 

#endif /* _HWEQU_H */
