/* i82510.h - Intel 82510 header file */

/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,28oct91,wmd  added Intel modifications.
01b,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01a,20aug91,del  installed.
*/

#ifndef __INCi82510h
#define __INCi82510h

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************/
/* 		Copyright (c) 1989, Intel Corporation

   Intel hereby grants you permission to copy, modify, and
   distribute this software and its documentation.  Intel grants
   this permission provided that the above copyright notice appears
   in all copies and that both the copyright notice and this
   permission notice appear in supporting documentation.  In
   addition, Intel grants this permission provided that you
   prominently mark as not part of the original any modifications
   made to this software or documentation, and that the name of
   Intel Corporation not be used in advertising or publicity
   pertaining to distribution of the software or the documentation
   without specific, written prior permission.

   Intel Corporation does not warrant, guarantee or make any
   representations regarding the use of, or the results of the use
   of, the software and documentation in terms of correctness,
   accuracy, reliability, currentness, or otherwise; and you rely
   on the software, documentation and results solely at your own
   risk.							  */
/******************************************************************/

/* #if 0 */
typedef volatile struct
    {
    unsigned char data;	/* TxD(write)/RxD(read) (BAL when LCR Bit 7 == 1) */
    unsigned char ger;	/* General Enable Register (BAH when LCR Bit 7 == 1) */
    unsigned char gir;	/* General Interrupt/Bank Register */
    unsigned char lcr;	/* Line Configure Register */
    unsigned char mcr;	/* Modem Control Register */
    unsigned char lsr;	/* Line Status Register */
    unsigned char msr;	/* Modem/I/O Pins Status Register */
    unsigned char acr0; /* Address/Control Character Register 0 */
    } I82510_REGS;	/* 8250A Compatible Registers (NAS), BANK 0 */

typedef volatile struct
    {
    unsigned char data;	/* TxD(write)/RxD(read) */
    unsigned char rxf;	/* Received Char(read)/Transmit(write) Flags Register */
    unsigned char gir;	/* General Interrupt/Bank Register */
    unsigned char tmst;	/* Timer Status Register */
    unsigned char flr;	/* Fifo Level Register */
    unsigned char rst;	/* Receive Mach. Stat.(read)/Recieve Cmd(write) Reg */
    unsigned char tcm;	/* Same as msr(read)/Transmit Command(write) Register */
    unsigned char gsr;	/* General Status(read)/Internal Command(write) Reg */
    } I82510_WORK;	/* General Work, Bank 1 */

typedef volatile struct
    {
    unsigned char dum;	/* unused */
    unsigned char fmd;	/* Fifo Mode Register */
    unsigned char gir;	/* General Interrupt/Bank Register */
    unsigned char tmd;	/* Transmit Machine Mode Register */
    unsigned char imd;	/* Internal Mode Register */
    unsigned char acr1;	/* Address/Control Character Register 1 */
    unsigned char rie;	/* Receive Interrupt Enable Register */
    unsigned char rmd;	/* Receive Machine Mode Register */
    } I82510_GENERAL;	/* General Configuration, Bank 2 */

typedef volatile struct
    {
    unsigned char clcf;	/* Clocks Configure Register (BBL when LCR bit 7 == 1)*/
    unsigned char bacf;	/* Baud Rate Genrator A Configuration Register (BBH) */
    unsigned char gir; 	/* General Interrupt/Bank Register */
    unsigned char bbcf;	/* Baud Rate Genrator B Configuration Register */
    unsigned char pmd; 	/* I/O Pin Mode Register */
    unsigned char mie; 	/* Modem Interrupt Enable Register */
    unsigned char tmie;	/* Timers/Interrupt Enable Register */
    unsigned char dum;	/* unused */
    } I82510_MODEM;	/* Modem Configuration, Bank 3 */
/*#endif*/

/* register offsets from base address of 82510 in memory */

#define TXD		0
#define RXD		0
#define BAL		0
#define BAH		1
#define GER		1
#define GIR		2
#define BANK		2
#define LCR		3
#define MCR		4
#define LSR		5
#define MSR		6
#define ACR0		7

#define RXF		1
#define TXF		1
#define TMST		3
#define TMCR		3
#define FLR		4
#define RST		5
#define RCM		5
#define TCM		6
#define GSR		7
#define ICM		7

#define FMD		1
#define TMD		3
#define IMD		4
#define ACR1		5
#define RIE		6
#define RMD		7

#define CLCF		0
#define BBL		0
#define BACF		1
#define BBH		1
#define BBCF		3
#define PMD		4
#define MIE		5
#define TMIE		6

