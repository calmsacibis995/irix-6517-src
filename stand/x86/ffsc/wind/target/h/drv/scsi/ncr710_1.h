/* ncr710_1.h - NCR 710 Script SCSI Controller header file */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
02m,08nov94,jds  renamed for SCSI1 compatability
02l,26sep92,ccc  ncr710Show() returns STATUS.
02k,22sep92,rrr  added support for c++
02j,26jul92,rrr  removed decl ncr710SyncMsgConvert, was made LOCAL in mod 02i
02i,21jul92,eve  clean an move debug macros to ncr710Lib.c.
01h,03jul92,eve  merge header with new driver.
01h,03jul92,eve  merge header with new driver.
01g,29jun92,ccc  fixed compile errors.
01f,26jun92,ccc  changed ASMLANGUAGE to _ASMLANGUAGE.
40c,26may92,rrr  the tree shuffle
40b,28apr92,wmd  Added defines for LITTLE_ENDIAN architectures, and fixed
		 typo for declaration of ncr710CtrlCreate(), ansified.
40a,12nov91,ccc  SPECIAL VERSION FOR 5.0.2 68040 RELEASE.
02a,26oct91,eve  Add description of hardware dependant
		 registers.
01a,23oct91,eve  Created driver header
*/

#ifndef __INCncr710_1h
#define __INCncr710_1h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE

#include "semlib.h"
#include "scsilib.h"
#include "ncr710script.h"

/* Structure used in the ncr710SetHwRegister() and ncr710GetHwRegister().
 * This is used to try to handle the possible differents hardware
 * implementation of the chip.This structure must contain the logical value
 * wishes e.g 0 sets register bit to null  and 1 sets to one.
 */

typedef struct
    {
    int ctest4Bit7;             /* Host bus multiplex mode */
    int ctest7Bit7;             /* Disable/enable burst cache capability */
    int ctest7Bit6;             /* Snoop control bit1 */
    int ctest7Bit5;             /* Snoop control bit0 */
    int ctest7Bit1;             /* invert tt1 pin (sync bus host mode only) */
    int ctest7Bit0;             /* enable differential scsi bus capability */
    int ctest8Bit0;             /* Set snoop pins mode */
    int dmodeBit7;              /* Burst Length transfer bit 1 */
    int dmodeBit6;              /* Burst Length transfer bit 0 */
    int dmodeBit5;              /* Function code bit FC2 */
    int dmodeBit4;              /* Function code bit FC1 */
    int dmodeBit3;              /* Program data bit ( FC0) */
    int dmodeBit1;              /* user  programmable transfer type */
    int dcntlBit5;              /* Enable Ack pin */
    int dcntlBit1;              /* Enable fast arbitration on host port */
    } NCR710_HW_REGS;

 /* Allocate one context per device ( 8 ID max and 8 LUN max per ID) */
typedef struct
    {
    NCR_CTL ncrCtl[SCSI_MAX_BUS_ID + 1][SCSI_MAX_LUN + 1];
    } NCR_CTL_CTXT;

/* SCSI controller structure */

