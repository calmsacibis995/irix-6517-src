/* wd33c93_1.h - Western Digital WD33C93 SBIC (SCSI-Bus Interface Ctrl) header
*/

/* Copyright 1984-1994 Wind River Systems, Inc. */
/*
modification history
--------------------
01o,10oct94,jds  fixed for SCSI1 and SCSI2 compatability
01n,22sep92,rrr  added support for c++
01m,22jul92,wmd  added definition of WDREG.
01l,03jun92,ccc  added control register bits.
01k,26may92,rrr  the tree shuffle
01j,26may92,ajm  got rid of HOST_DEC def's (new compiler)
01i,21apr92,ccc  added sbicXferCountSet() declaration.
01h,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01g,28sep91,ajm  ifdef'd HOST_DEC for compiler problem
01f,24oct90,dnw  declarations for void routines.
01e,19oct90,jcc  changed SBIC to WD_33C93_SCSI_CTRL in ANSI function prototypes;
		 removed unused macros.
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01c,20aug90,jcc  changed UTINY to UINT8.
01b,18jul90,jcc  moved _ASMLANGUAGE conditionals to encompass entire file.
01a,27feb90,jcc  written based on old version from EDI
*/

#ifndef __INCwd33c93_1h
#define __INCwd33c93_1h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#include "scsilib.h"

typedef struct                  /* WD_33C93_SCSI_CTRL - Western Digital wd33c93
                                   SCSI controller info */
    {
    SCSI_CTRL scsiCtrl;         /* generic SCSI controller info */
    SCSI_PHYS_DEV *pDevToSelect;/* device to select at intr. level or NULL */
    int   devType;              /* type of device (see define's below) */
    FUNCPTR sbicScsiReset;	/* function to perform RESET on SCSI bus */
    UINT8 scsiStatusCode;	/* status code of last interrupt */
    UINT8 defaultSelTimeOut;	/* default dev. select time-out (units var.) */
    volatile UINT8 *pAdrsReg;	/* ptr address reg.*/
    volatile UINT8 *pAuxStatReg;/* ptr aux. status reg.*/
    volatile UINT8 *pRegFile;	/* ptr register file */
    } WD_33C93_SCSI_CTRL;

typedef struct wd33c93functbl
    {
    FUNCPTR sbicCommand;
    FUNCPTR sbicIntr;
    FUNCPTR sbicRegRead;
    FUNCPTR sbicRegWrite;
    FUNCPTR sbicXferCountGet;
    FUNCPTR sbicXferCountSet;
    FUNCPTR wd33c93CtrlInit;
    FUNCPTR wd33c93Show;
    } SCSIDRV_FUNC_TBL;

/* SBIC device type */

#define SBIC_WD33C93		0
#define SBIC_WD33C93A		1

/* SBIC command set */

#define SBIC_CMD_RESET		((UINT8) 0x00)
#define SBIC_CMD_ABORT		((UINT8) 0x01)
#define SBIC_CMD_SET_ATN	((UINT8) 0x02)
#define SBIC_CMD_NEG_ACK	((UINT8) 0x03)
#define SBIC_CMD_DISCONNECT	((UINT8) 0x04)
#define SBIC_CMD_RESELECT	((UINT8) 0x05)
#define SBIC_CMD_SEL_ATN	((UINT8) 0x06)
#define SBIC_CMD_SELECT		((UINT8) 0x07)
#define SBIC_CMD_SEL_ATN_XFER	((UINT8) 0x08)
#define SBIC_CMD_SELECT_XFER	((UINT8) 0x09)
#define SBIC_CMD_RESELECT_RECV	((UINT8) 0x0a)
#define SBIC_CMD_RESELECT_SEND	((UINT8) 0x0b)
#define SBIC_CMD_WAIT_SEL_RCV	((UINT8) 0x0c)
#define SBIC_CMD_RECV_CMD	((UINT8) 0x10)
#define SBIC_CMD_RECV_DATA	((UINT8) 0x11)
#define SBIC_CMD_RECV_MSG_OUT	((UINT8) 0x12)
#define SBIC_CMD_RECV_INFO_OUT	((UINT8) 0x13)
#define SBIC_CMD_SEND_STATUS	((UINT8) 0x14)
#define SBIC_CMD_SEND_DATA	((UINT8) 0x15)
#define SBIC_CMD_SEND_MSG_IN	((UINT8) 0x16)
#define SBIC_CMD_SEND_INFO_IN	((UINT8) 0x17)
#define SBIC_CMD_XLATE_ADDR	((UINT8) 0x18)
#define SBIC_CMD_XFER_INFO	((UINT8) 0x20)
#define SBIC_CMD_XFER_PAD	((UINT8) 0x21)
#define SBIC_CMD_SING_BYTE_XFER	((UINT8) 0x80)

