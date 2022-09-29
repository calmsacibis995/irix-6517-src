/* if_ei.h - Intel 82596 network interface header */

/* Copyright 1990-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
02h,22sep92,rrr  added support for c++
02g,29jul92,rrr  Grabbed ricks if_ei.h header file and checked it in in hope of
                 getting the ei driver to work.
02f,16jul92,rfs  Moved driver specific items to driver file, where they belong.
02e,26may92,rrr  the tree shuffle
02d,12nov91,wmd  added two elements to EI_SOFTC, wid and transLock.
02c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02b,16sep91,rfs  added two elements to EI_SOFTC
02a,22jul91,jcf  completely rewritten to utilize simplified mode,
            rfs  added big endian support, reworked buffer loaning.
01e,01may91,elh  added rbufRefCnt for buffer loaning support.
01d,08jan91,gae  removed CDELAY.
01c,04jan91,gae  installed Intel changes and more cleanup.
01b,12dec90,elh  cleaned up, merged with if_eivar.h
01a,01nov90,rl   written.
*/

#ifndef __INCif_eih
#define __INCif_eih

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* constants needed within this file */

#define EH_SIZE		14  		/* avoid structure padding issues */
#define N_MCAST		12


/* Intel 82596 endian safe link macros and structure definitions */

#define LINK_WR(pLink,value)						\
	(((pLink)->lsw = (UINT16)((UINT32)(value) & 0x0000ffff)),	\
	((pLink)->msw  = (UINT16)(((UINT32)(value) >> 16) & 0x0000ffff)))

#define LINK_RD(pLink) ((pLink)->lsw | ((pLink)->msw << 16))

#define STAT_RD LINK_RD			/* statistic read is a link read */
#define STAT_WR LINK_WR			/* statistic write is a link write */

typedef struct ei_link 			/* EI_LINK - endian resolvable link */
    {
    UINT16 		lsw;		/* least significant word */
    UINT16 		msw;		/* most significant word */
    } EI_LINK;

typedef struct ei_node 			/* EI_NODE - common linked list object */
    {
    UINT16				field1;
    UINT16				field2;
    EI_LINK				lNext;						/* link to next node */
    EI_LINK				field4;
    UINT16				field5;
    UINT16				field6;
    UINT8				field7 [EH_SIZE];
    char				field8 [ETHERMTU];
	struct ei_node		*pNext;						/* ptr to next node */
    } EI_NODE;

typedef EI_LINK EI_STAT; 	/* EI_STAT - 82596 error statistic */


/* Intel 82596 structure and bit mask definitions */

/* PORT commands */

#define PORT_RESET		0x0
#define PORT_SELFTEST		0x1
#define PORT_NEWSCP		0x2
#define PORT_DUMP		0x3


/* System Configuration Pointer and bit field defines */

typedef struct scp 		/* SCP - System Configuration Pointer */
    {
    UINT16 scpRsv1;			/* reserved */
    UINT16 scpSysbus;			/* SYSBUS */
    UINT16 scpRsv2;			/* reserved */
    UINT16 scpRsv3;			/* reserved */
    EI_LINK pIscp;			/* ISCP address */
    } SCP;

#define SCP_MODE_82586		0x0000 	/* operate in i82586 mode */
#define SCP_MODE_SEGMENTED	0x0002 	/* operate in i82596 segmented mode */
#define SCP_MODE_LINEAR		0x0004 	/* operate in i82596 linear mode */
#define SCP_EXT_TRIGGER		0x0008 	/* ext. trig. of Bus Throttle timers */
#define SCP_LOCK_DISABLE	0x0010 	/* lock function disabled */
#define SCP_INT_LOW		0x0020 	/* interrupt pin is active low */
#define SCP_SYSBUS_BIT6		0x0040 	/* reserved - bit 6 - *MUST BE ONE* */
#define SCP_SYSBUS_BIT7		0x0080 	/* reserved - bit 7 - *MUST BE ZERO* */


/* Intermediate System Configuration Pointer */