typedef struct                  /* NCR_710_SCSI_CTRL - NCR710 */
                                /* SCSI controller info       */
    {
    SCSI_CTRL scsiCtrl;         /* generic SCSI controller info */
    SEM_ID pMutexData;          /* use to protect global siop data */
    SCSI_PHYS_DEV *pDevToSelect;/* device to select at intr. level or NULL */

				/* Hardware implementation dependencies */
    TBOOL slowCableMode;	/* TRUE to select slow cable mode */
    int chipType;		/* Device type NCR7X0_TYPE */
				/* Only 710 Supported today */
    int   devType;		/* type of device (see define's below) */
    UINT8 ctrlIdPow2;           /* controller id in power of 2 (1000000=id 7) */
    TBOOL resetReportDsbl;	/* TRUE to disable SCSI bus reset reporting */
    TBOOL parityTestMode;	/* TRUE enable parity test mode (DO NOT USE) */
    TBOOL parityCheckEnbl;	/* TRUE to enable parity checking */
    UINT8 defaultSelTimeOut;    /* default dev. select time-out (units var.) */
    UINT8 clkCvtFactor;    	/* value of the clock conversion factor */
    UINT8 saveIstat;		/* Save Reg under interrupt */
    UINT8 saveSstat0;		/* Save Reg under interrupt */
    UINT8 saveDstat;		/* Save Reg under interrupt */
    UINT8 saveIdentMsg;         /* Save Indent message in at interrupt level */
				/* when reconnect. */
    TBOOL commandRequest;       /* To check if we are reconnected with a */
				/* request pending */
    UINT  saveScriptIntrStat;   /* Save intr script status under interrupt */
    SEMAPHORE singleStepSem;	/* use to debug script in single step mode */
    NCR_CTL_CTXT *pNcrCtxt;     /* pointer to Array of data shared by script */
				/* and driver */
    NCR_CTL *pNcrCtl;           /* Current context pointer for intr level */
    NCR_CTL *pNcrCtlCmd;        /* current ctxt pointer to the command */
				/* pending */
    NCR710_HW_REGS hwRegs;	/* values used for hardware dependant regs */
    volatile UINT8    *pSien;        /* SIEN SCSI interrupt enable reg */
    volatile UINT8    *pSdid;        /* SDID SCSI destination ID register */
    volatile UINT8    *pScntl1;      /* SCTNL1 SCSI control register 1 */
    volatile UINT8    *pScntl0;      /* SCTNL0 SCSI control register 0 */
    volatile UINT8    *pSocl;        /* SOCL SCSI output control latch reg */
    volatile UINT8    *pSodl;        /* SCSI output data latch reg */
    volatile UINT8    *pSxfer;       /* SODL SCSI transfer register */
    volatile UINT8    *pScid;        /* SCID SCSI chip ID register */
    volatile UINT8    *pSbcl;        /* SBCL SCSI bus control lines reg */
    volatile UINT8    *pSbdl;        /* SBDL SCSI bus data lines register */
    volatile UINT8    *pSidl;        /* SIDL SCSI input data latch reg */
    volatile UINT8    *pSfbr;        /* SFBR SCSI first byte received reg */
    volatile UINT8    *pSstat2;      /* SSTAT2 SCSI status register 2 */
    volatile UINT8    *pSstat1;      /* SSTAT1 SCSI status register 1 */
    volatile UINT8    *pSstat0;      /* SSTAT0 SCSI status register 0 */
    volatile UINT8    *pDstat;       /* DSTAT DMA status register */
    volatile UINT     *pDsa;         /* DSA data structure address */
    volatile UINT8    *pCtest3;      /* CTEST3 chip test register 3 */
    volatile UINT8    *pCtest2;      /* CTEST2 chip test register 2 */
    volatile UINT8    *pCtest1;      /* CTEST1  chip test register 1 */
    volatile UINT8    *pCtest0;      /* CTEST0 chip test register 0 */
    volatile UINT8    *pCtest7;      /* CTEST7 chip test register 7 */
    volatile UINT8    *pCtest6;      /* CTEST6 chip test register 6 */
    volatile UINT8    *pCtest5;      /* CTEST5 chip test register 5 */
    volatile UINT8    *pCtest4;      /* CTEST4 chip test register 4 */
    volatile UINT     *pTemp;        /* TEMP temporary holding register */
    volatile UINT8    *pLcrc;        /* LCRC longitudinal parity register */
    volatile UINT8    *pCtest8;      /* CTEST8 chip test register */
    volatile UINT8    *pIstat;       /* ISTAT interrupt status register */
    volatile UINT8    *pDfifo;       /* DFIFO DMA FIFO control register */
    volatile UINT8    *pDcmd;	     /* DBC SIOP command register 8bits  */
    volatile UINT     *pDbc;	     /* DCMD SIOP command reg (24Bits Reg) */
    volatile UINT     *pDnad;        /* DNAD DMA buffer ptr (next address) */
    volatile UINT     *pDsp;         /* DSP SIOP scripts pointer register */
    volatile UINT     *pDsps;        /* DSPS SIOP scripts ptr save reg */
    volatile UINT8    *pScratch3;    /* SCRATCH3 general purpose scratch reg */
    volatile UINT8    *pScratch2;    /* SCRATCH2 general purpose scratch reg */
    volatile UINT8    *pScratch1;    /* SCRATCH1 general purpose scratch reg */
    volatile UINT8    *pScratch0;    /* SCRATCH0 general purpose scratch reg */
    volatile UINT8    *pDcntl;       /* DCTNL DMA control register */
    volatile UINT8    *pDwt;         /* DWT DMA watchdog timer register */
    volatile UINT8    *pDien;        /* DIEN DMA interrupt enable */
    volatile UINT8    *pDmode;       /* DMODE DMA operation mode register */
    volatile UINT     *pAdder;	     /* ADDER Adder output Register */
    } NCR_710_SCSI_CTRL;