/*  Status Register Codes   */

#define STAT_GROUP(arg)		((arg) & 0xf0)  /* reason for interrupt */
#define STAT_CODE(arg)		((arg) & 0x0f)  /* detail about reason */
#define STAT_XFER(arg)      	((arg) & 0x08)  /* transfer was in progress */

#define STAT_MCI_BITS		((UINT8) 0x07)	/* MSG, C/D and I/O bits */
#define STAT_PHASE_REQ  	((UINT8) 0x08)

#define STAT_GROUP_RESET	 ((UINT8) 0x00)
#define STAT_GROUP_SUCCESSFUL	 ((UINT8) 0x10)
#define STAT_GROUP_PAUSE_ABORTED ((UINT8) 0x20)
#define STAT_GROUP_TERMINATED	 ((UINT8) 0x40)
#define STAT_GROUP_SERVICE_REQ	 ((UINT8) 0x80)

#define STAT_SUCC_RESET		((UINT8) 0x00)
#define STAT_SUCC_RESELECT	((UINT8) 0x10)
#define STAT_SUCC_SELECT	((UINT8) 0x11)
#define STAT_SUCC_SEND_RECV	((UINT8) 0x13)
#define STAT_SUCC_SEND_RECV_ATN	((UINT8) 0x14)
#define STAT_SUCC_XLATE_ADRS	((UINT8) 0x15)
#define STAT_SUCC_SEL_XFER	((UINT8) 0x16)
#define STAT_SUCC_XFER_MCI	((UINT8) 0x1f)

#define STAT_PAUSE_MSG_IN	((UINT8) 0x20)
#define STAT_ABORT_SELX_SAVP    ((UINT8) 0x21) /* SEL/Xfer Aborted; Save Ptrs */
#define STAT_ABORT_SELECT	((UINT8) 0x22) /* SELECT Aborted */
#define STAT_ABORT_RESELECT	((UINT8) 0x22) /* RESELECT Aborted */
#define STAT_ABORT_RECV		((UINT8) 0x23) /* ReCeiVe Aborted; No ATN */
#define STAT_ABORT_SEND		((UINT8) 0x23) /* SeND Aborted; No ATN */
#define STAT_ABORT_RECV_ATN	((UINT8) 0x24) /* ReCeiVe Aborted; w/ATN */
#define STAT_ABORT_SEND_ATN	((UINT8) 0x24) /* SeND Aborted; w/ATN */
#define STAT_ABORT_XFER_MCI	((UINT8) 0x2f) /* Xfer Aborted; new MCI rqst */

#define STAT_TERM_INVALID_CMD	((UINT8) 0x40) /* Invalid Command */
#define STAT_TERM_UNEXP_DISCON	((UINT8) 0x41) /* Unexpected Disconnect */
#define STAT_TERM_TIMEOUT	((UINT8) 0x42) /* Selection Timeout */
#define STAT_TERM_PAR_ERR	((UINT8) 0x43) /* Parity Error; No ATN */
#define STAT_TERM_PAR_ERR_ATN	((UINT8) 0x44) /* Parity Error; w/ATN */
#define STAT_TERM_ADRS_ERR	((UINT8) 0x45) /* Address exceeds disk bounds */
#define STAT_TERM_BAD_RESEL	((UINT8) 0x46) /* Wrong target reselected */
#define STAT_TERM_BAD_BYTE	((UINT8) 0x47) /* Incorrect Message Byte rcvd */
#define STAT_TERM_UNX_PHASE  	((UINT8) 0x4f) /* Unexpected MCI (new phase) */