typedef struct iscp 		/* ISCP - Intermediate System Config. Ptr. */
    {
    volatile UINT16	iscpBusy;	/* i82596 is being initialized */
    UINT16		iscpRsv1;	/* reserved */
    EI_LINK		pScb;		/* SCB address */
    } ISCP;


/* System Control Block and bit field defines */

typedef struct scb 		/* SCB - System Control Block */
    {
    volatile UINT16	scbStatus;	/* Status Word */
    volatile UINT16	scbCommand;	/* Command Word */
    EI_LINK 		pCB;		/* command block address */
    EI_LINK 		pRF;		/* receive frame area address */
    EI_STAT 		crcErr;		/* CRC error count */
    EI_STAT 		allignErr;	/* frames misaligned and CRC err cnt */
    EI_STAT 		noResErr;	/* no resources error count */
    EI_STAT 		ovErr;		/* overrun error count */
    EI_STAT 		cdErr;		/* collision detected error count */
    EI_STAT 		frameErr;	/* short frame error count */
    UINT16  		tOff;		/* T-off timer */
    UINT16  		tOn;		/* T-on timer */
    } SCB;

#define SCB_S_RUIDLE	0x0000		/* RU is idle */
#define SCB_S_BTTL	0x0008		/* Bus Throttle timers loaded */
#define SCB_S_RUMASK	0x00f0		/* state mask */
#define SCB_S_RUSUSP	0x0010		/* RU is suspended */
#define SCB_S_RUNORSRC	0x0020		/* RU has no resources */
#define SCB_S_RURSV1	0x0030		/* reserved */
#define SCB_S_RUREADY	0x0040		/* RU is ready */
#define SCB_S_RUNRBD	0x0080		/* RU has no more RBD's */
#define SCB_S_RUNORSRCR	0x00a0		/* RU no more resources/no more RBD's */
#define SCB_S_CUMASK	0x0f00		/* state mask */
#define SCB_S_CUSUSP	0x0100		/* CU is suspended */
#define SCB_S_CUACTIVE	0x0200		/* CU is active */
#define SCB_S_CURSV1	0x0300		/* reserved */
#define SCB_S_CURSV2	0x0400		/* reserved */
#define SCB_S_CURSV3	0x0500		/* reserved */
#define SCB_S_CURSV4	0x0600		/* reserved */
#define SCB_S_CURSV5	0x0700		/* reserved */
#define SCB_S_XMASK	0xf000		/* state mask */
#define SCB_S_CUIDLE	0x0000		/* CU is idle */
#define SCB_S_RNR	0x1000		/* RU left the ready state */
#define SCB_S_CNA	0x2000		/* CU left the active state */
#define SCB_S_FR	0x4000		/* RU finished receiveing a frame */
#define SCB_S_CX	0x8000		/* CU finished a cmd with I bit set */

#define SCB_C_RUNOP	0x0000		/* NOP */
#define SCB_C_RUSTART	0x0010		/* start reception of frames */
#define SCB_C_RURESUME	0x0020		/* resume reception of frames */
#define SCB_C_RUSUSPEND	0x0030		/* suspend reception of frames */
#define SCB_C_RUABORT	0x0040		/* abort receiver immediately */
#define SCB_C_RURSV1	0x0050		/* reserved */
#define SCB_C_RURSV2	0x0060		/* reserved */
#define SCB_C_RURSV3	0x0070		/* reserved */
#define SCB_C_RESET	0x0080		/* reset chip */
#define SCB_C_CUNOP	0x0000		/* NOP */
#define SCB_C_CUSTART	0x0100		/* start execution */
#define SCB_C_CURESUME	0x0200		/* resume execution */
#define SCB_C_CUSUSPEND	0x0300		/* suspend execution after cur. cmd */
#define SCB_C_CUABORT	0x0400		/* abort current cmd immediately */
#define SCB_C_CULOADBT	0x0500		/* load bus throttle timers after cnt */
#define SCB_C_CULDBTIMM	0x0600		/* load bus throttle timers now */
#define SCB_C_CURSV1	0x0700		/* reserved */
#define SCB_C_ACK_RNR	0x1000		/* ACK that RU became not ready */
#define SCB_C_ACK_CNA	0x2000		/* ACK that CU bacame not active */
#define SCB_C_ACK_FR	0x4000		/* ACK that RU received a frame */
#define SCB_C_ACK_CX	0x8000		/* ACK that CU completed an action */