/* Bit Registers definitions for ncr710 */

/* SCNTL0 */
#define    B_ARB1    0x80	/* Arbitration bit 1 */
#define    B_ARB0    0x40	/* Arbitration bit 0 */
				/* 00 Simple/11 full */
#define    B_START   0x20	/* Start sequence */
#define    B_WATN    0x10	/* Select w/wo atn */
#define    B_EPC     0x08       /* Parity checking */
#define    B_EPG     0x04	/* Enable parity generation */
#define    B_AAP     0x02	/* Assert ATN on parity error */
#define    B_TRG     0x01	/* Target/initiator mode */

/* SCNTL1 */
#define    B_EXC     0x80	/* Extra Data Set up */
#define    B_ADB     0x40	/* Assert Data (SODL) onto scsi */
#define    B_ESR     0x20	/* Enable Select & reselect */
#define    B_CON     0x10	/* connected bit status */
#define    B_RST     0x08	/* Assert Rst on scsi */
#define    B_AESP    0x04	/* Assert even parity -force error */
#define    B_SND     0x02	/* Start send scsi operation */
#define    B_RCV     0x01	/* Start receive scsi operation */

/* SIEN (enable int)and SSTAT0 (RO Status) */
/* Enable interrupt */
#define    B_MA      0x80	/* Enable phase mismatch/Atn Active */
#define    B_FCMP    0x40	/* Enable funtion complete */
#define    B_STO     0x20	/* Enable scsi timout */
#define    B_SEL     0x10	/* Enable sel/resel */
#define    B_SGE     0x08	/* Enable scsi gross  error */
#define    B_UDC     0x04	/* Enable unexpected disconnect */
#define    B_RSTE    0x02	/* Enable Rst received */
#define    B_PAR     0x01	/* Enable parity int */

/* SXFER */
#define    B_DHP     0x80	/* Disable Halt on parity err */
#define    B_TP2     0x40	/* Synchronous transfer period 2 */
#define    B_TP1     0x20	/* Synchronous transfer period 1 */
#define    B_TP0     0x10	/* Synchronous transfer period 0 */
#define    B_MO3     0x08	/* Maximun scsi Synchronous transfer offset */
#define    B_MO2     0x04	/* Maximun scsi Synchronous transfer offset */
#define    B_MO1     0x02	/* Maximun scsi Synchronous transfer offset */
#define    B_MO0     0x01	/* Maximun scsi Synchronous transfer offset */

/*  SOCL (RW) and SBCL (RO not latched) */
#define    B_REQ     0x80	/* Assert scsi req */
#define    B_ACK     0x40	/* Assert scsi ack */
#define    B_BSY     0x20	/* Assert scsi busy */
#define    B_SEL     0x10	/* Assert scsi sel */
#define    B_ATN     0x08	/* Assert scsi atn */
#define    B_MSG     0x04	/* Assert scsi msg */
#define    B_CD      0x02	/* Assert scsi c/d */
#define    B_IO      0x01	/* Assert scsi i/o */
/* SSCF (Write only same @ as SBCL) */
#define    B_SSCF1   0x02	/* prescale clock for scsi core bit 1 */
#define    B_SSCF0   0x01	/* prescale clock for scsi core bit 0 */

/* DSTAT (RO) and DIEN (RW enable intr) */
#define    B_DFE     0x80	/* stat :Dma fifo empty */
#define    B_BF      0x20	/* stat :Bus access error */
#define    B_ABT     0x10	/* stat :Abort condition */
#define    B_SSI     0x08	/* stat :Scsi step interrupt */
#define    B_SIR     0x04	/* stat :Script interrupt received */
#define    B_WTD     0x02	/* stat :Watchdog timout */
#define    B_IID     0x01	/* stat :Illegal instruction occur */