#define STAT_SERV_REQ_RESELECT	((UINT8) 0x80) /* Reselected */
#define STAT_SERV_REQ_SEL	((UINT8) 0x82) /* Selected; No ATN */
#define STAT_SERV_REQ_SEL_ATN	((UINT8) 0x83) /* Selected; w/ATN */
#define STAT_SERV_REQ_ATN	((UINT8) 0x84) /* ATN asserted */
#define STAT_SERV_REQ_DISCON	((UINT8) 0x85) /* Disconnect */
#define STAT_SERV_REQ_ASSERTED	((UINT8) 0x8f) /* Request -- MCI gives reason */

#define AUX_STAT_REG            (0x00)
#define ADDRESS_REG             (0x00)
#define GENERIC_REG             (0x01)

#define SBIC_REG_OWN_ID		((UINT8) 0x00)
#define SBIC_REG_CONTROL	((UINT8) 0x01)
#define SBIC_REG_TO_PERIOD	((UINT8) 0x02)
#define SBIC_REG_CDB_1		((UINT8) 0x03)
#define	SBIC_REG_CDB_2		((UINT8) 0x04)
#define	SBIC_REG_TOT_CLY_MSB	((UINT8) 0x05)
#define	SBIC_REG_TOT_CLY_LSB	((UINT8) 0x06)
#define	SBIC_REG_LOG_ADR_MSB	((UINT8) 0x07)
#define	SBIC_REG_LOG_ADR_2SB	((UINT8) 0x08)
#define	SBIC_REG_LOG_ADR_3SB	((UINT8) 0x09)
#define	SBIC_REG_LOG_ADR_LSB	((UINT8) 0x0a)
#define	SBIC_REG_SECTOR_NUM	((UINT8) 0x0b)
#define	SBIC_REG_HEAD_NUM	((UINT8) 0x0c)
#define	SBIC_REG_CYL_NUM_MSB	((UINT8) 0x0d)
#define	SBIC_REG_CYL_NUM_LSB	((UINT8) 0x0e)
#define SBIC_REG_TARGET_LUN	((UINT8) 0x0f)
#define SBIC_REG_TARGET_STAT	((UINT8) 0x0f)
#define SBIC_REG_COMMAND_PHASE	((UINT8) 0x10)
#define SBIC_REG_SYNC_XFER	((UINT8) 0x11)
#define SBIC_REG_XFER_COUNT_MSB	((UINT8) 0x12)
#define SBIC_REG_XFER_COUNT_2SB	((UINT8) 0x13)
#define SBIC_REG_XFER_COUNT_LSB	((UINT8) 0x14)
#define SBIC_REG_DEST_ID	((UINT8) 0x15)
#define SBIC_REG_SOURCE_ID	((UINT8) 0x16)
#define SBIC_REG_SCSI_STATUS	((UINT8) 0x17)
#define SBIC_REG_COMMAND	((UINT8) 0x18)
#define SBIC_REG_DATA		((UINT8) 0x19)
#define	SBIC_REG_PADa		((UINT8) 0x1a)
#define	SBIC_REG_PADb		((UINT8) 0x1b)
#define	SBIC_REG_PADc		((UINT8) 0x1c)
#define	SBIC_REG_PADd		((UINT8) 0x1d)
#define	SBIC_REG_PADe		((UINT8) 0x1e)
#define	SBIC_REG_AUX_STATUS	((UINT8) 0x1f)

/* auxiliary status register bits */

