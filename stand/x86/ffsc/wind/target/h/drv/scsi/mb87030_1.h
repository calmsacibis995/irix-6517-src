/* mb87030_1.h - Fujitsu MB87030 SPC (SCSI Protocol Controller) header */

/* Copyright 1984-1994 Wind River Systems, Inc. */
/*
modification history
--------------------
01n,08nov94,jds  renamed for SCSI1 compatability
01m,24sep92,ccc  changed spcShow() to return STATUS.
01l,22sep92,rrr  added support for c++
01k,26may92,rrr  the tree shuffle
01j,26may92,ajm  got rid of HOST_DEC def's (new compiler)
                  updated copyright
01i,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01h,28sep91,ajm  ifdef'd HOST_DEC for compiler problem
01g,19oct90,jcc  changed SPC to MB_87030_SCSI_CTRL in ANSI function prototypes.
01f,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01e,20aug90,jcc  changed UTINY and USHORT to UINT8 and UINT16, respectively.
01d,18jul90,jcc  moved _ASMLANGUAGE conditionals to encompass entire file.
01c,12jul90,jcc  renamed MPU_DATA_PARITY_XX to SPC_DATA_PARITY_XX; added
		 checks for _ASMLANGUAGE around non-assembly legal code.
01b,18jan90,jcc  created MB_87030_SCSI_CTRL struct with hook into scsiLib;
		 replaced old register address macros with pointers.
01a,08apr89,jcc  written.
*/

#ifndef __INCmb87030_1h
#define __INCmb87030_1h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#include "scsilib.h"

typedef struct			/* MB_87030_SCSI_CTRL - Fujitsu mb87030
				   SCSI controller info */
    {
    SCSI_CTRL scsiCtrl;		/* generic SCSI controller info */
    SCSI_PHYS_DEV *pDevToSelect;/* device to select at intr. level or NULL */
    UINT16 defaultSelTimeOut;	/* default dev. select time-out (units var.) */
    int spcDataParity;		/* sense of the input data parity signal */
    UINT8 selectTempReg;	/* value for the TEMP reg. during select */
    int busFreeDelay;		/* tWAIT for TCL reg. during select (00-0f) */
    volatile UINT8 *pBdidReg;	/* ptr bus device ID reg.*/
    volatile UINT8 *pSctlReg;	/* ptr control reg. */
    volatile UINT8 *pScmdReg;	/* ptr command reg. */
    volatile UINT8 *pTmodReg;	/* ptr transfer mode reg. */
    volatile UINT8 *pIntsReg;	/* ptr int. sense (R) / reset int. (W) reg. */
    volatile UINT8 *pPsnsReg;	/* ptr phase sense (R) reg. */
    volatile UINT8 *pSdgcReg;	/* ptr diagnostic control (W) reg. */
    volatile UINT8 *pSstsReg;	/* ptr status (R) reg. */
    volatile UINT8 *pSerrReg;	/* ptr error status (R) reg. */
    volatile UINT8 *pPctlReg;	/* ptr phase control reg. */
    volatile UINT8 *pMbcReg;	/* ptr modified byte counter (R) reg. */
    volatile UINT8 *pDregReg;	/* ptr data reg. */
    volatile UINT8 *pTempReg;	/* ptr temporary reg. */
    volatile UINT8 *pTchReg;	/* ptr transfer counter high byte reg. */
    volatile UINT8 *pTcmReg;	/* ptr transfer counter middle byte reg. */
    volatile UINT8 *pTclReg;	/* ptr transfer counter low byte reg. */
    volatile UINT8 *pExbfReg;	/* ptr external buffer reg. */
    } MB_87030_SCSI_CTRL;

/* SPC Parity Possibilities */

#define SPC_DATA_PARITY_LOW	0
#define SPC_DATA_PARITY_HIGH	1
#define SPC_DATA_PARITY_VALID	2

/* SPC Control Register Fields */

#define SPC_SCTL_RESET_AND_DSBL ((UINT8) 0x80)    /* reset and disable */
#define SPC_SCTL_CTRL_RESET     ((UINT8) 0x40)    /* control reset */
#define SPC_SCTL_DIAG_MODE      ((UINT8) 0x20)    /* diagnostic mode */
#define SPC_SCTL_ARBIT_ENBL     ((UINT8) 0x10)    /* arbitration enable */
#define SPC_SCTL_PARITY_ENBL    ((UINT8) 0x08)    /* parity enable */
#define SPC_SCTL_SELECT_ENBL    ((UINT8) 0x04)    /* select enable */
#define SPC_SCTL_RESELECT_ENBL  ((UINT8) 0x02)    /* reselect enable */
#define SPC_SCTL_INT_ENBL       ((UINT8) 0x01)    /* interrupt enable */

/* SPC Command Register Command Codes */