/* SSTAT1 RO register */
#define    B_ILF     0x80	/* stat :SDIL register contain scsi data */
				/* in async mode only */
#define    B_ORF     0x40	/* stat :SODR output data in sync mode hidden */
#define    B_OLF     0x20       /* stat :SODL Register full (hidden) */
#define    B_AIP     0x10	/* stat :Arbitration in progress */
#define    B_LOA     0x08       /* stat :Lost arbitration */
#define    B_WOA     0x04	/* stat :won arbitration-full arbit mode */
#define    B_RSTNL   0x02	/* stat :current stat of RST line-Not Latched */
#define    B_PARNL   0x01	/* stat :curr stat Parity lines-Not Latched */

/* SSTAT2 RO register */
#define    B_FF3     0x80	/* Fifo flag 3 :number of bytes in fifo */
#define    B_FF2     0x40	/* Fifo flag 2 :number of bytes in fifo */
#define    B_FF1     0x20	/* Fifo flag 1 :number of bytes in fifo */
#define    B_FF0     0x10	/* Fifo flag 0 :number of bytes in fifo */
				/* 0000=0 ..... 1000=8 */
#define    B_SPDL    0x08	/* Latched parity line */
#define    B_MSGL    0x04	/* Latched state of MSG line */
#define    B_CDL     0x02	/* Latched state of C/D line */
#define    B_IOL     0x01	/* Latched sate of IO line */

/* DFIFO RW register */
#define    B_BO6     0x40	/* Byte offset counter 6 */
#define    B_BO5     0x20	/* Byte offset counter 5 */
#define    B_BO4     0x10	/* Byte offset counter 4 */
#define    B_BO3     0x08	/* Byte offset counter 3 */
#define    B_BO2     0x04	/* Byte offset counter 2 */
#define    B_BO1     0x02	/* Byte offset counter 1 */
#define    B_BO0     0x01	/* Byte offset counter 0 */

/* ISTAT RW register */
#define    B_ABORT   0X80	/* Abort operation */
#define    B_SOFTRST 0x40	/* Soft chip reset */
#define    B_SIGP    0x20	/* signal process */
#define    B_CONST   0x08	/* stat connected (reset does'nt disconnect) */
#define    B_SIP     0x02	/* Status scsi interrupt pending */
#define    B_DIP     0x01	/* Status Dma portion interrupt pending */

/* DMODE RW register */
#define    B_BL1     0x80	/* Burst Length transfer bit 1 */
#define    B_BL0     0x40	/* Burst Length transfer bit 0 */
				/* 00=1,01=2,10=4,11=8 */
#define    B_FC2     0x20	/* Function code bit 1 user defined */
#define    B_FC1     0x10	/* Function code bit 0 user defined */
#define    B_PD      0x08	/* Program data access */
#define    B_FAM     0x04	/* Fixed Address mode */
#define    B_U0TT0   0x02	/* user  programmable transfer type */
#define    B_MAN     0x01	/* Manual start mode ,when set disable */
				/* autostart script when writting in DSP */
/* DCTNL */
#define    B_CF1     0x80	/* Prescale bit 1 for scsi core */
#define    B_CF0     0x40	/* Prescale bit 0 for scsi core */
				/* involve all timing on scsi */
#define    B_EA      0x20	/* Enable ack (host/chip) */
#define    B_SSM     0x10	/* Enable single step mode */
#define    B_LLM     0x08	/* Enable Low level mode */
#define    B_STD     0x04	/* Start Dma operation used with B_MAN in */
				/* DMODE Register and single step mode */
#define    B_FA      0x02	/* Enable fast arbitration on host port */
#define    B_COM     0x01	/* When 0, enable compatible mode with ncr700 */

/* CTEST4 */
#define    B_MUX     0x80	/* Host mux bus mode */
#define    B_ZMOD    0x40	/* Host bus high impedance mode  */
#define    B_SZM     0x20	/* Scsi bus high impedance mode */
#define    B_SLBE    0x10	/* Enable loopback mode */
#define    B_SFWR    0x08	/* Scsi fifo write enable */
#define    B_FBL2    0x04	/* Enable bytes lane */
#define    B_FLB1    0x02	/* Bit one mux byte lane */
#define    B_FLB0    0x01	/* Bit zero mux byte lane */