/* Action Command Descriptions */

typedef struct ac_iasetup 	/* AC_IASETUP - Individual Address Setup */
    {
    UINT8  ciAddress[6];		/* local ethernet address */
    UINT16 ciFill;
    } AC_IASETUP;

typedef struct ac_config 	/* AC_CONFIG - i82596 Configure */
    {
    UINT8 ccByte8;			/* byte count, prefetched bit*/
    UINT8 ccByte9;			/* FIFO limit, monitor bits */
    UINT8 ccByte10;			/* save bad frames */
    UINT8 ccByte11;			/* address length, loopback */
    UINT8 ccByte12;			/* backoff method */
    UINT8 ccByte13;			/* interframe spacing */
    UINT8 ccByte14;			/* slot time -low byte */
    UINT8 ccByte15;			/* slot time -upper 3 bits, max retry */
    UINT8 ccByte16;			/* promiscuious mode, CRC, padding */
    UINT8 ccByte17;			/* carrier sense, collision detect */
    UINT8 ccByte18;			/* minimum frame length */
    UINT8 ccByte19;			/* auto retransmit, multicast,... */
    UINT8 ccByte20;			/* DCR slot num, full duplex */
    UINT8 ccByte21;			/* disable backoff, ... */
    UINT16 ccFill;
    } AC_CONFIG;

typedef struct ac_mcast 	/* AC_MCAST - Mulitcast Setup */
    {
    UINT16 cmMcCount;	 		/* the number of bytes in MC list */
    UINT8  cmAddrList[6 * N_MCAST];	/* mulitcast address list */
    } AC_MCAST;

typedef struct ac_tdr 		/* AC_TDR - Time Domain Reflectometry */
    {
    UINT16 ctInfo;			/* time, link OK, tx err, line err */
    UINT16 ctReserve1;			/* reserved */
    } AC_TDR;

typedef struct ac_dump 		/* AC_DUMP - Dump */
    {
    EI_LINK bufAddr; 			/* address of dump buffer */
    } AC_DUMP;


/* Command Frame Description and defines */

typedef struct cfd 		/* CFD - Command Frame Descriptor */
    {
    volatile UINT16	cfdStatus;	/* command status */
    UINT16		cfdCommand;	/* command */
    EI_LINK		link;		/* address of next CB */
    union 				/* command dependent section */
	{
	struct ac_iasetup	cfd_iasetup;	/* IA setup */
	struct ac_config	cfd_config;	/* config */
	struct ac_mcast		cfd_mcast;	/* multicast setup */
	struct ac_tdr		cfd_tdr;	/* TDR */
	struct ac_dump		cfd_dump;	/* dump */
	} cfd_cmd;
    } CFD;

#define cfdIASetup	cfd_cmd.cfd_iasetup
#define cfdConfig	cfd_cmd.cfd_config
#define cfdMcast 	cfd_cmd.cfd_mcast
#define cfdTDR 		cfd_cmd.cfd_tdr
#define cfdDump 	cfd_cmd.cfd_dump

#define CFD_C_NOP	0x0000		/* No Operation */
#define CFD_C_IASETUP	0x0001		/* Individual Address Setup */
#define CFD_C_CONFIG	0x0002		/* Configure Chip */
#define CFD_C_MASETUP	0x0003		/* Multicast Setup */
#define CFD_C_XMIT	0x0004		/* Transmit (see below too ...) */
#define CFD_C_TDR	0x0005		/* Time Domain Reflectometry */
#define CFD_C_DUMP	0x0006		/* Dump Registers */
#define CFD_C_DIAG	0x0007		/* Diagnose */
#define CFD_C_INT	0x2000		/* 596 interrupts CPU after execution */
#define CFD_C_SE	0x4000		/* CU should suspend after execution */
#define CFD_C_EL	0x8000		/* End of command list */