#define SPC_SCMD_CMD_MASK      ((UINT8) 0xe0) /* cmd field bit mask */
#define SPC_SCMD_BUS_RELEASE   ((UINT8) 0x00) /* bus release command code */
#define SPC_SCMD_SELECT        ((UINT8) 0x20) /* select command code */
#define SPC_SCMD_RESET_ATN     ((UINT8) 0x40) /* reset ATN command code */
#define SPC_SCMD_SET_ATN       ((UINT8) 0x60) /* set ATN command code */
#define SPC_SCMD_XFER          ((UINT8) 0x80) /* transfer command code */
#define SPC_SCMD_XFER_PAUSE    ((UINT8) 0xa0) /* transfer pause command code */
#define SPC_SCMD_RESET_ACK_REQ ((UINT8) 0xc0) /* reset ACK/REQ command code */
#define SPC_SCMD_SET_ACK_REQ   ((UINT8) 0xe0) /* set ACK/REQ command code */

/* SPC Command Register Fields */

#define SPC_SCMD_RESET_OUT      ((UINT8) 0x10) /* assert reset line */
#define SPC_SCMD_INTERCEPT_XFER ((UINT8) 0x08) /* set special transfer mode */
#define SPC_SCMD_PRG_XFER       ((UINT8) 0x04) /* program transfer mode */
#define SPC_SCMD_TERM_MODE      ((UINT8) 0x01) /* termination mode */

/* SPC Transfer Mode Register Fields */

#define SPC_TMOD_SYNC_XFER       ((UINT8) 0x80) /* synchronous transfer */
#define SPC_TMOD_MAX_OFFSET_MASK ((UINT8) 0x70) /* max. xfer offset bit mask */
#define SPC_TMOD_MIN_PERIOD_MASK ((UINT8) 0x0c) /* min. xfer period bit mask */

/* SPC Interrupt Sense & Reset Register Fields */

#define SPC_INTS_SELECTED     ((UINT8) 0x80)    /* selected interrupt */
#define SPC_INTS_RESELECTED   ((UINT8) 0x40)    /* reselected interrupt */
#define SPC_INTS_DISCONNECT   ((UINT8) 0x20)    /* disconnect interrupt */
#define SPC_INTS_COM_COMPLETE ((UINT8) 0x10)    /* command complete interrupt */
#define SPC_INTS_SERVICE_REQ  ((UINT8) 0x08)    /* service required interrupt */
#define SPC_INTS_TIMEOUT      ((UINT8) 0x04)    /* timeout interrupt */
#define SPC_INTS_HARD_ERROR   ((UINT8) 0x02)    /* hard error interrupt */
#define SPC_INTS_RESET_COND   ((UINT8) 0x01)    /* reset condition interrupt */

/* SPC Phase Sense Register Fields */

#define SPC_PSNS_REQ          ((UINT8) 0x80)    /* phase sense REQ line */
#define SPC_PSNS_ACK          ((UINT8) 0x40)    /* phase sense ACK line */
#define SPC_PSNS_ATN          ((UINT8) 0x20)    /* phase sense ATN line */
#define SPC_PSNS_SEL          ((UINT8) 0x10)    /* phase sense SEL line */
#define SPC_PSNS_BSY          ((UINT8) 0x08)    /* phase sense BSY line */
#define SPC_PSNS_MSG          ((UINT8) 0x04)    /* phase sense MSG line */
#define SPC_PSNS_C_D          ((UINT8) 0x02)    /* phase sense C/D line */
#define SPC_PSNS_I_O          ((UINT8) 0x01)    /* phase sense I/O line */
#define SPC_PSNS_PHASE_MASK   ((UINT8) 0x07)    /* SCSI bus phase mask */

/* SPC Diag Control Register Fields */

#define SPC_SDGC_DIAG_REQ     ((UINT8) 0x80)   /* diagnostic mode REQ */
#define SPC_SDGC_DIAG_ACK     ((UINT8) 0x40)   /* diagnostic mode ACK */
#define SPC_SDGC_DIAG_BSY     ((UINT8) 0x08)   /* diagnostic mode BSY */
#define SPC_SDGC_DIAG_MSG     ((UINT8) 0x04)   /* diagnostic mode MSG */
#define SPC_SDGC_DIAG_C_D     ((UINT8) 0x02)   /* diagnostic mode C/D */
#define SPC_SDGC_DIAG_I_O     ((UINT8) 0x01)   /* diagnostic mode I/O */

/* SPC Status Register Fields */

#define SPC_SSTS_CONNECTED    ((UINT8) 0xc0)   /* connected to SCSI */
#define SPC_SSTS_INITIATOR    ((UINT8) 0x80)   /* connected as initiator */
#define SPC_SSTS_TARGET       ((UINT8) 0x40)   /* connected as target */
#define SPC_SSTS_BUSY         ((UINT8) 0x20)   /* SPC busy */
#define SPC_SSTS_XFER         ((UINT8) 0x10)   /* transfer in progress */
#define SPC_SSTS_SCSI_RESET   ((UINT8) 0x08)   /* SCSI reset status */
#define SPC_SSTS_TC_0         ((UINT8) 0x04)   /* transfer count = 0 */
#define SPC_SSTS_DREG_FULL    ((UINT8) 0x02)   /* data register full */
#define SPC_SSTS_DREG_EMPTY   ((UINT8) 0x01)   /* data register empty */

/* SPC Status Register Status Codes */

