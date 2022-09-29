/* ncr5390_1.h - NCR 53C90 Advanced SCSI Controller header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,08nov94,jds  renamed for SCSI1 compatability
01g,22sep92,rrr  added support for c++
01f,27aug92,ccc  added function prototypes.
01e,26may92,rrr  the tree shuffle
01d,26may92,ajm  got rid of HOST_DEC def's (new compiler)
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,28sep91,ajm  ifdef'd HOST_DEC for compiler problem
01a,16may90,trl   written
*/

#ifndef __INCncr5390_1h
#define __INCncr5390_1h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#include "scsilib.h"

/* SCSI controller structure */

typedef struct                  /* NCR_5390_SCSI_CTRL - NCR 5390
                                   SCSI controller info */
    {
    SCSI_CTRL scsiCtrl;         /* generic SCSI controller info */
    SCSI_PHYS_DEV *pDevToSelect;/* device to select at intr. level or NULL */
    int   devType;		/* type of device (see define's below) */
    TBOOL slowCableMode;	/* TRUE to select slow cable mode */
    TBOOL resetReportDsbl;	/* TRUE to disable SCSI bus reset reporting */
    TBOOL parityTestMode;	/* TRUE to enable par test mode (DO NOT USE!) */
    TBOOL parityCheckEnbl;	/* TRUE to enable parity checking */
    UINT8 defaultSelTimeOut;    /* default dev. select time-out (units var.) */
    UINT8 clkCvtFactor;    	/* value of the clock conversion factor */
    UINT8 savedStatReg;       	/* status register from last interrupt */
    UINT8 savedIntrReg;       	/* interrupt register from last interrupt */
    UINT8 savedStepReg;       	/* sequence step register from last interrupt */
    volatile UINT8 *pTclReg;	/* ptr xfer count LSB reg */
    volatile UINT8 *pTchReg;	/* ptr xfer count MSB reg */
    volatile UINT8 *pFifoReg;	/* ptr FIFO reg */
    volatile UINT8 *pCmdReg;	/* ptr command reg */
    volatile UINT8 *pStatReg;	/* ptr status reg */
    volatile UINT8 *pIntrReg;	/* ptr interrupt reg */
    volatile UINT8 *pStepReg;	/* ptr sequence step reg */
    volatile UINT8 *pFlgsReg;	/* ptr FIFO flags reg */
    volatile UINT8 *pCfg1Reg;	/* ptr configuration 1 reg */
    volatile UINT8 *pClkReg;	/* ptr clock conversion factor reg */
    volatile UINT8 *pTestReg;	/* ptr test mode reg */
    volatile UINT8 *pCfg2Reg;	/* ptr configuration 2 reg */
    } NCR_5390_SCSI_CTRL;

#if FALSE

struct scsi
    {
    int tcl;                 /* transfer count register, low byte */
    int tch;                 /* transfer count register, high byte */
    int fifo;                /* fifo register */
    int cmd;                 /* command register */
    int sts_bid;             /* status and bus-id register */
    int intsts_tmo;          /* interrupt status and timeout register */
    int step_period;         /* step and period register */
    int fifoflag_offset;     /* fifo flag and offset register */
    int conf1;               /* configuration register 1 */
    int clock;               /* clock conversion factor register */
    int test;                /* test register */
    int conf2;               /* configuration register 2 */
    };

/* defines for the overlapping registers */

#define int_sts           intsts_tmo          /* interrupt status */
#define tmo               intsts_tmo          /* select/reselect timeout */
#define step              step_period         /* sequence step */
#define period            step_period         /* sync period */
#define fifo_flags        fifoflag_offset     /* fifo flags */
#define offset            fifoflag_offset     /* sync offset */

#endif	/* FALSE */

/* defines for the overlapping registers */

#define pBidReg           pStatReg		/* select/reselect bus id */
#define pTmoReg           pIntrReg		/* select/reselect bus id */

/* ASC device types */

#define ASC_NCR5390		0
#define ASC_NCR5390A		1
#define ASC_NCR5390B		2
#define ASC_NCR5394		3
#define ASC_NCR5395		4
#define ASC_NCR5396		5

/* FIFO register */

#define NCR5390_FIFO_DEPTH       16

/* command register */

#define NCR5390_NOP              0x00
#define NCR5390_FIFO_FLUSH       0x01
#define NCR5390_CHIP_RESET       0x02
#define NCR5390_BUS_RESET        0x03

#define NCR5390_INFO_TRANSFER    0x10
#define NCR5390_I_CMD_COMPLETE   0x11
#define NCR5390_MSG_ACCEPTED     0x12
#define NCR5390_SET_ATTENTION    0x13