/* CTEST5 */
#define    B_ADCK    0x80	/* Increment DNAD register */
#define    B_BBCK    0x40	/* Decrement DBC register */
#define    B_ROFF    0x20	/* Clear offset in scsi sync mode */
#define    B_MASR    0x10	/* set/reset values for bit 3:0 */
#define    B_DDIR    0x08	/* control internel DMAWR (scsi to host) */
#define    B_EOP     0x04	/* control internal EOP (DMA/SCSI) */
#define    B_DREQ    0x02	/* control internal DREQ */
#define    B_DACK    0x01	/* control internal DACK (DMA/SCSI) */

/* CTEST7 */
#define    B_CDIS    0x80	/* Disable Burst Cache */
#define    B_SC1     0x40	/* Snoop control bit1 */
#define    B_SC0     0x20	/* Snoop control bit0 */
#define    B_TOUT    0x10	/* disable scsi timout */
#define    B_DFP     0x08	/* Fifo parity bit */
#define    B_EVP     0x04	/* enable even parity on host/fifo */
#define    B_TT1     0x02	/* invert tt1 pin (sync host mode only) */
#define    B_DIFF    0x01       /* Valid differential scsi bus */

/* CTEST8 */
#define    B_V3      0x80	/* Chip Revision Bit 3 */
#define    B_V2      0x40	/* Chip Revision Bit 2 */
#define    B_V1      0x20	/* Chip Revision Bit 1 */
#define    B_V0      0x10	/* Chip Revision Bit 0 */
#define    B_FLF     0x08	/* Flush DMA FIFO */
#define    B_CLF     0x04	/* Clear DMA FIFO */
#define    B_FM      0x02	/* Fetch Pin mode */
#define    B_SM      0x01       /* Snoop pins mode */

/* offset registers */

#if _BYTE_ORDER==_BIG_ENDIAN

#define OFF_SIEN         (0X00)   /* scsi interrupt enable reg */
#define OFF_SDID         (0X01)   /* scsi destination ID reg */
#define OFF_SCNTL1       (0X02)   /* scsi control reg 1 */
#define OFF_SCNTL0       (0X03)   /* scsi control reg 0 */
#define OFF_SOCL         (0X04)   /* scsi output control latch reg */
#define OFF_SODL         (0X05)   /* scsi output data latch reg */
#define OFF_SXFER        (0X06)   /* scsi transfer reg */
#define OFF_SCID         (0X07)   /* scsi chip ID reg */
#define OFF_SBCL         (0X08)   /* scsi bus control lines reg */
#define OFF_SBDL         (0X09)   /* scsi bus data lines reg */
#define OFF_SIDL         (0X0A)   /* scsi input data latch reg */
#define OFF_SFBR         (0X0B)   /* scsi first byte received reg */
#define OFF_SSTAT2       (0X0C)   /* scsi status reg 2 */
#define OFF_SSTAT1       (0X0D)   /* scsi status reg 1 */
#define OFF_SSTAT0       (0X0E)   /* scsi status reg 0 */
#define OFF_DSTAT        (0X0F)   /* dma  status reg */
#define OFF_DSA          (0X10)   /* data structure address */
#define OFF_CTEST3       (0X14)   /* chip test reg 3 */
#define OFF_CTEST2       (0X15)   /* chip test reg 2 */
#define OFF_CTEST1       (0X16)   /* chip test reg 1 */
#define OFF_CTEST0       (0X17)   /* chip test reg 0 */
#define OFF_CTEST7       (0X18)   /* chip test reg 7 */
#define OFF_CTEST6       (0X19)   /* chip test reg 6 */
#define OFF_CTEST5       (0X1A)   /* chip test reg 5 */
#define OFF_CTEST4       (0X1B)   /* chip test reg 4 */
#define OFF_TEMP         (0X1C)   /* Temporary stack reg */
#define OFF_LCRC         (0X20)   /* CRC register */
#define OFF_CTEST8       (0X21)   /* Chip test reg 8 */
#define OFF_ISTAT        (0X22)   /* Interrupt status reg */
#define OFF_DFIFO        (0X23)   /* DMA FIFO reg */
#define OFF_DCMD         (0X24)   /* 8 bit reg DMA command reg */
#define OFF_DBC          (0X24)   /* 24 bit Reg DMA byte counter register */
#define OFF_DNAD         (0X28)   /* DMA next address for data reg */
#define OFF_DSP          (0X2C)   /* DMA scripts pointer reg */
#define OFF_DSPS         (0X30)   /* DMA scripts pointer save reg */
#define OFF_SCRATCH3     (0X34)   /* scratch register */
#define OFF_SCRATCH2     (0X35)   /* scratch register */
#define OFF_SCRATCH1     (0X36)   /* scratch register */
#define OFF_SCRATCH0     (0X37)   /* scratch register */
#define OFF_DCNTL        (0X38)   /* DMA control reg */
#define OFF_DWT          (0X39)   /* DMA watchdog timer */
#define OFF_DIEN         (0X3A)   /* DMA interrupt enable reg */
#define OFF_DMODE        (0X3B)   /* DMA mode reg */
#define OFF_ADDER        (0X3C)   /* internal adder register;don't use */