#define SPC_SSTS_OPER_STAT_MASK   ((UINT8) 0xf0) /* bit mask for oper. status */
#define SPC_SSTS_NO_CONNECT_IDLE  ((UINT8) 0x00) /* idle, unconnected to SCSI */
#define SPC_SSTS_SELECT_WAIT      ((UINT8) 0x20) /* waiting for bus free */
#define SPC_SSTS_TARGET_MANUAL    ((UINT8) 0x40) /* target in manual mode */
#define SPC_SSTS_RESELECT_EXEC    ((UINT8) 0x60) /* exec'ing reselect on SCSI */
#define SPC_SSTS_TARGET_XFER      ((UINT8) 0x70) /* target executing transfer */
#define SPC_SSTS_INITIATOR_MANUAL ((UINT8) 0x80) /* init'r in manual mode */
#define SPC_SSTS_INITIATOR_WAIT   ((UINT8) 0x90) /* init'r waiting on xfer com*/
#define SPC_SSTS_SELECT_EXEC      ((UINT8) 0xa0) /* executing select on SCSI */
#define SPC_SSTS_INITIATOR_XFER   ((UINT8) 0xb0) /* init'r executing transfer */

#define SPC_SSTS_DREG_STAT_MASK ((UINT8) 0x03) /* bit mask for dreg. status */
#define SPC_SSTS_DREG_PARTIAL   ((UINT8) 0x00) /* data reg. not empty or full */
#define SPC_SSTS_DREG_UNDEF     ((UINT8) 0x03) /* data reg. state undefined */

/* SPC Error Status Register Fields */

#define SPC_SERR_TC_PARITY    ((UINT8) 0x08) /* transfer counter parity error */
#define SPC_SERR_PHASE_ERROR  ((UINT8) 0x04) /* SCSI phase error */
#define SPC_SERR_SHORT_PERIOD ((UINT8) 0x02) /* short transfer period error */
#define SPC_SERR_OFFSET_ERROR ((UINT8) 0x01) /* transfer offset error */

/* SPC Error Status Register Status Codes */

#define SPC_SERR_PAR_ERROR_MASK ((UINT8) 0xc0)/* bit mask yielding par. error */
#define SPC_SERR_NO_PAR_ERROR   ((UINT8) 0x00)/* no parity error detected */
#define SPC_SERR_PAR_ERROR_OUT  ((UINT8) 0x40)/* parity error on output */
#define SPC_SERR_PAR_ERROR_IN   ((UINT8) 0xc0)/* parity error on input */

/* SPC Phase Control Register Fields */

#define SPC_PCTL_BF_INT_ENBL  ((UINT8) 0x80)    /* bus free interrupt enable */

/* SPC Phase Control Register Options */

#define SPC_PCTL_PHASE_MASK   ((UINT8) 0x07)  /* bit mask yielding xfer phase */
#define SPC_PCTL_DATA_OUT     ((UINT8) 0x00)  /* data out phase */
#define SPC_PCTL_SELECT       ((UINT8) 0x00)  /* select phase */
#define SPC_PCTL_DATA_IN      ((UINT8) 0x01)  /* data in phase */
#define SPC_PCTL_RESELECT     ((UINT8) 0x01)  /* reselect phase */
#define SPC_PCTL_COMMAND      ((UINT8) 0x02)  /* command phase */
#define SPC_PCTL_STATUS       ((UINT8) 0x03)  /* status phase */
#define SPC_PCTL_UNUSED_0     ((UINT8) 0x04)  /* unused */
#define SPC_PCTL_UNUSED_1     ((UINT8) 0x05)  /* unused */
#define SPC_PCTL_MESS_OUT     ((UINT8) 0x06)  /* message out phase */
#define SPC_PCTL_MESS_IN      ((UINT8) 0x07)  /* message in phase */

/* SPC Bit Manipulation Mneumonics */

#define SPC_PRESERVE_BIT        (-1)    /* preserve the bit's current state */
#define SPC_PRESERVE_FIELD      (0xff)  /* preserve the bit field's state */
#define SPC_RESET_BIT           (0)     /* reset the bit */
#define SPC_SET_BIT             (1)     /* set the bit */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

IMPORT    MB_87030_SCSI_CTRL * mb87030CtrlCreate (UINT8 *spcBaseAdrs,
						  int regOffset,
						  UINT clkPeriod,
						  int spcDataParity,
						  FUNCPTR spcDmaBytesIn,
						  FUNCPTR spcDmaBytesOut);
IMPORT    STATUS       mb87030CtrlInit (MB_87030_SCSI_CTRL *pSpc,
					int scsiCtrlBusId,
					UINT defaultSelTimeOut,
					int scsiPriority);
IMPORT    STATUS       spcShow (SCSI_CTRL *pSpc);
IMPORT    void         spcIntr (MB_87030_SCSI_CTRL *pSpc);

#else

IMPORT    MB_87030_SCSI_CTRL * mb87030CtrlCreate ();
IMPORT    STATUS       mb87030CtrlInit ();
IMPORT    STATUS       spcShow ();
IMPORT    void         spcIntr ();

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCmb87030_1h */