#define NCR5390_SEND_MESSAGE     0x20
#define NCR5390_SEND_STATUS      0x21
#define NCR5390_SEND_DATA        0x22
#define NCR5390_DISCONNECT_SEQ   0x23
#define NCR5390_TERMINATE_SEQ    0x24
#define NCR5390_T_CMD_COMPLETE   0x25
#define NCR5390_DISCONNECT       0x27
#define NCR5390_RCV_MESSAGE      0x28
#define NCR5390_RCV_COMMAND      0x29
#define NCR5390_RCV_DATA         0x2a
#define NCR5390_RCV_CMD_SEQ      0x2b

#define NCR5390_RESELECT         0x40
#define NCR5390_SELECT           0x41
#define NCR5390_ATN_SELECT       0x42
#define NCR5390_STOP_SELECT      0x43
#define NCR5390_SELECTION_ENBL   0x44
#define NCR5390_SELECTION_DSBL   0x45

#define NCR5390_DMA_OP           0x80

/* status register */

#define NCR5390_DOUT_PHASE       0x00
#define NCR5390_DIN_PHASE        0x01
#define NCR5390_CMND_PHASE       0x02
#define NCR5390_STAT_PHASE       0x03
#define NCR5390_MSGOUT_PHASE     0x06
#define NCR5390_MSGIN_PHASE      0x07
#define NCR5390_PHASE_MASK       0x07
#define NCR5390_VAL_GROUP        0x08
#define NCR5390_TERMINAL_CNT     0x10
#define NCR5390_PARITY_ERR       0x20
#define NCR5390_GROSS_ERR        0x40
#define NCR5390_INTERRUPT        0x80

/* interrupt status register */

#define NCR5390_SELECTED          0x01
#define NCR5390_ATN_SELECTED      0x02
#define NCR5390_RESELECTED        0x04
#define NCR5390_FUNC_COMPLETE     0x08
#define NCR5390_BUS_SERVICE       0x10
#define NCR5390_DISCONNECTED      0x20
#define NCR5390_ILLEGAL_CMD       0x40
#define NCR5390_SCSI_RESET        0x80

/* fifo flags register */

#define NCR5390_MORE_DATA         0x1f

/* configuration register 1 */

#define NCR5390_OWN_ID_MASK       0x07
#define NCR5390_CHIPTEST_ENBL     0x08
#define NCR5390_PAR_CHECK_ENBL    0x10
#define NCR5390_PAR_TEST_ENBL     0x20
#define NCR5390_RESET_REP_DSBL    0x40
#define NCR5390_SLOW_CABLE        0x80

/* test register */

#define NCR5390_TARGET	 	  0x01
#define NCR5390_INITIATOR         0x02
#define NCR5390_HIGH_IMP          0x04

/* configuration register 2 */

#define NCR5390_DMA_PAR_ENBL      0x01
#define NCR5390_REG_PAR_ENBL      0x02
#define NCR5390_PARITY_ABORT      0x04
#define NCR5390_SCSI_2            0x08
#define NCR5390_DREQ_HIGH_IMP     0x10

/* external declarations */

#if defined(__STDC__) || defined(__cplusplus)

IMPORT	NCR_5390_SCSI_CTRL * ncr5390CtrlCreate (UINT8 *baseAdrs, int regOffset,
						UINT clkPeriod,
						FUNCPTR ascDmaBytesIn,
						FUNCPTR ascDmaBytesOut);
IMPORT	STATUS	ncr5390CtrlInit (NCR_5390_SCSI_CTRL *pAsc, int scsiCtrlBusId,
				 UINT defaultSelTimeOut, int scsiPriority);
IMPORT	void	ascShow (NCR_5390_SCSI_CTRL *pAsc);
IMPORT	void	ascIntr (NCR_5390_SCSI_CTRL *pAsc);
IMPORT  void    ascCommand (NCR_5390_SCSI_CTRL *pAsc, int cmdCode);
IMPORT	STATUS	ascProgBytesIn (SCSI_PHYS_DEV *pScsiPhysDev, UINT8 *pBuffer,
				int bufLength, int scsiPhase);
IMPORT	STATUS	ascProgBytesOut (SCSI_PHYS_DEV *pScsiPhysDev, UINT8 *pBuffer,
				 int bufLength, int scsiPhase);
IMPORT	STATUS	ascXferCountSet (NCR_5390_SCSI_CTRL *pAsc, int count);

#else	/* __STDC__ */

IMPORT	NCR_5390_SCSI_CTRL * ncr5390CtrlCreate ();
IMPORT	STATUS  ncr5390CtrlInit ();
IMPORT	void    ascShow ();
IMPORT	void    ascIntr ();
IMPORT	void	ascCommand ();
IMPORT	STATUS	ascProgBytesIn ();
IMPORT	STATUS	ascProgBytesOut ();
IMPORT	STATUS	ascXferCountSet ();

#endif	/* __STDC__ */
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCncr5390_1h */