#define CFD_S_ABORTED	0x1000		/* Command was aborted via CU Abort */
#define CFD_S_OK	0x2000		/* Command completed successfully */
#define CFD_S_BUSY	0x4000		/* CU is executing this command */
#define CFD_S_COMPLETE	0x8000		/* Command complete */


/* 82596 Transmit/Receive Frames */

typedef struct tfd 		/* TFD - Transmit Frame Descriptor */
    {
    volatile UINT16		status;						/* status field */
    UINT16				command;					/* command field */
    EI_LINK				lNext;						/* link to next desc. */
    EI_LINK				lBufDesc;					/* link to buf desc. */
    UINT16				count;						/* length of data */
    UINT16				reserved;					/* not used */
    UINT8				enetHdr [EH_SIZE];			/* the ethernet header */
    char				enetData [ETHERMTU];		/* transmit data */
	struct tfd			*pNext;						/* ptr to next desc. */
    } TFD;

/* special TFD specific command block bit masks */

#define CFD_C_TX_FLEX	0x0008		/* Use Flexible Mode */
#define CFD_C_TX_NOCRC	0x0010		/* Do not insert CRC */

#define CFD_S_COLL_MASK	0x000f		/* to access number of collisions */
#define CFD_S_RETRY	0x0020		/* reached the max number of retries */
#define CFD_S_HBEAT	0x0040		/* Heartbeat Indicator */
#define CFD_S_TRDEF	0x0080		/* Transmission Deferred */
#define CFD_S_DMA_UNDR	0x0100		/* DMA Underrun (no data) */
#define CFD_S_NO_CTS	0x0200		/* Lost Clear To Send signal */
#define CFD_S_NO_CRS	0x0400		/* No Carrier Sense */
#define CFD_S_LATE_COLL	0x0800		/* Late collision is detected */

#define TFD_CNT_EOF	0x8000		/* all data kept in TFD */


typedef struct rfd 		/* RFD - Receive Frame Descriptor */
    {
    volatile UINT16		status;						/* status field */
    UINT16				command;					/* command field */
    EI_LINK				lNext;						/* link to next desc. */
    EI_LINK				lBufDesc;					/* link to buf desc. */
    volatile UINT16		actualCnt;					/* actual byte count */
    UINT16				bufSize;					/* buffer size */
    UINT8				enetHdr [EH_SIZE];			/* the ethernet header */
    char				enetData [ETHERMTU];		/* received data */
	struct tfd			*pNext;						/* ptr to next desc. */
    UINT8				refCnt;						/* reference count */
    } RFD;

/* RFD bit masks */

#define RFD_S_COLL	0x0001		/* collision during reception */
#define RFD_S_EOP	0x0040		/* no EOP flag */
#define RFD_S_MATCH	0x0080		/* dest Addr matched the IA address */
#define RFD_S_DMA	0x0100		/* DMA Overrun failure to get bus */
#define RFD_S_RSRC	0x0200		/* received, but ran out of buffers */
#define RFD_S_ALGN	0x0400		/* received misaligned with CRC error */
#define RFD_S_CRC	0x0800		/* received with CRC error */
#define RFD_S_LEN	0x1000		/* received with length error */
#define RFD_S_OK	0x2000		/* frame received successfully */
#define RFD_S_BUSY	0x4000		/* frame reception ready/in progress */
#define RFD_S_COMPLETE	0x8000		/* frame reception complete */

#define RFD_M_FLEXMODE	0x0008		/* flexible mode */
#define RFD_M_SUSPEND	0x4000		/* suspend RU after receiving frame */
#define RFD_M_EL	0x8000		/* end of RFD list */

#if CPU_FAMILY==I960
#pragma align 0				/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCif_eih */
