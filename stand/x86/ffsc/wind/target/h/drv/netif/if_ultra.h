/* if_ultra.h - SMC Elite Ultra network interface header */ 

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,30sep94,hdn  changed TSTART to TSTART[01] for two TX buffer.
01a,30oct93,hdn  written. 
*/


#ifndef	__INCif_ultrah
#define	__INCif_ultrah

#ifdef __cplusplus
extern "C" {
#endif

#define	ULTRA_RAMSIZE		0x4000			/* 16 Kbytes */
#define	ULTRA_TSTART0		0x00
#define	ULTRA_TSTART1		0x08
#define	ULTRA_PSTART		0x10
#define	ULTRA_PSTOP		0x40
#define ULTRA_MIN_SIZE		ETHERMIN		/* 46 */
#define ULTRA_MAX_SIZE		4096			/* 802.3 10base5 */

typedef struct 
    {
    UCHAR rstat;
    UCHAR next;
    UCHAR lowByteCnt;
    UCHAR uppByteCnt;
    } ULTRA_HEADER;

typedef struct 
    {
#define collisions	stat[0]
#define crcs		stat[1]
#define aligns		stat[2]
#define missed		stat[3]
#define overruns	stat[4]
#define disabled	stat[5]
#define deferring	stat[6]
#define underruns	stat[7]
#define aborts		stat[8]
#define outofwindow	stat[9]
#define heartbeats	stat[10]
#define badPacket	stat[11]
#define shortPacket	stat[12]
#define longPacket	stat[13]
#define tnoerror	stat[14]
#define rnoerror	stat[15]
#define terror		stat[16]
#define rerror		stat[17]
#define overwrite	stat[18]
#define wrapped		stat[19]
#define interrupts	stat[20]
#define reset		stat[21]
#define strayint	stat[22]
    UINT stat[23];
    } ULTRA_STAT;

typedef struct 
    {
    struct arpcom es_ac;
#define es_if		es_ac.ac_if
#define es_enaddr	es_ac.ac_enaddr
    USHORT ioAddr;
    UINT memAddr;
    UINT memSize;
    int intLevel;
    int intVec;
    int config;
    UINT transmitPage[2];
    UINT transmitCnt;
    ULTRA_STAT stat;
    UCHAR next;
    UCHAR uppByteCnt;
    UCHAR current;
    UCHAR istat;
    UCHAR tstat;
    UCHAR rstat;
    UCHAR receiveBuf[ULTRA_MAX_SIZE];
    int flags;
    } ULTRA_SOFTC;

typedef struct
    {
    char irq;
    char reg;
    } IRQ_TABLE;

#define	CAST

/* SMC Elite Ultra */ 

#define ULTRA_REG_ADDR_INTERVAL	1
#define ULTRA_ADRS(base,reg)   (CAST (base+(reg*ULTRA_REG_ADDR_INTERVAL)))

#define CTRL_CON(base)		ULTRA_ADRS(base,0x00)	/* Control */
#define CTRL_EEROM(base)	ULTRA_ADRS(base,0x01)	/* EEROM */
#define CTRL_HARD(base)		ULTRA_ADRS(base,0x04)	/* Hardware */
#define CTRL_BIOS(base)		ULTRA_ADRS(base,0x05)	/* BIOS page */
#define CTRL_INT(base)		ULTRA_ADRS(base,0x06)	/* Interrupt control */
#define CTRL_REV(base)		ULTRA_ADRS(base,0x07)	/* Revision */
#define CTRL_LAN0(base)		ULTRA_ADRS(base,0x08)	/* SWH=0 LAN address */
#define CTRL_LAN1(base)		ULTRA_ADRS(base,0x09)	/* SWH=0 LAN address */
#define CTRL_LAN2(base)		ULTRA_ADRS(base,0x0a)	/* SWH=0 LAN address */
#define CTRL_LAN3(base)		ULTRA_ADRS(base,0x0b)	/* SWH=0 LAN address */
#define CTRL_LAN4(base)		ULTRA_ADRS(base,0x0c)	/* SWH=0 LAN address */
#define CTRL_LAN5(base)		ULTRA_ADRS(base,0x0d)	/* SWH=0 LAN address */
#define CTRL_BDID(base)		ULTRA_ADRS(base,0x0e)	/* SWH=0 Board ID */
#define CTRL_CKSM(base)		ULTRA_ADRS(base,0x0f)	/* SWH=0 Checksum */

#define CTRL_PIDL(base)		ULTRA_ADRS(base,0x08)	/* SWH=1 POS ID */
#define CTRL_PIDH(base)		ULTRA_ADRS(base,0x09)	/* SWH=1 POS ID */
#define CTRL_IOADDR(base)	ULTRA_ADRS(base,0x0a)	/* SWH=1 IO address */
#define CTRL_MEMADDR(base)	ULTRA_ADRS(base,0x0b)	/* SWH=1 MEM address */
#define CTRL_BIO(base)		ULTRA_ADRS(base,0x0c)	/* SWH=1 BIOS base */
#define CTRL_GCON(base)		ULTRA_ADRS(base,0x0d)	/* SWH=1 Gen control */

/* page-0, read */

#define LAN_CMD(base)		ULTRA_ADRS(base,0x10)	/* command */
#define LAN_BOUND(base)		ULTRA_ADRS(base,0x13)	/* boundary page */
#define LAN_TSTAT(base)		ULTRA_ADRS(base,0x14)	/* transmit status */
#define LAN_COLCNT(base)	ULTRA_ADRS(base,0x15)	/* collision error */
#define LAN_INTSTAT(base)	ULTRA_ADRS(base,0x17)	/* interrupt status */
#define LAN_RSTAT(base)		ULTRA_ADRS(base,0x1c)	/* receive status */
#define LAN_ALICNT(base)	ULTRA_ADRS(base,0x1d)	/* alignment error */
#define LAN_CRCCNT(base)	ULTRA_ADRS(base,0x1e)	/* crc error */
#define LAN_MPCNT(base)		ULTRA_ADRS(base,0x1f)	/* missed packet */

/* page-0, write */

#define LAN_RSTART(base)	ULTRA_ADRS(base,0x11)	/* receive start */
#define LAN_RSTOP(base)		ULTRA_ADRS(base,0x12)	/* receive stop */
#define LAN_TSTART(base)	ULTRA_ADRS(base,0x14)	/* transmit start */
#define LAN_TCNTL(base)		ULTRA_ADRS(base,0x15)	/* transmit counter */
#define LAN_TCNTH(base)		ULTRA_ADRS(base,0x16)	/* transmit counter */
#define LAN_RCON(base)		ULTRA_ADRS(base,0x1c)	/* receive config */
#define LAN_TCON(base)		ULTRA_ADRS(base,0x1d)	/* transmit config */
#define LAN_DCON(base)		ULTRA_ADRS(base,0x1e)	/* data config */
#define LAN_INTMASK(base)	ULTRA_ADRS(base,0x1f)	/* interrupt mask */

/* page-1, read and write */

#define LAN_STA0(base)		ULTRA_ADRS(base,0x11)	/* station address */
#define LAN_STA1(base)		ULTRA_ADRS(base,0x12)	/* station address */
#define LAN_STA2(base)		ULTRA_ADRS(base,0x13)	/* station address */
#define LAN_STA3(base)		ULTRA_ADRS(base,0x14)	/* station address */
#define LAN_STA4(base)		ULTRA_ADRS(base,0x15)	/* station address */
#define LAN_STA5(base)		ULTRA_ADRS(base,0x16)	/* station address */
#define LAN_CURR(base)		ULTRA_ADRS(base,0x17)	/* current page */
#define LAN_MAR0(base)		ULTRA_ADRS(base,0x18)	/* multicast address */
#define LAN_MAR1(base)		ULTRA_ADRS(base,0x19)	/* multicast address */
#define LAN_MAR2(base)		ULTRA_ADRS(base,0x1a)	/* multicast address */
#define LAN_MAR3(base)		ULTRA_ADRS(base,0x1b)	/* multicast address */
#define LAN_MAR4(base)		ULTRA_ADRS(base,0x1c)	/* multicast address */
#define LAN_MAR5(base)		ULTRA_ADRS(base,0x1d)	/* multicast address */
#define LAN_MAR6(base)		ULTRA_ADRS(base,0x1e)	/* multicast address */
#define LAN_MAR7(base)		ULTRA_ADRS(base,0x1f)	/* multicast address */

/* page-2, read and write */

#define LAN_NEXT(base)		ULTRA_ADRS(base,0x15)	/* next page */
#define LAN_ENH(base)		ULTRA_ADRS(base,0x17)	/* enable features */


/* Control Register */

#define CON_RESET		0x80
#define CON_MENABLE		0x40

/* Hardware Support Register */

#define HARD_SWH		0x80

/* BIOS Page Register */

#define BIOS_M16EN		0x80

/* Interrupt Control Register */

#define INT_ENABLE		0x01

/* Command Register */

#define CMD_PS1			0x80
#define CMD_PS0			0x40
#define CMD_TXP			0x04
#define CMD_STA			0x02
#define CMD_STP			0x01

/* Receive Configuration Register */

#define RCON_MON		0x20
#define RCON_PROM		0x10
#define RCON_GROUP		0x08
#define RCON_BROAD		0x04
#define RCON_RUNTS		0x02
#define RCON_SEP		0x01

/* Transmit Configuration Register */

#define TCON_LB1		0x04
#define TCON_LB0		0x02
#define TCON_CRCN		0x01

/* Data Configuration Register */

#define DCON_BSIZE1		0x40
#define DCON_BSIZE0		0x20
#define DCON_BUS16		0x01

/* Interrupt Mask Register */

#define IM_CNTE			0x20
#define IM_OVWE			0x10
#define IM_TXEE			0x08
#define IM_RXEE			0x04
#define IM_PTXE			0x02
#define IM_PRXE			0x01

/* Interrupt Status Register */

#define ISTAT_RST		0x80
#define ISTAT_CNT		0x20
#define ISTAT_OVW		0x10
#define ISTAT_TXE		0x08
#define ISTAT_RXE		0x04
#define ISTAT_PTX		0x02
#define ISTAT_PRX		0x01

/* Transmit Status Register */

#define TSTAT_OWC		0x80
#define TSTAT_CDH		0x40
#define TSTAT_UNDER		0x20
#define TSTAT_CRL		0x10
#define TSTAT_ABORT		0x08
#define TSTAT_TWC		0x04
#define TSTAT_NDT		0x02
#define TSTAT_PTX		0x01

/* Receive Status Register */

#define RSTAT_DFR		0x80
#define RSTAT_DIS		0x40
#define RSTAT_GROUP		0x20
#define RSTAT_MPA		0x10
#define RSTAT_OVER		0x08
#define RSTAT_FAE		0x04
#define RSTAT_CRC		0x02
#define RSTAT_PRX		0x01

/* Enable Features */

#define ENH_WAIT1		0x80
#define ENH_WAIT0		0x40
#define ENH_SLOT1		0x10
#define ENH_SLOT0		0x08

/* flags - software synchronize bit difinitions */

#define TXING			0x01
#define RXING			0x02
#define TXREQ			0x04

#ifdef __cplusplus
}
#endif

#endif	/* __INCif_ultrah */
