/* if_elc.h - SMC 8013WC network interface header */ 

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,03oct94,hdn  changed TSTART to TSTART[01] for two TX buffer.
01d,03nov93,hdn  made RAMSIZE 16Kbytes.
01c,23jun93,hdn  added a structure IRQ_TABLE.
01b,17jun93,hdn  renamed to if_elc.h. 
01a,09jun92,hdn  written. 
*/


#ifndef	__INCif_elch
#define	__INCif_elch

#ifdef __cplusplus
extern "C" {
#endif

#define	SMC8013WC_RAMSIZE	0x4000
#define	SMC8013WC_TSTART0	0x00
#define	SMC8013WC_TSTART1	0x08
#define	SMC8013WC_PSTART	0x10
#define	SMC8013WC_PSTOP		0x3f

typedef struct 
    {
    UCHAR rstat;
    UCHAR next;
    UCHAR lowByteCnt;
    UCHAR uppByteCnt;
    } ELC_HEADER;

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
#define tnoerror	stat[13]
#define rnoerror	stat[14]
#define terror		stat[15]
#define rerror		stat[16]
#define overwrite	stat[17]
#define wrapped		stat[18]
#define interrupts	stat[19]
#define reset		stat[20]
#define strayint	stat[21]
    UINT stat[23];
    } ELC_STAT;

typedef struct 
    {
    struct arpcom es_ac;
#define es_if		es_ac.ac_if
#define es_enaddr	es_ac.ac_enaddr
    USHORT bicAddr;
    USHORT elcAddr;
    UINT memAddr;
    UINT memSize;
    int intLevel;
    int intVec;
    int config;
    UINT transmitPage[2];
    UINT transmitCnt;
    ELC_STAT elcStat;
    UCHAR next;
    UCHAR uppByteCnt;
    UCHAR current;
    UCHAR imask;
    UCHAR istat;
    UCHAR tstat;
    UCHAR rstat;
    UCHAR receiveBuf[0x4000];
    int flags;
    UINT txreq;
    } ELC_SOFTC;

typedef struct
    {
    UCHAR ir2;
    UCHAR ir01;
    } IRQ_TABLE;

#define	CAST

/* SMC 83c584 BIC (Bus Interface Controller) */ 

#define BIC_REG_ADDR_INTERVAL	1
#define BIC_ADRS(base,reg)   (CAST (base+(reg*BIC_REG_ADDR_INTERVAL)))

#define BIC_MSR(base)		BIC_ADRS(base,0x00)	/* memory select */
#define BIC_ICR(base)		BIC_ADRS(base,0x01)	/* interface config */
#define BIC_IAR(base)		BIC_ADRS(base,0x02)	/* IO address */
#define BIC_BIO(base)		BIC_ADRS(base,0x03)	/* BIOS ROM address */
#define BIC_IRR(base)		BIC_ADRS(base,0x04)	/* interrupt request */
#define BIC_LAAR(base)		BIC_ADRS(base,0x05)	/* LA address */
#define BIC_JUMP(base)		BIC_ADRS(base,0x06)	/* jumpers */
#define BIC_GP2(base)		BIC_ADRS(base,0x07)	/* general purpose */
#define BIC_LAR0(base)		BIC_ADRS(base,0x08)	/* LAN address */
#define BIC_LAR1(base)		BIC_ADRS(base,0x09)	/* LAN address */
#define BIC_LAR2(base)		BIC_ADRS(base,0x0a)	/* LAN address */
#define BIC_LAR3(base)		BIC_ADRS(base,0x0b)	/* LAN address */
#define BIC_LAR4(base)		BIC_ADRS(base,0x0c)	/* LAN address */
#define BIC_LAR5(base)		BIC_ADRS(base,0x0d)	/* LAN address */
#define BIC_ID(base)		BIC_ADRS(base,0x0e)	/* board ID */
#define BIC_CKSM(base)		BIC_ADRS(base,0x0f)	/* checksum */

/* MSR: Memory Select Register */
#define MSR_RST			0x80
#define MSR_MEN			0x40
#define MSR_RA18		0x20
#define MSR_RA17		0x10
#define MSR_RA16		0x08
#define MSR_RA15		0x04
#define MSR_RA14		0x02
#define MSR_RA13		0x01