#else  /* _BYTE_ORDER==_BIG_ENDIAN */

#define OFF_SIEN         (0X03)   /* scsi interrupt enable reg */
#define OFF_SDID         (0X02)   /* scsi destination ID reg */
#define OFF_SCNTL1       (0X01)   /* scsi control reg 1 */
#define OFF_SCNTL0       (0X00)   /* scsi control reg 0 */
#define OFF_SOCL         (0X07)   /* scsi output control latch reg */
#define OFF_SODL         (0X06)   /* scsi output data latch reg */
#define OFF_SXFER        (0X05)   /* scsi transfer reg */
#define OFF_SCID         (0X04)   /* scsi chip ID reg */
#define OFF_SBCL         (0X0B)   /* scsi bus control lines reg */
#define OFF_SBDL         (0X0A)   /* scsi bus data lines reg */
#define OFF_SIDL         (0X09)   /* scsi input data latch reg */
#define OFF_SFBR         (0X08)   /* scsi first byte received reg */
#define OFF_SSTAT2       (0X0F)   /* scsi status reg 2 */
#define OFF_SSTAT1       (0X0E)   /* scsi status reg 1 */
#define OFF_SSTAT0       (0X0D)   /* scsi status reg 0 */
#define OFF_DSTAT        (0X0C)   /* dma  status reg */
#define OFF_DSA          (0X10)   /* data structure address */
#define OFF_CTEST3       (0X17)   /* chip test reg 3 */
#define OFF_CTEST2       (0X16)   /* chip test reg 2 */
#define OFF_CTEST1       (0X15)   /* chip test reg 1 */
#define OFF_CTEST0       (0X14)   /* chip test reg 0 */
#define OFF_CTEST7       (0X1B)   /* chip test reg 7 */
#define OFF_CTEST6       (0X1A)   /* chip test reg 6 */
#define OFF_CTEST5       (0X19)   /* chip test reg 5 */
#define OFF_CTEST4       (0X18)   /* chip test reg 4 */
#define OFF_TEMP         (0X1C)   /* Temporary stack reg */
#define OFF_LCRC         (0X23)   /* CRC register */
#define OFF_CTEST8       (0X22)   /* Chip test reg 8 */
#define OFF_ISTAT        (0X21)   /* Interrupt status reg */
#define OFF_DFIFO        (0X20)   /* DMA FIFO reg */
#define OFF_DCMD         (0X27)   /* 8 bit reg DMA command reg */
#define OFF_DBC          (0X24)   /* 24 bit Reg DMA byte counter register */
#define OFF_DNAD         (0X28)   /* DMA next address for data reg */
#define OFF_DSP          (0X2C)   /* DMA scripts pointer reg */
#define OFF_DSPS         (0X30)   /* DMA scripts pointer save reg */
#define OFF_SCRATCH3     (0X37)   /* scratch register */
#define OFF_SCRATCH2     (0X36)   /* scratch register */
#define OFF_SCRATCH1     (0X35)   /* scratch register */
#define OFF_SCRATCH0     (0X36)   /* scratch register */
#define OFF_DCNTL        (0X3B)   /* DMA control reg */
#define OFF_DWT          (0X3A)   /* DMA watchdog timer */
#define OFF_DIEN         (0X39)   /* DMA interrupt enable reg */
#define OFF_DMODE        (0X38)   /* DMA mode reg */
#define OFF_ADDER        (0X3C)   /* internal adder register;don't use */

