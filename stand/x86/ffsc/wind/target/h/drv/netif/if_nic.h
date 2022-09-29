/* if_nic.h - NIC driver header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01j,22sep92,rrr  added support for c++
01i,26may92,rrr  the tree shuffle
01h,26may92,ajm  got rid of HOST_DEC def's (new compiler)
                  updated copyright
01g,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01f,28sep91,ajm  ifdef'd HOST_DEC for compiler problem
01e,02Aug91,rfs  simplified device struct, removed we32104 items
01d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01c,18sep89,cwp  added multiple transmit buffer support.
01b,07sep89,dab  added modification history; fixed header.
01a,12jan89,hk   written.
*/

#ifndef __INCif_nich
#define __INCif_nich

#ifdef __cplusplus
extern "C" {
#endif


typedef	union nic_regs
    {
    u_long reg [16];
    struct
		{
		volatile u_long cr;
		volatile u_long pstart;
		volatile u_long pstop;
		volatile u_long bnry;
		volatile u_long tpsr;
		volatile u_long tbcr0;
		volatile u_long tbcr1;
		volatile u_long isr;
		volatile u_long rsar0;
		volatile u_long rsar1;
		volatile u_long rbcr0;
		volatile u_long rbcr1;
		volatile u_long rcr;
		volatile u_long tcr;
		volatile u_long dcr;
		volatile u_long imr;
		} nic_pg0;
    struct
		{
		volatile u_long cr;
		volatile u_long par0;
		volatile u_long par1;
		volatile u_long par2;
		volatile u_long par3;
		volatile u_long par4;
		volatile u_long par5;
		volatile u_long curr;
		volatile u_long mar0;
		volatile u_long mar1;
		volatile u_long mar2;
		volatile u_long mar3;
		volatile u_long mar4;
		volatile u_long mar5;
		volatile u_long mar6;
		volatile u_long mar7;
		} nic_pg1;
    } NIC_DEVICE;

#define Cr		nic_pg0.cr
#define Pstart	nic_pg0.pstart
#define Pstop	nic_pg0.pstop
#define Bnry	nic_pg0.bnry
#define Tpsr	nic_pg0.tpsr
#define Tsr		nic_pg0.tpsr
#define Tbcr0	nic_pg0.tbcr0
#define Tbcr1	nic_pg0.tbcr1
#define Isr		nic_pg0.isr
#define Rsar0	nic_pg0.rsar0
#define Rsar1	nic_pg0.rsar1
#define Rbcr0	nic_pg0.rbcr0
#define Rbcr1	nic_pg0.rbcr1
#define Rcr		nic_pg0.rcr
#define Rsr		nic_pg0.rcr
#define Tcr		nic_pg0.tcr
#define Dcr		nic_pg0.dcr
#define Imr		nic_pg0.imr

#define Par0	nic_pg1.par0
#define Par1	nic_pg1.par1
#define Par2	nic_pg1.par2
#define Par3	nic_pg1.par3
#define Par4	nic_pg1.par4
#define Par5	nic_pg1.par5
#define Curr	nic_pg1.curr

#define	IO_BASE		0x00800000
#define DMA_BASE	(IO_BASE + 0x200)
#define NIC_BASE	(IO_BASE + 0x103)
#define NICRAM_BOT	0x8000
#define NICRAM_TOP	0xFFFF
#define NICROM_BOT	0x0000
#define NICROM_TOP	0x001F
#define NICROMSIZ	(NICROM_TOP - NICROM_BOT)
#define NICRAMSIZ	(NICRAM_TOP - NICRAM_BOT)

#define PSTART		0x80
#define PSTOP		0xe0
#define XMTBUF		0xe100							/* start of Tx area */
#define MAXXMT		4
#define	BNRY		PSTART
#define CURR		PSTART+1

#define  ENETB        (IO_BASE + 0x000100)        /* Ethernet base chan 2 */

#define  BURST_ON     *((char *) ENETB + 0x40)
#define  BURST_OFF    *((char *) ENETB + 0x50)
#define  NIC_BUF_PORT	((char *)0x800163)
#define  RESET_ON     *((char *) ENETB + 0x70)
#define  RESET_OFF    *((char *) ENETB + 0x74)
#define  LED_ON       *((char *) ENETB + 0x78)
#define  LED_OFF      *((char *) ENETB + 0x7C)

/*
 * CR Register bits
 */

#define STP		0x1
#define STA		0x2
#define TXP		0x4
#define RREAD	0x8
#define RWRITE	0x10
#define SPKT	0x18
#define ABORT	0x20
#define RPAGE0	0x00
#define RPAGE1	0x40
#define RPAGE2	0x80

/*
 * ISR Register
 */

#define PRX		0x1
#define PTX		0x2
#define RXE		0x4
#define TXE		0x8
#define OVW		0x10
#define CNT		0x20
#define RDC		0x40
#define RST		0x80

/*
 * IMR
 */

#define PRXE	0x1
#define PTXE	0x2
#define RXEE	0x4
#define TXEE	0x8
#define OVWE	0x10
#define CNTE	0x20
#define RDCE	0x40

/*
 * DCR
 */

#define WTS		0x1
#define BOS		0x2
#define LAS		0x4
#define NOTLS	0x8
#define ARM		0x10
#define FIFO2	0x00
#define FIFO4	0x20
#define FIFO8	0x40
#define FIFO12	0x60

/*
 * TCR
 */

#define CRC 	0x1
#define MODE0	0x0
#define MODE1	0x2
#define MODE2	0x4
#define MODE3	0x6
#define ATD		0x8
#define OFST	0x10

/*
 * TSR
 */

#define TPTX	0x1
#define COL		0x4
#define ABT		0x8
#define CRS		0x10
#define FU		0x20
#define CDH		0x40
#define OWC		0x80

/*
 * RCR
 */

#define	SEP		0x1
#define AR		0x2
#define AB		0x4
#define AM		0x8
#define PRO		0x10
#define MON		0x20

/*
 * RSR
 */

#define CRCR	0x2
#define FAE		0x4
#define FO		0x8
#define MPA		0x10
#define PHY		0x20
#define DIS		0x40
#define DFR		0x80

#define MINPKTSIZE	64

#ifdef __cplusplus
}
#endif

#endif /* __INCif_nich */