#define SBIC_AUX_STAT_INT_PEND	  ((UINT8) 0x80) /* interrupt pending bit */
#define SBIC_AUX_STAT_LCI	  ((UINT8) 0x40) /* last commmand ignored bit */
#define SBIC_AUX_STAT_BUSY	  ((UINT8) 0x20) /* busy bit */
#define SBIC_AUX_STAT_CMD_IN_PROG ((UINT8) 0x10) /* command in progress bit */
#define SBIC_AUX_STAT_PAR_ERROR	  ((UINT8) 0x02) /* parity erro bit */
#define SBIC_AUX_STAT_DBUF_READY  ((UINT8) 0x01) /* data buffer ready bit */

/* control register bits */

#define	SBIC_CTRL_HALT_SPE	((UINT8) 0x01)	/* halt on SCSI parity error */
#define	SBIC_CTRL_HALT_ATN	((UINT8) 0x02)	/* halt on attention */
#define	SBIC_CTRL_INTM_DIS	((UINT8) 0x04)	/* intermediate disconnect */
#define	SBIC_CTRL_END_DIS_INT	((UINT8) 0x08)	/* ending disconnect inter */
#define	SBIC_CTRL_HALT_HPE	((UINT8) 0x10)	/* halt on host parity error */
#define	SBIC_CTRL_DMA_POLLED	((UINT8) 0x00)	/* polled i/o; on DMA */
#define	SBIC_CTRL_DMA_BURST	((UINT8) 0x20)	/* burst mode; demand mode */
#define	SBIC_CTRL_DMA_WDBUS	((UINT8) 0x40)	/* wd bus mode */
#define	SBIC_CTRL_DMA_BYTE	((UINT8) 0x80)	/* regular DMA mode */

/* frequency select codes (wd33c93a only) */

#define SBIC_FREQ_SELECT_LOW	0x00		/*  8 - 10 MHz clock */
#define SBIC_FREQ_SELECT_MID	0x40		/* 12 - 15 MHz clock */
#define SBIC_FREQ_SELECT_HIGH	0x80		/*      16 MHz clock */

/* variable declarations */

extern SCSI_CTRL *pSysScsiCtrl;
extern SCSIDRV_FUNC_TBL *pWd33c93IfTbl;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern  void wd33c93IfTblInit ();
extern  STATUS sbicProgBytesOut (SCSI_PHYS_DEV *, UINT8 *, int, int);
extern  STATUS sbicProgBytesIn (SCSI_PHYS_DEV *, FAST UINT8 *, FAST int, int);
extern	WD_33C93_SCSI_CTRL *wd33c93CtrlCreate (UINT8 *, int, UINT, int,
	                                           FUNCPTR, FUNCPTR, FUNCPTR);

#ifndef WD33C93_1_LOCAL_FUNCS
extern	STATUS	wd33c93CtrlInit (WD_33C93_SCSI_CTRL *, int, UINT, int);
extern	STATUS	sbicShow (SCSI_CTRL *);
extern	void	sbicIntr (WD_33C93_SCSI_CTRL *);
extern	void	sbicCommand (WD_33C93_SCSI_CTRL *, UINT8);
extern	void	sbicRegWrite (WD_33C93_SCSI_CTRL *, UINT8, UINT8);
extern	STATUS	sbicXferCountSet (WD_33C93_SCSI_CTRL *, UINT);
#endif

#else

extern  void wd33c93IfTblInit ();
extern  STATUS sbicProgBytesOut ();
extern  STATUS sbicProgBytesIn ();
extern	WD_33C93_SCSI_CTRL * wd33c93CtrlCreate ();

#ifndef WD33C93_1_LOCAL_FUNCS
extern	STATUS	wd33c93CtrlInit ();
extern	STATUS	sbicShow ();
extern	void	sbicIntr ();
extern	void	sbicCommand ();
extern	void	sbicRegWrite ();
extern	STATUS	sbicXferCountSet ();
#endif

#endif	/* __STDC__ */

#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCwd33c93_1h */