#endif  /* _BYTE_ORDER==_BIG_ENDIAN */

/* Mask Values */
#define NCR710_COUNT_MASK    ((UINT)0x00ffffff) /* Mask 24 bit value in block */
                                                /* move description */
#define INIT_BUS_ID  	     ((UINT)0x00010000) /* initial value for scsi id */

/* DMA Fifo register */
#define NCR710_MAXDMA_BYTE 	/* fifo deep in bytes 64 */
#define NCR710_MAXDMA_LONG	/* fifo deep in long 16 */

/* Sync offset */
#define NCR710_MAXSYNC_OFF  0x08	/* Maximum sync offset */
#define NCR710_MAX_XFERP    0x08        /* Maximum sxfer value for TP2-0 */

/* Clock conversion factor */
/* prescale factor for scsi core (dctnl) */
#define NCR710_16MHZ_DIV        0x80    /* 16-25Mhz chip */
#define NCR710_25MHZ_DIV        0x40    /* 25-37.5Mhz chip */
#define NCR710_50MHZ_DIV        0x00    /* 37.5-50Mhz chip */
#define NCR710_66MHZ_DIV        0xC0    /* 50-66Mhz chip */

/* ns x 100 clock period */
#define NCR710_1667MHZ  5998    /* 16.67Mhz chip */
#define NCR710_20MHZ    5000    /* 20Mhz chip */
#define NCR710_25MHZ    4000    /* 25Mhz chip */
#define NCR710_3750MHZ  2666    /* 37.50Mhz chip */
#define NCR710_40MHZ    2500    /* 40Mhz chip */
#define NCR710_50MHZ    2000    /* 50Mhz chip */
#define NCR710_66MHZ    1515    /* 66Mhz chip */
#define NCR710_6666MHZ  1500    /* 66Mhz chip */

/* Chip Type */
#define NCR700_TYPE	0x700   /* not Supported */
#define NCR710_TYPE	0x710   /* supported */
#define NCR720_TYPE	0x720   /* not supported */

/* Macros to acces to ncrShareCtl */
#define SCRIPTADDR( pSiop,index )   ( pSiop->pNcrCtl->scriptPtr[index])

/* Default value to initialize some registers involve
 * in the hardware implementation.Those values could be
 * overwritten by a bsp call with the ncr710HwSetRegister().
 * The ncr710 Data Manual documentation will you give all
 * the light regarding values choose. See also the
 * documentation of ncr710SetHwRegister call.
 * See below the NCR710_HW_REGS for the meaning of the
 * values prefill.
 */
#define DEFAULT_710_HW_REGS { 0,0,0,0,1,0,0,0,0,0,0,0,0,1,0 }

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

IMPORT  NCR_710_SCSI_CTRL * ncr710CtrlCreate (UINT8 *siopBaseAdrs,
                                              UINT clkPeriod);
IMPORT  STATUS  ncr710CtrlInit (NCR_710_SCSI_CTRL *pSiop, int scsiCtrlBusId,
                                int scsiPriority);
IMPORT  STATUS 	ncr710Show (SCSI_CTRL *pScsiCtrl);
IMPORT	STATUS	ncr710SetHwRegister(NCR_710_SCSI_CTRL *pScsiCtrl,
				    NCR710_HW_REGS *pHwRegs);
IMPORT  void    ncr710Intr (NCR_710_SCSI_CTRL *pSiop);

#else	/* __STDC__ */

IMPORT  NCR_710_SCSI_CTRL * ncr710CtrlCreate ();
IMPORT  STATUS  ncr710CtrlInit ();
IMPORT  STATUS 	ncr710SetHwRegister ();
IMPORT  STATUS 	ncr710Show ();
IMPORT  void    ncr710Intr ();

#endif  /* __STDC__ */

#endif	/* _ASMLANGUAGE */
#ifdef __cplusplus
}
#endif

#endif /* __INCncr710_1h */