/* bit definitions */

/* GER, general enable register */
/* GSR, general status register, same bits as GER */

#define TIMER_BIT	0x20
#define Tx_BIT		0x10
#define MODEM_BIT	0x08
#define Rx_BIT		0x04
#define TxFIFO_BIT	0x02
#define RxFIFO_BIT	0x01

/* GIR, general interrupt register/bank register

/* bank select bits, R/W */
#define NAS0		0
#define WORK1		0x20
#define GEN2		0x40
#define MODM3		0x60

#define GIR_IP		0x01 /* interrupt pending bit, active low */
#define GIR_IMASK	0x0e /* mask for interrupt vector */
#define TIMER_INT	0x0A /* highest priority */
#define Tx_INT		0x08
#define Rx_INT		0x06
#define RxFIFO_INT	0x04
#define TxFIFO_INT	0x02
#define MODEM_INT	0x00 /* lowest priority */

/* LSR, line status register, shares 5 lsb's with RST */

#define RxBREAK_DETECT	0x10
#define RxFRAMING_ERR	0x08
#define RxPARITY_ERR	0x04
#define RxOVERRUN_ERR	0x02
#define RxCHAR_AVAIL	0x01
#define Rx_ERR_RESET (RxBREAK_DETECT|RxFRAMING_ERR|RxPARITY_ERR|RxOVERRUN_ERR)

/* ICM, Internal Command Register */

#define SOFT_RESET	0x10

/* LCR, Line Configure Register */

#define DLAB		0x80	/* divisor latch bit */
#define SET_BREAK	0x40	/* set break bit */
#define PM2		0x20	/* parity mode bit 2 */
#define PM1		0x10	/* parity mode bit 1 */
#define PM0		0x08	/* parity mode bit 0 */
#define STOP_BIT_LEN_0  0x04	/* stop bit lenght bit 0, with SBL1/2 in TMD */
#define CL1		0x02	/* character lenght bit 1, with NBCL in TMD */
#define CL0		0x01	/* character lenght bit 0, with NBCL in TMD */

#define PARITY_NONE	0	/* parity is for farmers */
#define PARITY_ODD	PM0	/* odd parity */
#define PARITY_EVEN	(PM0|PM1) /* even parity, SPF in TMD reg must be 0 */
#define PARITY_HIGH	(PM0|PM2) /* high parity, SPF in TMD reg must be 0 */
#define PARITY_LOW	(PM0|PM1|PM2)/* low parity, SPF in TMD reg must be 0 */
#define PARITY_SOFT	PM0	/* software parity, SPF in TMD reg must be 1 */

#define CHAR_LEN_5	0	/* 5 bit characters, NBCL in TMD must be 0 */
#define CHAR_LEN_6	CL0	/* 6 bit characters, NBCL in TMD must be 0 */
#define CHAR_LEN_7	CL1	/* 7 bit characters, NBCL in TMD must be 0 */
#define CHAR_LEN_8	(CL0|CL1)/* 8 bit characters, NBCL in TMD must be 0 */
#define CHAR_LEN_9	0	/* 9 bit characters, NBCL in TMD must be 1 */

/* MCR, Modem/I/O Pins Control Register */

#define DTR	0x01

/* MSR, Modem status register */

#define MSR_BITS 0x3

/* CLCF, Clock Configuration Register */

#define RxCLOCK_1X	0x80	/* 0 = 16X, 1 = 1X */
#define RxCLK_SRC_BRGA	0x40	/* 0 = BRGB, 1 = BRGA */
#define TxCLOCK_1X	0x20	/* 0 = 16X, 1 = 1X */
#define TxCLK_SRC_BRGA	0x10	/* 0 = BRGB, 1 = BRGA */

/* BAL/BBL */

#define BAUD_LO(baud)	((XTAL/(32*baud)) & 0xff)

/* BAH/BBH */
#define BAUD_HI(baud)	(((XTAL/(32*baud)) & 0xff00) >> 8)

/* set baud rate for the 18.432 Mhz Clock */

#define	BAUD_38400	16	/* XTAL/(32*BAUD) gives 15 (like manaul says) */
#define BAUD_19200	32	/* 30 */
#define BAUD_9600	64	/* 60 */
#define BAUD_4800	128	/* 120 */
#define BAUD_2400	256	/* 240 */
#define BAUD_1200	512	/* 480, so don't use these... */

#define XTAL		18432000

#ifdef __cplusplus
}
#endif

#endif /* __INCi82510h */