/* ICR: Interface Configuration Register */
#define ICR_STO			0x80
#define ICR_RIO			0x40
#define ICR_RX7			0x20
#define ICR_RLA			0x10
#define ICR_MSZ			0x08
#define ICR_IR2			0x04
#define ICR_EAR			0x02
#define ICR_BIT16		0x01

/* IAR: IO Address Register */
#define IAR_IA15		0x80
#define IAR_IA14		0x40
#define IAR_IA13		0x20
#define IAR_IA9			0x10
#define IAR_IA8			0x08
#define IAR_IA7			0x04
#define IAR_IA6			0x02
#define IAR_IA5			0x01

/* BIO: BIOS ROM Address Register */
#define BIO_RS1			0x80
#define BIO_RS0			0x40
#define BIO_BA18		0x20
#define BIO_BA17		0x10
#define BIO_BA16		0x08
#define BIO_BA15		0x04
#define BIO_BA14		0x02
#define BIO_INT			0x01

/* EAR: EEROM Address Register */
#define EAR_EA6			0x80
#define EAR_EA5			0x40
#define EAR_EA4			0x20
#define EAR_EA3			0x10
#define EAR_RAM			0x08
#define EAR_RPE			0x04
#define EAR_RP1			0x02
#define EAR_RP			0x01

/* IRR: Interrupt Request Register */
#define IRR_IEN			0x80
#define IRR_IR1			0x40
#define IRR_IR0			0x20
#define IRR_FLSH		0x10
#define IRR_OUT3		0x08
#define IRR_OUT2		0x04
#define IRR_OUT1		0x02
#define IRR_ZWS8		0x01

/* LAAR: LA Address  Register */
#define LAAR_M16EN		0x80
#define LAAR_L16EN		0x40
#define LAAR_ZWS16		0x20
#define LAAR_LA23		0x10
#define LAAR_LA22		0x08
#define LAAR_LA21		0x04
#define LAAR_LA20		0x02
#define LAAR_LA19		0x01

/* SMC 83c690 ELC (Ethernet LAN Controller) */ 

#define ELC_REG_ADDR_INTERVAL	1
#define ELC_ADRS(base,reg)   (CAST (base+(reg*ELC_REG_ADDR_INTERVAL)))

/* page-0, read */

#define ELC_CMD(base)		ELC_ADRS(base,0x00)	/* command */
#define ELC_TRINCRL(base)	ELC_ADRS(base,0x01)	/* TRDMA incrementer */
#define ELC_TRINCRH(base)	ELC_ADRS(base,0x02)	/* TRDMA incrementer */
#define ELC_BOUND(base)		ELC_ADRS(base,0x03)	/* boundary page */
#define ELC_TSTAT(base)		ELC_ADRS(base,0x04)	/* transmit status */
#define ELC_COLCNT(base)	ELC_ADRS(base,0x05)	/* collision error */
#define ELC_INTSTAT(base)	ELC_ADRS(base,0x07)	/* interrupt status */
#define ELC_RSTAT(base)		ELC_ADRS(base,0x0c)	/* receive status */
#define ELC_ALICNT(base)	ELC_ADRS(base,0x0d)	/* alignment error */
#define ELC_CRCCNT(base)	ELC_ADRS(base,0x0e)	/* crc error */
#define ELC_MPCNT(base)		ELC_ADRS(base,0x0f)	/* missed packet */

/* page-0, write */

#define ELC_RSTART(base)	ELC_ADRS(base,0x01)	/* receive start */
#define ELC_RSTOP(base)		ELC_ADRS(base,0x02)	/* receive stop */
#define ELC_TSTART(base)	ELC_ADRS(base,0x04)	/* transmit start */
#define ELC_TCNTL(base)		ELC_ADRS(base,0x05)	/* transmit counter */
#define ELC_TCNTH(base)		ELC_ADRS(base,0x06)	/* transmit counter */
#define ELC_RBCR0(base)		ELC_ADRS(base,0x0a)	/* remote byte count */
#define ELC_RBCR1(base)		ELC_ADRS(base,0x0b)	/* remote byte count */
#define ELC_RCON(base)		ELC_ADRS(base,0x0c)	/* receive config */
#define ELC_TCON(base)		ELC_ADRS(base,0x0d)	/* transmit config */
#define ELC_DCON(base)		ELC_ADRS(base,0x0e)	/* data config */
#define ELC_INTMASK(base)	ELC_ADRS(base,0x0f)	/* interrupt mask */

/* page-1, read and write */

#define ELC_STA0(base)		ELC_ADRS(base,0x01)	/* station address */
#define ELC_STA1(base)		ELC_ADRS(base,0x02)	/* station address */
#define ELC_STA2(base)		ELC_ADRS(base,0x03)	/* station address */
#define ELC_STA3(base)		ELC_ADRS(base,0x04)	/* station address */
#define ELC_STA4(base)		ELC_ADRS(base,0x05)	/* station address */
#define ELC_STA5(base)		ELC_ADRS(base,0x06)	/* station address */
#define ELC_CURR(base)		ELC_ADRS(base,0x07)	/* current page */
#define ELC_MAR0(base)		ELC_ADRS(base,0x08)	/* multicast address */
#define ELC_MAR1(base)		ELC_ADRS(base,0x09)	/* multicast address */
#define ELC_MAR2(base)		ELC_ADRS(base,0x0a)	/* multicast address */
#define ELC_MAR3(base)		ELC_ADRS(base,0x0b)	/* multicast address */
#define ELC_MAR4(base)		ELC_ADRS(base,0x0c)	/* multicast address */
#define ELC_MAR5(base)		ELC_ADRS(base,0x0d)	/* multicast address */
#define ELC_MAR6(base)		ELC_ADRS(base,0x0e)	/* multicast address */
#define ELC_MAR7(base)		ELC_ADRS(base,0x0f)	/* multicast address */

/* page-2, read and write */

#define ELC_NEXT(base)		ELC_ADRS(base,0x05)	/* next page */
#define ELC_BLOCK(base)		ELC_ADRS(base,0x06)	/* block address */
#define ELC_ENH(base)		ELC_ADRS(base,0x07)	/* enable features */

/* COMMAND: Command Register */
#define CMD_PS1			0x80
#define CMD_PS0			0x40
#define CMD_CMD5		0x20
#define CMD_CMD4		0x10
#define CMD_CMD3		0x08
#define CMD_TXP			0x04
#define CMD_STA			0x02
#define CMD_STP			0x01

/* RCON: Receive Configuration Register */
#define RCON_MON		0x20
#define RCON_PROM		0x10
#define RCON_GROUP		0x08
#define RCON_BROAD		0x04
#define RCON_RUNTS		0x02
#define RCON_SEP		0x01

/* TCON: Transmit Configuration Register */
#define TCON_LB1		0x04
#define TCON_LB0		0x02
#define TCON_CRCN		0x01

/* DCON: Data Configuration Register */
#define DCON_BSIZE1		0x40
#define DCON_BSIZE0		0x20
#define DCON_BUS16		0x01

/* INTMASK: Interrupt Mask Register */
#define IM_XDCE			0x40
#define IM_CNTE			0x20
#define IM_OVWE			0x10
#define IM_TXEE			0x08
#define IM_RXEE			0x04
#define IM_PTXE			0x02
#define IM_PRXE			0x01

/* INTSTAT: Interrupt Status Register */
#define ISTAT_RST		0x80
#define ISTAT_CNT		0x20
#define ISTAT_OVW		0x10
#define ISTAT_TXE		0x08
#define ISTAT_RXE		0x04
#define ISTAT_PTX		0x02
#define ISTAT_PRX		0x01

/* TSTAT: Transmit Status Register */
#define TSTAT_OWC		0x80
#define TSTAT_CDH		0x40
#define TSTAT_UNDER		0x20
#define TSTAT_CRL		0x10
#define TSTAT_ABORT		0x08
#define TSTAT_TWC		0x04
#define TSTAT_NDT		0x02
#define TSTAT_PTX		0x01

/* RSTAT: Receive Status Register */
#define RSTAT_DFR		0x80
#define RSTAT_DIS		0x40
#define RSTAT_GROUP		0x20
#define RSTAT_MPA		0x10
#define RSTAT_OVER		0x08
#define RSTAT_FAE		0x04
#define RSTAT_CRC		0x02
#define RSTAT_PRX		0x01

/* ENH: Enable Features */
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

#endif	/* __INCif_elch */
